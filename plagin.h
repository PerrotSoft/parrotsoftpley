#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#else
typedef void* HMODULE;
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
        if (type == T_ARR && v_arr && is_dyn) delete v_arr;
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
};

class PVM {
public:
    std::vector<uint8_t> code;
    std::vector<Cell> cells;
    uint32_t pc = 0;
    uint32_t sp;
    bool running = true;
};