#pragma once
#include "../LoaderPlatform.h"
#include "DSPicoReadSdSectorsPatchCode.h"
#include "DSPicoReadSdSectorDmaPatchCode.h"
#include "DSPicoWriteSdSectorsPatchCode.h"
#include "dspicoDldi_bin.h"

/// @brief Implementation of LoaderPlatform for the DS pico flashcard
class DSPicoLoaderPlatform : public LoaderPlatform
{
public:
    const IReadSectorsPatchCode* CreateSdReadPatchCode(
        PatchCodeCollection& patchCodeCollection, PatchHeap& patchHeap) const override
    {
        return patchCodeCollection.GetOrAddSharedPatchCode([&]
        {
            return new DSPicoReadSdSectorsPatchCode(patchHeap,
                patchCodeCollection.GetOrAddSharedPatchCode([&]
                {
                    return new DSPicoReadSdSectorsDirectPatchCode(patchHeap);
                }));
        });
    }

    const IReadSectorsDmaPatchCode* CreateSdReadDmaPatchCode(PatchCodeCollection& patchCodeCollection,
        PatchHeap& patchHeap, const void* miiCardDmaCopy32Ptr) const override
    {
        auto pollSdDataReadyPatchCode = patchCodeCollection.GetOrAddSharedPatchCode([&]
        {
            return new DSPicoReadSdSectorDmaPollSdDataReadyPatchCode(patchHeap);
        });

        return patchCodeCollection.AddUniquePatchCode<DSPicoReadSdSectorDmaPatchCode>(
            patchHeap, pollSdDataReadyPatchCode, miiCardDmaCopy32Ptr);
    }

    const IWriteSectorsPatchCode* CreateSdWritePatchCode(
        PatchCodeCollection& patchCodeCollection, PatchHeap& patchHeap) const override
    {
        return patchCodeCollection.GetOrAddSharedPatchCode([&]
        {
            return new DSPicoWriteSdSectorsPatchCode(patchHeap);
        });
    }

    LoaderPlatformType GetPlatformType() const override { return LoaderPlatformType::Slot1; }

    bool HasDmaSdReads() const override { return true; }

    /// @note DSpico.dldi from LNH-team/dspico-dldi v1.0.1 (commit 8ba45f6), unchanged.
    ///       It sends the same card commands as the patch code above, so a change
    ///       to that protocol needs a new copy here as well.
    const void* GetBuiltInDldiDriver(u32& size) const override
    {
        size = dspicoDldi_bin_size;
        return dspicoDldi_bin;
    }
};
