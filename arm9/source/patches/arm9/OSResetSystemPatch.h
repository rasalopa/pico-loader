#pragma once
#include "../Patch.h"
#include "LoaderInfo.h"

class PatchHeap;
class OSResetSystemPatchCode;

/// @brief Arm9 patch to make OS_ResetSystem reboot into Pico Loader.
class OSResetSystemPatch : public Patch
{
public:
    OSResetSystemPatch(const loader_info_t* loaderInfo, bool runInDSiMode)
        : _runInDSiMode(runInDSiMode), _loaderInfo(loaderInfo) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    /// @brief \c true when the game has an OS_ResetSystem, so \see ApplyPatch emits the reboot code.
    bool HasTarget() const
    {
        return _osResetSystem != nullptr;
    }

    /// @brief Emits the code that reboots into Pico Loader, without hooking anything.
    ///        \see ApplyPatch uses it for the game's own OS_ResetSystem, and the
    ///        in-game exit for a game that has none.
    /// @param patchContext The patch context to use.
    /// @param patchHeap The heap to take loader_info_t and both code sections from,
    ///        and the platform sd read code when the collection does not have it yet.
    void EmitRebootCode(PatchContext& patchContext, PatchHeap& patchHeap);

    /// @brief Returns the reboot code, or \c nullptr when it was not emitted.
    const OSResetSystemPatchCode* GetRebootCode() const
    {
        return _rebootCode;
    }

    void** GetCheatsPointerAtTarget() const
    {
        return _cheatsPointer;
    }

private:
    u32* _osResetSystem = nullptr;
    u16 _hybrid = false;
    u16 _runInDSiMode;
    const loader_info_t* _loaderInfo;
    void** _cheatsPointer = nullptr;
    const OSResetSystemPatchCode* _rebootCode = nullptr;
};
