#pragma once
#include "../Patch.h"
#include "ndsHeader.h"
#include "OSResetSystemPatch.h"
#include "MainMemoryArenaHi.h"

/// @brief Arm9 patch that hooks the SDK IRQ dispatcher to react to the in-game
///        exit combo the arm7 patch detects, and reboot into the launcher through
///        the OS_ResetSystem reboot code, which it emits itself when the game has
///        no OS_ResetSystem. Optional: silently not applied when any prerequisite
///        is missing.
class ExitComboPatch : public Patch
{
public:
    /// @param romHeader The header of the game, for its arm9 and arm7 placement.
    /// @param arm9Size The size of the decompressed arm9.
    /// @param bssEnd The end of the arm9 bss, or 0 when unknown.
    /// @param isCloneBootRom \c true if the rom being loaded is a clone boot rom.
    ExitComboPatch(OSResetSystemPatch* osResetSystemPatch, const nds_header_ntr_t* romHeader,
        u32 arm9Size, u32 bssEnd, bool isCloneBootRom)
        : _osResetSystemPatch(osResetSystemPatch), _romHeader(romHeader)
        , _arm9Size(arm9Size), _bssEnd(bssEnd), _isCloneBootRom(isCloneBootRom) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

    bool IsInstalled() const { return _installed != 0; }

    /// @brief Takes the patch back out, leaving the game as it would have been
    ///        without it. For when the arm7 half could not be installed, so the
    ///        exit could never fire. Must run before the game boots.
    void Deactivate();

private:
    /// @brief Finds room for \p chunkSize bytes at the top of the game's main memory
    ///        arena, right under its arena hi.
    /// @return The start of that room, or 0 when the game cannot give it.
    u32 FindBlockUnderArenaHi(u32 chunkSize);

    // 32-bit fields only: patches live in vram, where byte writes are lost on DS and DS Lite.
    OSResetSystemPatch* _osResetSystemPatch;
    const nds_header_ntr_t* _romHeader;
    u32 _arm9Size;
    u32 _bssEnd;
    u32 _isCloneBootRom;
    MainMemoryArenaHi _mainMemoryArenaHi;
    u32* _dispatchTail = nullptr;
    u32 _savedTail[2];
    // Everything written besides the hook and a lowered arena hi lives in this one
    // chunk, so a copy of what was there before is all it takes to undo it.
    u32* _chunk = nullptr;
    u32 _chunkSize = 0;
    u32* _savedChunk = nullptr;
    u32 _installed = 0;
};
