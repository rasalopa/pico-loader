#pragma once
#include "../Patch.h"
#include "OSResetSystemPatch.h"

/// @brief Arm9 patch that hooks the SDK IRQ dispatcher to react to the in-game
///        exit combo the arm7 patch detects, and reboot into the launcher through
///        the resident OS_ResetSystem patch code. Optional: silently not applied
///        when any prerequisite is missing.
class ExitComboPatch : public Patch
{
public:
    explicit ExitComboPatch(const OSResetSystemPatch* osResetSystemPatch)
        : _osResetSystemPatch(osResetSystemPatch) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    bool IsInstalled() const { return _installed != 0; }

private:
    // 32-bit fields only: patches live in vram, where byte writes are lost on DS and DS Lite.
    const OSResetSystemPatch* _osResetSystemPatch;
    u32* _dispatchTail = nullptr;
    u32 _installed = 0;
};
