
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

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
        for (auto *user : V->users()) {
            if (auto *instr = llvm::dyn_cast<llvm::Instruction>(user)) {
                if (llvm::DebugLoc debugLoc = instr->getDebugLoc()) {
                    unsigned line = debugLoc.getLine();
                    unsigned col = debugLoc.getCol();
                    llvm::StringRef file = debugLoc->getScope()->getFilename();
                    llvm::StringRef dir = debugLoc->getScope()->getDirectory();
                    llvm::errs() << "\tLocation: " << dir << "/" << file << ":" << line << ":" << col << "\n";
                }
            }
        }
    }

    // Helper function to find the variable name associated with an instruction
    void printValueName(Value *V) {

        errs() << "\thas name: " << V->hasName() << ", value name: " << V->getName() << "\n";

    }

    std::vector<Instruction*> checked_seminal_inputs;

    void checkSeminalInput(Value *V) {

        errs() << "\t\tcheckSeminalInput: " << V->getName() << "\n";

        if (auto *callInst = dyn_cast<CallInst>(V)) {
            if (Function *calledFunc = callInst->getCalledFunction()) {
                errs() << "\t\t  called function: " << calledFunc->getName() << "\n";
                if (calledFunc->getName().contains("scanf")) {
                    errs() << "\t\t  Variable originates from scanf: " << *callInst << " --\n";
                }

                if (calledFunc->getName().contains("getc")) {
                    llvm::errs() << "\t\t  Found a call to getc\n";

                    Value *arg = callInst->getArgOperand(0);    // Get the first argument
                    errs() << "\t\t  Argument passed to getc: " << *arg << "\n";
                    
                }

            }
        } else if (auto *allocaInst = dyn_cast<AllocaInst>(V)) {
            errs() << "\t\t  Variable allocated: " << *allocaInst << "\n";
        } else {
            errs() << "\t\t  Unhandled input source: " << *V << "\n";
        }
    }

    void printDefUseChains(llvm::Value *Val) {
        errs() << "\tprintDefUseChains()\n";
        for (auto *User : Val->users()) {
            llvm::errs() << "\t  Value is used in: " << *User << "\n";
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

    void checkBeforeTrace(Instruction *Inst) {
        if (isInstructionInVector(traced_instructions, Inst) && isInstructionInVector(checked_seminal_inputs, Inst) ) {
            return;
        } else if (!isInstructionInVector(traced_instructions, Inst)) {
            traced_instructions.push_back(Inst);
        } else if (!isInstructionInVector(checked_seminal_inputs, Inst)) {
            checked_seminal_inputs.push_back(Inst);
        }

        for (Use &U : Inst->operands()) {
            Value *Operand = U.get();
            errs() << "\t  Operand: " << *Operand << "\n";
            checkSeminalInput(Operand);
            traceVariableOrigin(Operand); // Recursively trace the operand
        }
    }

    void traceVariableOrigin(Value *V) {        

        // If it's an argument, print and return
        if (isa<Argument>(V)) {
            errs() << "\tVariable originates as a function argument: " << *V << "\n";
            printValueName(V);
            printDefUseChains(V);
            return;
        }

        // If it's an alloca instruction, it's a local variable
        if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
            errs() << "\tVariable originates from an alloca: " << *AI << "\n";
            printValueName(V);
            printDefUseChains(V);
            return;
        }

        // If it's a global variable
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
            errs() << "\tVariable originates from a global variable: " << *GV << "\n";
            printValueName(V);
            printDefUseChains(V);
            return;
        }

        // If it's a store instruction
        if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
            errs() << "\tVariable defined by store instruction: " << *SI << "\n";
            printValueName(V);
            printDefUseChains(V);
            return;
        }

        // If it's defined by an instruction, trace back its operands
        if (Instruction *Inst = dyn_cast<Instruction>(V)) {
            errs() << "\tTracing variable defined by instruction: " << *Inst << "\n";
            checkBeforeTrace(Inst);
        }
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

        for (auto &F : M) {
            errs() << "I see a function called " << F.getName() << "\n";

            for (auto &BB : F) {
              errs() << "I see a basic block " << BB.getName() << "\n";
              for (auto &I : BB) {

                errs() << "analyzing uses of: " << I << "\n";
                
                if (BranchInst *br = dyn_cast<BranchInst>(&I)) {

                    if (br->isConditional()) {
                        Value *condition = br->getCondition();
                        errs() << "  branch instruction condition: " << condition << "\n";

                        traceVariableOrigin(condition);                        
                    }
                    else {
                        errs() << "branch instruction: " << br->getSuccessor(0) << "\n";
                    }
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
