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
// Deliberately small. It goes in what the arm9 patch heap has left after every
// other patch, and otherwise in a block taken from the top of the game's main
// heap, so every instruction here can cost a game its fit in the patch heap, or
// memory the game would have had. It still has to carry the teardown itself
// rather than push it into the shared reboot code: that code is allocated for
// every game whose own OS_ResetSystem was found, so growing it can starve a game.
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

    // Only between card operations. The sdk holds its card lock word for every
    // rom read, and the DSpico stops for good when one is cut short, so this
    // waits for a vblank where the word is free. Both sdk families keep it 0x3C0
    // past resetParam, in the same shared page.
    ldr r3, patch_exitcombo_resetparam_address
    add r3, r3, #0x3C0
    mcr p15, 0, r3, c7, c14, 1 // clean and invalidate the line, should a game cache it
    ldr r2, [r3] // lockFlag, nonzero while held
    cmp r2, #0
    bne dispatch // try again on the next vblank

    ldr r3, ioBase
    mov r2, #0
    str r2, [r3, #0x208] // IME = 0. Per cpu, so the arm7 keeps taking its vblank.

    // The reboot code hands over to the arm7 through this sync, so clear what
    // the game left in it before the arm7 can see the request.
    strb r2, [r3, #0x181] // REG_IPC_SYNC data out

    ldr r12, requestMagic
    str r12, [r1] // exit_request_block_t::requestMagic
    mcr p15, 0, r1, c7, c10, 1 // clean dcache entry by MVA
    mcr p15, 0, r2, c7, c10, 4 // drain write buffer

    // Wait for the arm7 to park, or to cancel when it cannot stop safely. The
    // pause between reads keeps this off the bus while the arm7 finishes.
    ldr r0, parkMagic
1:
    mov r12, #0x400
2:
    subs r12, r12, #1
    bne 2b
    mcr p15, 0, r1, c7, c6, 1 // invalidate the line
    ldr r12, [r1, #0x0C] // exit_request_block_t::comboMagic
    subs r12, r12, r0 // 0 = parked, 1 = cancelled
    beq 3f
    cmp r12, #1
    bne 1b

    // Cancelled: take the request back and let the game go on, starting with
    // the vblank it is in. Irqs that came meanwhile are still pending.
    str r2, [r1] // exit_request_block_t::requestMagic = 0
    mcr p15, 0, r1, c7, c10, 1 // clean dcache entry by MVA
    mcr p15, 0, r2, c7, c10, 4 // drain write buffer
    mov r12, #1
    str r12, [r3, #0x208] // IME = 1
    mov r0, #0 // irq index 0 = vblank
    b dispatch

3:
    // Take the card back: the loader hands slot 1 to the arm7 before booting a
    // game and only the arm9 can undo that. Nothing is in flight on it: the
    // lock word was free and the arm7 parked between its own card accesses.
    add r0, r3, #0x204 // REG_EXMEMCNT, halfword offsets are only 8 bit
    ldrh r12, [r0]
    bic r12, r12, #0x880
    strh r12, [r0]

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

comboMagic:
    .word 0x424D4F43 // EXIT_REQUEST_COMBO_MAGIC

parkMagic:
    .word 0x4B524150 // EXIT_REQUEST_PARK_MAGIC, EXIT_REQUEST_CANCEL_MAGIC is one more

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
