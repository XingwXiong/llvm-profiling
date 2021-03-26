#pragma once

#include <cstdint>

extern "C" {
// branch
void initBranchInfo();
void updateBranchInfo(bool taken);
void printOutBranchInfo();

// instruction count
void initInstrInfo();
void updateInstrInfo(unsigned num, uint32_t *keys, uint32_t *values);
void printOutInstrInfo();

// memory alloca
void updateMemoryAllocInfo(uint32_t byte_size);
void printOutMemoryAllocInfo();

// for debug
void printOutCalledFunction(const char* func_name);

// instrution reuse distance
void initLRUInstCache();
void insertLRUInstCache(std::intptr_t ins_id);
void printInstrReuseDist();

}
