; ─────────────────────────────────────────────
; example4.asm  —  Optimizer Demo
;
; This file is written deliberately to trigger
; all three optimization passes.
;
; BEFORE optimization (7 instructions):
;   MOV AX, 3
;   MOV BX, 4
;   ADD AX, BX     ← constant fold: AX=3, BX=4 → MOV AX, 7
;   MOV CX, 0      ← dead code: CX overwritten below before use
;   MOV CX, 2
;   MUL AX, CX     ← strength reduction: MUL x2 → ADD AX, AX
;   HLT
;
; AFTER optimization (5 instructions):
;   MOV AX, 7      (folded from ADD AX,BX when both known)
;   MOV BX, 4      (kept — BX still written)
;   MOV CX, 2      (dead MOV CX,0 removed)
;   ADD AX, AX     (strength reduced from MUL AX,2)
;   HLT
;
; Final result: AX = 14
; ─────────────────────────────────────────────

    MOV AX, 3
    MOV BX, 4
    ADD AX, BX      ; both known → folded to MOV AX, 7
    MOV CX, 0       ; dead write → eliminated (CX immediately overwritten)
    MOV CX, 2
    MUL AX, CX      ; MUL x2 → strength reduced to ADD AX, AX
    HLT