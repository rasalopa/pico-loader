#pragma once
#include "loader/dldiHeader.h"

bool dldi_init();
bool dldi_patchTo(dldi_header_t* stub);
void dldi_copyTo(void* target);

/// @brief Gets the dldi driver built into Pico Loader for the platform, placed
///        where the launcher passes its own, or \c nullptr when there is none.
///        The in-game exit needs it: the reloaded loader can still reach the sd
///        card through the platform patch code, but that is not a driver it can
///        hand to the launcher.
void* dldi_getBuiltInDriver();

/// @brief Checks whether the loaded dldi driver is the one built into Pico Loader.
bool dldi_isBuiltInDriverLoaded();

/// @brief Drops the driver in the buffer, so that it is no longer handed on,
///        after it failed to start.
void dldi_forgetDriver();

#ifdef __cplusplus
extern "C" {
#endif

bool dldi_readSectors(void* buffer, u32 sector, u32 count);
bool dldi_writeSectors(const void* buffer, u32 sector, u32 count);

#ifdef __cplusplus
}
#endif
