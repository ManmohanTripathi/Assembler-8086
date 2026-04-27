#pragma once
#include "assembler.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <iostream>

// ─────────────────────────────────────────────────────────────────
//  Optimizer  — runs between Parser (Pass 1) and CodeGen (Pass 2)
//
//  Three classical compiler optimizations:
//
//  1. CONSTANT FOLDING
//     If both operands of ADD/SUB/MUL etc. are known compile-time
//     constants, compute the result now and replace with MOV.
//     Conservative: skips registers that are modified inside loops.
//     e.g.  MOV AX,3 / MOV BX,4 / ADD AX,BX  →  MOV AX,7
//
//  2. STRENGTH REDUCTION
//     Replace expensive instructions with cheaper equivalents:
//     MUL reg, 2  →  ADD reg, reg   (add is faster than multiply)
//     MUL reg, 1  →  removed        (identity)
//     MUL reg, 0  →  MOV reg, 0
//     MUL reg, 4  →  ADD reg,reg; ADD reg,reg
//
//  3. DEAD CODE ELIMINATION
//     Remove writes whose result is overwritten before any read.
//     Conservative: only runs on straight-line code (no jumps).
//     e.g.  MOV CX, 0  followed immediately by  MOV CX, 2  →  first removed
// ─────────────────────────────────────────────────────────────────

struct OptimizationStats {
    int constant_folds      = 0;
    int strength_reductions = 0;
    int dead_code_removed   = 0;

    void print() const {
        std::cout << "\n=== OPTIMIZER REPORT ===\n";
        std::cout << "  Constant folds:      " << constant_folds      << "\n";
        std::cout << "  Strength reductions: " << strength_reductions << "\n";
        std::cout << "  Dead code removed:   " << dead_code_removed   << "\n";
        int total = constant_folds + strength_reductions + dead_code_removed;
        std::cout << "  Total optimizations: " << total               << "\n";
    }
};

class Optimizer {
public:
    OptimizationStats stats;

    std::vector<Instruction> optimize(const std::vector<Instruction>& input) {
        std::vector<Instruction> pass1 = constantFolding(input);
        std::vector<Instruction> pass2 = strengthReduction(pass1);
        std::vector<Instruction> pass3 = deadCodeElimination(pass2);
        return pass3;
    }

private:
    // ─────────────────────────────────────────────
    //  Helper: does this instruction list contain jumps?
    // ─────────────────────────────────────────────
    bool hasControlFlow(const std::vector<Instruction>& instrs) {
        for (const auto& inst : instrs) {
            const std::string& m = inst.mnemonic;
            if (m=="JMP"||m=="JE"||m=="JNE"||m=="JZ"||m=="JNZ"||
                m=="JG"||m=="JL"||m=="CALL"||m=="RET")
                return true;
        }
        return false;
    }

    // ─────────────────────────────────────────────
    //  Pass A: Constant Folding
    //  Safe version: if any jumps exist, treat all
    //  written registers as non-constant (loop-safe).
    // ─────────────────────────────────────────────
    std::vector<Instruction> constantFolding(const std::vector<Instruction>& instrs) {
        std::vector<Instruction> out;
        std::unordered_map<std::string, int> known;

        // If there are jumps, we can't safely track register values
        // across loop back-edges — mark all written regs as "unknown"
        std::unordered_set<std::string> unsafe;
        if (hasControlFlow(instrs)) {
            for (const auto& inst : instrs)
                if (inst.op1.type == OperandType::REG)
                    unsafe.insert(inst.op1.reg_name);
        }

        for (auto inst : instrs) {
            const std::string& m = inst.mnemonic;

            // Skip folding for any register that might be a loop variable
            bool dst_safe = (inst.op1.type != OperandType::REG) ||
                            (unsafe.find(inst.op1.reg_name) == unsafe.end());
            bool src_safe = (inst.op2.type != OperandType::REG) ||
                            (unsafe.find(inst.op2.reg_name) == unsafe.end());

            // MOV reg, IMM → record constant
            if (m=="MOV" && inst.op1.type==OperandType::REG
                         && inst.op2.type==OperandType::IMM && dst_safe) {
                known[inst.op1.reg_name] = inst.op2.imm_val;
                out.push_back(inst);
                continue;
            }

            // MOV reg, reg → propagate
            if (m=="MOV" && inst.op1.type==OperandType::REG
                         && inst.op2.type==OperandType::REG && dst_safe && src_safe) {
                auto it = known.find(inst.op2.reg_name);
                if (it != known.end()) known[inst.op1.reg_name] = it->second;
                else known.erase(inst.op1.reg_name);
                out.push_back(inst);
                continue;
            }

            // Arithmetic reg, IMM — fold if dst known and safe
            if (inst.op1.type==OperandType::REG && inst.op2.type==OperandType::IMM
             && dst_safe) {
                auto it = known.find(inst.op1.reg_name);
                if (it != known.end()) {
                    int lhs=it->second, rhs=inst.op2.imm_val, result=0;
                    bool folded=true;
                    if      (m=="ADD") result=lhs+rhs;
                    else if (m=="SUB") result=lhs-rhs;
                    else if (m=="MUL") result=lhs*rhs;
                    else if (m=="AND") result=lhs&rhs;
                    else if (m=="OR")  result=lhs|rhs;
                    else if (m=="XOR") result=lhs^rhs;
                    else folded=false;
                    if (folded) {
                        inst.mnemonic="MOV";
                        inst.op2.type=OperandType::IMM;
                        inst.op2.imm_val=(uint16_t)(result&0xFFFF);
                        known[inst.op1.reg_name]=result;
                        out.push_back(inst);
                        stats.constant_folds++;
                        continue;
                    }
                }
            }

            // Arithmetic reg, reg — fold if both known and safe
            if (inst.op1.type==OperandType::REG && inst.op2.type==OperandType::REG
             && dst_safe && src_safe) {
                auto it1=known.find(inst.op1.reg_name);
                auto it2=known.find(inst.op2.reg_name);
                if (it1!=known.end() && it2!=known.end()) {
                    int lhs=it1->second, rhs=it2->second, result=0;
                    bool folded=true;
                    if      (m=="ADD") result=lhs+rhs;
                    else if (m=="SUB") result=lhs-rhs;
                    else if (m=="MUL") result=lhs*rhs;
                    else if (m=="AND") result=lhs&rhs;
                    else if (m=="OR")  result=lhs|rhs;
                    else if (m=="XOR") result=lhs^rhs;
                    else folded=false;
                    if (folded) {
                        inst.mnemonic="MOV";
                        inst.op2.type=OperandType::IMM;
                        inst.op2.imm_val=(uint16_t)(result&0xFFFF);
                        known[inst.op1.reg_name]=result;
                        out.push_back(inst);
                        stats.constant_folds++;
                        continue;
                    }
                }
            }

            if (inst.op1.type==OperandType::REG)
                known.erase(inst.op1.reg_name);

            out.push_back(inst);
        }
        return out;
    }

    // ─────────────────────────────────────────────
    //  Pass B: Strength Reduction
    // ─────────────────────────────────────────────
    std::vector<Instruction> strengthReduction(const std::vector<Instruction>& instrs) {
        std::vector<Instruction> out;
        for (auto inst : instrs) {
            if (inst.mnemonic=="MUL" && inst.op1.type==OperandType::REG
             && inst.op2.type==OperandType::IMM) {
                int imm = inst.op2.imm_val;
                if (imm == 0) {
                    inst.mnemonic="MOV"; inst.op2.imm_val=0;
                    out.push_back(inst); stats.strength_reductions++; continue;
                }
                if (imm == 1) {
                    stats.strength_reductions++; continue;  // identity
                }
                if (imm == 2) {
                    inst.mnemonic="ADD";
                    inst.op2.type=OperandType::REG;
                    inst.op2.reg_name=inst.op1.reg_name;
                    out.push_back(inst); stats.strength_reductions++; continue;
                }
                if (imm == 4) {
                    Instruction add=inst;
                    add.mnemonic="ADD";
                    add.op2.type=OperandType::REG;
                    add.op2.reg_name=inst.op1.reg_name;
                    out.push_back(add); out.push_back(add);
                    stats.strength_reductions++; continue;
                }
            }
            out.push_back(inst);
        }
        return out;
    }

    // ─────────────────────────────────────────────
    //  Pass C: Dead Code Elimination
    //  Conservative: only runs on straight-line code.
    // ─────────────────────────────────────────────
    std::vector<Instruction> deadCodeElimination(const std::vector<Instruction>& instrs) {
        if (hasControlFlow(instrs)) return instrs;  // skip for loops/branches

        int n = instrs.size();
        std::vector<bool> dead(n, false);
        std::unordered_map<std::string, int> lastWrite;

        for (int i = 0; i < n; i++) {
            const auto& inst = instrs[i];
            // Mark src as read → un-kill its last write
            auto markRead = [&](const Operand& op) {
                if (op.type==OperandType::REG) lastWrite.erase(op.reg_name);
            };
            markRead(inst.op2);
            if (inst.op2.type==OperandType::REG && inst.op1.type==OperandType::REG
             && inst.op1.reg_name==inst.op2.reg_name) markRead(inst.op1);

            if (inst.op1.type==OperandType::REG) {
                auto it=lastWrite.find(inst.op1.reg_name);
                if (it!=lastWrite.end()) {
                    dead[it->second]=true;
                    stats.dead_code_removed++;
                }
                lastWrite[inst.op1.reg_name]=i;
            }
        }

        std::vector<Instruction> out;
        for (int i=0; i<n; i++)
            if (!dead[i]) out.push_back(instrs[i]);
        return out;
    }
};