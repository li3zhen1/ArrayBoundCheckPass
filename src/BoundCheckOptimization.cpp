#include "BoundCheckOptimization.h"
#include "CommonDef.h"

using namespace llvm;

PreservedAnalyses BoundCheckOptimization::run(Function &F, FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  llvm::errs() << "BoundCheckOptimization\n";
  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {

}