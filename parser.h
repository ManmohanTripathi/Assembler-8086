#pragma once
#include "assembler.h"
#include "lexer.h"
#include <stdexcept>

// ─────────────────────────────────────────────
//  Instruction size table (simplified 8086)
//  Real 8086 encoding is complex; we use a clean
//  pedagogical subset with fixed sizes.
// ─────────────────────────────────────────────
static int instructionSize(const std::string& mnem, OperandType op1, OperandType op2) {
    // Jumps / calls encode a 2-byte address → 3 bytes (opcode + 2)
    if (mnem=="JMP"||mnem=="JE"||mnem=="JNE"||
        mnem=="JZ"||mnem=="JNZ"||mnem=="JG"||
        mnem=="JL"||mnem=="CALL")
        return 3;

    if (mnem=="NOP"||mnem=="HLT"||mnem=="RET") return 1;
    if (mnem=="NOT"||mnem=="INC"||mnem=="DEC"||
        mnem=="PUSH"||mnem=="POP") return 2;
    if (mnem=="INT") return 2;

    // Two-operand:
    //   REG,REG = opcode(1) + dst(1) + src(1)  = 3 bytes
    //   REG,IMM = opcode(1) + dst(1) + imm16(2) = 4 bytes
    //   REG,MEM = opcode(1) + dst(1) + addr16(2) = 4 bytes
    if (op2 == OperandType::REG)  return 3;
    if (op2 == OperandType::IMM)  return 4;
    if (op2 == OperandType::MEM)  return 4;
    return 3;
}

// ─────────────────────────────────────────────
//  Parser Class
// ─────────────────────────────────────────────
class Parser {
public:
    // Parsed instructions + symbol table (labels)
    std::vector<Instruction>            instructions;
    std::unordered_map<std::string, Symbol> symbols;

    void parse(const std::vector<Token>& tokens) {
        pos = 0;
        toks = &tokens;
        uint16_t addr = 0x0000;

        while (current().type != TokenType::END_OF_FILE) {
            skipNewlines();
            if (current().type == TokenType::END_OF_FILE) break;

            // Label definition
            if (current().type == TokenType::LABEL_DEF) {
                std::string name = current().value;
                symbols[name] = {name, addr, true};
                advance();
                skipNewlines();
                if (current().type == TokenType::END_OF_FILE) break;
            }

            // Instruction
            if (current().type == TokenType::MNEMONIC) {
                Instruction inst = parseInstruction();
                inst.address = addr;
                inst.size    = instructionSize(inst.mnemonic,
                                               inst.op1.type,
                                               inst.op2.type);
                addr += inst.size;
                instructions.push_back(inst);
            }

            skipToNextLine();
        }
    }

private:
    size_t pos;
    const std::vector<Token>* toks;

    const Token& current() { return (*toks)[pos]; }
    const Token& advance() { return (*toks)[pos++]; }

    void skipNewlines() {
        while (current().type == TokenType::NEWLINE) advance();
    }
    void skipToNextLine() {
        while (current().type != TokenType::NEWLINE &&
               current().type != TokenType::END_OF_FILE)
            advance();
    }

    Instruction parseInstruction() {
        Instruction inst;
        inst.source_line = current().line;
        inst.mnemonic    = current().value;
        advance();

        // No-operand instructions
        if (inst.mnemonic == "NOP" || inst.mnemonic == "HLT" ||
            inst.mnemonic == "RET") {
            inst.op1.type = OperandType::NONE;
            inst.op2.type = OperandType::NONE;
            return inst;
        }

        // Single-operand
        if (inst.mnemonic == "PUSH" || inst.mnemonic == "POP" ||
            inst.mnemonic == "NOT"  || inst.mnemonic == "INC" ||
            inst.mnemonic == "DEC"  || inst.mnemonic == "INT" ||
            inst.mnemonic == "JMP"  || inst.mnemonic == "JE"  ||
            inst.mnemonic == "JNE"  || inst.mnemonic == "JZ"  ||
            inst.mnemonic == "JNZ"  || inst.mnemonic == "JG"  ||
            inst.mnemonic == "JL"   || inst.mnemonic == "CALL") {
            inst.op1 = parseOperand();
            inst.op2.type = OperandType::NONE;
            return inst;
        }

        // Two-operand: dst, src
        inst.op1 = parseOperand();
        if (current().type == TokenType::COMMA) advance();
        inst.op2 = parseOperand();
        return inst;
    }

    Operand parseOperand() {
        Operand op;
        op.type = OperandType::NONE;

        if (current().type == TokenType::REGISTER) {
            op.type     = OperandType::REG;
            op.reg_name = current().value;
            advance();
        }
        else if (current().type == TokenType::NUMBER) {
            op.type = OperandType::IMM;
            uint16_t val = 0;
            parseNumber(current().value, val);
            op.imm_val = val;
            advance();
        }
        else if (current().type == TokenType::LBRACKET) {
            advance(); // consume '['
            op.type = OperandType::MEM;
            if (current().type == TokenType::NUMBER) {
                parseNumber(current().value, op.imm_val);
                advance();
            } else if (current().type == TokenType::REGISTER) {
                // [BX] style — encode reg index as address placeholder
                op.reg_name = current().value;
                advance();
            }
            if (current().type == TokenType::RBRACKET) advance();
        }
        else if (current().type == TokenType::LABEL_REF) {
            op.type       = OperandType::LABEL;
            op.label_name = current().value;
            advance();
        }
        return op;
    }
};