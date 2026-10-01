#pragma once
#include "../Patch.h"
#include "LoaderInfo.h"

/// @brief Arm9 patch to make OS_ResetSystem reboot into Pico Loader.
class OSResetSystemPatch : public Patch
{
public:
    /// @param emitWithoutTarget Emit the reboot code even when the game has no
    ///        OS_ResetSystem to hook. The in-game exit combo needs the code in
    ///        that case, and plenty of games (Mario Kart DS, Metal Slug 7, ...)
    ///        never call OS_ResetSystem at all.
    OSResetSystemPatch(const loader_info_t* loaderInfo, bool runInDSiMode, bool emitWithoutTarget)
        : _runInDSiMode(runInDSiMode), _loaderInfo(loaderInfo), _emitWithoutTarget(emitWithoutTarget) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    void** GetCheatsPointerAtTarget() const
    {
        return _cheatsPointer;
    }

    /// @brief Returns the entry to reboot into Pico Loader for a caller that has
    ///        already parked the arm7, or nullptr when the code was not emitted.
    const void* GetArm7ParkedEntryAtTarget() const
    {
        return _arm7ParkedEntryAtTarget;
    }

private:
    u32* _osResetSystem = nullptr;
    u16 _hybrid = false;
    u16 _runInDSiMode;
    const loader_info_t* _loaderInfo;
    u16 _emitWithoutTarget;
    void** _cheatsPointer = nullptr;
    const void* _arm7ParkedEntryAtTarget = nullptr;
};
