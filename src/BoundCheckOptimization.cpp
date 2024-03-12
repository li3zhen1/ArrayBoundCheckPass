#include "BoundCheckOptimization.h"
#include "CommonDef.h"
#include "SubscriptExpr.h"



using namespace llvm;

void ComputeEffect(Function &F) {
  // Page 141

  auto effect = [&](const BasicBlock &BB, const Instruction &Inst) {
    if (isa<CallInst>(Inst)) {
      const auto *CB = cast<CallInst>(&Inst);
      const auto F = CB->getCalledFunction();
      if (F->getName() == "checkBound") {
        // Inst.print(errs());
        // llvm::errs() << "\n";
        const Value *bound = CB->getArgOperand(0);
        const Value *checked = CB->getArgOperand(1);

        const SubscriptExpr BoundExpr = SubscriptExpr::evaluate(bound);
        const SubscriptExpr SubExpr = SubscriptExpr::evaluate(checked);

        CB->print(llvm::errs());
        
        llvm::errs() << "    ";
        BoundExpr.dump(llvm::errs());
        llvm::errs() << ", ";
        SubExpr.dump(llvm::errs());
        llvm::errs() << "\n";
      }
    }
  };

  for (const auto &BB : F) {
    for (const Instruction &Inst : BB) {
      effect(BB, Inst);
    }
  }
}

PreservedAnalyses BoundCheckOptimization::run(Function &F,
                                              FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  llvm::errs() << "BoundCheckOptimization\n";

  ComputeEffect(F);
  
  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}