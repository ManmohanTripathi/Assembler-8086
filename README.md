
# ASM86 — A Two-Pass 8086-Subset Assembler in C++

A from-scratch two-pass assembler for a simplified 8086-inspired instruction set, written in C++17. Includes an optimizer that runs between the two passes and a register-based virtual machine (VM) to execute the assembled bytecode.

Built as a systems software project to understand how compilers, assemblers, and machine architecture fit together.

---

## Architecture Overview

```
Source (.asm)
     │
     ▼
┌─────────────┐
│    LEXER    │  Tokenizes source into: MNEMONIC, REGISTER,
│  (lexer.h)  │  NUMBER, LABEL_DEF, LABEL_REF, COMMA, etc.
└──────┬──────┘
       │  Token stream
       ▼
┌─────────────┐  Pass 1: Builds symbol table (label → address)
│   PARSER    │  Assigns addresses to each instruction
│  (parser.h) │  Calculates instruction sizes
└──────┬──────┘
       │  Instruction list + Symbol table
       ▼
┌─────────────┐  Three classical optimization passes:
│  OPTIMIZER  │  1. Constant folding
│(optimizer.h)│  2. Strength reduction
└──────┬──────┘  3. Dead code elimination
       │  Optimized instruction list
       ▼
┌─────────────┐  Pass 2: Resolves label references
│  CODEGEN    │  Encodes each instruction to bytecode
│ (codegen.h) │  Produces binary image + assembly listing
└──────┬──────┘
       │  Binary bytecode
       ▼
┌─────────────┐
│     VM      │  Loads binary into 64KB memory
│   (vm.h)    │  Executes instructions, maintains registers & flags
└─────────────┘
```

---

## Why Two Passes?

A single pass can't resolve **forward references** — labels used before they're defined:

```asm
    JMP EXIT       ; EXIT not yet seen — address unknown in pass 1
    ; ... other instructions ...
EXIT:
    HLT
```

**Pass 1** scans all instructions and records each label's address into a **symbol table**.  
**Pass 2** re-reads instructions and substitutes label references with the now-known addresses.

This is the same fundamental technique used by real assemblers (NASM, GAS) and linkers.

---

## Optimizer

The optimizer runs between Pass 1 and Pass 2 on the instruction list. It performs three classical compiler optimizations:

### 1. Constant Folding
If both operands of an arithmetic instruction are known compile-time constants, the result is computed at assemble time and the instruction is replaced with a `MOV`.

```asm
; Before
MOV AX, 3
MOV BX, 4
ADD AX, BX      ; both operands known → AX will always be 7

; After
MOV AX, 7       ; computed at assemble time
MOV BX, 4
```

### 2. Strength Reduction
Replaces expensive instructions with cheaper equivalents:

```asm
MUL AX, 2   →   ADD AX, AX    ; addition is faster than multiply
MUL AX, 1   →   (removed)     ; multiply by 1 is identity
MUL AX, 0   →   MOV AX, 0    ; multiply by 0 is always zero
```

### 3. Dead Code Elimination
Removes writes whose result is overwritten before any read:

```asm
; Before
MOV CX, 0       ; dead — CX is overwritten on the very next line
MOV CX, 2

; After
MOV CX, 2       ; MOV CX, 0 eliminated
```

The optimizer is **conservative with control flow**: if any jump instructions are present in the code, constant folding is skipped for any register modified inside the loop — preventing incorrect transformations across loop iterations.

---

## Supported Instructions

| Category     | Instructions                                      |
|--------------|--------------------------------------------------|
| Data Move    | `MOV reg, reg/imm`                               |
| Arithmetic   | `ADD`, `SUB`, `MUL`, `DIV`, `INC`, `DEC`        |
| Logic        | `AND`, `OR`, `XOR`, `NOT`                        |
| Comparison   | `CMP`                                            |
| Jumps        | `JMP`, `JE`, `JNE`, `JZ`, `JNZ`, `JG`, `JL`    |
| Stack        | `PUSH`, `POP`                                    |
| Subroutines  | `CALL`, `RET`                                    |
| System       | `INT`, `NOP`, `HLT`                              |

## Supported Registers

`AX BX CX DX` — general purpose 16-bit  
`AH AL BH BL CH CL DH DL` — 8-bit halves  
`SI DI SP BP` — index and pointer registers

---

## Instruction Encoding

Each instruction is encoded as:

```
[ OPCODE (1 byte) ] [ OPERAND1 ] [ OPERAND2 ]
```

| Operand type | Encoding          | Size    |
|--------------|-------------------|---------|
| Register     | reg code (1 byte) | 1 byte  |
| Immediate    | little-endian     | 2 bytes |
| Address/Label| little-endian     | 2 bytes |

Opcode convention: `base_opcode` = REG,REG variant; `base_opcode + 8` = REG,IMM variant. This mirrors how x86 uses the `w` bit and `mod-reg-r/m` byte to distinguish operand types.

---

## VM Design

The VM simulates an 8086-inspired register machine:

- **16 registers** (AX–BH), each 16-bit
- **64KB flat address space**
- **Flags**: ZF (zero), SF (sign), CF (carry)
- **Stack** growing downward from SP
- **Instruction pointer** (IP) walks through binary
- Execution guard: halts after 100,000 steps to catch infinite loops

---

## Build & Run

**Requirements:** g++ with C++17 support

```bash
# Build (all source is header-only, included by main.cpp)
g++ -std=c++17 -Wall -Wextra -o asm86 main.cpp

# Run a program (assemble + execute)
./asm86 Example1.asm

# Show assembly listing (hex bytes alongside each instruction)
./asm86 Example1.asm --listing

# Assemble only (write .bin file, don't run VM)
./asm86 Example1.asm --no-run

# Run with VM instruction trace
./asm86 Example1.asm --verbose

# Skip optimizer (useful for comparison)
./asm86 Example1.asm --no-opt
```

---

## Examples

### Example1.asm — Basic Arithmetic
Computes `(5 + 3) * 2 = 16` → result in `AX`

```asm
    MOV AX, 5
    MOV BX, 3
    ADD AX, BX      ; AX = 8
    MOV CX, 2
    MUL AX, CX      ; AX = 16
    HLT
```

Output:
```
AX=10  BX=3  CX=2  DX=0     (0x10 = 16 decimal)
```
<img width="593" height="384" alt="Screenshot 2026-04-28 at 2 28 16 AM" src="https://github.com/user-attachments/assets/4c153499-27af-4920-9337-49bfcd058f53" />

The optimizer folds `MOV AX,5 / MOV BX,3 / ADD AX,BX` into `MOV AX,8` at assemble time, reducing 6 instructions to 4.

---

### Example2.asm — Loop
Sums 1 to 5 using `DEC` + `JNZ` → result `AX = 15`

```asm
    MOV AX, 0
    MOV CX, 5
LOOP_START:
    ADD AX, CX
    DEC CX
    JNZ LOOP_START
    HLT
```

Output:
```
AX=F  CX=0  ZF=1    (0xF = 15 decimal)
Steps: 18           (2 setup + 3 per iteration × 5 + 1 HLT)
```
<img width="539" height="422" alt="Screenshot 2026-04-28 at 2 29 21 AM" src="https://github.com/user-attachments/assets/c1e485b2-d483-4680-8dc7-51cd5c85587a" />

The optimizer correctly detects the `JNZ` and skips constant folding — register values change across loop iterations so folding would be unsafe.

---

### Example3.ASM — Subroutine Call
Calls a `DOUBLE` subroutine via `CALL`/`RET`, demonstrates stack usage for return addresses.

```asm
    MOV AX, 7
    CALL DOUBLE     ; pushes return address onto stack, jumps to DOUBLE
    HLT

DOUBLE:
    MOV BX, 2
    MUL AX, BX      ; AX = AX * 2
    RET             ; pops return address and jumps back
```

Output:
```
AX=E  BX=2          (0xE = 14 decimal)
```

---
<img width="429" height="408" alt="Screenshot 2026-04-28 at 2 31 24 AM" src="https://github.com/user-attachments/assets/0fc20bef-59d4-447b-9aa9-b7e16765508d" />

### Example4.asm — Optimizer Demo
Deliberately written to trigger all three optimization passes.

```asm
    MOV AX, 3
    MOV BX, 4
    ADD AX, BX      ; constant fold:      AX=3, BX=4 → MOV AX, 7
    MOV CX, 0       ; dead code:          overwritten before any read → eliminated
    MOV CX, 2
    MUL AX, CX      ; strength reduction: MUL x2 → ADD AX, AX
    HLT
```

Before optimization: 7 instructions. After: 4 instructions. Final result: `AX=E` (14 decimal).

Run with and without `--no-opt` to compare:
```bash
./asm86 Example4.asm           # optimizer on: 4 instructions, AX=E
./asm86 Example4.asm --no-opt  # optimizer off: 7 instructions, AX=E
```

---
<img width="471" height="368" alt="Screenshot 2026-04-28 at 2 32 28 AM" src="https://github.com/user-attachments/assets/045986bb-f86a-4b7f-a7ee-cc75aadb8cc3" />

## File Structure

```
Assembler8086/
├── assembler.h    — Core data structures (Token, Instruction, Operand, Symbol)
├── lexer.h        — Tokenizer: source text → token stream
├── parser.h       — Pass 1: token stream → instructions + symbol table
├── optimizer.h    — Three-pass optimizer: constant folding, strength reduction, DCE
├── codegen.h      — Pass 2: instructions → bytecode + listing
├── vm.h           — Register-based VM: executes bytecode
├── main.cpp       — CLI driver
├── Example1.asm   — Arithmetic
├── Example2.ASM   — Loop
├── Example3.ASM   — Subroutine
├── Example4.asm   — Optimizer demo
└── Makefile
```

---

## Key Concepts Demonstrated

- **Lexical analysis** — converting raw text into typed tokens using character classification
- **Symbol table** — hash map of label names to resolved addresses, populated in Pass 1
- **Forward reference resolution** — why two passes are needed (same reason linkers exist)
- **Constant folding** — evaluating expressions at compile time rather than runtime
- **Strength reduction** — replacing slow instructions with faster equivalents
- **Dead code elimination** — removing writes whose results are never read
- **Instruction encoding** — mapping mnemonics + operands to opcode bytes (mirrors ISA design)
- **Little-endian encoding** — 16-bit values stored LSB first, matching x86 convention
- **Stack discipline** — PUSH/POP and CALL/RET maintaining the call stack via SP
- **Flags register** — ZF/SF/CF updated by arithmetic ops, consumed by conditional jumps

---

## Relation to Real 8086

This assembler uses a simplified but structurally faithful subset of 8086:

| Feature              | Real 8086              | This Assembler             |
|----------------------|------------------------|----------------------------|
| Instruction encoding | mod-reg-r/m byte       | Simplified fixed-width     |
| Addressing modes     | 7 modes                | REG, IMM, [addr]           |
| Registers            | 14 registers           | 16 slots (same names)      |
| Memory               | Segmented (20-bit)     | Flat 64KB                  |
| Interrupts           | Full IDT               | INT 21h subset only        |
| Optimizer            | —                      | Constant fold, SR, DCE     |

The encoding simplification makes the code readable and educational while preserving all the key architectural concepts.
