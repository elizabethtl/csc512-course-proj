#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct SkeletonPass : public PassInfoMixin<SkeletonPass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        for (auto &F : M) {
            errs() << "I saw a function called " << F.getName() << "!\n";

            for (auto &BB : F) {
              errs() << "I saw a basic block " << BB.getName() << "\n";
              for (auto &I : BB) {
                errs() << "I saw an instruction " << I.getOpcode() << ", " << I.getOpcodeName() << "\n";

                if (BranchInst *br = dyn_cast<BranchInst>(&I)) {
                    if (br->isConditional()) {
                        errs() << "branch instruction condition " << br->getCondition() << "\n";
                    }
                    else {
                        errs() << "branch instruction " << br->getSuccessor(0) << "\n";
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
