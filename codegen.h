#pragma once
#include "assembler.h"
#include "parser.h"
#include <iomanip>
#include <sstream>

// ─────────────────────────────────────────────
//  Opcode Table  (1 byte per mnemonic)
// ─────────────────────────────────────────────
static const std::unordered_map<std::string, uint8_t> OPCODES = {
    {"MOV",  0x10}, {"ADD",  0x20}, {"SUB",  0x21},
    {"MUL",  0x22}, {"DIV",  0x23},
    {"AND",  0x30}, {"OR",   0x31}, {"XOR",  0x32}, {"NOT",  0x33},
    {"INC",  0x40}, {"DEC",  0x41},
    {"CMP",  0x50},
    {"JMP",  0x60}, {"JE",   0x61}, {"JNE",  0x62},
    {"JZ",   0x63}, {"JNZ",  0x64}, {"JG",   0x65}, {"JL",   0x66},
    {"PUSH", 0x70}, {"POP",  0x71},
    {"CALL", 0x80}, {"RET",  0x81},
    {"NOP",  0xF0}, {"HLT",  0xFF},
    {"INT",  0xCD}
};

// Register encoding (matches 8086 mod-reg-r/m convention subset)
static const std::unordered_map<std::string, uint8_t> REG_CODES = {
    {"AX",0x00},{"CX",0x01},{"DX",0x02},{"BX",0x03},
    {"SP",0x04},{"BP",0x05},{"SI",0x06},{"DI",0x07},
    {"AL",0x08},{"CL",0x09},{"DL",0x0A},{"BL",0x0B},
    {"AH",0x0C},{"CH",0x0D},{"DH",0x0E},{"BH",0x0F}
};

// ─────────────────────────────────────────────
//  CodeGenerator  (Pass 2)
// ─────────────────────────────────────────────
class CodeGenerator {
public:
    std::vector<AssembledLine> output;
    std::vector<uint8_t>       binary;   // flat binary image

    void generate(std::vector<Instruction>& instructions,
                  std::unordered_map<std::string, Symbol>& symbols)
    {
        for (auto& inst : instructions) {
            AssembledLine line;
            line.address = inst.address;
            line.source  = inst.mnemonic;

            uint8_t opcode = getOpcode(inst.mnemonic);
            line.bytes.push_back(opcode);

            encodeOperands(inst, symbols, line.bytes);

            output.push_back(line);
            for (uint8_t b : line.bytes) binary.push_back(b);
        }
    }

    // Produce a human-readable listing
    std::string listing() const {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill('0');
        oss << "ADDR   BYTES              SOURCE\n";
        oss << "-----  -----------------  -------\n";
        for (const auto& line : output) {
            oss << std::setw(4) << line.address << "   ";
            for (uint8_t b : line.bytes)
                oss << std::setw(2) << (int)b << " ";
            // Pad to align source column
            int pad = 17 - (int)line.bytes.size() * 3;
            for (int i = 0; i < pad; i++) oss << " ";
            oss << " " << line.source << "\n";
        }
        return oss.str();
    }

private:
    uint8_t getOpcode(const std::string& mnem) {
        auto it = OPCODES.find(mnem);
        if (it == OPCODES.end())
            throw std::runtime_error("Unknown mnemonic: " + mnem);
        return it->second;
    }

    uint8_t getRegCode(const std::string& reg) {
        auto it = REG_CODES.find(reg);
        if (it == REG_CODES.end())
            throw std::runtime_error("Unknown register: " + reg);
        return it->second;
    }

    void encodeOperand(const Operand& op,
                       std::unordered_map<std::string, Symbol>& symbols,
                       std::vector<uint8_t>& bytes,
                       bool wide = false)
    {
        switch (op.type) {
            case OperandType::REG:
                bytes.push_back(getRegCode(op.reg_name));
                break;
            case OperandType::IMM:
                bytes.push_back(op.imm_val & 0xFF);
                if (wide) bytes.push_back((op.imm_val >> 8) & 0xFF);
                break;
            case OperandType::MEM:
                bytes.push_back(op.imm_val & 0xFF);
                bytes.push_back((op.imm_val >> 8) & 0xFF);
                break;
            case OperandType::LABEL: {
                auto it = symbols.find(op.label_name);
                if (it == symbols.end())
                    throw std::runtime_error("Undefined label: " + op.label_name);
                uint16_t addr = it->second.address;
                bytes.push_back(addr & 0xFF);
                bytes.push_back((addr >> 8) & 0xFF);
                break;
            }
            default: break;
        }
    }

    void encodeOperands(Instruction& inst,
                        std::unordered_map<std::string, Symbol>& symbols,
                        std::vector<uint8_t>& bytes)
    {
        const std::string& m = inst.mnemonic;

        // Jumps/Calls: encode 16-bit target address
        if (m=="JMP"||m=="JE"||m=="JNE"||m=="JZ"||
            m=="JNZ"||m=="JG"||m=="JL"||m=="CALL") {
            encodeOperand(inst.op1, symbols, bytes, /*wide=*/true);
            return;
        }
        // INT: single immediate
        if (m=="INT") {
            encodeOperand(inst.op1, symbols, bytes);
            return;
        }
        // Single-reg instructions
        if (m=="PUSH"||m=="POP"||m=="NOT"||m=="INC"||m=="DEC") {
            encodeOperand(inst.op1, symbols, bytes);
            return;
        }
        // NOP / HLT / RET: no operands
        if (m=="NOP"||m=="HLT"||m=="RET") return;

        // ── Two-operand instructions ──────────────────────────────
        // For MOV/ADD/SUB/etc we patch the opcode byte already pushed
        // to differentiate REG,REG from REG,IMM at the VM level.
        // Convention: base opcode = REG,REG  base+8 = REG,IMM
        if (inst.op2.type == OperandType::IMM) {
            // Patch last emitted opcode byte: add 1 to distinguish REG,IMM
            // We use a separate opcode: we rewrite bytes[last opcode pos]
            // Since we're inside encodeOperands, the opcode was pushed just before.
            // Patch it: opcode+8 means REG,IMM variant
            if (!bytes.empty()) bytes.back() += 8;
        }

        encodeOperand(inst.op1, symbols, bytes);
        // IMM src: always 16-bit (2 bytes)
        bool wide = (inst.op2.type == OperandType::IMM);
        encodeOperand(inst.op2, symbols, bytes, wide);
    }
};