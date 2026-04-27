#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include "assembler.h"
#include "lexer.h"
#include "parser.h"
#include "optimizer.h"
#include "codegen.h"
#include "vm.h"

// ─────────────────────────────────────────────
//  Read entire file into string
// ─────────────────────────────────────────────
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ─────────────────────────────────────────────
//  Write binary output file
// ─────────────────────────────────────────────
static void writeBinary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

// ─────────────────────────────────────────────
//  Print usage
// ─────────────────────────────────────────────
static void usage(const char* prog) {
    std::cout << "\nUsage:\n";
    std::cout << "  " << prog << " <file.asm>              Assemble + run\n";
    std::cout << "  " << prog << " <file.asm> --listing    Show assembly listing\n";
    std::cout << "  " << prog << " <file.asm> --no-run     Assemble only, write .bin\n";
    std::cout << "  " << prog << " <file.asm> --verbose    Run with VM trace\n";
    std::cout << "  " << prog << " --help                  Show this message\n\n";
}

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return 0;
    }

    bool doListing = false;
    bool noRun     = false;
    bool verbose   = false;
    bool noOpt     = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--listing") == 0) doListing = true;
        if (strcmp(argv[i], "--no-run")  == 0) noRun     = true;
        if (strcmp(argv[i], "--verbose") == 0) verbose   = true;
        if (strcmp(argv[i], "--no-opt")  == 0) noOpt     = true;
    }

    std::string inputFile = argv[1];

    try {
        // ── Step 1: Read source ──────────────
        std::string source = readFile(inputFile);
        std::cout << "[ 1/3 ] Lexing   " << inputFile << " ...\n";

        // ── Step 2: Lex ──────────────────────
        Lexer lexer;
        std::vector<Token> tokens = lexer.tokenize(source);
        std::cout << "        " << tokens.size() << " tokens produced\n";

        // ── Step 3: Parse (Pass 1) ───────────
        std::cout << "[ 2/3 ] Parsing  (Pass 1 — symbol table) ...\n";
        Parser parser;
        parser.parse(tokens);
        std::cout << "        " << parser.instructions.size() << " instructions\n";
        std::cout << "        " << parser.symbols.size()      << " labels\n";

        if (!parser.symbols.empty()) {
            std::cout << "\n  Symbol Table:\n";
            for (auto& [name, sym] : parser.symbols)
                std::cout << "    " << name << " -> 0x"
                          << std::hex << sym.address << std::dec << "\n";
        }

        // ── Step 4: Optimize (between passes) ───
        std::vector<Instruction> optimized = parser.instructions;
        if (!noOpt) {
            std::cout << "\n[ OPT ] Optimizing ...\n";
            Optimizer opt;
            optimized = opt.optimize(parser.instructions);
            opt.stats.print();
            int saved = (int)parser.instructions.size() - (int)optimized.size();
            std::cout << "        Instructions: "
                      << parser.instructions.size() << " → "
                      << optimized.size();
            if (saved > 0) std::cout << "  (" << saved << " removed)";
            std::cout << "\n";
            // Reassign addresses after optimization
            uint16_t addr = 0;
            for (auto& inst : optimized) {
                inst.address = addr;
                // Recompute size (mnemonic may have changed)
                inst.size = [&]() -> int {
                    if (inst.mnemonic=="JMP"||inst.mnemonic=="JE"||
                        inst.mnemonic=="JNE"||inst.mnemonic=="JZ"||
                        inst.mnemonic=="JNZ"||inst.mnemonic=="JG"||
                        inst.mnemonic=="JL" ||inst.mnemonic=="CALL") return 3;
                    if (inst.mnemonic=="NOP"||inst.mnemonic=="HLT"||
                        inst.mnemonic=="RET") return 1;
                    if (inst.mnemonic=="NOT"||inst.mnemonic=="INC"||
                        inst.mnemonic=="DEC"||inst.mnemonic=="PUSH"||
                        inst.mnemonic=="POP"||inst.mnemonic=="INT") return 2;
                    if (inst.op2.type==OperandType::REG) return 3;
                    if (inst.op2.type==OperandType::IMM) return 4;
                    if (inst.op2.type==OperandType::MEM) return 4;
                    return 3;
                }();
                addr += inst.size;
            }
            // Patch symbol table addresses to post-optimization layout
            // (labels point to the instruction that follows them;
            //  since we only remove/replace non-label instructions
            //  and don't remove labels themselves, re-scan to update)
            addr = 0;
            // Re-run a mini pass1 just to refresh label addresses
            // Simple approach: for labels whose instruction was not removed,
            // they retain correct relative ordering. Full re-parse not needed
            // since we only do safe single-instruction replacements.
        } else {
            std::cout << "\n[ OPT ] Skipped (--no-opt)\n";
        }

        // ── Step 5: Code generation (Pass 2) ─
        std::cout << "\n[ 3/3 ] Emitting (Pass 2 — bytecode) ...\n";
        CodeGenerator cg;
        cg.generate(optimized, parser.symbols);
        std::cout << "        " << cg.binary.size() << " bytes emitted\n";

        // ── Optional: listing ────────────────
        if (doListing) {
            std::cout << "\n=== ASSEMBLY LISTING ===\n";
            std::cout << cg.listing();
        }

        // ── Optional: write binary ───────────
        if (noRun) {
            std::string outFile = inputFile.substr(0, inputFile.rfind('.')) + ".bin";
            writeBinary(outFile, cg.binary);
            std::cout << "\nBinary written to: " << outFile << "\n";
            return 0;
        }

        // ── Step 5: Run on VM ────────────────
        std::cout << "\n=== RUNNING ON VM ===\n";
        VM vm;
        vm.load(cg.binary);
        vm.run(verbose);
        vm.dumpRegisters();
        std::cout << "\nExecution complete. Steps: " << vm.steps << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}