#pragma once
#include "Patch.h"

/// @brief Patch for games that run on the bios of the DSi. The bios of the DS
///        implements the lz77 decompression with a read callback as swi 0x12.
///        The bios of the DSi has it at swi 0x02 and ignores 0x12, so the sdk
///        thunk is pointed at the right number. Works on either cpu.
///        Optional: not applied when the thunk is not found.
class DsiBiosLz77SwiPatch : public Patch
{
public:
    /// @param arm7 \c true to patch the arm7 binary, \c false for the arm9 one.
    explicit DsiBiosLz77SwiPatch(bool arm7) : _arm7(arm7 ? 1 : 0) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    // 32-bit fields only: patches live in vram, where byte writes are lost on DS and DS Lite.
    u32 _arm7;
    u32 _thunk = 0;
};
