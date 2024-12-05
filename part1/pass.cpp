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
    void printSourceLocation(Instruction *instr) {
        if (DILocation *loc = instr->getDebugLoc()) {
            errs() << "    [Source file: " << loc->getFilename() 
                   << ", Line: " << loc->getLine() 
                   << ", Column: " << loc->getColumn() << "]\n";
        } else if (auto *DbgDeclare = dyn_cast<DbgDeclareInst>(instr)) {
            if (DILocalVariable *Var = DbgDeclare->getVariable()) {
            errs() << "    Variable Name: " << Var->getName() << "\n";
            }
        }else {
            errs() << "    [No debug info available]\n";
        }
    }

    // Helper function to find the variable name associated with an instruction
    void printValueName(Value *V) {

        errs() << "\thas name: " << V->hasName() << "\n";
        errs() << "\tvalue name: " << V->getName() << "\n";
        errs() << "\ttype: " << V->getType() << "\n";

        if (isa<Instruction>(V)) {
            errs() << "\tValue is an instruction: " << *V << "\n";    
        } else if (isa<Constant>(V)) {
            errs() << "\tValue is a constant: " << *V << "\n";
        } else if (isa<Argument>(V)) {
            errs() << "\tValue is a function argument: " << *V << "\n";
        } else if (isa<GlobalVariable>(V)) {
            errs() << "\tValue is a global variable: " << *V << "\n";
        } else {
            errs() << "\tUnknown Value type: " << *V << "\n";
            return;
        }

        if (DbgDeclareInst *DbgDeclare = dyn_cast<DbgDeclareInst>(V)) {
            if (DILocalVariable *Var = DbgDeclare->getVariable()) {
                errs() << "\tVariable name (from debug metadata): " << Var->getName() << "\n";
                return;
            }
        } else if (DbgValueInst *DbgValue = dyn_cast<DbgValueInst>(V)) {
            if (DILocalVariable *Var = DbgValue->getVariable()) {
                errs() << "\tVariable name (from debug metadata): " << Var->getName() << "\n";
                return;
            }
        } else {
            errs() << "\tNo debug information or name found for this value.\n";
        }
    }

    void traceVariableOrigin(Value *V) {

        printValueName(V);

        // If it's an argument, print and return
        if (isa<Argument>(V)) {
            errs() << "\tVariable originates as a function argument: " << *V << "\n";
            return;
        }

        // If it's an alloca instruction, it's a local variable
        if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
            errs() << "\tVariable originates from an alloca: " << *AI << "\n";
            return;
        }

        // If it's a global variable
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
            errs() << "\tVariable originates from a global variable: " << *GV << "\n";
            return;
        }

        // If it's a store instruction
        if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
            errs() << "\tVariable originates from a store: " << *SI << "\n";
            return;
        }

        // If it's defined by an instruction, trace back its operands
        if (Instruction *Inst = dyn_cast<Instruction>(V)) {
            errs() << "\tTracing variable defined by instruction: " << *Inst << "\n";
            

            for (Use &U : Inst->operands()) {
                Value *Operand = U.get();
                errs() << "\t  Operand: " << *Operand << "\n";
                traceVariableOrigin(Operand); // Recursively trace the operand
            }
        }
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        
        for (auto &F : M) {
            errs() << "I see a function called " << F.getName() << "\n";

            for (auto &BB : F) {
              errs() << "I see a basic block " << BB.getName() << "\n";
              for (auto &I : BB) {

                errs() << "analyzing uses of: " << I << "\n";

                

                if (auto *DII = dyn_cast<DbgDeclareInst>(&I)) {
                    auto *Var = DII->getVariable();
                    errs() << "Variable: " << Var->getName() << "\n";
                } else if (auto *DVI = dyn_cast<DbgValueInst>(&I)) {
                    auto *Var = DVI->getVariable();
                    errs() << "Variable: " << Var->getName() << "\n";
                }
                
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
