#include "common.h"
#include <memory>
#include <string.h>
#include <libtwl/ipc/ipcFifo.h>
#include <libtwl/ipc/ipcSync.h>
#include <libtwl/i2c/i2cMcu.h>
#include <libtwl/sio/sioRtc.h>
#include <libtwl/sound/sound.h>
#include <libtwl/sound/soundChannel.h>
#include <libtwl/sound/soundCapture.h>
#include "core/Environment.h"
#include "logger/NitroEmulatorOutputStream.h"
#include "logger/PicoAgbAdapterOutputStream.h"
#include "logger/NocashOutputStream.h"
#include "logger/NullLogger.h"
#include "logger/PlainLogger.h"
#include "fat/dldi.h"
#include "loader/NdsLoader.h"
#include "sharedMemory.h"
#include "ndsHeader.h"
#include "globalHeap.h"
#include "mmc/tmio.h"
#include <libtwl/sys/twlScfg.h>

#define HANDSHAKE_PART0     0xA
#define HANDSHAKE_PART1     0xB
#define HANDSHAKE_PART2     0xC
#define HANDSHAKE_PART3     0xD

ILogger* gLogger;
FATFS gFatFs;

static NdsLoader sLoader;

static void initLogger()
{
    std::unique_ptr<IOutputStream> outputStream;
    if (Environment::IsIsNitroEmulator() && Environment::SupportsAgbSemihosting())
    {
        outputStream = std::make_unique<NitroEmulatorOutputStream>();
    }
    // else if (Environment::HasPicoAgbAdapter())
    // {
    //     outputStream = std::make_unique<PicoAgbAdapterOutputStream>();
    // }
    else if (Environment::SupportsNocashPrint())
    { 
        outputStream = std::make_unique<NocashOutputStream>();
    }
    else
    {
        gLogger = new NullLogger();
        return;
    }
    gLogger = new PlainLogger(LogLevel::All, std::move(outputStream));
}

static bool mountDldi()
{
    FRESULT res = f_mount(&gFatFs, "fat:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("dldi mount failed: %d\n", res);
        return false;
    }
    f_chdrive("fat:");
    return true;
}

static bool mountDsiSd()
{
    FRESULT res = f_mount(&gFatFs, "sd:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("dsi sd mount failed: %d\n", res);
        return false;
    }
    f_chdrive("sd:");
    return true;
}

static bool mountAgbSemihosting()
{
    FRESULT res = f_mount(&gFatFs, "pc2:", 1);
    if (res != FR_OK)
    {
        LOG_ERROR("pc2 sd mount failed: %d\n", res);
        return false;
    }
    f_chdrive("pc2:");
    return true;
}

extern "C" void __libc_init_array();

static void handleSavePath()
{
    if (gLoaderHeader.loadParams.savePath[0] == 0)
    {
        char* savePath = (char*)gLoaderHeader.loadParams.savePath;
        strcpy(savePath, gLoaderHeader.loadParams.romPath);
        char* extension = strrchr(savePath, '.');
        if (!extension)
            extension = &savePath[strlen(savePath)];
        extension[0] = '.';
        extension[1] = 's';
        extension[2] = 'a';
        extension[3] = 'v';
        extension[4] = 0;
    }
    sLoader.SetSavePath(gLoaderHeader.loadParams.savePath);
}

// Copy of the exit request block in the low 4MB mirror, where a DS-mode game
// on a DSi (running with 4MB RAM) writes it.
#define EXIT_REQUEST_LOW_MIRROR ((exit_request_block_t*)0x023FF680)

static bool isExitRequestPathValid(const exit_request_block_t& block)
{
    if (block.pathMagic != EXIT_REQUEST_PATH_MAGIC)
    {
        return false;
    }
    if (block.launcherPath[0] == 0 ||
        block.launcherPath[sizeof(block.launcherPath) - 1] != 0)
    {
        return false;
    }
    return block.pathChecksum == exit_request_calcChecksum(block.launcherPath);
}

static bool isExitRequestBlockValid(const exit_request_block_t& block)
{
    return block.requestMagic == EXIT_REQUEST_REQUEST_MAGIC && isExitRequestPathValid(block);
}

// Returns the copy of the dldi driver the last game kept for the launcher, or
// nullptr. Only the launcher path has to check out: after the game's own soft
// reset no exit was requested, but the copy is still where it was kept.
static void* findCarriedDldiDriver()
{
    const exit_request_block_t* blocks[] = { &TWL_SHARED_MEMORY->exitRequest, EXIT_REQUEST_LOW_MIRROR };
    for (const exit_request_block_t* block : blocks)
    {
        u32 address = block->dldiDriverAddress;
        if (isExitRequestPathValid(*block) && address >= 0x02000000 && address < 0x03000000 && (address & 3) == 0)
        {
            return (void*)address;
        }
    }
    return nullptr;
}

// Returns a pending valid exit request, or nullptr. Both copies are checked
// because a DS-mode game on a DSi may only reach the low mirror.
static exit_request_block_t* findExitRequest()
{
    if (isExitRequestBlockValid(TWL_SHARED_MEMORY->exitRequest))
    {
        return &TWL_SHARED_MEMORY->exitRequest;
    }
    if (isExitRequestBlockValid(*EXIT_REQUEST_LOW_MIRROR))
    {
        return EXIT_REQUEST_LOW_MIRROR;
    }
    return nullptr;
}

static void clearExitRequest()
{
    TWL_SHARED_MEMORY->exitRequest.requestMagic = 0;
    TWL_SHARED_MEMORY->exitRequest.comboMagic = 0;
    EXIT_REQUEST_LOW_MIRROR->requestMagic = 0;
    EXIT_REQUEST_LOW_MIRROR->comboMagic = 0;
}

static void clearSoundRegisters()
{
    REG_SOUNDCNT = 0;
    REG_SNDCAP0CNT = 0;
    REG_SNDCAP1CNT = 0;

    for (int i = 0; i < 16; i++)
    {
        REG_SOUNDxCNT(i) = 0;
        REG_SOUNDxSAD(i) = 0;
        REG_SOUNDxTMR(i) = 0;
        REG_SOUNDxPNT(i) = 0;
        REG_SOUNDxLEN(i) = 0;
    }
}

static void initIpc()
{
    ipc_clearSendFifo();
    ipc_ackFifoError();
    ipc_disableRecvFifoNotEmptyIrq();
    ipc_enableFifo();

    while (!ipc_isRecvFifoEmpty())
    {
        ipc_recvWordDirect();
    }

    ipc_setArm7SyncBits(HANDSHAKE_PART0);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART0);
    ipc_setArm7SyncBits(HANDSHAKE_PART1);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART1);
    ipc_setArm7SyncBits(HANDSHAKE_PART2);
    while (ipc_getArm9SyncBits() != HANDSHAKE_PART2);
    ipc_setArm7SyncBits(HANDSHAKE_PART3);
    while (ipc_getArm9SyncBits() == HANDSHAKE_PART2);
}

extern "C" void loaderMain()
{
    __libc_init_array();

    clearSoundRegisters();
    rtos_initIrq();
    rtos_startMainThread();
    initIpc();

    bool dsiMode = ipc_getArm9SyncBits() == 1;

    Environment::Initialize(dsiMode);
    heap_init();
    initLogger();

    rtc_init(); // ensure rtc irqs are disabled

    LOG_DEBUG("Pico Loader ARM7 started\n");

    if (Environment::IsDsiMode())
    {
        if ((REG_SCFG_EXT & 0xC000u) == 0)
        {
            // After a soft reset from a ds mode game that kept the dsi bios, the
            // arm7 side is still configured as a ds. Put back what a fresh dsi
            // boot has, except the main memory size: what the game left behind
            // is read through its 4 MB view until NdsLoader restores the rest.
            REG_SCFG_EXT = 0x93FFF707u & ~0xC000u;
            REG_SCFG_CLK = 0x187;
        }
        // Let the mcu handle the power button
        mcu_writeReg(MCU_REG_MODE, 0);
        TMIO_init();
    }

    memset(&gFatFs, 0, sizeof(gFatFs));
    bool multiboot = (gLoaderHeader.bootDrive & PLOAD_BOOT_DRIVE_MULTIBOOT_FLAG) != 0;
    gLoaderHeader.bootDrive &= ~PLOAD_BOOT_DRIVE_MULTIBOOT_FLAG;
    bool usingCarriedDldi = false;
    bool retailSoftReset = !multiboot &&
        ((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader)->arm7EntryAddress == (u32)gLoaderHeader.entryPoint;
    if (retailSoftReset && gLoaderHeader.bootDrive == PLOAD_BOOT_DRIVE_DLDI && gLoaderHeader.dldiDriver == nullptr)
    {
        // A soft reset reloads Pico Loader without a dldi driver in its header.
        // Use the copy the game kept, if any, so this boot can pass it on.
        gLoaderHeader.dldiDriver = findCarriedDldiDriver();
        usingCarriedDldi = gLoaderHeader.dldiDriver != nullptr;
    }
    switch (gLoaderHeader.bootDrive)
    {
        case PLOAD_BOOT_DRIVE_DLDI:
        {
            bool dldiReady = dldi_init();
            if (!dldiReady && usingCarriedDldi)
            {
                // The copy the game kept did not start. The platform code can
                // still reach the sd card, as before there was a copy.
                LOG_WARNING("Carried dldi driver failed, using the platform code\n");
                dldi_forgetDriver();
                gLoaderHeader.dldiDriver = nullptr;
                dldiReady = dldi_init();
            }
            if (dldiReady)
            {
                mountDldi();
            }
            break;
        }
        case PLOAD_BOOT_DRIVE_DSI_SD:
        {
            if (Environment::IsDsiMode())
            {
                mountDsiSd();
            }
            break;
        }
        case PLOAD_BOOT_DRIVE_AGB_SEMIHOSTING:
        {
            if (Environment::SupportsAgbSemihosting())
            {
                mountAgbSemihosting();
            }
            break;
        }
    }

    if (gLoaderHeader.v3.cheats != nullptr && gLoaderHeader.v3.cheats->numberOfCheats != 0)
    {
        // Copy cheats to vram
        auto cheats = (pload_cheats_t*)malloc(gLoaderHeader.v3.cheats->length);
        memcpy(cheats, gLoaderHeader.v3.cheats, gLoaderHeader.v3.cheats->length);
        sLoader.SetCheats(cheats);
    }

    if (multiboot)
    {
        LOG_DEBUG("Multiboot\n");
        // a multiboot child must not inherit the parent's exit request block
        memset(&TWL_SHARED_MEMORY->exitRequest, 0, sizeof(exit_request_block_t));
        sLoader.Load(BootMode::Multiboot);
    }
    else if (retailSoftReset)
    {
        LOG_DEBUG("Retail soft reset detected\n");
        u32 originalArm7EntryAddress = ((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.cardRomHeader)->arm7EntryAddress;
        ((nds_header_ntr_t*)TWL_SHARED_MEMORY->ntrSharedMem.romHeader)->arm7EntryAddress = originalArm7EntryAddress;
        exit_request_block_t* exitRequest = findExitRequest();
        // clear both copies first so a stale request can never fire twice
        clearExitRequest();
        if (exitRequest)
        {
            LOG_DEBUG("Exit to launcher requested\n");
            // after a soft reset the header contains the file image, so the
            // launcher path only survives inside the exit request block;
            // gLoaderHeader lives in vram and is safe from ClearMainMemory
            strncpy(gLoaderHeader.loadParams.romPath, exitRequest->launcherPath,
                sizeof(gLoaderHeader.loadParams.romPath) - 1);
            gLoaderHeader.loadParams.romPath[sizeof(gLoaderHeader.loadParams.romPath) - 1] = 0;
            gLoaderHeader.loadParams.savePath[0] = 0;
            sLoader.SetCheats(nullptr); // the copied cheats belong to the game
            sLoader.SetRomPath(gLoaderHeader.loadParams.romPath);
            handleSavePath();
            sLoader.SetArguments(nullptr, 0);
            // boot the launcher exactly like a fresh launcher boot would,
            // pointing its return path at itself
            sLoader.SetLauncherPath(gLoaderHeader.loadParams.romPath);
            sLoader.Load(BootMode::Normal);
        }
        else
        {
            sLoader.Load(BootMode::SdkResetSystem);
        }
    }
    else
    {
        sLoader.SetRomPath(gLoaderHeader.loadParams.romPath);
        handleSavePath();
        sLoader.SetArguments(gLoaderHeader.loadParams.arguments, gLoaderHeader.loadParams.argumentsLength);
        sLoader.SetLauncherPath(gLoaderHeader.v2.launcherPath);
        sLoader.Load(BootMode::Normal);
    }

    while (true);
}