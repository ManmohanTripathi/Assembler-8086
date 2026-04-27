; ─────────────────────────────────────────────
; example1.asm  —  Basic arithmetic
; Computes: AX = (5 + 3) * 2
; ─────────────────────────────────────────────

    MOV AX, 5          ; AX = 5
    MOV BX, 3          ; BX = 3
    ADD AX, BX         ; AX = AX + BX  -> 8
    MOV CX, 2          ; CX = 2
    MUL AX, CX         ; AX = AX * CX  -> 16  (result stored in AX)
    HLT