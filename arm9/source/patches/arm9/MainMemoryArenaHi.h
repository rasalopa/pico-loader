#pragma once

/// @brief The top of the game's main memory arena, as its OS_GetInitArenaHi returns
///        it for OS_ARENA_MAIN. Handled: the arm and thumb builds that load it from a
///        literal, and the sdk 2.0 builds that make it as an immediate, in place or
///        in a leaf case that only returns it. Not the games that also run in dsi
///        mode, which choose between two values at run time.
class MainMemoryArenaHi
{
public:
    /// @brief Bytes of code \see Lower puts next to the block for the leaf build,
    ///        which returns the lowered value from there.
    static constexpr u32 STUB_SIZE = 12;

    /// @brief The builds \see Find tells apart.
    static constexpr u32 FORM_LITERAL = 1;
    static constexpr u32 FORM_IMMEDIATE_IN_PLACE = 2;
    static constexpr u32 FORM_IMMEDIATE_LEAF = 3;

    /// @brief Searches the decompressed arm9 for where the value comes from. It
    ///        searches only once, so it can be asked again for free.
    /// @param image The decompressed arm9.
    /// @param imageSize The size of \p image in bytes.
    /// @return True when it was found in a build this handles.
    bool Find(u32* image, u32 imageSize);

    /// @brief The value the game would use, once \see Find returned true.
    u32 GetOriginal() const { return _original; }

    /// @brief The room \see Lower needs for its code: \see STUB_SIZE for the leaf
    ///        build, 0 for the others.
    u32 GetStubSize() const { return _form == FORM_IMMEDIATE_LEAF ? STUB_SIZE : 0; }

    /// @brief Whether \see Lower can make the arena end at \p arenaHi: the immediate
    ///        builds only take a distance an arm immediate can hold.
    bool CanLowerTo(u32 arenaHi) const;

    /// @brief Makes the game's main memory arena end at \p arenaHi instead.
    /// @param stub Room for \see GetStubSize bytes of code, or null when that is 0.
    void Lower(u32 arenaHi, u32* stub);

    /// @brief Puts the original value or code back, unless something else wrote
    ///        there since \see Lower did.
    void Restore();

private:
    // 32-bit fields only: this lives in vram, where byte writes are lost on DS and DS Lite.
    // The site is the literal, the mov r0 of the in place build, or the switch's b to
    // the leaf case.
    u32* _site = nullptr;
    u32 _form = 0;
    u32 _searched = 0;
    u32 _original = 0;
    u32 _lowered = 0;
    u32 _saved[2] = { 0, 0 };
    u32 _written[2] = { 0, 0 };
};
