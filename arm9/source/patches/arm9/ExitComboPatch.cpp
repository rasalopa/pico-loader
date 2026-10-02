#include "common.h"
#include "patches/PatchContext.h"
#include "patches/platform/LoaderPlatform.h"
#include "sharedMemory.h"
#include "OSResetSystemPatchCode.h"
#include "ExitComboPatchCode.h"
#include "ExitComboPatch.h"

// Tail of the SDK IRQ dispatcher: ldr r1, [pc, #8]; ldr r0, [r1, r0, lsl #2];
// ldr lr, [pc, #4]; bx r0. The irq table address and the irq return address
// literals follow directly after these four instructions. This four-word
// table-dispatch-and-branch sequence is highly distinctive, so it is searched
// for directly rather than anchoring on the dispatcher prologue, whose exact
// bytes vary across SDK versions.
static const u32 sIrqDispatchTailPattern[] = { 0xE59F1008u, 0xE7910100u, 0xE59FE004u, 0xE12FFF10u };

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

    // One chunk for all of it. Unlike the mandatory patches this one gives up
    // instead of halting when the heap is exhausted.
    u32 chunkSize = SECTION_SIZE(patch_exitcombo);
    if (emitRebootCode)
    {
        chunkSize += sizeof(loader_info_t)
            + SECTION_SIZE(patch_osresetsystem) + SECTION_SIZE(patch_osresetsystem_boot);
    }
    if (!patchContext.GetPatchHeap().CanAlloc(chunkSize))
    {
        LOG_DEBUG("No patch heap space for the exit combo\n");
        return;
    }
    PatchHeap chunkHeap;
    chunkHeap.AddFreeSpace(patchContext.GetPatchHeap().Alloc(chunkSize), chunkSize);
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

    _dispatchTail[0] = 0xE51FF004; // ldr pc,= address
    _dispatchTail[1] = (u32)exitComboPatchCode->GetExitComboFunction();
    _installed = 1;

    LOG_DEBUG("Exit combo enabled\n");
}
