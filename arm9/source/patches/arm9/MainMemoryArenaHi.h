#pragma once

/// @brief The top of the game's main memory arena, as its OS_GetInitArenaHi returns
///        it for OS_ARENA_MAIN. Only the arm code that loads it from a literal is
///        handled; the thumb and the immediate forms are not.
class MainMemoryArenaHi
{
public:
    /// @brief Searches the decompressed arm9 for the literal. It searches only once,
    ///        so it can be asked again for free.
    /// @param image The decompressed arm9.
    /// @param imageSize The size of \p image in bytes.
    /// @return True when the literal was found.
    bool Find(u32* image, u32 imageSize);

    /// @brief The value the game would use, once \see Find returned true.
    u32 GetOriginal() const { return _original; }

    /// @brief Makes the game's main memory arena end at \p arenaHi instead.
    void Lower(u32 arenaHi);

    /// @brief Puts the original value back, unless something else wrote the literal
    ///        since \see Lower did.
    void Restore();

private:
    // 32-bit fields only: this lives in vram, where byte writes are lost on DS and DS Lite.
    u32* _literal = nullptr;
    u32 _searched = 0;
    u32 _original = 0;
    u32 _lowered = 0;
};
