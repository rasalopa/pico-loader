.cpu arm946e-s
.syntax unified
.arm

// Arm9 half of the in-game exit to the launcher, in four pieces so each one fits
// a gap the arm9 patch heap has left after every mandatory patch. A piece never
// reaches another one pc-relative: every jump between pieces goes through a
// literal the loader fills, as each piece lands at its own address.
//
// Registers: the bios saved r0-r3, r12 and lr before entering the dispatcher, and
// the dispatcher tail the hook displaces loads r1 and lr itself, so r0-r3, r12 and
// lr are free; r4-r11 must be preserved.
//
// Fixed distances in the shared page, the same in both mirrors (static_asserts in
// ExitComboPatch.cpp): resetParam is block + 0x5A0, the sdk card lock word is
// block + 0x960 (resetParam + 0x3C0).

// ---------------------------------------------------------------- hook
// Reached through ldr pc from the first instruction of the SDK irq dispatcher
// tail, with r0 = irq index. Runs on every irq, so it stays short: only a vblank
// with the arm7's combo magic set goes on to the check piece, which returns here
// with r0 = 0 whenever the game goes on.
.section "patch_exitcombo_hook", "ax"
.global patch_exitcombo_hook_entry
.type patch_exitcombo_hook_entry, %function
patch_exitcombo_hook_entry:
    cmp r0, #0 // irq index 0 = vblank
    bne 1f
    ldr r1, patch_exitcombo_hook_block_address
    mcr p15, 0, r1, c7, c6, 1 // invalidate the line, the arm7 has no cache
    ldr r2, [r1, #0x0C] // exit_request_block_t::comboMagic
    ldr r3, hookComboMagic
    cmp r2, r3
    ldreq r12, patch_exitcombo_hook_check_address
    blxeq r12
1:
    // the dispatcher tail displaced by the hook
    ldr r1, patch_exitcombo_hook_table_address
    ldr r0, [r1, r0, lsl #2]
    ldr lr, patch_exitcombo_hook_return_address
    bx r0

hookComboMagic:
    .word 0x424D4F43 // EXIT_REQUEST_COMBO_MAGIC
.global patch_exitcombo_hook_block_address
patch_exitcombo_hook_block_address:
    .word 0
.global patch_exitcombo_hook_check_address
patch_exitcombo_hook_check_address:
    .word 0
.global patch_exitcombo_hook_table_address
patch_exitcombo_hook_table_address:
    .word 0
.global patch_exitcombo_hook_return_address
patch_exitcombo_hook_return_address:
    .word 0

// ---------------------------------------------------------------- check
// r1 = block, r0 = 0, lr = back into the hook.
// Only between card operations: the sdk holds its card lock word for every rom
// read, and the DSpico stops for good when one is cut short, so this waits for a
// vblank where the word is free.
.section "patch_exitcombo_check", "ax"
.global patch_exitcombo_check_entry
.type patch_exitcombo_check_entry, %function
patch_exitcombo_check_entry:
    add r3, r1, #0x960 // sdk card lock word
    mcr p15, 0, r3, c7, c14, 1 // clean and invalidate the line, should a game cache it
    ldr r2, [r3] // lockFlag, nonzero while held
    cmp r2, #0
    bxne lr // try again on the next vblank

    mov r3, #0x04000000
    str r2, [r3, #0x208] // IME = 0. Per cpu, so the arm7 keeps taking its vblank.
    // The reboot code hands over to the arm7 through this sync, so clear what
    // the game left in it before the arm7 can see the request.
    strb r2, [r3, #0x181] // REG_IPC_SYNC data out

    ldr r12, checkRequestMagic
    str r12, [r1] // exit_request_block_t::requestMagic
    mcr p15, 0, r1, c7, c10, 1 // clean dcache entry by MVA
    mcr p15, 0, r2, c7, c10, 4 // drain write buffer
    ldr pc, patch_exitcombo_check_wait_address // r1 = block, r2 = 0, r3 = io

checkRequestMagic:
    .word 0x54495845 // EXIT_REQUEST_REQUEST_MAGIC
.global patch_exitcombo_check_wait_address
patch_exitcombo_check_wait_address:
    .word 0

// ---------------------------------------------------------------- wait
// r1 = block, r2 = 0, r3 = 0x04000000, lr = back into the hook.
// Waits for the arm7 to park, or to cancel when it cannot stop safely. The pause
// between reads keeps this off the bus while the arm7 finishes.
.section "patch_exitcombo_wait", "ax"
.global patch_exitcombo_wait_entry
.type patch_exitcombo_wait_entry, %function
patch_exitcombo_wait_entry:
    ldr r0, waitParkMagic
1:
    mov r12, #0x400
2:
    subs r12, r12, #1
    bne 2b
    mcr p15, 0, r1, c7, c6, 1 // invalidate the line
    ldr r12, [r1, #0x0C] // exit_request_block_t::comboMagic
    subs r12, r12, r0 // 0 = parked, 1 = cancelled
    ldreq pc, patch_exitcombo_wait_teardown_address
    cmp r12, #1
    bne 1b

    // Cancelled: take the request back and let the game go on, starting with
    // the vblank it is in. Irqs that came meanwhile are still pending.
    str r2, [r1] // exit_request_block_t::requestMagic = 0
    mcr p15, 0, r1, c7, c10, 1 // clean dcache entry by MVA
    mcr p15, 0, r2, c7, c10, 4 // drain write buffer
    mov r0, #1
    str r0, [r3, #0x208] // IME = 1
    mov r0, #0 // irq index 0 = vblank, for the hook's dispatch
    bx lr

waitParkMagic:
    .word 0x4B524150 // EXIT_REQUEST_PARK_MAGIC, EXIT_REQUEST_CANCEL_MAGIC is one more
.global patch_exitcombo_wait_teardown_address
patch_exitcombo_wait_teardown_address:
    .word 0

// ---------------------------------------------------------------- teardown
// r1 = block, r2 = 0, r3 = 0x04000000. Never returns.
.section "patch_exitcombo_teardown", "ax"
.global patch_exitcombo_teardown_entry
.type patch_exitcombo_teardown_entry, %function
patch_exitcombo_teardown_entry:
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

    str r2, [r1, #0x5A0] // resetParam

    // Into the reboot code past its reset of the game's arm7, which ours already
    // parked, with the registers its entry would have loaded from its pool.
    ldr r0, patch_exitcombo_teardown_resetpool_address
    ldmia r0, {r0, r1, r3, r4, r5, r6, r7}
    mov r9, r4 // readSdSectors
    mov r10, r7 // bootPicoLoader
    ldr r12, patch_exitcombo_teardown_bootloaderbinaries_address
    bx r12 // thumb

.global patch_exitcombo_teardown_resetpool_address
patch_exitcombo_teardown_resetpool_address:
    .word 0
.global patch_exitcombo_teardown_bootloaderbinaries_address
patch_exitcombo_teardown_bootloaderbinaries_address:
    .word 0

.end
