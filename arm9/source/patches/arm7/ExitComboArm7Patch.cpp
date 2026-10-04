#include "common.h"
#include "patches/PatchContext.h"
#include "sharedMemory.h"
#include "ExitComboArm7PatchCode.h"
#include "ReadSavePatchCode.h"
#include "WriteSavePatchCode.h"
#include "VerifySavePatchCode.h"
#include "ExitComboArm7Patch.h"

// Tail of the SDK irq dispatcher, the same four word sequence used on the arm9
// side: ldr r1, [pc, #8]; ldr r0, [r1, r0, lsl #2]; ldr lr, [pc, #4]; bx r0.
// The irq table address and the irq return address literals follow it.
static const u32 sIrqDispatchTailPattern[] = { 0xE59F1008u, 0xE7910100u, 0xE59FE004u, 0xE12FFF10u };

bool ExitComboArm7Patch::FindPatchTarget(PatchContext& patchContext)
{
    if (_searched)
        return true;

    _searched = 1;
    _dispatchTail = patchContext.FindPattern32(sIrqDispatchTailPattern, sizeof(sIrqDispatchTailPattern));
    if (_dispatchTail)
    {
        LOG_DEBUG("Arm7 irq dispatch tail found at 0x%p\n", _dispatchTail);
    }
    else
    {
        LOG_DEBUG("Arm7 irq dispatch tail not found\n");
    }
    return true; // optional patch
}

void ExitComboArm7Patch::ApplyPatch(PatchContext& patchContext)
{
    if (!_dispatchTail)
        return;

    // unlike the mandatory patches this one gives up instead of halting
    // when the heap is exhausted
    if (!patchContext.GetPatchHeap().CanAlloc(SECTION_SIZE(patch_exitcombo_arm7)))
    {
        LOG_DEBUG("No arm7 patch heap space for the exit combo\n");
        return;
    }

    u32 blockAddress = (u32)&TWL_SHARED_MEMORY->exitRequest;
    if (!patchContext.GetSdkVersion().IsTwlSdk())
    {
        // SDK2-4 games access shared memory through the ntr mirror
        blockAddress -= 0x800000;
    }

    // The save code reaches the sd card through these, set by the save patch applied
    // before this one, or 0 when the game got none. All three save routines use the
    // same read.
    u32 sdRead = readsave_sdread_asm_address;
    u32 sdWrite = writesave_sdwrite_asm_address;

    auto exitComboPatchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<ExitComboArm7PatchCode>
    (
        patchContext.GetPatchHeap(),
        blockAddress,
        sdRead,
        sdWrite,
        _dispatchTail[4], // irq table address literal
        _dispatchTail[5]  // irq return address literal
    );

    _dispatchTail[0] = 0xE51FF004; // ldr pc,= address
    _dispatchTail[1] = (u32)exitComboPatchCode->GetExitComboFunction();
    _installed = 1;

    // Route the save code through the stub, so it knows when a save is running.
    // These addresses are template values that are only copied once every patch
    // has been applied, so changing them here still reaches the copy.
    if (sdRead)
    {
        u32 readWrapper = (u32)exitComboPatchCode->GetSdReadFunction();
        if (readsave_sdread_asm_address == sdRead)
            readsave_sdread_asm_address = readWrapper;
        if (writesave_sdread_asm_address == sdRead)
            writesave_sdread_asm_address = readWrapper;
        if (verifysave_sdread_asm_address == sdRead)
            verifysave_sdread_asm_address = readWrapper;
    }
    if (sdWrite)
    {
        writesave_sdwrite_asm_address = (u32)exitComboPatchCode->GetSdWriteFunction();
    }

    LOG_DEBUG("Arm7 exit combo enabled\n");
}
