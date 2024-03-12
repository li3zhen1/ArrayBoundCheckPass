#include "BoundCheckOptimization.h"
#include "CommonDef.h"
#include "SubscriptExpr.h"

#include <optional>

using namespace llvm;

using EffectMap =
    DenseMap<const Value *,
             DenseMap<const BasicBlock *, std::optional<SubscriptExpr>>>;

/**
 * @brief Subscript < Bound
 *
 */
struct BoundPredict {
  SubscriptExpr Bound;
  SubscriptExpr Subscript;

  BoundPredict(const SubscriptExpr &Bound, const SubscriptExpr &Subscript)
      : Bound(Bound), Subscript(Subscript) {}
};

void ComputeEffect(Function &F, EffectMap &effects) {
  // Page 141
  llvm::SmallPtrSet<const Value *, 4> referencedValues = {};

  for (const auto &BB : F) {
    llvm::SmallVector<BoundPredict, 4> predicts = {};
    for (const Instruction &Inst : BB) {
      if (isa<CallInst>(Inst)) {
        const auto CB = cast<CallInst>(&Inst);
        const auto F = CB->getCalledFunction();
        if (F->getName() == "checkBound") {
          const Value *bound = CB->getArgOperand(0);
          const Value *checked = CB->getArgOperand(1);

          const SubscriptExpr BoundExpr = SubscriptExpr::evaluate(bound);
          const SubscriptExpr SubExpr = SubscriptExpr::evaluate(checked);

          if (!SubExpr.isConstant()) {
            referencedValues.insert(SubExpr.i);
          }
          predicts.push_back({BoundExpr, SubExpr});
        }
      }
    }
  }

  {
    llvm::errs() << "========== Value referenced in subscript ========== \n";
    for (const auto *RefVal : referencedValues) {
      llvm::errs() << *RefVal << "\n";
    }
    llvm::errs() << "=================================================== \n";
  }

  for (const auto *RefVal : referencedValues) {
    llvm::errs() << "------------------- Mutations on ";
    RefVal->printAsOperand(llvm::errs());
    llvm::errs() << " ------------------- \n";
    for (const auto &BB : F) {
      for (const Instruction &Inst : BB) {
        if (isa<StoreInst>(Inst)) {
          const auto SI = cast<StoreInst>(&Inst);
          const auto Ptr = SI->getPointerOperand();
          const auto Val = SI->getValueOperand();
          if (RefVal == Ptr) {
            SubscriptExpr::evaluate(Val).dump(llvm::errs());
            llvm::errs() << " --> ";
            Ptr->printAsOperand(llvm::errs());
            llvm::errs() << "\n";
          }
        }
      }
    }
    llvm::errs() << "------------------------------------------------------------ \n";
  }
}

PreservedAnalyses BoundCheckOptimization::run(Function &F,
                                              FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  llvm::errs() << "BoundCheckOptimization\n";

  EffectMap effects{};

  ComputeEffect(F, effects);

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}