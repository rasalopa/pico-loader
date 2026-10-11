.cpu arm7tdmi
.section "patch_dsibiospitch7", "ax"
.syntax unified

@ GetPitchTable of the bios of the DSi reads the sine table instead of the
@ pitch table. Moving the index back by 0x46A entries lands on the pitch table.
@ Entered through the sdk veneers (arm) or straight from thumb callers.

.arm
.global patch_dsibiospitch7_arm
.type patch_dsibiospitch7_arm, %function
patch_dsibiospitch7_arm:
    ldr r1,= 0x46A
    sub r0, r0, r1
    svc 0x1B0000
    mov r0, r0, lsl #16
    mov r0, r0, lsr #16
    bx lr

.thumb
.global patch_dsibiospitch7_thumb
.type patch_dsibiospitch7_thumb, %function
patch_dsibiospitch7_thumb:
    ldr r1,= 0x46A
    subs r0, r0, r1
    svc 0x1B
    lsls r0, r0, #16
    lsrs r0, r0, #16
    bx lr

.balign 4

.pool
.end
