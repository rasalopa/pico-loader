.cpu arm7tdmi
.section "patch_exitcombo_arm7", "ax"
.syntax unified
.arm

// Resident arm7 half of the in-game exit to the launcher.
//
// Reached through ldr pc from the first instruction of the SDK irq dispatcher
// tail, so r0 holds the irq index and the original r1/lr loads did not run yet.
// The bios saved r0-r3, r12 and lr before entering the dispatcher, so those are
// free to use; r4-r11 must be preserved.
//
// This half owns the whole combo detection, because the arm7 patch heap has
// hundreds of bytes to spare while the arm9 one barely has room for the reboot
// path. It also means a missing arm9 patch cannot hang anything: without it
// nobody ever answers the request and the combo simply does nothing.
.global patch_exitcombo_arm7_entry
.type patch_exitcombo_arm7_entry, %function
patch_exitcombo_arm7_entry:
    cmp r0, #0 // irq index 0 = vblank
    bne dispatch

    ldr r1, patch_exitcombo_arm7_block_address

    // has the arm9 taken the request and asked us to get out of the way?
    ldr r2, [r1] // exit_request_block_t::requestMagic
    ldr r3, requestMagic
    cmp r2, r3
    beq park

    // only armed when the loader stored a valid launcher path
    ldr r2, [r1, #4] // exit_request_block_t::pathMagic
    ldr r3, pathMagic
    cmp r2, r3
    bne dispatch

    ldr r2, regKeyInput
    ldr r2, [r2] // masked below, so the keycnt half is discarded
    ldr r3, comboMask
    ands r2, r2, r3 // keys are active low, 0 = whole combo held
    adr r12, holdCounter
    bne notHeld

    ldr r2, [r12]
    add r2, r2, #1
    str r2, [r12]
    cmp r2, #45
    blo dispatch

    // ask for the exit and carry on: quiescing happens once the arm9 answers,
    // so a game running without an arm9 patch is never left with a dead arm7
    ldr r2, comboMagic
    str r2, [r1, #0x0C] // exit_request_block_t::comboMagic
    b dispatch

notHeld:
    mov r2, #0
    str r2, [r12]
    b dispatch

park:
    // Quiesce this cpu. Everything left out here is reinitialised by Pico Loader
    // itself on boot: irq enable/flags, the irq vector table, the fifo and the
    // individual sound channels. Stopping dma is the one thing nothing else does
    // in time, and the master sound register is silenced so the handover is quiet.
    mov r2, #0
    ldr r3, regIme
    str r2, [r3]
    ldr r3, regDmaBase
    str r2, [r3, #0x08] // DMA0CNT
    str r2, [r3, #0x14] // DMA1CNT
    str r2, [r3, #0x20] // DMA2CNT
    str r2, [r3, #0x2C] // DMA3CNT
    ldr r3, regSoundCnt
    strh r2, [r3]

    // Tell the arm9 we are out of the way, wait for it to finish reloading Pico
    // Loader, then release it and jump. Same three step sync the sdk reset
    // handler speaks, so the arm9 side of the reboot code is unchanged.
    ldr r3, regIpcSync
    mov r2, #1
    strb r2, [r3, #1] // sync out = 1, parked
1:
    ldrb r2, [r3]
    cmp r2, #1
    bne 1b // while ipc sync from arm9 is not 1

    mov r2, #0
    strb r2, [r3, #1] // sync out = 0, released

    // We were entered from an irq, so leave irq mode before handing over.
    // System mode specifically: Pico Loader's arm7 crt0 sets up only the stack
    // pointer of whatever mode it is entered in, and libtwl's thread switcher
    // saves and restores the user bank one, so it has to run as a system mode
    // thread. That is also the mode the other ways into the loader arrive in.
    msr cpsr_c, #0xDF // system mode, irqs off
    mov r0, #0x06000000 // vram c, mapped to the arm7 by the arm9 side
    ldr r0, [r0] // pload_header7_t::entryPoint
    bx r0

dispatch:
    // the dispatcher tail displaced by the hook
    ldr r1, patch_exitcombo_arm7_table_address
    ldr r0, [r1, r0, lsl #2]
    ldr lr, patch_exitcombo_arm7_return_address
    bx r0

.balign 4

regKeyInput:
    .word 0x04000130

comboMask:
    .word 0x0384 // L | R | Down | Select

regIme:
    .word 0x04000208

regDmaBase:
    .word 0x040000B0

regSoundCnt:
    .word 0x04000500

regIpcSync:
    .word 0x04000180

requestMagic:
    .word 0x54495845 // EXIT_REQUEST_REQUEST_MAGIC

pathMagic:
    .word 0x58454C50 // EXIT_REQUEST_PATH_MAGIC

comboMagic:
    .word 0x424D4F43 // EXIT_REQUEST_COMBO_MAGIC

holdCounter:
    .word 0

.global patch_exitcombo_arm7_block_address
patch_exitcombo_arm7_block_address:
    .word 0

.global patch_exitcombo_arm7_table_address
patch_exitcombo_arm7_table_address:
    .word 0

.global patch_exitcombo_arm7_return_address
patch_exitcombo_arm7_return_address:
    .word 0

.pool
.end
