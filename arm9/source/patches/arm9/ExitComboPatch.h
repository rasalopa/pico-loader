#pragma once
#include "../Patch.h"
#include "OSResetSystemPatch.h"

/// @brief Arm9 patch that hooks the SDK IRQ dispatcher to detect an in-game
///        button combo and reboot into the launcher through the resident
///        OS_ResetSystem patch code. Optional: silently not applied when any
///        prerequisite is missing.
class ExitComboPatch : public Patch
{
public:
    explicit ExitComboPatch(const OSResetSystemPatch* osResetSystemPatch)
        : _osResetSystemPatch(osResetSystemPatch) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    const OSResetSystemPatch* _osResetSystemPatch;
    u32* _dispatchTail = nullptr;
};
