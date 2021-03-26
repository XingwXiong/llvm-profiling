#include "utils.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <iostream>
using namespace llvm;

#define DEBUG_TYPE "inst-reuse-dist"

namespace {
struct InstReuseDist : public ModulePass {
    static char ID;
    llvm::DenseMap<uint32_t, uint64_t> static_count;

    InstReuseDist() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        for (auto F = M.begin(); F != M.end(); ++F) {
            runOnFunction(*F);
        }

        FunctionCallee irt = M.getOrInsertFunction("initLRUInstCache", Type::getVoidTy(M.getContext()));
        
        Function *mainFunc = M.getFunction("main");
        if (!mainFunc) mainFunc = M.getFunction("MAIN_");
        if (mainFunc) {
            IRBuilder<> Builder(&*(mainFunc->getEntryBlock().begin()));
            Builder.CreateCall(irt);
            for (auto B = mainFunc->begin(); B != mainFunc->end(); B++) {
                for (auto I = B->begin(); I != B->end(); I++) {
                    // insert at the end of main function
                    bool is_exit = false;
                    if (isa<ReturnInst>(I)) {
                        is_exit = true;
                    }
                    if (!is_exit) continue;
                    FunctionCallee prt = M.getOrInsertFunction("printInstrReuseDist", Type::getVoidTy(M.getContext()));
                    IRBuilder<> Builder(&*I);
                    Builder.CreateCall(prt);
                    llvm::outs() << "[printInstrReuseDist instrumented] " 
                                 << "Function: "    << mainFunc->getName() << ", "
                                 << "Instruction: " << *I << "\n";
                }
            }
        } else {
            llvm::outs() << "[main function is not found]" << "\n";
            for (auto F = M.begin(); F != M.end(); ++F) {
                if (F->isIntrinsic()) continue;
                llvm::outs() << F->getName() << "\n";
            }
        }
        return false;
    }

    bool runOnFunction(Function &F) {
        // skip instrumentations before global variables get initialized
        std::string func_name = F.getName();
        if (func_name == "__cxx_global_var_init" || func_name.find("_GLOBAL__sub_I_") != func_name.npos) {
            llvm::outs() << "skip function " << F.getName() << "\n";
            return false;
        }
        // void insertLRUInstCache(intptr_t ins_id);
        FunctionCallee ist = F.getParent()->getOrInsertFunction(
            "insertLRUInstCache", Type::getVoidTy(F.getParent()->getContext()),            
            Type::getInt32Ty(F.getParent()->getContext()));

        for (Function::iterator B = F.begin(); B != F.end(); ++B) {
            for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
                if (isa<CallInst>(I)) {
                    // to avoid call bitcast
                    CallInst* CI = dyn_cast<CallInst>(&*I);
                    if (const Function *call_func = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts()))
                    {
                        if (call_func->isIntrinsic() || call_func->getName() == "updateInstrInfo" ||
                            call_func->getName() == "initInstrInfo" || call_func->getName() == "printOutInstrInfo")
                            continue;
                        // llvm::outs() << *I << "\n";
                        std::string caller = call_func->getName();
                        if (caller != "exit" && caller != "f90_stop08a" &&
                            caller.find("quit_flag_") == std::string::npos) {
                            continue;
                        }
                        FunctionCallee prt = F.getParent()->getOrInsertFunction(
                            "printInstrReuseDist", Type::getVoidTy(F.getParent()->getContext()));
                        IRBuilder<> builder_(&*I);
                        builder_.CreateCall(prt);
                        llvm::outs() << "[printInstrReuseDist instrumented] " 
                                    << "Function: "    << F.getName() << ", "
                                    << "Instruction: " << *I << "\n";
                    }
                }
                IRBuilder<> Builder(&*I);
                std::vector<Value *> args = {
                    ConstantInt::get(Type::getInt32Ty(F.getParent()->getContext()),
                                     reinterpret_cast<std::intptr_t>(dyn_cast<Instruction>(&*I)))
                };
                Builder.CreateCall(ist, args);
            }
        } // end for each basic block

        return false;
    }
}; // end of struct InstReuseDist
} // end of anonymous namespace

char InstReuseDist::ID = 0;
static RegisterPass<InstReuseDist> X(DEBUG_TYPE, "Instuction Reuse Distance profiling analysis");

static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<filename>.ll"), cl::init(""));
static cl::opt<std::string> OutputputFilename(cl::Positional,
                                              cl::desc("<filename>-instrumented.bc"), cl::init(""));

#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif

int main(int argc, const char *argv[]) {
    LLVMContext &Context = getGlobalContext();
    // static LLVMContext Context;
    SMDiagnostic Err;
    // Parse the command line to read the Inputfilename
    cl::ParseCommandLineOptions(argc, argv, "Instuction Reuse Distance profiling analysis...\n");

    // load the input module
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return -1;
    }

    llvm::legacy::PassManager Passes;

    /// Transform it to SSA
    // Passes.add(llvm::createPromoteMemoryToRegisterPass());
    // Passes.add(new LoopInfoWrapperPass());

    Passes.add(new InstReuseDist());
    Passes.run(*M.get());

    // Write back the instrumentation info into LLVM IR
    std::error_code EC;
    std::unique_ptr<ToolOutputFile> Out(new ToolOutputFile(OutputputFilename, EC, sys::fs::F_None));
    WriteBitcodeToFile(*M.get(), Out->os());
    Out->keep();

    return 0;
}
