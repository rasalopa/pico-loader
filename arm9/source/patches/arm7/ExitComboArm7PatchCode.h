#pragma once
#include "sections.h"
#include "patches/PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_exitcombo_arm7);

extern "C" void patch_exitcombo_arm7_entry(void);

extern u32 patch_exitcombo_arm7_block_address;
extern u32 patch_exitcombo_arm7_table_address;
extern u32 patch_exitcombo_arm7_return_address;

class ExitComboArm7PatchCode : public PatchCode
{
public:
    ExitComboArm7PatchCode(PatchHeap& patchHeap, u32 blockAddress,
        u32 irqTableAddress, u32 irqReturnAddress)
        : PatchCode(SECTION_START(patch_exitcombo_arm7), SECTION_SIZE(patch_exitcombo_arm7), patchHeap)
    {
        patch_exitcombo_arm7_block_address = blockAddress;
        patch_exitcombo_arm7_table_address = irqTableAddress;
        patch_exitcombo_arm7_return_address = irqReturnAddress;
    }

    const void* GetExitComboFunction() const
    {
        return GetAddressAtTarget((void*)patch_exitcombo_arm7_entry);
    }
};
