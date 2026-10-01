#include "common.h"
#include "PatchHeap.h"

PatchHeap::PatchHeap()
    : _freeBlocks(nullptr)
{
    _unusedBlockPool = &_blocks[0];
    for (u32 i = 0; i < _blocks.size() - 1; i++)
        _blocks[i].next = &_blocks[i + 1];
    _blocks[_blocks.size() - 1].next = nullptr;
}

void PatchHeap::AddFreeSpace(void* block, u32 size)
{
    PatchHeapBlock* cur = _freeBlocks;

    bool added = false;
    while (cur)
    {
        if ((u8*)cur->block + cur->size == block)
        {
            cur->size += size;
            added = true;
            break;
        }
        else if ((u8*)block + size == cur->block)
        {
            cur->block = block;
            cur->size += size;
            added = true;
            break;
        }

        cur = cur->next;
    }

    if (added)
        return;

    PatchHeapBlock* heapBlock = _unusedBlockPool;
    _unusedBlockPool = _unusedBlockPool->next;

    heapBlock->block = block;
    heapBlock->size = size;
    heapBlock->next = _freeBlocks;
    _freeBlocks = heapBlock;
}

bool PatchHeap::CanAlloc(u32 size) const
{
    const PatchHeapBlock* cur = _freeBlocks;
    while (cur)
    {
        if (cur->size >= size)
            return true;
        cur = cur->next;
    }
    return false;
}

void* PatchHeap::Alloc(u32 size)
{
    PatchHeapBlock* prev = nullptr;
    PatchHeapBlock* cur = _freeBlocks;

    PatchHeapBlock* bestBlock = nullptr;
    PatchHeapBlock* bestBlockPrev = nullptr;

    while (cur)
    {
        if (cur->size >= size && (!bestBlock || cur->size < bestBlock->size))
        {
            bestBlock = cur;
            bestBlockPrev = prev;
            if (bestBlock->size == size)
                break;
        }

        prev = cur;
        cur = cur->next;
    }

    if (!bestBlock)
    {
        LOG_FATAL("No space found to put patch of size 0x%x\n", size);
        while (1);
        return nullptr;
    }

    void* result = bestBlock->block;

    if (bestBlock->size == size)
    {
        if (bestBlockPrev)
            bestBlockPrev->next = bestBlock->next;

        if (_freeBlocks == bestBlock)
            _freeBlocks = bestBlock->next;

        bestBlock->next = _unusedBlockPool;
        _unusedBlockPool = bestBlock;
    }
    else
    {
        bestBlock->block = (u8*)bestBlock->block + size;
        bestBlock->size -= size;
    }

    LOG_DEBUG("Allocated 0x%p, size = 0x%X\n", result, size);

    return result;
}
