#pragma once
#include <array>

/// @brief Class implementing a heap intended for patch code.
///        The heap consists of a linked list of blocks of free
///        space. It is possible to add many small pieces.
class PatchHeap
{
public:
    PatchHeap();

    /// @brief Adds a block of free space to the patch heap.
    /// @param block The start of the block.
    /// @param size The size of the block.
    void AddFreeSpace(void* block, u32 size);

    /// @brief Allocated a block of the given size from the patch heap.
    /// @param size The size to allocate.
    /// @return A pointer to the allocated block if successful, or nullptr otherwise.
    void* Alloc(u32 size);

    /// @brief Checks whether a block of the given size could be allocated.
    ///        Unlike \see Alloc this does not halt when no space is available.
    /// @param size The size to check.
    /// @return True if a contiguous free block of at least the given size exists.
    bool CanAlloc(u32 size) const;

private:
    struct PatchHeapBlock
    {
        PatchHeapBlock* next;
        void* block;
        u32 size;
    };

    std::array<PatchHeapBlock, 64> _blocks;
    PatchHeapBlock* _unusedBlockPool;

    PatchHeapBlock* _freeBlocks;
};
