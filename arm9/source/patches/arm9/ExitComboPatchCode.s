.cpu arm946e-s
.section "patch_exitcombo", "ax"
.syntax unified
.arm

// Arm9 half of the in-game exit to the launcher.
//
// Reached through ldr pc from the first instruction of the SDK irq dispatcher
// tail, so r0 holds the irq index and the original r1/lr loads did not run yet.
// The bios saved r0-r3, r12 and lr before entering the dispatcher, so those are
// free to use; r4-r11 must be preserved.
//
// Deliberately small. The arm9 patch heap is scraped out of gaps in the game's
// secure area and measured in a couple of hundred bytes, and PatchHeap gives up
// on the whole patch when it does not fit, so every instruction here costs
// coverage. It still has to carry the teardown itself rather than push it into
// the shared reboot code: that code is allocated for every game whose own
// OS_ResetSystem was found, so growing it can starve a game that boots today.
.global patch_exitcombo_entry
.type patch_exitcombo_entry, %function
patch_exitcombo_entry:
    cmp r0, #0 // irq index 0 = vblank
    bne dispatch

    ldr r1, patch_exitcombo_block_address
    mcr p15, 0, r1, c7, c6, 1 // invalidate the line, the arm7 has no cache
    ldr r2, [r1, #0x0C] // exit_request_block_t::comboMagic
    ldr r3, comboMagic
    cmp r2, r3
    bne dispatch

    ldr r3, ioBase
    mov r2, #0
    str r2, [r3, #0x208] // IME = 0. Per cpu, so the arm7 keeps taking its vblank.

    // Publish a known sync out before the request can be seen, otherwise the
    // arm7 could read a 1 the game left behind and hand over too early.
    strb r2, [r3, #0x181] // REG_IPC_SYNC data out

    ldr r12, requestMagic
    str r12, [r1] // exit_request_block_t::requestMagic
    mcr p15, 0, r1, c7, c10, 1 // clean dcache entry by MVA
    mcr p15, 0, r2, c7, c10, 4 // drain write buffer

    // Wait for the arm7 to park. It set comboMagic, so it is running its hook
    // and will see the request on its next vblank.
1:
    ldrb r12, [r3, #0x180] // REG_IPC_SYNC data in
    cmp r12, #1
    bne 1b

    // Take the card back: the loader hands slot 1 to the arm7 before booting a
    // game and only the arm9 can undo that. Done after the arm7 parked so we are
    // not pulling the bus out from under it.
    add r0, r3, #0x204 // REG_EXMEMCNT, halfword offsets are only 8 bit
    ldrh r12, [r0]
    bic r12, r12, #0x880
    strh r12, [r0]

    // Drain any in flight card transfer so the cart is left between commands
    // before the reboot code issues its own reads. The default read path is cpu
    // polled, so with the arm7 parked we are the only reader left.
    ldr r1, regCardData
2:
    ldr r12, [r3, #0x1A4] // REG_ROMCTRL
    tst r12, #0x80000000 // block busy?
    beq 3f
    tst r12, #0x00800000 // data word ready?
    ldrne r12, [r1] // consume and discard
    b 2b
3:
    // the game side of OS_ResetSystem would have quiesced its own dma
    str r2, [r3, #0xB8] // DMA0CNT
    str r2, [r3, #0xC4] // DMA1CNT
    str r2, [r3, #0xD0] // DMA2CNT
    str r2, [r3, #0xDC] // DMA3CNT

    ldr r3, patch_exitcombo_resetparam_address
    str r2, [r3]

    // Into the reboot code past its reset of the game's arm7, which ours already
    // parked, with the registers its entry would have loaded from its pool.
    ldr r0, patch_exitcombo_resetpool_address
    ldmia r0, {r0, r1, r3, r4, r5, r6, r7}
    mov r9, r4 // readSdSectors
    mov r10, r7 // bootPicoLoader
    ldr r12, patch_exitcombo_bootloaderbinaries_address
    bx r12 // thumb

dispatch:
    // the dispatcher tail displaced by the hook
    ldr r1, patch_exitcombo_table_address
    ldr r0, [r1, r0, lsl #2]
    ldr lr, patch_exitcombo_return_address
    bx r0

.balign 4

ioBase:
    .word 0x04000000

regCardData:
    .word 0x04100010

comboMagic:
    .word 0x424D4F43 // EXIT_REQUEST_COMBO_MAGIC

requestMagic:
    .word 0x54495845 // EXIT_REQUEST_REQUEST_MAGIC

.global patch_exitcombo_block_address
patch_exitcombo_block_address:
    .word 0

.global patch_exitcombo_resetparam_address
patch_exitcombo_resetparam_address:
    .word 0

.global patch_exitcombo_resetpool_address
patch_exitcombo_resetpool_address:
    .word 0

.global patch_exitcombo_bootloaderbinaries_address
patch_exitcombo_bootloaderbinaries_address:
    .word 0

.global patch_exitcombo_table_address
patch_exitcombo_table_address:
    .word 0

.global patch_exitcombo_return_address
patch_exitcombo_return_address:
    .word 0

.pool
.end
