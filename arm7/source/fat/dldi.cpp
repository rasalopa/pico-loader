#include "common.h"
#include <string.h>
#include "ipc.h"
#include "ipcCommands.h"
#include "loader/DldiDriver.h"

static u8 sDldiBuffer[16 * 1024] alignas(32);
static DldiDriver sDldiDriver = DldiDriver((dldi_header_t*)sDldiBuffer);

[[gnu::target("thumb")]]
static bool readSectorsWithPatchCode(u32 sector, u32 count, void* buffer)
{
    typedef void (*patch_code_read_sd_sectors_t)(u32 srcSector, void* dst, u32 sectorCount);
    (*(patch_code_read_sd_sectors_t*)0x037F8000)(sector, buffer, count);
    return true;
}

[[gnu::target("thumb")]]
static bool writeSectorsWithPatchCode(u32 sector, u32 count, const void* buffer)
{
    typedef void (*patch_code_write_sd_sectors_t)(u32 dstSector, const void* src, u32 sectorCount);
    (*(patch_code_write_sd_sectors_t*)0x037F8004)(sector, buffer, count);
    return true;
}

bool dldi_init()
{
    auto driver = (const dldi_header_t*)gLoaderHeader.dldiDriver;
    if (!driver || driver->dldiMagic != DLDI_MAGIC || driver->driverMagic == DLDI_DRIVER_MAGIC_NONE)
    {
        LOG_DEBUG("No dldi driver found\n");

        // Need to initialize before getting the patch code
        sendToArm9(IPC_COMMAND_ARM9_INITIALIZE_SD_CARD);
        if (!receiveFromArm9())
        {
            LOG_ERROR("Sd card initialization failed\n");
            return false;
        }

        // Try to get the patch code
        sendToArm9(IPC_COMMAND_ARM9_GET_SD_FUNCTIONS);
        if (!receiveFromArm9())
        {
            LOG_ERROR("Getting patch code failed\n");
            return false;
        }

        LOG_DEBUG("Using patch code sd read/write\n");
        ((dldi_header_t*)sDldiBuffer)->readSectorsFuncAddress = (u32)readSectorsWithPatchCode;
        ((dldi_header_t*)sDldiBuffer)->writeSectorsFuncAddress = (u32)writeSectorsWithPatchCode;
    }
    else
    {
        u32 driverSize = 1 << driver->driverSize;
        if (driverSize > sizeof(sDldiBuffer))
        {
            LOG_ERROR("Not enough space for dldi driver of size %d\n", driverSize);
            return false;
        }

        memcpy(sDldiBuffer, driver, driverSize);

        sDldiDriver.Relocate();
        sDldiDriver.PrepareForUse();

        if (!sDldiDriver.Startup())
        {
            LOG_ERROR("DLDI startup failed\n");
            return false;
        }

        sendToArm9(IPC_COMMAND_ARM9_INITIALIZE_SD_CARD);
        if (!receiveFromArm9())
        {
            LOG_ERROR("Sd card initialization failed\n");
            return false;
        }
    }

    return true;
}

extern "C" bool dldi_readSectors(void* buffer, u32 sector, u32 count)
{
    return sDldiDriver.ReadSectors(sector, count, buffer);
}

extern "C" bool dldi_writeSectors(const void* buffer, u32 sector, u32 count)
{
    return sDldiDriver.WriteSectors(sector, count, buffer);
}

bool dldi_patchTo(dldi_header_t* stub)
{
    return sDldiDriver.PatchTo(stub);
}

void dldi_copyTo(void* target)
{
    memcpy(target, sDldiBuffer, sizeof(sDldiBuffer));
}

void dldi_forgetDriver()
{
    ((dldi_header_t*)sDldiBuffer)->dldiMagic = 0;
}

void* dldi_getBuiltInDriver()
{
    sendToArm9(IPC_COMMAND_ARM9_GET_BUILT_IN_DLDI_DRIVER);
    return (void*)receiveFromArm9();
}

bool dldi_isBuiltInDriverLoaded()
{
    auto loaded = (const dldi_header_t*)sDldiBuffer;
    if (loaded->dldiMagic != DLDI_MAGIC)
    {
        return false;
    }
    dldi_header_t builtIn;
    u32* builtInWords = (u32*)&builtIn;
    sendToArm9(IPC_COMMAND_ARM9_GET_BUILT_IN_DLDI_HEADER);
    builtInWords[0] = receiveFromArm9();
    if (builtInWords[0] == 0)
    {
        return false;
    }
    for (u32 i = 1; i < sizeof(dldi_header_t) / 4; i++)
    {
        builtInWords[i] = receiveFromArm9();
    }
    // By name and io type, not byte by byte: builds of a driver differ with the
    // compiler, and the platform code already relies on the card protocol the
    // built-in one speaks.
    return builtIn.driverMagic == loaded->driverMagic
        && strncmp((const char*)builtIn.driverName, (const char*)loaded->driverName,
            sizeof(loaded->driverName)) == 0;
}
