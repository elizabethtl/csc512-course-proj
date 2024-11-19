#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
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
        } else {
            errs() << "    [No debug info available]\n";
        }
    }

    // Helper function to find the variable name associated with an instruction
    void printVariableName(Value *value) {
        if (auto *instr = dyn_cast<Instruction>(value)) {
            // Check if there's metadata related to the value
            for (auto *user : instr->users()) {
                if (auto *dbgDeclare = dyn_cast<DbgDeclareInst>(user)) {
                    if (auto *var = dbgDeclare->getVariable()) {
                        errs() << "    Variable: " << var->getName() << "\n";
                        return;
                    }
                }
                if (auto *dbgValue = dyn_cast<DbgValueInst>(user)) {
                    if (auto *var = dbgValue->getVariable()) {
                        errs() << "    Variable: " << var->getName() << "\n";
                        return;
                    }
                }
            }
        }
    }

    // Helper function to trace back to the definition of a condition variable
    void findConditionDefinition(Value *condition) {
        if (Instruction *instr = dyn_cast<Instruction>(condition)) {
            errs() << "  Condition defined at: " << *instr << "\n";
            printSourceLocation(instr);
            printVariableName(condition);

            // Traverse backwards to find the original definition
            while (instr) {
                if (instr->isBinaryOp() || isa<ICmpInst>(instr) || isa<LoadInst>(instr)) {
                    errs() << "    Found definition: " << *instr << "\n";
                    printSourceLocation(instr);
                    printVariableName(instr);
                    return;
                }

                // Handle PHI nodes
                if (auto *phiNode = dyn_cast<PHINode>(instr)) {
                    errs() << "    PHI node detected. Values:\n";
                    for (unsigned i = 0; i < phiNode->getNumIncomingValues(); ++i) {
                        errs() << "      Incoming from block "
                               << phiNode->getIncomingBlock(i)->getName() << ": ";
                        phiNode->getIncomingValue(i)->print(errs());
                        errs() << "\n";
                        printVariableName(phiNode->getIncomingValue(i));
                    }
                    return;
                }

                instr = dyn_cast<Instruction>(instr->getOperand(0));
            }
        } else {
            errs() << "  Condition is not derived from an instruction.\n";
        }
    }

    // Function to trace the declaration of a variable used in a condition
    void traceVariableDeclaration(Value *value) {
        if (Argument *arg = dyn_cast<Argument>(value)) {
            // The value is a function argument
            errs() << "  Variable is a function argument: " << arg->getName() << "\n";
            // if (DILocation *loc = arg->getParent()->getSubprogram()->getUnit()->getFilename()) {
            //     errs() << "  [Declared in function: " << arg->getParent()->getName() << "]\n";
            // }
            return;
        }

        if (Instruction *instr = dyn_cast<Instruction>(value)) {
            errs() << "  Tracing back from: " << *instr << "\n";
            printSourceLocation(instr);

            // Walk backwards to find where the variable is defined
            while (instr) {
                if (auto *allocaInst = dyn_cast<AllocaInst>(instr)) {
                    errs() << "    Variable declared with alloca: ";
                    allocaInst->print(errs());
                    errs() << "\n";
                    printSourceLocation(allocaInst);
                    return;
                }

                if (auto *storeInst = dyn_cast<StoreInst>(instr)) {
                    Value *storedVal = storeInst->getValueOperand();
                    Value *ptr = storeInst->getPointerOperand();
                    errs() << "    Store found: Storing ";
                    storedVal->print(errs());
                    errs() << " into ";
                    ptr->print(errs());
                    errs() << "\n";
                    printSourceLocation(storeInst);
                    return;
                }

                if (instr->getNumOperands() > 0) {
                    instr = dyn_cast<Instruction>(instr->getOperand(0));
                } else {
                    break;
                }
            }
        }
    }

    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        
        for (auto &F : M) {
            errs() << "I saw a function called " << F.getName() << "\n";

            for (auto &BB : F) {
              errs() << "I saw a basic block " << BB.getName() << "\n";
              for (auto &I : BB) {

                errs() << "analyzing uses of: " << I << "\n";
                
                if (BranchInst *br = dyn_cast<BranchInst>(&I)) {

                    if (br->isConditional()) {
                        Value *condition = br->getCondition();
                        errs() << "  branch instruction condition: " << condition << "\n";
                        // findConditionDefinition(condition);
                        traceVariableDeclaration(condition);
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
