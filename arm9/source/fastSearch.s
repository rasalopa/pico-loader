.section ".itcm", "ax"
.arm

// r0: data
// r1: size
// r2: pattern (4 words, each word should be unique)
.global fastSearch16
.type fastSearch16, %function
fastSearch16:
    push {r4-r11,lr}
    add r1, r1, r0
    sub r1, r1, #44 // 32 bytes to load at once + 12 bytes after it
    ldmia r2, {r2-r5}

1:
    ldmia r0!, {r6-r12,lr}
    cmp r6, r2
    cmpne r7, r2
    cmpne r8, r2
    cmpne r9, r2
    cmpne r10, r2
    cmpne r11, r2
    cmpne r12, r2
    cmpne lr, r2
    beq fastSearch16_firstWordMatch
fastSearch16_continueFastSearch:
    cmp r0, r1
    ble 1b

    // only need to handle the last couple of words now
    add r1, #(44 - 16)
2:
    ldr r6, [r0], #4
    cmp r6, r2
    beq fastSearch16_firstWordMatchLast
fastSearch16_continueLastSearch:
    cmp r0, r1
    ble 2b

    mov r0, #0
    pop {r4-r11,pc}

fastSearch16_firstWordMatchLast:
    ldr r6, [r0], #4
    cmp r6, r3
    ldreq r6, [r0], #4
    cmpeq r6, r4
    ldreq r6, [r0], #4
    cmpeq r6, r5
    bne fastSearch16_continueLastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch:
    cmp r6, r2
    beq fastSearch16_firstWordMatch_r6
    cmpne r7, r2
    beq fastSearch16_firstWordMatch_r7
    cmpne r8, r2
    beq fastSearch16_firstWordMatch_r8
    cmpne r9, r2
    beq fastSearch16_firstWordMatch_r9
    cmpne r10, r2
    beq fastSearch16_firstWordMatch_r10
    cmpne r11, r2
    beq fastSearch16_firstWordMatch_r11
    cmpne r12, r2
    beq fastSearch16_firstWordMatch_r12
fastSearch16_firstWordMatch_lr:
    ldr r6, [r0], #4
    cmp r6, r3
    ldreq r6, [r0], #4
    cmpeq r6, r4
    ldreq r6, [r0], #4
    cmpeq r6, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r6:
    sub r0, r0, #16
    cmp r7, r3
    cmpeq r8, r4
    cmpeq r9, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r7:
    sub r0, r0, #12
    cmp r8, r3
    cmpeq r9, r4
    cmpeq r10, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r8:
    sub r0, r0, #8
    cmp r9, r3
    cmpeq r10, r4
    cmpeq r11, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r9:
    sub r0, r0, #4
    cmp r10, r3
    cmpeq r11, r4
    cmpeq r12, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r10:
    cmp r11, r3
    cmpeq r12, r4
    cmpeq lr, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r11:
    cmp r12, r3
    cmpeq lr, r4
    ldreq r6, [r0], #4
    cmpeq r6, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

fastSearch16_firstWordMatch_r12:
    cmp lr, r3
    ldreq r6, [r0], #4
    cmpeq r6, r4
    ldreq r6, [r0], #4
    cmpeq r6, r5
    bne fastSearch16_continueFastSearch
    sub r0, r0, #16
    pop {r4-r11,pc}

// r0: data
// r1: size
// r2: pattern (2 words)
// Unlike fastSearch16, a word that matches the first pattern word but is not
// followed by the second one only moves the search on by one word, and nothing
// at or past data + size is read, whatever the size.
.global fastSearch8
.type fastSearch8, %function
fastSearch8:
    push {r4-r11,lr}
    add r1, r1, r0
    sub r1, r1, #36 // 32 bytes to load at once + 4 bytes after it
    ldmia r2, {r2-r3}
    cmp r0, r1
    bhi fastSearch8_last

1:
    ldmia r0!, {r4-r11}
    cmp r4, r2
    cmpne r5, r2
    cmpne r6, r2
    cmpne r7, r2
    cmpne r8, r2
    cmpne r9, r2
    cmpne r10, r2
    cmpne r11, r2
    beq fastSearch8_firstWordMatch
fastSearch8_continueFastSearch:
    cmp r0, r1
    bls 1b

fastSearch8_last:
    // only need to handle the last couple of words now
    add r1, #(36 - 8)
2:
    cmp r0, r1
    bhi fastSearch8_notFound
    ldr r4, [r0], #4
    cmp r4, r2
    bne 2b
    ldr r4, [r0]
    cmp r4, r3
    bne 2b
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_notFound:
    mov r0, #0
    pop {r4-r11,pc}

fastSearch8_firstWordMatch:
    cmp r4, r2
    beq fastSearch8_firstWordMatch_r4
    cmp r5, r2
    beq fastSearch8_firstWordMatch_r5
    cmp r6, r2
    beq fastSearch8_firstWordMatch_r6
    cmp r7, r2
    beq fastSearch8_firstWordMatch_r7
    cmp r8, r2
    beq fastSearch8_firstWordMatch_r8
    cmp r9, r2
    beq fastSearch8_firstWordMatch_r9
    cmp r10, r2
    beq fastSearch8_firstWordMatch_r10
fastSearch8_firstWordMatch_r11:
    // r0 already points at the word after r11, which is still inside the data
    ldr r12, [r0]
    cmp r12, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

// Each case first moves r0 to the word after the first word match, where the
// search goes on when the second word does not match.
fastSearch8_firstWordMatch_r4:
    sub r0, r0, #28
    cmp r5, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r5:
    sub r0, r0, #24
    cmp r6, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r6:
    sub r0, r0, #20
    cmp r7, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r7:
    sub r0, r0, #16
    cmp r8, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r8:
    sub r0, r0, #12
    cmp r9, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r9:
    sub r0, r0, #8
    cmp r10, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}

fastSearch8_firstWordMatch_r10:
    sub r0, r0, #4
    cmp r11, r3
    bne fastSearch8_continueFastSearch
    sub r0, r0, #4
    pop {r4-r11,pc}
