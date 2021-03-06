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
#include "llvm/IR/CallSite.h"
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

#define DEBUG_TYPE "count-dynamic-insts"

namespace {
struct CountDynamicInstructions : public ModulePass {
    static char ID;
    llvm::DenseMap<uint32_t, uint64_t> static_count;

    CountDynamicInstructions() : ModulePass(ID) {}

    void print_static_count() {
        std::unordered_map<std::string, uint64_t> instype_map;
        llvm::outs() << "====> Static Instuction Count <====\n";
        for (auto &kv : static_count) {
            llvm::outs() << llvm::Instruction::getOpcodeName(kv.first) << "\t" << kv.second << "\n";
            instype_map[getOpcodeType(kv.first)] += kv.second;
        }
        llvm::outs() << "\n====> Static Instuction Types Count <====\n";
        for (auto &kv : instype_map) {
            llvm::outs() << kv.first << "\t" << kv.second << "\n";
        }
    }
    bool runOnModule(Module &M) override {
        for (auto F = M.begin(); F != M.end(); ++F) {
            runOnFunction(*F);
        }
        print_static_count();

        FunctionCallee irt = M.getOrInsertFunction("initInstrInfo", Type::getVoidTy(M.getContext()));
        
        bool prt_instrumented = false;
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
                    FunctionCallee prt = M.getOrInsertFunction("printOutInstrInfo", Type::getVoidTy(M.getContext()));
                    IRBuilder<> Builder(&*I);
                    Builder.CreateCall(prt);
                    prt_instrumented = true;
                    llvm::outs() << "[printOutInstrInfo instrumented] " 
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
        if(!prt_instrumented) llvm::outs() << "[printOutInstrInfo is not instrumented]" << "\n";
        return false;
    }

    bool runOnFunction(Function &F) {
        // void updateInstrInfo(unsigned num, uint32_t * keys, uint32_t * values);
        FunctionCallee udt = F.getParent()->getOrInsertFunction(
            "updateInstrInfo", Type::getVoidTy(F.getParent()->getContext()),
            Type::getInt32Ty(F.getParent()->getContext()), Type::getInt32PtrTy(F.getParent()->getContext()),
            Type::getInt32PtrTy(F.getParent()->getContext()));

        for (Function::iterator B = F.begin(); B != F.end(); ++B) {
            // key: instruction opcode
            // value: counts of this inst in current basic block
            std::map<int, int> cdi;
            for (BasicBlock::iterator I = B->begin(); I != B->end(); ++I) {
                ++cdi[I->getOpcode()];
                
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
                            "printOutInstrInfo", Type::getVoidTy(F.getParent()->getContext()));
                        IRBuilder<> Builder(&*I);
                        Builder.CreateCall(prt);
                        llvm::outs() << "[printOutInstrInfo instrumented] " 
                                    << "Function: "    << F.getName() << ", "
                                    << "Instruction: " << *I << "\n";
                    }
                }
            }
            int count = cdi.size();
            std::vector<Constant *> k;
            std::vector<Constant *> v;
            for (auto &pair : cdi) {
                static_count[pair.first] += pair.second;
                k.push_back(ConstantInt::get(Type::getInt32Ty(F.getParent()->getContext()), pair.first));
                v.push_back(ConstantInt::get(Type::getInt32Ty(F.getParent()->getContext()), pair.second));
            }

            GlobalVariable *KG = new GlobalVariable(
                *(F.getParent()), ArrayType::get(Type::getInt32Ty(F.getParent()->getContext()), count), true,
                GlobalValue::InternalLinkage,
                ConstantArray::get(ArrayType::get(Type::getInt32Ty(F.getParent()->getContext()), count), k),
                "keys global");

            GlobalVariable *VG = new GlobalVariable(
                *(F.getParent()), ArrayType::get(Type::getInt32Ty(F.getParent()->getContext()), count), true,
                GlobalValue::InternalLinkage,
                ConstantArray::get(ArrayType::get(Type::getInt32Ty(F.getParent()->getContext()), count), v),
                "values global");

            // for instrument function
            // void updateInstrInfo(unsigned num, uint32_t * keys, uint32_t * values);
            std::vector<Value *> args;
            args.push_back(ConstantInt::get(Type::getInt32Ty(F.getParent()->getContext()), count));

            IRBuilder<> Builder(&*B);
            Builder.SetInsertPoint(B->getTerminator());

            args.push_back(Builder.CreatePointerCast(KG, Type::getInt32PtrTy(F.getParent()->getContext())));
            args.push_back(Builder.CreatePointerCast(VG, Type::getInt32PtrTy(F.getParent()->getContext())));

            // instrument updateInstrInfo
            Builder.CreateCall(udt, args);

        } // end for each basic block

        return false;
    }
}; // end of struct CountDynamicInstructions
} // end of anonymous namespace

char CountDynamicInstructions::ID = 0;
static RegisterPass<CountDynamicInstructions> X(DEBUG_TYPE, "Collecting Dynamic Instruction Counts");

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
    cl::ParseCommandLineOptions(argc, argv, "Dynamic instructions profiling analysis...\n");

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

    Passes.add(new CountDynamicInstructions());
    Passes.run(*M.get());

    // Write back the instrumentation info into LLVM IR
    std::error_code EC;
    std::unique_ptr<ToolOutputFile> Out(new ToolOutputFile(OutputputFilename, EC, sys::fs::F_None));
    WriteBitcodeToFile(*M.get(), Out->os());
    Out->keep();

    return 0;
}
