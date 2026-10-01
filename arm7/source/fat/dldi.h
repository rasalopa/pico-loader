#pragma once
#include "loader/dldiHeader.h"

bool dldi_init();
bool dldi_patchTo(dldi_header_t* stub);
void dldi_copyTo(void* target);

/// @brief Returns the size of the loaded dldi driver, or 0 when there is none.
///        Used to carry the driver across a reboot into Pico Loader, which the
///        in-game exit needs: the reloaded loader can still reach the sd card
///        through the platform patch code, but that is not a driver it can hand
///        to homebrew.
u32 dldi_getDriverSize();

/// @brief Copies the loaded dldi driver, at most \p size bytes.
void dldi_copyToSized(void* target, u32 size);

#ifdef __cplusplus
extern "C" {
#endif

bool dldi_readSectors(void* buffer, u32 sector, u32 count);
bool dldi_writeSectors(const void* buffer, u32 sector, u32 count);

#ifdef __cplusplus
}
#endif
