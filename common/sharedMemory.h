#pragma once
#include <nds/ndstypes.h>

#define BOOT_TYPE_INVALID   0
#define BOOT_TYPE_CARD      1
#define BOOT_TYPE_MULTIBOOT 2
#define BOOT_TYPE_NAND      3
#define BOOT_TYPE_MEMORY    4

struct shared_memory_multi_boot_info_t
{
    u16 bootType;
    u8 multibootInfo[0x40 - 2];
};

//0x02FFF800: 2KB
struct shared_memory_ntr_t
{
    u8 gap0[0x200];
    u8 gap200[0x80];
    u8 cardRomHeader[0x160]; // without debug footer
    u8 downloadParams[0x20];
    u8 bootCheckInfo[0x20];
    u32 resetParam;
    u8 gap424[8];
    u32 romOffset;
    u8 slot2Info[0xC];
    u32 vblankCounter;
    shared_memory_multi_boot_info_t multibootInfo;
    u8 firmwareUserData[0x100];
    u8 arm9ExceptionStack[0x1C];
    u32 arm9ExceptionVector;
    u8 arenaInfo[0x48];
    u8 rtcData[8];
    u8 systemConfig[6];
    u8 arm9Print;
    u8 arm7Print;
    u8 arm9ErrorPrint;
    u8 arm7ErrorPrint;
    u8 gap1FA[6];
    u8 romHeader[0x160];
    u8 isDebuggerData[0x20];
    u32 arm9IpcSignalParam;
    u32 arm7IpcSignalParam;
    u32 arm9IpcHandleChecker;
    u32 arm7IpcHandleChecker;
    u32 lastMicrophoneAddress;
    u16 microphoneSampleData;
    u16 wifiCallbackControl;
    u16 wifiRssiPool;
    u8 setSlot2InfoOnce;
    u8 slot2Inserted;
    u32 arm7Param;
    u32 arm9ThreadInfo;
    u32 arm7ThreadInfo;
    u16 padXYBuffer;
    u8 touchData[4];
    u16 autoloadSync;
    u8 arm9LockId[8];
    u8 arm7LockId[8];
    u8 vramCLock[8];
    u8 vramDLock[8];
    u8 wram0Lock[8];
    u8 wram1Lock[8];
    u8 slot1Lock[8];
    u8 slot2Lock[8];
    u8 initLock[8];
    u16 arm9MainMemoryCheck;
    u16 arm7MainMemoryCheck;
    u16 mainMemoryCmd;
};

static_assert(sizeof(shared_memory_ntr_t) == 0x800);

#define NTR_SHARED_MEMORY      ((shared_memory_ntr_t*)0x027FF800)
#define NTR_SHARED_MEMORY_SDK5 ((shared_memory_ntr_t*)0x02FFF800)

#define EXIT_REQUEST_REQUEST_MAGIC  0x54495845 // 'EXIT'
#define EXIT_REQUEST_PATH_MAGIC     0x58454C50 // 'PLEX'
#define EXIT_REQUEST_COMBO_MAGIC    0x424D4F43 // 'COMB'
#define EXIT_REQUEST_CHECKSUM_XOR   0x5A5A5A5A

/// @brief Block used to request an in-game exit to the launcher.
///        The loader fills pathMagic, pathChecksum and launcherPath at boot;
///        the arm9 exit combo patch code only ever writes requestMagic, and the
///        arm7 exit patch code only ever writes comboMagic.
/// @note  The first four words deliberately share one cache line: the arm9 patch
///        code invalidates that single line to read the comboMagic published by
///        the arm7, which has no cache of its own.
struct exit_request_block_t
{
    u32 requestMagic;
    u32 pathMagic;
    u32 pathChecksum;

    /// @brief Set by the arm7 exit patch code once the user has held the combo.
    ///        The arm7 owns the whole detection because its patch heap has room
    ///        to spare, while the arm9 one is measured in a couple of hundred
    ///        bytes. It also makes a missing arm9 patch harmless: nothing ever
    ///        consumes this and the combo just does nothing.
    u32 comboMagic;

    char launcherPath[256];

    /// @brief Where the loader kept a copy of the dldi driver for the next boot,
    ///        or zero. A soft reset, the game's own or the in-game exit, reloads
    ///        Pico Loader without a driver in its header; it takes this one, so
    ///        the launcher gets its filesystem back and the next game can keep
    ///        a copy again.
    u32 dldiDriverAddress;
};

static_assert(sizeof(exit_request_block_t) == 0x114);

static inline u32 exit_request_calcChecksum(const char (&launcherPath)[256])
{
    u32 sum = 0;
    for (u32 i = 0; i < sizeof(launcherPath); i++)
        sum += (u8)launcherPath[i];
    return sum ^ EXIT_REQUEST_CHECKSUM_XOR;
}

//0x02FFC000: 16KB
struct shared_memory_twl_t
{
    u8 twlCardRomHeader[0x1000];
    u8 gap1000[0x7B0];
    char sysMenuVersionInfoContentId[9];
    char sysMenuVersionInfoContentLastInitialCode;
    u16 sysMenuDisableSetHotboot;
    u8 sdNandContext[0x44];
    u8 titleIdList[0x400];
    u8 mountInfo[0x3C0];
    u8 bootSrlPath[0x40];
    u8 twlRomHeader[0x1000];
    u8 sharedArena[0x680];
    exit_request_block_t exitRequest;
    u8 gap3794[0x180 - sizeof(exit_request_block_t)];
    shared_memory_ntr_t ntrSharedMem;
};

static_assert(sizeof(shared_memory_twl_t) == 0x4000);

#define TWL_SHARED_MEMORY ((shared_memory_twl_t*)0x02FFC000)
