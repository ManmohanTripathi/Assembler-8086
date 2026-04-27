#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

// ─────────────────────────────────────────────
//  Token Types
// ─────────────────────────────────────────────
enum class TokenType {
    MNEMONIC,       // MOV, ADD, SUB, etc.
    REGISTER,       // AX, BX, CX, DX, SI, DI, SP, BP
    NUMBER,         // 0x1F, 42, 0b1010
    LABEL_DEF,      // loop:
    LABEL_REF,      // jmp loop
    COMMA,          // ,
    LBRACKET,       // [
    RBRACKET,       // ]
    NEWLINE,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

// ─────────────────────────────────────────────
//  Symbol Table Entry
// ─────────────────────────────────────────────
struct Symbol {
    std::string name;
    uint16_t address;
    bool defined;
};

// ─────────────────────────────────────────────
//  Instruction Encoding
// ─────────────────────────────────────────────
enum class OperandType {
    NONE,
    REG,
    IMM,
    MEM,        // [addr]
    LABEL
};

struct Operand {
    OperandType type;
    std::string reg_name;   // for REG
    uint16_t    imm_val;    // for IMM / MEM address
    std::string label_name; // for LABEL
};

struct Instruction {
    std::string   mnemonic;
    Operand       op1, op2;
    uint16_t      address;      // assigned in pass 1
    int           size;         // byte size of encoded instruction
    int           source_line;
};

// ─────────────────────────────────────────────
//  Assembled Output
// ─────────────────────────────────────────────
struct AssembledLine {
    uint16_t address;
    std::vector<uint8_t> bytes;
    std::string source;     // original asm text for listing
};