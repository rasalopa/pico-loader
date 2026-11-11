#include "common.h"
#include <stddef.h>
#include "header.h"

extern "C" void _start();
extern u8 __bss_start[];
extern u8 __bss_size[];

[[gnu::section(".crt0")]]
[[gnu::used]]
pload_header7_t gLoaderHeader
{
    .entryPoint = (void*)&_start,
    .apiVersion = PICO_LOADER_API_VERSION,
    .enhanced = { .magic = PLOAD_ENHANCED_MAGIC }
};

static_assert(offsetof(pload_header7_t, reservedForOfficialParts) == 0x414);
static_assert(offsetof(pload_header7_t, enhanced) == 0x480);