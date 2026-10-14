#pragma once
#include "../Patch.h"
#include "OSResetSystemPatch.h"

/// @brief Arm9 patch that hooks the SDK IRQ dispatcher to react to the in-game
///        exit combo the arm7 patch detects, and reboot into the launcher through
///        the OS_ResetSystem reboot code, which it emits itself when the game has
///        no OS_ResetSystem. Optional: silently not applied when any prerequisite
///        is missing, or when its pieces do not all fit what the patch heap has left.
class ExitComboPatch : public Patch
{
public:
    explicit ExitComboPatch(OSResetSystemPatch* osResetSystemPatch)
        : _osResetSystemPatch(osResetSystemPatch) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    bool IsInstalled() const { return _installed != 0; }

    /// @brief Takes the hook back out and puts back what the game had where the
    ///        pieces went. For when the arm7 half could not be installed, so the
    ///        exit could never fire. Must run before the game boots.
    void Deactivate();

private:
    // 32-bit fields only: patches live in vram, where byte writes are lost on DS and DS Lite.
    OSResetSystemPatch* _osResetSystemPatch;
    u32* _dispatchTail = nullptr;
    u32 _savedTail[2];
    u32 _hookEntry = 0;
    u32* _savedGameWords = nullptr;
    u32 _savedRegionAddress[7]; // loader_info_t, both reboot code sections and the four pieces
    u32 _savedRegionSize[7];
    u32 _savedRegionCount = 0;
    u32 _installed = 0;
};
