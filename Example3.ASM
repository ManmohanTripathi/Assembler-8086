; ─────────────────────────────────────────────
; example3.asm  —  Subroutine call via CALL/RET
; Demonstrates stack usage
; DOUBLE subroutine: AX = AX * 2
; ─────────────────────────────────────────────

    MOV AX, 7          ; AX = 7
    CALL DOUBLE        ; call subroutine — pushes return addr on stack
    HLT                ; AX should = 14

DOUBLE:
    MOV BX, 2
    MUL AX, BX         ; AX = AX * 2
    RET                ; pop return address and jump back