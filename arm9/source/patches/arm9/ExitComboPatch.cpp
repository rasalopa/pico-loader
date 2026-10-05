#include "common.h"
#include "gameCode.h"
#include "patches/PatchContext.h"
#include "patches/platform/LoaderPlatform.h"
#include "sharedMemory.h"
#include "OSResetSystemPatchCode.h"
#include "ExitComboPatchCode.h"
#include "ExitComboPatch.h"

// ExitComboPatchCode.s finds the sdk card lock word as resetParam + 0x3C0.
static_assert(offsetof(shared_memory_ntr_t, slot1Lock) - offsetof(shared_memory_ntr_t, resetParam) == 0x3C0);

// Tail of the SDK IRQ dispatcher: ldr r1, [pc, #8]; ldr r0, [r1, r0, lsl #2];
// ldr lr, [pc, #4]; bx r0. The irq table address and the irq return address
// literals follow directly after these four instructions. This four-word
// table-dispatch-and-branch sequence is highly distinctive, so it is searched
// for directly rather than anchoring on the dispatcher prologue, whose exact
// bytes vary across SDK versions.
static const u32 sIrqDispatchTailPattern[] = { 0xE59F1008u, 0xE7910100u, 0xE59FE004u, 0xE12FFF10u };

// These games lay out their own memory up to a fixed address near the top of main
// memory instead of asking the sdk, so a lower arena hi would not keep them out of
// a block taken from there.
static bool keepsOwnHeapTop(u32 gameCode)
{
    switch (gameCode & 0xFFFFFF)
    {
        case GAMECODE_NO_REGION("ARM"): // Mario & Luigi: Partners in Time
        case GAMECODE_NO_REGION("CLJ"): // Mario & Luigi: Bowser's Inside Story
        case GAMECODE_NO_REGION("AH9"): // Tony Hawk's American Sk8land
            return true;
        default:
            return false;
    }
}

// Main memory is mirrored, so ranges are compared by their offset into it.
static bool overlapsPhysically(u32 a, u32 aSize, u32 b, u32 bSize)
{
    a &= 0x3FFFFF;
    b &= 0x3FFFFF;
    return a < b + bSize && b < a + aSize;
}

bool ExitComboPatch::FindPatchTarget(PatchContext& patchContext)
{
    _dispatchTail = patchContext.FindPattern32(sIrqDispatchTailPattern, sizeof(sIrqDispatchTailPattern));
    if (_dispatchTail)
    {
        LOG_DEBUG("Irq dispatch tail found at 0x%p\n", _dispatchTail);
    }
    else
    {
        LOG_DEBUG("Irq dispatch tail not found\n");
    }
    return true; // optional patch
}

void ExitComboPatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_dispatchTail)
        return;

    // A game without an OS_ResetSystem of its own gets the reboot code from here.
    // That code calls the platform sd read code, which the card read patches have
    // already put in, except on a platform that reads the rom some other way.
    bool emitRebootCode = !_osResetSystemPatch->HasTarget();
    if (emitRebootCode && patchContext.GetLoaderPlatform()->HasRomReads())
        return;

    // One chunk for all of it, so it can be taken back as a whole. Unlike the
    // mandatory patches this one gives up instead of halting when the heap is
    // exhausted.
    u32 chunkSize = SECTION_SIZE(patch_exitcombo);
    if (emitRebootCode)
    {
        chunkSize += sizeof(loader_info_t)
            + SECTION_SIZE(patch_osresetsystem) + SECTION_SIZE(patch_osresetsystem_boot);
    }
    // When the patch heap has no room left, the chunk goes in a block taken from the
    // top of the game's main heap instead, and the game is told its heap ends there.
    u32 block = 0;
    if (patchContext.GetPatchHeap().CanAlloc(chunkSize))
    {
        _chunk = (u32*)patchContext.GetPatchHeap().Alloc(chunkSize);
    }
    else
    {
        block = FindBlockUnderArenaHi(chunkSize);
        if (!block)
        {
            LOG_DEBUG("No room for the exit combo\n");
            return;
        }
        _chunk = (u32*)block;
    }
    _chunkSize = chunkSize;

    // Before anything goes in: loader_info_t is copied over as soon as it is allocated.
    _savedChunk = new u32[chunkSize / 4];
    for (u32 i = 0; i < chunkSize / 4; i++)
        _savedChunk[i] = _chunk[i];

    PatchHeap chunkHeap;
    chunkHeap.AddFreeSpace(_chunk, chunkSize);
    if (emitRebootCode)
    {
        _osResetSystemPatch->EmitRebootCode(patchContext, chunkHeap);
    }
    auto rebootCode = _osResetSystemPatch->GetRebootCode();

    u32 blockAddress = (u32)&TWL_SHARED_MEMORY->exitRequest;
    u32 resetParamAddress = (u32)&TWL_SHARED_MEMORY->ntrSharedMem.resetParam;
    if (!patchContext.GetSdkVersion().IsTwlSdk())
    {
        // SDK2-4 games access shared memory through the NTR mirror
        blockAddress -= 0x800000;
        resetParamAddress -= 0x800000;
    }

    auto exitComboPatchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<ExitComboPatchCode>
    (
        chunkHeap,
        blockAddress,
        resetParamAddress,
        (u32)rebootCode->GetPoolAtTarget(),
        (u32)rebootCode->GetBootLoaderBinariesFunction(),
        _dispatchTail[4], // irq table address literal
        _dispatchTail[5]  // irq return address literal
    );

    _savedTail[0] = _dispatchTail[0];
    _savedTail[1] = _dispatchTail[1];
    _dispatchTail[0] = 0xE51FF004; // ldr pc,= address
    _dispatchTail[1] = (u32)exitComboPatchCode->GetExitComboFunction();
    if (block)
    {
        _mainMemoryArenaHi.Lower(block);
        LOG_DEBUG("Main memory arena hi lowered to 0x%x\n", block);
    }
    _installed = 1;

    LOG_DEBUG("Exit combo enabled\n");
}

u32 ExitComboPatch::FindBlockUnderArenaHi(u32 chunkSize)
{
    // A clone boot game sends the start of its arm9 to download play clients from
    // memory, and a lowered arena hi there could break that.
    if (_isCloneBootRom || keepsOwnHeapTop(_romHeader->gameCode))
        return 0;
    if (!_mainMemoryArenaHi.Find((u32*)_romHeader->arm9LoadAddress, _arm9Size))
        return 0;

    u32 blockSize = (chunkSize + 31) & ~31;
    u32 block = _mainMemoryArenaHi.GetOriginal() - blockSize;

    // above everything the arm9 binary takes, its bss included
    u32 arm9End = _romHeader->arm9LoadAddress + _arm9Size;
    if (_bssEnd > arm9End)
        arm9End = _bssEnd;
    if (block < arm9End)
        return 0;

    // and clear of the arm7 binary, which is still being loaded while this runs
    if (overlapsPhysically(block, blockSize, _romHeader->arm7LoadAddress, _romHeader->arm7Size))
        return 0;

    return block;
}

void ExitComboPatch::Deactivate()
{
    if (!_installed)
        return;

    // the hook only if it still jumps into the chunk
    if (_dispatchTail[0] == 0xE51FF004 && _dispatchTail[1] - (u32)_chunk < _chunkSize)
    {
        _dispatchTail[0] = _savedTail[0];
        _dispatchTail[1] = _savedTail[1];
    }
    _mainMemoryArenaHi.Restore();

    for (u32 i = 0; i < _chunkSize / 4; i++)
        _chunk[i] = _savedChunk[i];

    delete[] _savedChunk;
    _savedChunk = nullptr;
    _installed = 0;

    LOG_DEBUG("Exit combo taken back out\n");
}
