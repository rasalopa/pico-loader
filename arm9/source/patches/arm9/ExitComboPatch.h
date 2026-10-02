#pragma once
#include "../Patch.h"
#include "OSResetSystemPatch.h"

/// @brief Arm9 patch that hooks the SDK IRQ dispatcher to react to the in-game
///        exit combo the arm7 patch detects, and reboot into the launcher through
///        the OS_ResetSystem reboot code, which it emits itself when the game has
///        no OS_ResetSystem. Optional: silently not applied when any prerequisite
///        is missing.
class ExitComboPatch : public Patch
{
public:
    explicit ExitComboPatch(OSResetSystemPatch* osResetSystemPatch)
        : _osResetSystemPatch(osResetSystemPatch) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    bool IsInstalled() const { return _installed != 0; }

    /// @brief Takes the patch back out, leaving the game as it would have been
    ///        without it. For when the arm7 half could not be installed, so the
    ///        exit could never fire. Must run before the game boots.
    void Deactivate();

private:
    // 32-bit fields only: patches live in vram, where byte writes are lost on DS and DS Lite.
    OSResetSystemPatch* _osResetSystemPatch;
    u32* _dispatchTail = nullptr;
    u32 _savedTail[2];
    // Everything written besides the hook lives in this one chunk, so a copy of
    // what was there before is all it takes to undo it.
    u32* _chunk = nullptr;
    u32 _chunkSize = 0;
    u32* _savedChunk = nullptr;
    u32 _installed = 0;
};
