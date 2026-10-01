#pragma once
#include "../PatchCode.h"
#include "sections.h"

DEFINE_SECTION_SYMBOLS(patch_exitcombo);

extern "C" void patch_exitcombo_entry(void);

extern u32 patch_exitcombo_block_address;
extern u32 patch_exitcombo_resetparam_address;
extern u32 patch_exitcombo_resetentry_address;
extern u32 patch_exitcombo_table_address;
extern u32 patch_exitcombo_return_address;

class ExitComboPatchCode : public PatchCode
{
public:
    ExitComboPatchCode(PatchHeap& patchHeap, u32 blockAddress, u32 resetParamAddress,
        u32 resetEntryAddress, u32 irqTableAddress, u32 irqReturnAddress)
        : PatchCode(SECTION_START(patch_exitcombo), SECTION_SIZE(patch_exitcombo), patchHeap)
    {
        patch_exitcombo_block_address = blockAddress;
        patch_exitcombo_resetparam_address = resetParamAddress;
        patch_exitcombo_resetentry_address = resetEntryAddress;
        patch_exitcombo_table_address = irqTableAddress;
        patch_exitcombo_return_address = irqReturnAddress;
    }

    const void* GetExitComboFunction() const
    {
        return GetAddressAtTarget((void*)patch_exitcombo_entry);
    }
};
