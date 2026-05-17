#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <cmath>
#include <memory>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <cstdlib>

std::string tos = "Unknown OS";

#ifndef MY_OP
void print(const char* msg) {
    std::cout << msg << std::flush;
}
inline void print(const std::string& msg) {
    print(msg.c_str());
}
void error(const char* msg) {
    if (!msg) msg = "Unknown Error";
    std::cerr << "\n[ERROR]: " << msg << std::endl;
    exit(1);
}
inline void error(const std::string& msg) {
    error(msg.c_str());
}
std::string read() {
    std::string s;
    if (!std::getline(std::cin, s)) {
        return "";
    }
    return s;
}
#endif

enum VarType { T_NULL, T_INT, T_FLOAT, T_STR, T_PTR, T_ARR };

struct Cell {
    VarType type = T_NULL;
    int64_t v_int = 0;
    double v_float = 0.0;
    std::string v_str = "";
    void* v_ptr = nullptr;
    std::vector<Cell>* v_arr = nullptr;
    bool is_dyn = false;

    void clear() {
        if (type == T_ARR && v_arr && is_dyn) {
            delete v_arr;
        }
        if (type == T_PTR && v_ptr && is_dyn) {
            free(v_ptr);
        }
        type = T_NULL;
        v_arr = nullptr;
        v_str = "";
        is_dyn = false;
        v_ptr = nullptr;
    }
};

class PVM;

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string getName() = 0;
    virtual void execute(uint32_t cmd, uint32_t start_cell, PVM* vm) = 0;
    virtual std::string getCODE() = 0;
};

class PVM {
public:
    std::vector<uint8_t> code;
    std::vector<Cell> cells;
    uint32_t pc = 0;
    uint32_t sp;
    bool running = true;
    std::map<uint32_t, uint32_t> func_table;
    std::map<std::string, std::shared_ptr<IPlugin>> plugins;
    std::vector<uint32_t> call_stack;

    PVM(const std::vector<uint8_t>& bc, size_t mem_size = 65536) : code(bc) {
        cells.resize(mem_size);
        sp = (uint32_t)mem_size - 1;
    }
    ~PVM() {
        for (auto& cell : cells) cell.clear();
    }

    void loadPlugin(std::shared_ptr<IPlugin> p) { plugins[p->getName()] = p; }
    void setCode(const std::vector<uint8_t>& newCode) {
        this->code = newCode;
        this->pc = 0;
    }
    uint32_t read32() {
        uint32_t v;
        std::memcpy(&v, &code[pc], 4);
        pc += 4;
        return v;
    }
    int64_t read64() { int64_t v; std::memcpy(&v, &code[pc], 8); pc += 8; return v; }
    double read_dbl() { double v; std::memcpy(&v, &code[pc], 8); pc += 8; return v; }

    void step() {
        if (pc >= code.size()) { running = false; return; }
        uint8_t op = code[pc++];
        switch (op) {
        case 0x00: running = false; break;
        case 0x01: break; // NOP
        case 0x02: pc = read32(); break; // JMP
        case 0x03: { uint32_t c = read32(), t = read32(); if (cells[c].v_int == 0) pc = t; break; } // JZ
        case 0x04: { uint32_t c = read32(), t = read32(); if (cells[c].v_int != 0) pc = t; break; } // JNZ
        case 0x05: { uint32_t t = read32(); cells[--sp].v_int = pc; cells[sp].type = T_INT; call_stack.push_back(pc++); pc = t; break; } // CALL
        case 0x06: { // RET
            if (!call_stack.empty()) {
                pc = call_stack.back();
                call_stack.pop_back();
            }
            else {
                return;
            }
            break;
        }
        case 0x07: { cells[--sp] = cells[read32()]; break; } // PUSH
        case 0x08: { cells[read32()] = cells[sp++]; break; } // POP
        case 0x09: { cells[sp - 1] = cells[sp]; sp--; break; } // DUP
        case 0x10: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int + cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x11: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int - cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x12: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int * cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x13: { uint32_t r = read32(), a = read32(), b = read32(); if (cells[b].v_int) cells[r].v_int = cells[a].v_int / cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x14: { uint32_t r = read32(), a = read32(), b = read32(); if (cells[b].v_int) cells[r].v_int = cells[a].v_int % cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x15: { cells[read32()].v_int++; break; } // INC
        case 0x16: { cells[read32()].v_int--; break; } // DEC
        case 0x17: { uint32_t r = read32(), a = read32(); cells[r].v_int = -cells[a].v_int; cells[r].type = T_INT; break; } // NEG
        case 0x20: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_float = cells[a].v_float + cells[b].v_float; cells[r].type = T_FLOAT; break; }
        case 0x21: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_float = cells[a].v_float - cells[b].v_float; cells[r].type = T_FLOAT; break; }
        case 0x22: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_float = cells[a].v_float * cells[b].v_float; cells[r].type = T_FLOAT; break; }
        case 0x23: { uint32_t r = read32(), a = read32(), b = read32(); if (cells[b].v_float != 0) cells[r].v_float = cells[a].v_float / cells[b].v_float; cells[r].type = T_FLOAT; break; }
        case 0x24: { uint32_t r = read32(), a = read32(); cells[r].v_float = -cells[a].v_float; cells[r].type = T_FLOAT; break; }
        case 0x30: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int & cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x31: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int | cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x32: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int ^ cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x33: { uint32_t r = read32(), a = read32(); cells[r].v_int = ~cells[a].v_int; cells[r].type = T_INT; break; }
        case 0x34: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int << cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x35: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = cells[a].v_int >> cells[b].v_int; cells[r].type = T_INT; break; }
        case 0x36: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = (cells[a].v_int == cells[b].v_int); cells[r].type = T_INT; break; }
        case 0x37: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = (cells[a].v_int > cells[b].v_int); cells[r].type = T_INT; break; }
        case 0x38: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = (cells[a].v_int < cells[b].v_int); cells[r].type = T_INT; break; }
        case 0x40: { uint32_t r = read32(); cells[r].v_int = read64(); cells[r].type = T_INT; break; }
        case 0x41: { uint32_t r = read32(); cells[r].v_float = read_dbl(); cells[r].type = T_FLOAT; break; }
        case 0x42: { uint32_t r = read32(), len = read32(); cells[r].v_str = std::string((char*)&code[pc], len); pc += len; cells[r].type = T_STR; break; }
        case 0x43: { uint32_t d = read32(), s = read32(); cells[d] = cells[s]; break; } // MOV
        case 0x44: { uint32_t a = read32(), b = read32(); Cell t = cells[a]; cells[a] = cells[b]; cells[b] = t; break; } // SWAP
        case 0x45: { uint32_t r = read32(); if (cells[r].type == T_STR) { try { cells[r].v_int = std::stoll(cells[r].v_str); cells[r].type = T_INT; } catch (...) { cells[r].v_int = 0; } } break; } // TO_INT
        case 0x46: { uint32_t r = read32(); if (cells[r].type == T_INT) { cells[r].v_str = std::to_string(cells[r].v_int); cells[r].type = T_STR; } break; } // TO_STR
        case 0x47: { uint32_t r = read32(), s = read32(); cells[r].v_int = cells[s].type; cells[r].type = T_INT; break; } // TYPEOF
        case 0x48: {
            uint32_t d = read32();
            cells[d].clear();
            break;
        }
        case 0x50: {
            uint32_t r = read32();
            if (cells[r].type == T_INT) print(std::to_string(cells[r].v_int));
            else if (cells[r].type == T_STR) print(cells[r].v_str);
            else if (cells[r].type == T_FLOAT) print(std::to_string(cells[r].v_float));
            break;
        }
        case 0x51: { print("\n"); break; }
        case 0x52: { uint32_t r = read32(); cells[r].v_str = read(); cells[r].type = T_STR; break; }
        case 0x53: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_str = cells[a].v_str + cells[b].v_str; cells[r].type = T_STR; break; } // CONCAT
        case 0x54: { uint32_t r = read32(), a = read32(); cells[r].v_int = cells[a].v_str.length(); cells[r].type = T_INT; break; } // STRLEN
        case 0x55: { uint32_t r = read32(), a = read32(), b = read32(); cells[r].v_int = (cells[a].v_str == cells[b].v_str); cells[r].type = T_INT; break; } // STRCMP
        case 0x60: {
            uint32_t v = read32();
            if (cells[v].type == T_STR) {
                system(cells[v].v_str.c_str());
            }
            break;
        }
        case 0x61: {
            uint32_t v = read32();
            std::string info = "";

#ifdef _WIN32
            info += "Windows";
#elif __linux__
            info += "Linux";
#else
            info += tos;
#endif

            info += "|Loader: PIBR v1.0";
            info += "|Brand: ParrotSoft";

            cells[v].v_str = info;
            cells[v].type = T_STR;
            break;
        }
        case 0x62: { uint32_t r = read32(), arr = read32(), idx = read32(); if (cells[arr].type == T_ARR) cells[r] = (*cells[arr].v_arr)[cells[idx].v_int]; break; }
        case 0x63: {
            uint32_t r = read32(), sz = read32();
            cells[r].v_ptr = malloc(cells[sz].v_int);
            cells[r].type = T_PTR;
            cells[r].is_dyn = true;
            break;
        }
        case 0x64: {
            uint32_t r = read32();
            if (cells[r].v_ptr) free(cells[r].v_ptr);
            cells[r].v_ptr = nullptr;
            cells[r].is_dyn = false;
            break;
        }
        case 0x70: {
            uint8_t len = code[pc++]; std::string name((char*)&code[pc], len); pc += len;
            uint32_t cmd = read32(); uint32_t sc = read32();
            if (plugins.count(name)) plugins[name]->execute(cmd, sc, this); break;
        }
        case 0x71: { // DEF_FUNC
            uint32_t id = read32();
            uint32_t addr = read32();
            func_table[id] = addr;
            while (pc < code.size()) {
                uint8_t next_op = code[pc++];
                if (next_op == 0x06) {
                    break;
                }
            }
            break;
        }
        case 0x72: { // CALL
            uint32_t id = read32();
            if (func_table.count(id)) {
                call_stack.push_back(pc);
                pc = func_table[id];
            }
            else {
                error("Function " + std::to_string(id) + " not found!");
            }
            break;
        }
        case 0xFF: { // HALT
            running = false;
            break;
        }
        }
    }

    void run() { while (running) step(); }
};

class AOTCompiler {
    std::vector<uint8_t> code;
    uint32_t pc = 0;
    std::stringstream out;
    uint32_t max_mem = 0;
    uint32_t current_func_id = 0;
    uint32_t current_func_entry = 0;
    uint32_t current_func_end = 0;
    std::map<uint32_t, uint32_t> func_map;

    uint32_t read32() { uint32_t v; std::memcpy(&v, &code[pc], 4); pc += 4; return v; }
    int64_t read64() { int64_t v; std::memcpy(&v, &code[pc], 8); pc += 8; return v; }
    double read_dbl() { double v; std::memcpy(&v, &code[pc], 8); pc += 8; return v; }

    std::string idx(uint32_t i) {
        if (i >= max_mem) return "0 /* OOB */";
        return std::to_string(i);
    }

public:
    std::string compile(const std::vector<uint8_t>& bytecode, uint32_t mem_size, const std::vector<std::shared_ptr<IPlugin>>& embedded_plugins) {
        code = bytecode;
        pc = 0;
        max_mem = mem_size;

        out << "#include <iostream>\n#include <vector>\n#include <string>\n#include <map>\n#include <cstdint>\n#include <memory>\n#include <cstdlib>\n#include <cstring>\n";

        for (const auto& plugin : embedded_plugins) {
            out << plugin->getCODE() << "\n";
        }

        out << "enum VarType { T_NULL, T_INT, T_FLOAT, T_STR, T_PTR, T_ARR };\n";
        out << "struct Cell {\n"
            "    VarType type = T_NULL;\n"
            "    int64_t v_int = 0;\n"
            "    double v_float = 0.0;\n"
            "    std::string v_str = \"\";\n"
            "    void* v_ptr = nullptr;\n"
            "    void* v_arr = nullptr;\n"
            "    bool is_dyn = false;\n"
            "    void clear() {\n"
            "        if(type == T_STR && !v_str.empty() && is_dyn) free((void*)v_str.c_str());\n"
            "        if(type == T_ARR && v_arr && is_dyn) delete (std::vector<Cell>*)v_arr;\n"
            "        if(type == T_PTR && v_ptr && is_dyn) free(v_ptr);\n"
            "        type = T_NULL; v_arr = nullptr; v_str = \"\"; v_ptr = nullptr; is_dyn = false;\n"
            "    }\n"
            "};\n\n";

        out << "class PVM { public: std::vector<uint8_t> code; std::vector<Cell> cells; uint32_t pc; uint32_t sp; bool running; };\n";
        out << "class IPlugin { public: virtual ~IPlugin() = default; virtual std::string getName() = 0; virtual void execute(uint32_t cmd, uint32_t sc, PVM* vm) = 0; virtual std::string getCODE() = 0; };\n";

        out << "int main() {\n";
        out << "    std::vector<Cell> cells(" << mem_size << ");\n";
        out << "    uint32_t sp = " << mem_size - 1 << ";\n";
        out << "    std::map<std::string, std::shared_ptr<IPlugin>> plugins;\n";
        out << "    std::map<uint32_t, uint32_t> func_table;\n";
        out << "    PVM vm;\n";
        out << "    vm.cells = cells;\n\n";

        for (const auto& plugin : embedded_plugins) {
            out << "    {\n"
                << "        auto pl = std::shared_ptr<IPlugin>(create_plugin_" << plugin->getName() << "());\n"
                << "        plugins[pl->getName()] = pl;\n"
                << "    }\n";
        }

        while (pc < code.size()) {
            uint32_t addr = pc;
            uint8_t op = code[pc++];
            out << "L_" << addr << ": ";

            switch (op) {
            case 0x01: out << "; // NOP\n"; break;
            case 0x02: out << "goto L_" << read32() << "; // JMP\n"; break;
            case 0x03: { uint32_t c = read32(), t = read32(); out << "if(cells[" << idx(c) << "].v_int == 0) goto L_" << t << "; // JZ\n"; break; }
            case 0x04: { uint32_t c = read32(), t = read32(); out << "if(cells[" << idx(c) << "].v_int != 0) goto L_" << t << "; // JNZ\n"; break; }
            case 0x05: {
                uint32_t t = read32();
                out << "cells[--sp].v_int = " << pc << "; cells[sp].type = T_INT; goto L_" << t << "; // CALL\n";
                break;
            }
            case 0x06: { // RET
                out << "  {\n";
                out << "    uint32_t r_addr = cells[sp++].v_int;\n";
                out << "    switch(r_addr) {\n";

                for (uint32_t m = 0; m < code.size(); m++) {
                    if (code[m] == 0x05) {
                        uint32_t target;
                        std::memcpy(&target, &code[m + 1], 4);

                        if (target == current_func_entry) {
                            uint32_t ret_point = m + 5;
                            out << "      case " << ret_point << ": goto L_" << ret_point << ";\n";
                        }
                    }

                    if (code[m] == 0x72) {
                        uint32_t target_id;
                        std::memcpy(&target_id, &code[m + 1], 4);
                        if (target_id == current_func_id) {
                            uint32_t ret_point = m + 5;
                            out << "      case " << ret_point << ": goto L_" << ret_point << ";\n";
                        }
                    }
                }

                out << "      default: return 0;\n";
                out << "    }\n";
                out << "  }\n";
                break;
            }
            case 0x07: out << "cells[--sp] = cells[" << idx(read32()) << "]; // PUSH\n"; break;
            case 0x08: out << "cells[" << idx(read32()) << "] = cells[sp++]; // POP\n"; break;
            case 0x09: out << "cells[sp-1] = cells[sp]; sp--; // DUP\n"; break;

            case 0x10: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int + cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x11: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int - cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x12: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int * cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x13: { uint32_t r = read32(), a = read32(), b = read32(); out << "if(cells[" << idx(b) << "].v_int) cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int / cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x14: { uint32_t r = read32(), a = read32(), b = read32(); out << "if(cells[" << idx(b) << "].v_int) cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int % cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x15: out << "cells[" << idx(read32()) << "].v_int++;\n"; break;
            case 0x16: out << "cells[" << idx(read32()) << "].v_int--;\n"; break;
            case 0x17: { uint32_t r = read32(), a = read32(); out << "cells[" << idx(r) << "].v_int = -cells[" << idx(a) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }

            case 0x20: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_float = cells[" << idx(a) << "].v_float + cells[" << idx(b) << "].v_float; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }
            case 0x21: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_float = cells[" << idx(a) << "].v_float - cells[" << idx(b) << "].v_float; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }
            case 0x22: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_float = cells[" << idx(a) << "].v_float * cells[" << idx(b) << "].v_float; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }
            case 0x23: { uint32_t r = read32(), a = read32(), b = read32(); out << "if(cells[" << idx(b) << "].v_float != 0) cells[" << idx(r) << "].v_float = cells[" << idx(a) << "].v_float / cells[" << idx(b) << "].v_float; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }
            case 0x24: { uint32_t r = read32(), a = read32(); out << "cells[" << idx(r) << "].v_float = -cells[" << idx(a) << "].v_float; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }

            case 0x30: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int & cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x31: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int | cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x32: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int ^ cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x33: { uint32_t r = read32(), a = read32(); out << "cells[" << idx(r) << "].v_int = ~cells[" << idx(a) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x34: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int << cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x35: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_int >> cells[" << idx(b) << "].v_int; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x36: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = (cells[" << idx(a) << "].v_int == cells[" << idx(b) << "].v_int); cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x37: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = (cells[" << idx(a) << "].v_int > cells[" << idx(b) << "].v_int); cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x38: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = (cells[" << idx(a) << "].v_int < cells[" << idx(b) << "].v_int); cells[" << idx(r) << "].type = T_INT;\n"; break; }

            case 0x40: { uint32_t r = read32(); out << "cells[" << idx(r) << "].v_int = " << read64() << "LL; cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x41: { uint32_t r = read32(); out << "cells[" << idx(r) << "].v_float = " << std::fixed << read_dbl() << "; cells[" << idx(r) << "].type = T_FLOAT;\n"; break; }
            case 0x42: {
                uint32_t r = read32(), len = read32();
                std::string s((char*)&code[pc], len); pc += len;
                out << "cells[" << idx(r) << "].v_str = \"" << s << "\"; cells[" << idx(r) << "].type = T_STR;\n"; break;
            }
            case 0x43: { uint32_t d = read32(), s = read32(); out << "cells[" << idx(d) << "] = cells[" << idx(s) << "]; // MOV\n"; break; }
            case 0x44: { uint32_t a = read32(), b = read32(); out << "{ Cell t = cells[" << idx(a) << "]; cells[" << idx(a) << "] = cells[" << idx(b) << "]; cells[" << idx(b) << "] = t; } // SWAP\n"; break; }
            case 0x45: { uint32_t r = read32(); out << "if(cells[" << idx(r) << "].type == T_STR) { try { cells[" << idx(r) << "].v_int = std::stoll(cells[" << idx(r) << "].v_str); cells[" << idx(r) << "].type = T_INT; } catch(...) { cells[" << idx(r) << "].v_int = 0; } }\n"; break; }
            case 0x46: { uint32_t r = read32(); out << "if(cells[" << idx(r) << "].type == T_INT) { cells[" << idx(r) << "].v_str = std::to_string(cells[" << idx(r) << "].v_int); cells[" << idx(r) << "].type = T_STR; }\n"; break; }
            case 0x47: { uint32_t r = read32(), s = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(s) << "].type; cells[" << idx(r) << "].type = T_INT; // TYPEOF\n"; break; }
            case 0x48: out << "cells[" << idx(read32()) << "].clear();\n"; break;

            case 0x50: { uint32_t r = read32(); out << "if(cells[" << idx(r) << "].type==T_INT) std::cout<<cells[" << idx(r) << "].v_int; else if(cells[" << idx(r) << "].type==T_STR) std::cout<<cells[" << idx(r) << "].v_str; else if(cells[" << idx(r) << "].type==T_FLOAT) std::cout<<cells[" << idx(r) << "].v_float; std::cout.flush();\n"; break; }
            case 0x51: out << "std::cout << std::endl;\n"; break;
            case 0x52: out << "std::cin >> cells[" << idx(read32()) << "].v_str; cells[" << idx(read32() - 0) << "].type = T_STR;\n"; break;
            case 0x53: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_str = cells[" << idx(a) << "].v_str + cells[" << idx(b) << "].v_str; cells[" << idx(r) << "].type = T_STR;\n"; break; }
            case 0x54: { uint32_t r = read32(), a = read32(); out << "cells[" << idx(r) << "].v_int = cells[" << idx(a) << "].v_str.length(); cells[" << idx(r) << "].type = T_INT;\n"; break; }
            case 0x55: { uint32_t r = read32(), a = read32(), b = read32(); out << "cells[" << idx(r) << "].v_int = (cells[" << idx(a) << "].v_str == cells[" << idx(b) << "].v_str); cells[" << idx(r) << "].type = T_INT;\n"; break; }

            case 0x60: { // system
                uint32_t v = read32();
                out << "if(cells[" << idx(v) << "].type == T_STR) system(cells[" << idx(v) << "].v_str.c_str());\n";
                break;
            }
            case 0x61: { // get_info
                uint32_t v = read32();
                out << "{\n"
                    << "    std::string info = \"\";\n"
                    << "    #ifdef _WIN32\n"
                    << "        info += \"Windows\";\n"
                    << "    #elif __ANDROID__\n"
                    << "        info += \"Android\";\n"
                    << "    #elif __APPLE__\n"
                    << "        info += \"macOS\";\n"
                    << "    #elif __linux__\n"
                    << "        info += \"Linux\";\n"
                    << "    #else\n"
                    << "        info += \"" << tos << "\";\n"
                    << "    #endif\n"
                    << "    info += \"|Loader: PIBR AOT|Brand: ParrotSoft\";\n"
                    << "    cells[" << idx(v) << "].v_str = info;\n"
                    << "    cells[" << idx(v) << "].type = T_STR;\n"
                    << "}\n";
                break;
            }
            case 0x62: { // arr_get
                uint32_t r = read32(), arr = read32(), i = read32();
                out << "if(cells[" << idx(arr) << "].type == T_ARR) cells[" << idx(r) << "] = (*cells[" << idx(arr) << "].v_arr)[cells[" << idx(i) << "].v_int];\n";
                break;
            }
            case 0x63: { // malloc
                uint32_t r = read32(), sz = read32();
                out << "cells[" << idx(r) << "].v_ptr = malloc(cells[" << idx(sz) << "].v_int);\n";
                out << "cells[" << idx(r) << "].type = T_PTR;\n";
                break;
            }
            case 0x64: { // free
                uint32_t r = read32();
                out << "if(cells[" << idx(r) << "].v_ptr) free(cells[" << idx(r) << "].v_ptr);\n";
                out << "cells[" << idx(r) << "].v_ptr = nullptr;\n";
                break;
            }

            case 0x70: {
                uint8_t len = code[pc++];
                std::string name((char*)&code[pc], len);
                pc += len;
                uint32_t cmd = read32();
                uint32_t sc = read32();
                out << "    vm.cells = cells;if(plugins.count(\"" << name << "\")) "
                    << "plugins[\"" << name << "\"]->execute(" << cmd << ", " << sc << ", &vm); "
                    << "else std::cerr << \"Plugin " << name << " not found\" << std::endl;\n";
                break;
            }
            case 0x71: { // DEF_FUNC
                uint32_t id = read32();
                uint32_t entry_addr = read32();
                current_func_id = id;
                current_func_entry = entry_addr;
                func_map[id] = entry_addr;
                uint32_t search_ptr = pc;
                while (search_ptr < code.size() && code[search_ptr] != 0x06) {
                    search_ptr++;
                }
                current_func_end = search_ptr;

                out << "func_table[" << id << "] = " << entry_addr << ";\n";
                out << "goto L_" << (current_func_end + 1) << "; // Пропускаем тело функции\nF_" << entry_addr << ":\n";

                break;
            }
            case 0x72: {
                uint32_t id = read32();
                out << "if(func_table.count(" << id << ")) {\n";
                out << "    uint32_t target = func_table[" << id << "];\n";
                out << "    cells[--sp].v_int = " << pc << "; cells[sp].type = T_INT;\n";
                out << "    switch(target) {\n";
                for (auto const& [f_id, f_addr] : func_map) {
                    out << "        case " << f_addr << ": goto F_" << f_addr << ";\n";
                }
                out << "        default: std::cerr << \"Runtime Error: Unknown function address \" << target << std::endl; return 1;\n";
                out << "    }\n";
                out << "}\n";
                break;
            }
            default: out << "// Unknown OP " << (int)op << "\n"; break;
            }
        }
        out << "    return 0;\n}\n";

        return out.str();
    }
};