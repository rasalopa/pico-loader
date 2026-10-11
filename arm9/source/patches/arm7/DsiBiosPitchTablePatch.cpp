#include "common.h"
#include "../PatchContext.h"
#include "AutoloadAdjuster.h"
#include "moduleParams.h"
#include "sharedMemory.h"
#include "ndsHeader.h"
#include "DsiBiosPitchTablePatchAsm.h"
#include "DsiBiosPitchTablePatch.h"

// The sdk thunk: swi 0x1B; bx lr.
static const u16 sThunkSwi1B = 0xDF1B;
static const u16 sThunkReturn = 0x4770;

// Arm callers reach the thumb thunk through linker veneers. Absolute form:
// ldr r12, [pc]; bx r12; .word thunk. Relative form: ldr r12, [pc, #4];
// add r12, pc, r12; bx r12; .word thunk-(add+8).
static const u32 sVeneerLdr = 0xE59FC000;
static const u32 sVeneerBx = 0xE12FFF1C;
static const u32 sVeneerLdrRel = 0xE59FC004;
static const u32 sVeneerAddRel = 0xE08FC00C;

static bool isPlausibleModuleParams(const nds_header_ntr_t* romHeader, const module_params_ntr_t* moduleParams)
{
    u32 start = romHeader->arm7LoadAddress;
    u32 end = start + romHeader->arm7Size;
    if ((u32)moduleParams < start || (u32)moduleParams + 12 > end)
        return false;
    u32 listStart = moduleParams->autoloadListStart;
    u32 listEnd = moduleParams->autoloadListEnd;
    if (listStart < start || listEnd > end || listEnd < listStart
        || (listEnd - listStart) % sizeof(autoload_list_entry_t) != 0
        || moduleParams->autoloadStart < start || moduleParams->autoloadStart > end)
    {
        return false;
    }

    // The autoloaded data ends where the list starts. This also rejects the
    // 16 byte entries of a twl arm7, whose count can be a multiple of 12 bytes.
    u32 dataEnd = moduleParams->autoloadStart;
    for (auto entry = (const autoload_list_entry_t*)listStart; entry != (const autoload_list_entry_t*)listEnd; entry++)
    {
        dataEnd += entry->size;
    }
    return dataEnd == listStart;
}

// The sdk crt0 fetches the module params through a literal near the entry:
// ldr r0, [pc, #n]; ldr r1, [r0]; ldr r2, [r0, #4]; ldr r3, [r0, #8]. The
// early sdks put no magic words after the params, so this comes first.
static const module_params_ntr_t* findArm7ModuleParams(const nds_header_ntr_t* romHeader)
{
    u32 start = romHeader->arm7LoadAddress;
    u32 end = start + romHeader->arm7Size;
    u32 limit = romHeader->arm7Size < 0x400 ? romHeader->arm7Size : 0x400;
    for (u32 offset = 0; offset + 16 <= limit; offset += 4)
    {
        const u32* ptr = (const u32*)(start + offset);
        if ((ptr[0] & 0xFFFFF000) == 0xE59F0000 && ptr[1] == 0xE5901000 && ptr[2] == 0xE5902004 && ptr[3] == 0xE5903008)
        {
            u32 literal = (u32)ptr + 8 + (ptr[0] & 0xFFF);
            if (literal + 4 > end)
                continue;
            auto moduleParams = (const module_params_ntr_t*)*(const u32*)literal;
            if (isPlausibleModuleParams(romHeader, moduleParams))
                return moduleParams;
        }
    }

    for (u32 i = 0; i + 8 <= romHeader->arm7Size; i += 4)
    {
        u32* ptr = (u32*)(start + i);
        if (ptr[0] == MODULE_PARAMS_NTR_MAGIC_BE && ptr[1] == MODULE_PARAMS_NTR_MAGIC_LE)
        {
            auto moduleParams = (const module_params_ntr_t*)((u8*)ptr + 8 - sizeof(module_params_ntr_t));
            if (isPlausibleModuleParams(romHeader, moduleParams))
                return moduleParams;
        }
    }
    return nullptr;
}

bool DsiBiosPitchTablePatch::FindPatchTarget(PatchContext& patchContext)
{
    auto romHeader = (const nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader;
    _moduleParams = 0;
    _thunkAddress = 0;

    auto moduleParams = findArm7ModuleParams(romHeader);
    if (!moduleParams)
    {
        LOG_DEBUG("Arm7 module params not found, pitch table left alone\n");
        return true;
    }

    u32 thunk = 0;
    for (u32 offset = 0; offset + 4 <= romHeader->arm7Size; offset += 2)
    {
        const u16* ptr = (const u16*)(romHeader->arm7LoadAddress + offset);
        if (ptr[0] == sThunkSwi1B && ptr[1] == sThunkReturn)
        {
            thunk = (u32)ptr;
            break;
        }
    }

    if (!thunk)
    {
        LOG_DEBUG("Arm7 pitch table thunk not found\n");
        return true;
    }

    AutoloadAdjuster<autoload_list_entry_t> autoload(
        (const autoload_list_entry_t*)moduleParams->autoloadListStart,
        (const autoload_list_entry_t*)moduleParams->autoloadListEnd,
        moduleParams->autoloadStart);
    _moduleParams = (u32)moduleParams;
    _thunkAddress = autoload.AdjustInitialToFinal(thunk) | 1;
    LOG_DEBUG("Arm7 pitch table thunk at 0x%X, runs at 0x%X\n", thunk, _thunkAddress);
    return true; // optional patch
}

void DsiBiosPitchTablePatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_thunkAddress)
        return;

    if (!patchContext.GetPatchHeap().CanAlloc(SECTION_SIZE(patch_dsibiospitch7)))
    {
        LOG_DEBUG("No arm7 patch heap space for the pitch table stub\n");
        return;
    }

    auto patchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<DsiBiosPitchTablePatchCode>(
        patchContext.GetPatchHeap());
    u32 armStub = (u32)patchCode->GetArmFunction();
    u32 thumbStub = (u32)patchCode->GetThumbFunction();

    auto romHeader = (const nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader;
    auto moduleParams = (const module_params_ntr_t*)_moduleParams;
    AutoloadAdjuster<autoload_list_entry_t> autoload(
        (const autoload_list_entry_t*)moduleParams->autoloadListStart,
        (const autoload_list_entry_t*)moduleParams->autoloadListEnd,
        moduleParams->autoloadStart);

    u32 start = romHeader->arm7LoadAddress;
    u32 size = romHeader->arm7Size & ~3;
    u32 veneers = 0;
    u32 branches = 0;
    u32 unreachable = 0;

    for (u32 offset = 0; offset + 12 <= size; offset += 4)
    {
        u32* ptr = (u32*)(start + offset);
        if (ptr[0] == sVeneerLdr && ptr[1] == sVeneerBx && ptr[2] == _thunkAddress)
        {
            ptr[2] = armStub;
            veneers++;
        }
        else if (offset + 16 <= size && ptr[0] == sVeneerLdrRel && ptr[1] == sVeneerAddRel && ptr[2] == sVeneerBx)
        {
            u32 addPc = autoload.AdjustInitialToFinal((u32)ptr) + 12;
            if (addPc + ptr[3] == _thunkAddress)
            {
                ptr[3] = armStub - addPc;
                veneers++;
            }
        }
    }

    // Thumb callers branch straight to the thunk with bl, which only reaches
    // 4 MB each way, so those get the thumb stub when it is close enough.
    for (u32 offset = 0; offset + 4 <= size; offset += 2)
    {
        u16* ptr = (u16*)(start + offset);
        if ((ptr[0] & 0xF800) != 0xF000 || (ptr[1] & 0xF800) != 0xF800)
            continue;

        u32 branchOffset = ((u32)(ptr[0] & 0x7FF) << 12) | ((u32)(ptr[1] & 0x7FF) << 1);
        if (ptr[0] & 0x400)
            branchOffset |= 0xFF800000;
        u32 site = autoload.AdjustInitialToFinal((u32)ptr) + 4;
        if ((site + branchOffset) != (_thunkAddress & ~1))
            continue;

        s32 newOffset = (s32)(thumbStub - site);
        if (newOffset < -0x400000 || newOffset >= 0x400000)
        {
            unreachable++;
            continue;
        }

        ptr[0] = 0xF000 | (((u32)newOffset >> 12) & 0x7FF);
        ptr[1] = 0xF800 | (((u32)newOffset >> 1) & 0x7FF);
        branches++;
    }

    if (unreachable > 0)
    {
        LOG_WARNING("Arm7 pitch table: %d calls out of reach of the stub\n", unreachable);
    }
    LOG_DEBUG("Arm7 pitch table: %d veneers and %d branches redirected\n", veneers, branches);
}
