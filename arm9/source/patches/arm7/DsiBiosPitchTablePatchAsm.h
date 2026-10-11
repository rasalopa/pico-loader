#pragma once
#include "../PatchCode.h"
#include "sections.h"

DEFINE_SECTION_SYMBOLS(patch_dsibiospitch7);

extern "C" void patch_dsibiospitch7_arm(void);
extern "C" void patch_dsibiospitch7_thumb(void);

class DsiBiosPitchTablePatchCode : public PatchCode
{
public:
    explicit DsiBiosPitchTablePatchCode(PatchHeap& patchHeap)
        : PatchCode(SECTION_START(patch_dsibiospitch7), SECTION_SIZE(patch_dsibiospitch7), patchHeap) { }

    const void* GetArmFunction() const
    {
        return GetAddressAtTarget((void*)patch_dsibiospitch7_arm);
    }

    const void* GetThumbFunction() const
    {
        return GetAddressAtTarget((void*)((u32)patch_dsibiospitch7_thumb & ~1));
    }
};
