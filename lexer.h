#pragma once
#include "assembler.h"
#include <sstream>
#include <algorithm>
#include <cctype>

// Known 8086 mnemonics supported by this assembler
static const std::vector<std::string> MNEMONICS = {
    "MOV", "ADD", "SUB", "MUL", "DIV",
    "AND", "OR",  "XOR", "NOT",
    "INC", "DEC",
    "JMP", "JE",  "JNE", "JZ", "JNZ", "JG", "JL",
    "CMP",
    "PUSH", "POP",
    "CALL", "RET",
    "NOP", "HLT",
    "INT"
};

// Known 8086 registers
static const std::vector<std::string> REGISTERS = {
    "AX","BX","CX","DX",
    "AH","AL","BH","BL","CH","CL","DH","DL",
    "SI","DI","SP","BP",
    "CS","DS","ES","SS"
};

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static bool isMnemonic(const std::string& s) {
    std::string u = toUpper(s);
    return std::find(MNEMONICS.begin(), MNEMONICS.end(), u) != MNEMONICS.end();
}

static bool isRegister(const std::string& s) {
    std::string u = toUpper(s);
    return std::find(REGISTERS.begin(), REGISTERS.end(), u) != REGISTERS.end();
}

// Parse numeric literals: decimal, 0x hex, 0b binary
static bool parseNumber(const std::string& s, uint16_t& out) {
    try {
        if (s.size() > 2 && s[0] == '0' && (s[1]=='x'||s[1]=='X'))
            out = (uint16_t)std::stoul(s.substr(2), nullptr, 16);
        else if (s.size() > 2 && s[0] == '0' && (s[1]=='b'||s[1]=='B'))
            out = (uint16_t)std::stoul(s.substr(2), nullptr, 2);
        else
            out = (uint16_t)std::stoul(s, nullptr, 10);
        return true;
    } catch (...) { return false; }
}

// ─────────────────────────────────────────────
//  Lexer Class
// ─────────────────────────────────────────────
class Lexer {
public:
    std::vector<Token> tokenize(const std::string& source) {
        std::vector<Token> tokens;
        std::istringstream stream(source);
        std::string line;
        int lineNo = 1;

        while (std::getline(stream, line)) {
            tokenizeLine(line, lineNo, tokens);
            tokens.push_back({TokenType::NEWLINE, "\n", lineNo});
            lineNo++;
        }
        tokens.push_back({TokenType::END_OF_FILE, "", lineNo});
        return tokens;
    }

private:
    void tokenizeLine(const std::string& line, int lineNo, std::vector<Token>& tokens) {
        size_t i = 0;
        size_t n = line.size();

        while (i < n) {
            // Skip whitespace
            if (std::isspace(line[i])) { i++; continue; }

            // Skip comments (; to end of line)
            if (line[i] == ';') break;

            // Comma
            if (line[i] == ',') {
                tokens.push_back({TokenType::COMMA, ",", lineNo});
                i++; continue;
            }

            // Brackets
            if (line[i] == '[') {
                tokens.push_back({TokenType::LBRACKET, "[", lineNo});
                i++; continue;
            }
            if (line[i] == ']') {
                tokens.push_back({TokenType::RBRACKET, "]", lineNo});
                i++; continue;
            }

            // Identifier or keyword: letter/underscore start
            if (std::isalpha(line[i]) || line[i] == '_') {
                size_t start = i;
                while (i < n && (std::isalnum(line[i]) || line[i] == '_')) i++;
                std::string word = line.substr(start, i - start);

                // Check for label definition (word followed by ':')
                if (i < n && line[i] == ':') {
                    i++; // consume ':'
                    tokens.push_back({TokenType::LABEL_DEF, toUpper(word), lineNo});
                    continue;
                }

                std::string upper = toUpper(word);
                if (isMnemonic(upper))
                    tokens.push_back({TokenType::MNEMONIC, upper, lineNo});
                else if (isRegister(upper))
                    tokens.push_back({TokenType::REGISTER, upper, lineNo});
                else
                    tokens.push_back({TokenType::LABEL_REF, upper, lineNo});
                continue;
            }

            // Number literal
            if (std::isdigit(line[i]) || (line[i]=='-' && i+1<n && std::isdigit(line[i+1]))) {
                size_t start = i;
                if (line[i] == '-') i++;
                while (i < n && (std::isalnum(line[i]))) i++;
                tokens.push_back({TokenType::NUMBER, line.substr(start, i - start), lineNo});
                continue;
            }

            // Unknown character — skip with warning
            i++;
        }
    }
};