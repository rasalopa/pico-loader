#pragma once
#include "../Patch.h"

/// @brief Arm7 patch that hooks the SDK irq dispatcher to publish a heartbeat
///        and, when the arm9 side asks for it, quiesce the arm7 and hand it over
///        to Pico Loader. This is what makes an in-game exit work on games that
///        have no OS_ResetSystem of their own. Optional: silently not applied
///        when the dispatcher is not found.
class ExitComboArm7Patch : public Patch
{
public:
    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    u32* _dispatchTail = nullptr;
};
