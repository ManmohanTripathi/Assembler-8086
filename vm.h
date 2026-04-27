#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

// ─────────────────────────────────────────────
//  8086-inspired Register-Based VM
//  Registers: AX BX CX DX SI DI SP BP
//  Flags:     ZF (zero), SF (sign), OF (overflow)
// ─────────────────────────────────────────────

static const std::unordered_map<uint8_t, std::string> REG_NAMES = {
    {0x00,"AX"},{0x01,"CX"},{0x02,"DX"},{0x03,"BX"},
    {0x04,"SP"},{0x05,"BP"},{0x06,"SI"},{0x07,"DI"}
};

class VM {
public:
    std::array<uint16_t, 16> regs{};   // 16 register slots (AX..BH)
    std::array<uint8_t, 65536> mem{};  // 64KB address space
    uint16_t ip = 0;                   // instruction pointer
    bool ZF = false, SF = false, CF = false;
    bool halted = false;
    int  steps  = 0;

    static const int MAX_STEPS = 100000; // guard against infinite loops

    void load(const std::vector<uint8_t>& binary, uint16_t origin = 0x0000) {
        for (size_t i = 0; i < binary.size(); i++)
            mem[origin + i] = binary[i];
        ip = origin;
    }

    void run(bool verbose = false) {
        while (!halted && steps < MAX_STEPS) {
            step(verbose);
        }
        if (steps >= MAX_STEPS)
            std::cerr << "[VM] Execution limit reached (possible infinite loop)\n";
    }

    void dumpRegisters() const {
        std::cout << "\n=== REGISTER DUMP ===\n";
        std::cout << std::hex << std::uppercase;
        std::cout << "AX=" << regs[0x00] << "  BX=" << regs[0x03]
                  << "  CX=" << regs[0x01] << "  DX=" << regs[0x02] << "\n";
        std::cout << "SI=" << regs[0x06] << "  DI=" << regs[0x07]
                  << "  SP=" << regs[0x04] << "  BP=" << regs[0x05] << "\n";
        std::cout << "ZF=" << ZF << "  SF=" << SF << "  CF=" << CF << "\n";
        std::cout << "IP=" << ip  << "\n";
        std::cout << std::dec;
    }

private:
    uint8_t  fetchByte()  { return mem[ip++]; }
    uint16_t fetchWord()  {
        uint16_t lo = mem[ip++];
        uint16_t hi = mem[ip++];
        return lo | (hi << 8);
    }

    void setFlags(uint32_t result) {
        ZF = ((result & 0xFFFF) == 0);
        SF = ((result & 0x8000) != 0);
        CF = (result > 0xFFFF);
    }

    // Stack helpers
    void push16(uint16_t val) {
        regs[0x04] -= 2;
        mem[regs[0x04]]   = val & 0xFF;
        mem[regs[0x04]+1] = (val >> 8) & 0xFF;
    }
    uint16_t pop16() {
        uint16_t lo = mem[regs[0x04]];
        uint16_t hi = mem[regs[0x04]+1];
        regs[0x04] += 2;
        return lo | (hi << 8);
    }

    void step(bool verbose) {
        if (ip >= 65535) { halted = true; return; }
        steps++;
        uint8_t opcode = fetchByte();

        if (verbose) {
            std::cout << "[IP=" << std::hex << (ip-1)
                      << "] OP=" << (int)opcode << std::dec << "\n";
        }

        switch (opcode) {

        // ── MOV ──────────────────────────────
        case 0x10: { // MOV REG, REG  (opcode=0x10, REG,REG)
            uint8_t dst = fetchByte();
            uint8_t src = fetchByte();
            regs[dst] = regs[src];
            break;
        }
        case 0x18: { // MOV REG, IMM16  (opcode=0x10+8)
            uint8_t  dst = fetchByte();
            uint16_t imm = fetchWord();
            regs[dst] = imm;
            break;
        }

        // ── ADD / SUB / MUL / DIV ─────────────
        case 0x20: { // ADD REG, REG
            uint8_t d = fetchByte(), s = fetchByte();
            uint32_t r = regs[d] + regs[s];
            setFlags(r); regs[d] = r & 0xFFFF; break;
        }
        case 0x28: { // ADD REG, IMM16
            uint8_t d = fetchByte(); uint16_t imm = fetchWord();
            uint32_t r = regs[d] + imm;
            setFlags(r); regs[d] = r & 0xFFFF; break;
        }
        case 0x21: { // SUB REG, REG
            uint8_t d = fetchByte(), s = fetchByte();
            uint32_t r = regs[d] - regs[s];
            setFlags(r); regs[d] = r & 0xFFFF; break;
        }
        case 0x29: { // SUB REG, IMM16
            uint8_t d = fetchByte(); uint16_t imm = fetchWord();
            uint32_t r = regs[d] - imm;
            setFlags(r); regs[d] = r & 0xFFFF; break;
        }
        case 0x22: { // MUL REG, REG  → result in AX
            uint8_t d = fetchByte(), s = fetchByte();
            uint32_t r = regs[d] * regs[s];
            setFlags(r); regs[0] = r & 0xFFFF; break;
        }
        case 0x23: { // DIV REG, REG  → quotient in AX
            uint8_t d = fetchByte(), s = fetchByte();
            if (regs[s] == 0) throw std::runtime_error("Division by zero");
            regs[0] = regs[d] / regs[s];
            ZF = (regs[0] == 0); break;
        }

        // ── Logic ────────────────────────────
        case 0x30: { uint8_t d=fetchByte(),s=fetchByte(); regs[d]&=regs[s]; setFlags(regs[d]); break; } // AND
        case 0x31: { uint8_t d=fetchByte(),s=fetchByte(); regs[d]|=regs[s]; setFlags(regs[d]); break; } // OR
        case 0x32: { uint8_t d=fetchByte(),s=fetchByte(); regs[d]^=regs[s]; setFlags(regs[d]); break; } // XOR
        case 0x33: { uint8_t d=fetchByte(); regs[d]=~regs[d]; setFlags(regs[d]); break; }               // NOT

        // ── INC / DEC ─────────────────────────
        case 0x40: { uint8_t d=fetchByte(); regs[d]++; ZF=(regs[d]==0); SF=(regs[d]&0x8000)!=0; break; }
        case 0x41: { uint8_t d=fetchByte(); regs[d]--; ZF=(regs[d]==0); SF=(regs[d]&0x8000)!=0; break; }

        // ── CMP ──────────────────────────────
        case 0x50: {
            uint8_t d=fetchByte(), s=fetchByte();
            uint32_t r = regs[d] - regs[s]; setFlags(r); break;
        }

        // ── Jumps ────────────────────────────
        case 0x60: { ip = fetchWord(); break; }                         // JMP
        case 0x61: { uint16_t a=fetchWord(); if (ZF)      ip=a; break;} // JE
        case 0x62: { uint16_t a=fetchWord(); if (!ZF)     ip=a; break;} // JNE
        case 0x63: { uint16_t a=fetchWord(); if (ZF)      ip=a; break;} // JZ
        case 0x64: { uint16_t a=fetchWord(); if (!ZF)     ip=a; break;} // JNZ
        case 0x65: { uint16_t a=fetchWord(); if (!ZF&&!SF)ip=a; break;} // JG
        case 0x66: { uint16_t a=fetchWord(); if (SF)      ip=a; break;} // JL

        // ── PUSH / POP ───────────────────────
        case 0x70: { uint8_t r=fetchByte(); push16(regs[r]); break; }
        case 0x71: { uint8_t r=fetchByte(); regs[r]=pop16();  break; }

        // ── CALL / RET ───────────────────────
        case 0x80: { uint16_t a=fetchWord(); push16(ip); ip=a; break; }
        case 0x81: { ip = pop16(); break; }

        // ── INT (software interrupt) ─────────
        case 0xCD: {
            uint8_t n = fetchByte();
            if (n == 0x21) handleDOS(); // Simulate a tiny DOS int 21h
            break;
        }

        // ── NOP / HLT ────────────────────────
        case 0xF0: break;           // NOP
        case 0xFF: halted=true; break; // HLT

        default:
            std::cerr << "[VM] Unknown opcode 0x" << std::hex << (int)opcode
                      << " at IP=" << (ip-1) << std::dec << "\n";
            halted = true;
        }
    }

    // Minimal DOS int 21h simulation
    void handleDOS() {
        uint8_t ah = (regs[0x00] >> 8) & 0xFF; // AH
        if (ah == 0x02) {
            // Print character in DL
            char c = regs[0x02] & 0xFF;
            std::cout << c;
        } else if (ah == 0x4C) {
            // Exit program
            halted = true;
        }
    }
};