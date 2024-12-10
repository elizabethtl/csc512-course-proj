#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>
#include <unordered_set>

#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/raw_ostream.h"


using namespace llvm;

namespace {

struct SkeletonPass : public PassInfoMixin<SkeletonPass> {


    // Helper function to find the source location of an instruction
    void printInstrDebugLocation(Instruction *I){
        // Retrieve and print source location (if available)
        if (DebugLoc debugLoc = I->getDebugLoc()) {
            unsigned line = debugLoc.getLine();
            unsigned col = debugLoc.getCol();
            auto *scope = debugLoc->getScope();
            StringRef file = scope->getFilename();
            StringRef dir = scope->getDirectory();

            errs() << "\tSource Location: " << dir << "/" << file << ":" << line << ":" << col << "\n";
        } else {
            errs() << "\tNo debug location available for this instruction.\n";
        }
    }

    void printValueSourceLocation(Value *V) {
        User* lastUser = nullptr;
        for (auto *user : V->users()) {
            lastUser = user;
        }

        if (auto *instr = llvm::dyn_cast<llvm::Instruction>(lastUser)) {
            if (llvm::DebugLoc debugLoc = instr->getDebugLoc()) {
                unsigned line = debugLoc.getLine();
                unsigned col = debugLoc.getCol();
                llvm::StringRef file = debugLoc->getScope()->getFilename();
                llvm::StringRef dir = debugLoc->getScope()->getDirectory();
                llvm::errs() << "\tLocation: " << dir << "/" << file << ":" << line << ":" << col << "\n";
            }
        }
    }

    // Helper function to find the variable name associated with an instruction
    void printValueName(Value *V) {
        errs() << "\tValue has name: " << V->hasName() << ", value name: " << V->getName() << "\n";
    }

    std::vector<Instruction*> checked_seminal_inputs;

    void checkSeminalInput(Value *V) {

        // errs() << "\t\tcheckSeminalInput: " << V->getName() << "\n";

        if (auto *callInst = dyn_cast<CallInst>(V)) {
            if (Function *calledFunc = callInst->getCalledFunction()) {
                errs() << "  called function: " << calledFunc->getName() << "\n";
                if (calledFunc->getName().contains("scanf")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value originates from scanf: " << *callInst << " --\n";
                }

                if (calledFunc->getName().contains("getc")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value from a call to getc\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t  Argument passed to getc: " << *arg << "\n";
                }

                if (calledFunc->getName().contains("fopen")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value from a call to fopen\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t  Argument passed to fopen: " << *arg << "\n";
                }
                if (calledFunc->getName().contains("fwrite")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value from a call to fwrite\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t  Argument passed to fwrite: " << *arg << "\n";
                }
                if (calledFunc->getName().contains("fclose")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value from a call to fclose\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t  Argument passed to fclose: " << *arg << "\n";
                }
                if (calledFunc->getName().contains("fread")) {
                    errs() << "\t  --- SEMINAL INPUT ---\n";
                    errs() << "\t   Value from a call to fread\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t  Argument passed to fread: " << *arg << "\n";
                }

            }
        } 
    }

    void findDefUseChains(llvm::Value *Val) {
        errs() << "\tfindDefUseChains()\n";
        for (auto *User : Val->users()) {
            // errs() << "\t   DefUseChain value is used in: " << *User << "\n";

            if (auto *instr = llvm::dyn_cast<llvm::Instruction>(User)) {
                checkBeforeTrace(instr);
            }   
        }
    }

    bool isInstructionInVector(const std::vector<Instruction*>& instructions, Instruction* inst) {
        // Use std::find to check if the instruction is already in the vector
        return std::find(instructions.begin(), instructions.end(), inst) != instructions.end();
    }

    std::vector<Instruction*> traced_instructions;
    std::unordered_set<Value*> seenOperands;

    void checkBeforeTrace(Instruction *Inst) {
        if (isInstructionInVector(traced_instructions, Inst) && isInstructionInVector(checked_seminal_inputs, Inst) ) {
            return;
        } else if (!isInstructionInVector(traced_instructions, Inst)) {
            traced_instructions.push_back(Inst);
        } else if (!isInstructionInVector(checked_seminal_inputs, Inst)) {
            checked_seminal_inputs.push_back(Inst);
        }

        checkSeminalInput(Inst);

        for (Use &U : Inst->operands()) {
            Value *Operand = U.get();

            if (seenOperands.find(Operand) != seenOperands.end()) {
                continue; // Skip duplicate operand
            }
            seenOperands.insert(Operand);

            // errs() << "\t   Operand: " << *Operand << "\n";
            checkSeminalInput(Operand);
            traceVariableOrigin(Operand); // Recursively trace the operand
        }
    }

    void traceVariableOrigin(Value *V) {        

        // If it's an argument, print and return
        if (isa<Argument>(V)) {
            errs() << "\tVariable originates as a function argument: " << *V << "\n";
            printValueName(V);
            printValueSourceLocation(V);
            findDefUseChains(V);
            return;
        }

        // If it's an alloca instruction, it's a local variable
        if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
            errs() << "\tVariable originates from an alloca: " << *AI << "\n";
            printValueName(V);
            printValueSourceLocation(V);
            findDefUseChains(V);
            return;
        }

        // If it's a global variable
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
            errs() << "\tVariable originates from a global variable: " << *GV << "\n";
            printValueName(V);
            printValueSourceLocation(V);
            findDefUseChains(V);
            return;
        }

        // If it's a store instruction
        if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
            errs() << "\tVariable defined by store instruction: " << *SI << "\n";
            printValueName(V);
            printValueSourceLocation(V);
            findDefUseChains(V);
            return;
        }

        // If it's defined by an instruction, trace back its operands
        if (Instruction *Inst = dyn_cast<Instruction>(V)) {
            if (isInstructionInVector(traced_instructions, Inst)) {
                return;
            }
            errs() << "Tracing variable defined by instruction: " << *Inst << "\n";
            checkBeforeTrace(Inst);
        }
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

        for (auto &F : M) {
            // errs() << "I see a function called " << F.getName() << "\n";

            for (auto &BB : F) {
            //   errs() << "I see a basic block " << BB.getName() << "\n";
              for (auto &I : BB) {

                // errs() << "analyzing uses of: " << I << "\n";
                
                if (BranchInst *br = dyn_cast<BranchInst>(&I)) {

                    if (br->isConditional()) {
                        Value *condition = br->getCondition();
                        // errs() << "branch instruction condition: " << condition << "\n";
                        // checkBeforeTrace(condition);
                        traceVariableOrigin(condition);                        
                    }
                    // else {
                    //     errs() << "branch instruction: " << br->getSuccessor(0) << "\n";
                    // }
                }
              }
            }
        }
        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Skeleton pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    MPM.addPass(SkeletonPass());
                });
        }
    };
}