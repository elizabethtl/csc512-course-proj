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

        for (auto *U : V->users()) { // Iterates over all uses of V
            if (Instruction *Inst = dyn_cast<Instruction>(U)) {
                errs() << "\t  Used in instruction: " << *Inst << "\n";

                for (auto *User : V->users()) {
                    if (DbgDeclareInst *DbgDeclare = dyn_cast<DbgDeclareInst>(User)) {
                        if (DILocalVariable *Var = DbgDeclare->getVariable()) {
                        errs() << "\t  Variable name (from debug metadata): " << Var->getName() << "\n";
                        return;
                        }
                    } else if (DbgValueInst *DbgValue = dyn_cast<DbgValueInst>(User)) {
                        if (DILocalVariable *Var = DbgValue->getVariable()) {
                        errs() << "\t  Variable name (from debug metadata): " << Var->getName() << "\n";
                        return;
                        }
                    }
                }
            }
            errs() << "\t  No debug information or name found for this value.\n";
            if (V->hasName()) {
                errs() << "\t  Name of Value: " << V->getName() << "\n";
            }
        }
    }

    // Helper function to trace back to the definition of a condition variable
    void findConditionDefinition(Value *condition) {
        if (Instruction *instr = dyn_cast<Instruction>(condition)) {
            errs() << "  Condition defined at: " << *instr << "\n";
            printSourceLocation(instr);
            printValueName(condition);

            // Traverse backwards to find the original definition
            while (instr) {
                if (instr->isBinaryOp() || isa<ICmpInst>(instr) || isa<LoadInst>(instr)) {
                    errs() << "    Found definition: " << *instr << "\n";
                    printSourceLocation(instr);
                    printValueName(instr);
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
                        printValueName(phiNode->getIncomingValue(i));
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

            if (!instr->getName().empty()) {
                llvm::errs() << "  Instruction name: " << instr->getName() << "\n";
            }

            // Walk backwards to find where the variable is defined
            while (instr) {

                // this should give the seminal point
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
                    errs() << "    op:" << *instr << "\n";
                } else {
                    break;
                }
            }
        }
    }

    void traceVariableOrigin(Value *V) {
        // If it's an argument, print and return
        if (isa<Argument>(V)) {
            errs() << "\tVariable originates as a function argument: " << *V << "\n";
            return;
        }

        // If it's an alloca instruction, it's a local variable
        if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
            errs() << "\tVariable originates from an alloca: " << *AI << "\n";
            printValueName(V);
            return;
        }

        // If it's a global variable
        if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
            errs() << "\tVariable originates from a global variable: " << *GV << "\n";
            printValueName(V);
            return;
        }

        // If it's a store instruction
        if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
            errs() << "\tVariable originates from a store: " << *SI << "\n";
            printValueName(V);
            return;
        }

        // If it's defined by an instruction, trace back its operands
        if (Instruction *Inst = dyn_cast<Instruction>(V)) {
            errs() << "\tTracing variable defined by instruction: " << *Inst << "\n";
            printValueName(V);

            for (Use &U : Inst->operands()) {
                Value *Operand = U.get();
                errs() << "\t  Operand: " << *Operand << "\n";
                traceVariableOrigin(Operand); // Recursively trace the operand
            }
        }
    }

    void printDefUseChains(llvm::Value *Val) {
        errs() << "\tprintDefUseChains()\n";

        for (auto *User : Val->users()) {
            llvm::errs() << "\t  Value is used in: " << *User << "\n";
        }
    }

    void analyzeDefUse(Instruction *Inst) {
        errs() << "\tanalyzeDefUse()\n";

        // Iterate over all uses of the instruction
        for (auto &U : Inst->uses()) {
            User *UserInst = U.getUser(); // Get the user of this value
            errs() << "\t  Used in: " << *UserInst << "\n";
        }
    }

    void DefOfUse(Instruction *Inst) {
        errs() << "\tDefOfUse()\n";

        for (auto &Operand : Inst->operands()) {
            if (Value *Def = dyn_cast<Value>(Operand)) {
                errs() << "\t  Operand defined at: " << *Def << "\n";
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

                
                
                if (BranchInst *br = dyn_cast<BranchInst>(&I)) {

                    if (br->isConditional()) {
                        Value *condition = br->getCondition();
                        errs() << "  branch instruction condition: " << condition << "\n";
                        // findConditionDefinition(condition);
                        // traceVariableDeclaration(condition);
                        traceVariableOrigin(condition);

                        // printDefUseChains(condition);

                        // analyzeDefUse(&I);
                        
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
