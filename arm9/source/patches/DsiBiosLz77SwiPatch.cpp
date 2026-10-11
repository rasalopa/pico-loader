#include "common.h"
#include "PatchContext.h"
#include "sharedMemory.h"
#include "ndsHeader.h"
#include "DsiBiosLz77SwiPatch.h"

// The sdk thunk: swi 0x12; bx lr. On the arm9 it sits with the other svc
// thunks near the start of the binary, so only that part is searched there.
static const u16 sThunkSwi12 = 0xDF12;
static const u16 sThunkSwi02 = 0xDF02;
static const u16 sThunkReturn = 0x4770;

bool DsiBiosLz77SwiPatch::FindPatchTarget(PatchContext& patchContext)
{
    auto romHeader = (const nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader;
    u32 start = _arm7 ? romHeader->arm7LoadAddress : romHeader->arm9LoadAddress;
    u32 size = _arm7 ? romHeader->arm7Size : romHeader->arm9Size;
    if (!_arm7 && size > 0x1000)
        size = 0x1000;

    _thunk = 0;
    for (u32 offset = 0; offset + 4 <= size; offset += 2)
    {
        const u16* ptr = (const u16*)(start + offset);
        if (ptr[0] == sThunkSwi12 && ptr[1] == sThunkReturn)
        {
            _thunk = (u32)ptr;
            break;
        }
    }

    if (_thunk)
    {
        LOG_DEBUG("Arm%d lz77 callback swi thunk found at 0x%X\n", _arm7 ? 7 : 9, _thunk);
    }
    else
    {
        LOG_DEBUG("Arm%d lz77 callback swi thunk not found\n", _arm7 ? 7 : 9);
    }

    return true; // optional patch
}

void DsiBiosLz77SwiPatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_thunk)
        return;

    *(u16*)_thunk = sThunkSwi02;
}
