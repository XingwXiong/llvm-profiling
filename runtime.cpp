#include <iostream>
#include <cstdlib>
#include <cmath>
#include <unordered_map>

#include "llvm/IR/Instruction.h"

#include "utils.h"
#include "runtime.h"
#include "lru_cache.hpp"

#include "unordered_map.hpp"
#include "lru_cache_custom.hpp"

// static std::unordered_map<uint32_t, uint64_t> instr_map;

// void updateInstrInfo(unsigned num, uint32_t *keys, uint32_t *values) {
//     for (unsigned i = 0; i < num; i++) {
//         uint32_t key = keys[i];
//         uint32_t value = values[i];
//         if (instr_map.find(key) == instr_map.end())
//             instr_map.insert(std::make_pair(key, value));
//         else
//             instr_map[key] = instr_map[key] + value;
//     }
//     return;
// }

// void printOutInstrInfo() {
//     // REF: https://github.com/llvm-mirror/llvm/blob/master/lib/IR/llvm::Instruction.cpp#L290-L371
//     std::unordered_map<std::string, uint64_t> instype_map;
//     std::cout << "====> Dynamic Instuction Count <====\n";
//     for (auto &kv : instr_map) {
//         std::cout << llvm::Instruction::getOpcodeName(kv.first) << "\t" << kv.second << "\n";
//         instype_map[getOpcodeType(kv.first)] += kv.second;
//     }
//     std::cout << "\n====> Dynamic Instuction Types Count <====\n";
//     for (auto &kv : instype_map) {
//         std::cout << kv.first << "\t" << kv.second << "\n";
//     }
//     return;
// }

static const int instr_map_size = 1000;
static uint64_t instr_map[instr_map_size];

void initInstrInfo() {
    static bool is_initialed = false;
    if (!is_initialed) {
        is_initialed = true;
        memset(instr_map, 0, sizeof(instr_map));
        std::cout << "[INFO: instr_map is initialized]" << std::endl;
    }
}

void updateInstrInfo(unsigned num, uint32_t *keys, uint32_t *values) {
    for (unsigned i = 0; i < num; i++) {
        uint32_t key = keys[i];
        uint32_t value = values[i];
        assert(key < instr_map_size);
        instr_map[key] += value;
    }
    return;
}

void printOutInstrInfo() {
    // REF: https://github.com/llvm-mirror/llvm/blob/master/lib/IR/llvm::Instruction.cpp#L290-L371
    std::unordered_map<std::string, uint64_t> instype_map;
    std::cout << "====> Dynamic Instuction Count <====\n";
    for(int i = 0; i < instr_map_size; ++i) {
        if(instr_map[i] == 0) continue;
        std::cout << llvm::Instruction::getOpcodeName(i) << "\t" << instr_map[i] << "\n";
        instype_map[getOpcodeType(i)] += instr_map[i];
    }
    std::cout << "\n====> Dynamic Instuction Types Count <====\n";
    for (auto &kv : instype_map) {
        std::cout << kv.first << "\t" << kv.second << "\n";
    }
    return;
}

void printOutCalledFunction(const char* func_name) {
    std::cout << "[INFO: called function: " << func_name << "]\n";
}

// If taken is true, then a conditional branch is taken;
// If taken is false, then a conditional branch is not taken.
// branch_count[0]: branch taken number
// branch_count[1]: total branches number
uint64_t branch_count[2] = {0, 0};
void updateBranchInfo(bool taken) {
    if (taken)
        branch_count[0]++;
    branch_count[1]++;
    return;
}

void printOutBranchInfo() {
    std::cout << "====> Profiling Branch Bias <====\n";
    std::cout << "taken\t" << branch_count[0] << '\n';
    std::cout << "total\t" << branch_count[1] << '\n';

    branch_count[0] = 0;
    branch_count[1] = 0;
    return;
}

uint64_t memory_alloc_bytes = 0, memory_alloc_times = 0;
void updateMemoryAllocInfo(uint32_t byte_sizes) {
    memory_alloc_bytes += byte_sizes;
    memory_alloc_times += 1;
}

void printOutMemoryAllocInfo() {
    std::cout << "====> Profiling Memory Allocation <====\n";
    std::cout << "memory_alloc_bytes\t" << memory_alloc_bytes << '\n';
    std::cout << "memory_alloc_times\t" << memory_alloc_times << '\n';
    return;
}

const int INST_DIST_STEP = 21;
const int LRU_CACHE_MAX_SIZE = 1 << INST_DIST_STEP;
lru::Cache<std::intptr_t, uint64_t> lru_cache(LRU_CACHE_MAX_SIZE, LRU_CACHE_MAX_SIZE);
// lru_custom::Cache<
//     std::intptr_t, uint64_t,
//     xxw::unordered_map<std::intptr_t, lru_custom::Node<std::intptr_t, uint64_t> *>>
//     lru_cache(LRU_CACHE_MAX_SIZE, LRU_CACHE_MAX_SIZE);
uint64_t cur_inst_pc = 0; // instructon program counter
/**
 * inst_dist_count[i] means number of inst_dist is [2^i, 2^(i+1)), e.g. 1, 2, 4, 8, 16, ...
 * specially, inst_dist_count[INST_DIST_STEP] means reuse distance is greater than 2^INST_DIST_STEP
*/
uint64_t inst_dist_count[INST_DIST_STEP + 1];

void initLRUInstCache() {
    lru_cache.clear();
    cur_inst_pc = 0;
    memset(inst_dist_count, 0, sizeof(inst_dist_count));
    std::cout << "[INFO: lru cache initialized" << "]\n";
}

void insertLRUInstCache(std::intptr_t ins_id) {
    ++ cur_inst_pc;
    if (ins_id == 0) {
        std::cout << "[WARNING: ins_id == 0 in insertLRUInstCache]\n";
        return;
    }
    if (lru_cache.contains(ins_id)) {
        uint64_t prev_inst_pc = lru_cache.get(ins_id, false);
        uint64_t reuse_dist = cur_inst_pc - prev_inst_pc;
        int log2_reuse_dist = std::min(int(log2(reuse_dist)), INST_DIST_STEP);        
        ++ inst_dist_count[log2_reuse_dist];
    } else {
        ++ inst_dist_count[INST_DIST_STEP];
    }
    lru_cache.insert(ins_id, cur_inst_pc);
}

void printInstrReuseDist() {
    std::cout << "====> Instruction Reuse Distance <====\n";
    for (int i = 0; i < INST_DIST_STEP; ++i) {
        printf("[%8d, %8d): %lu\n", 1 << i, 1 << (i + 1), inst_dist_count[i]);
    }
    printf("[%8d, %8s): %lu\n", 1 << INST_DIST_STEP, "inf", inst_dist_count[INST_DIST_STEP]);
}
