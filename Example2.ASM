; ─────────────────────────────────────────────
; example2.asm  —  Loop: sum 1 to 5
; DEC sets ZF=1 when result hits 0.
; JNZ jumps while ZF=0 (CX not yet zero).
; Result: AX = 5+4+3+2+1 = 15
; ─────────────────────────────────────────────

    MOV AX, 0          ; AX = sum accumulator
    MOV CX, 5          ; CX = loop counter

LOOP_START:
    ADD AX, CX         ; AX += CX  (adds 5,4,3,2,1)
    DEC CX             ; CX-- sets ZF if CX becomes 0
    JNZ LOOP_START     ; if ZF=0 (CX != 0), loop again

    HLT                ; AX = 15