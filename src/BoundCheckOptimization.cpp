#include "BoundCheckOptimization.h"
#include "CommonDef.h"
#include <climits>

using namespace llvm;

struct BoundPredict {
  int minInclusive = INT_MIN;
  int maxExclusive = INT_MAX;
};

struct SubscriptExpr {
  ConstantInt *A;
  Value *i;
  ConstantInt *B;
};

__attribute__((__always_inline__)) inline const Value *
GetOriginatedPtr(const Value *V) {
  llvm::errs() << "Calling GetOriginatedPtr on ";
  V->print(errs());
  llvm::errs() << "\n\n";

  const Value *curr = V;
  do {
    if (isa<SExtInst>(curr)) {
      const auto SI = cast<SExtInst>(curr);
      const auto V = SI->getOperand(0);
      curr = V;
      // llvm::errs() << "    SExtInst\n";
    } else if (isa<LoadInst>(curr)) {
      const auto LI = cast<LoadInst>(curr);
      const auto Ptr = LI->getPointerOperand();
      // curr = Ptr;
      return Ptr;
    // } else if (isa<StoreInst>(curr)) {
    //   const auto SI = cast<StoreInst>(curr);
    //   const auto Ptr = SI->getPointerOperand();
    //   curr = Ptr;
      // llvm::errs() << "    StoreInst\n";
    } else if (isa<AddOperator>(curr)) {
      const auto AO = cast<AddOperator>(curr);
      const auto Op1 = AO->getOperand(0);
      const auto Op2 = AO->getOperand(1);

      if (isa<ConstantInt>(Op1) && isa<ConstantInt>(Op2)) {
        llvm_unreachable("AddOperator: i + i");
      } else if (isa<ConstantInt>(Op1)) {
        const auto A = cast<ConstantInt>(Op1);
        return GetOriginatedPtr(Op2);
      } else if (isa<ConstantInt>(Op2)) {
        const auto B = cast<ConstantInt>(Op2);
        return GetOriginatedPtr(Op1);
      } else {
        llvm_unreachable("Unexpected AddOperator: ptr + ptr");
      }
    

      // llvm::errs() << "    AddOperator\n";
    // } else if (isa<SubOperator>(curr)) {
    //   llvm::errs() << "    SubOperator\n";
    // } else if (isa<MulOperator>(curr)) {
    //   llvm::errs() << "    MulOperator\n";
    } else {
      curr->print(errs());
      llvm_unreachable("Unexpected instruction");
    }
  } while (curr);
  return curr;
}

void ComputeEffect(Function &F) {
  // Page 141

  auto effect = [&](const BasicBlock &BB, const Instruction &Inst) {
    if (isa<CallInst>(Inst)) {
      const auto *CB = cast<CallInst>(&Inst);
      const auto F = CB->getCalledFunction();
      if (F->getName() == "checkBound") {
        Inst.print(errs());
        llvm::errs() << "\n";
        const Value *checked = CB->getArgOperand(1);
        const Value *ptr = GetOriginatedPtr(checked);
        if (ptr) {
          llvm::errs() << "    Ptr: \n";
          ptr->print(errs());
          llvm::errs() << "\n";
        }
        llvm::errs() << "\n";
      }
    }

    // for (const auto& U: V.uses()) {
    //   llvm::errs() << "  ";
    //   U.get()->print(errs());
    //   llvm::errs() << "\n";
    // }

    // const Value* curr = &Inst;
    // do {
    //   if (isa<SExtInst>(curr)) {
    //     const auto SI = cast<SExtInst>(curr);
    //     const auto V = SI->getOperand(0);
    //     curr = V;
    //     llvm::errs() << "    SExtInst\n";
    //   } else if (isa<LoadInst>(curr)) {
    //     const auto LI = cast<LoadInst>(curr);
    //     const auto Ptr = LI->getPointerOperand();
    //     curr = Ptr;
    //     break;
    //   } else if (isa<StoreInst>(curr)) {
    //     const auto SI = cast<StoreInst>(curr);
    //     const auto Ptr = SI->getPointerOperand();
    //     curr = Ptr;
    //     llvm::errs() << "    StoreInst\n";
    //   } else if (isa<AddOperator>(curr)) {
    //     const auto AO = cast<AddOperator>(curr);
    //     const auto Op1 = AO->getOperand(0);
    //     const auto Op2 = AO->getOperand(1);

    //     llvm::errs() << "    AddOperator\n";
    //   } else if (isa<SubOperator>(curr)) {
    //     llvm::errs() << "    SubOperator\n";
    //   } else if (isa<MulOperator>(curr)) {
    //     llvm::errs() << "    MulOperator\n";
    //   } else {
    //     curr->print(errs());
    //     llvm_unreachable("Unexpected instruction");
    //   }
    // } while (curr);
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