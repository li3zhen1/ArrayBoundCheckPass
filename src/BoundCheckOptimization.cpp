#include "BoundCheckOptimization.h"
#include "BoundPredicate.h"
#include "BoundPredicateSet.h"
#include "CommonDef.h"
#include "Effect.h"
#include "SubscriptExpr.h"

using namespace llvm;

using BoundCheckSetList = SmallVector<BoundPredicateSet>;

using CMap =
    SmallDenseMap<const Value *,
                  SmallDenseMap<const BasicBlock *, BoundCheckSetList>>;

using ValuePtrSet = SmallPtrSet<const Value *, 32>;
using EffectMap =
    DenseMap<const Value *,
             DenseMap<const BasicBlock *, SmallVector<SubscriptExpr>>>;

void print(CMap &C, raw_ostream &O) {
  for (const auto &It : C) {
    O << "----------------- Value: ";
    It.first->printAsOperand(O);
    O << "----------------- \n";
    for (const auto &BB : It.second) {
      BB.first->printAsOperand(O);
      O << "\n";
      for (const auto &BCP : BB.second) {
        BCP.print(O);
        O << "\n";
      }
    }
    O << "\n";
  }
}

inline auto FindMergableCheckSet(BoundCheckSetList &BCSL,
                                 const SubscriptIndentity &SI)
    -> BoundPredicateSet * {
  if (BCSL.empty()) {
    return nullptr;
  }
  return llvm::find_if(BCSL, [&](const auto &BCS) {
    if (BCS.getSubscriptIdentity().has_value() == false) {
      return false;
    }
    return BCS.getSubscriptIdentity().value() == SI;
  });
  return nullptr;
}

void InitializeToEmpty(Function &F, CMap &C, const ValuePtrSet &ValueKeys) {
  for (const auto *V : ValueKeys) {
    for (const auto &BB : F) {
      C[V][&BB] = {};
    }
  }
}

void ComputeEffects(Function &F, CMap &Grouped_C_GEN, EffectMap &effects,
                    ValuePtrSet &ValuesReferencedInBoundCheck) {

  SmallDenseMap<const BasicBlock *, BoundCheckSetList> C_GEN{};

  for (const auto &BB : F) {
    BoundCheckSetList predicts = {};
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
            ValuesReferencedInBoundCheck.insert(SubExpr.i);
          }
          BoundPredicateSet BCS;

          BCS.addPredicate(UpperBoundPredicate{BoundExpr - 1, SubExpr});
          BCS.addPredicate(
              LowerBoundPredicate{SubscriptExpr::getZero(), SubExpr});
          predicts.push_back(BCS);
        }
      }
    }
    C_GEN[&BB] = predicts;
  }

  VERBOSE_PRINT {
    llvm::errs() << "\n\n===================== Bound checks normalized "
                    "===================== \n\n";
    for (const auto &CG : C_GEN) {
      if (CG.second.empty()) {
        continue;
      }
      llvm::errs() << "----- BoundCheckSet(s) in ";
      CG.first->printAsOperand(llvm::errs());
      llvm::errs() << "----- \n";
      for (const auto &BCP : CG.second) {
        BCP.print(llvm::errs());
      }
      llvm::errs() << "---------------------------------------- \n\n\n";
    }

    llvm::errs() << "========== Value referenced in subscript ========== \n";
    for (const auto *RefVal : ValuesReferencedInBoundCheck) {
      llvm::errs() << *RefVal << "\n";
    }

    llvm::errs()
        << "\n\n\n===================== Mutations ===================== \n";
  }

  for (const auto *RefVal : ValuesReferencedInBoundCheck) {
    VERBOSE_PRINT {
      llvm::errs() << "------------------- Mutations on ";
      RefVal->printAsOperand(llvm::errs());
      llvm::errs() << " -------------------- \n";
    }

    effects[RefVal] =
        DenseMap<const BasicBlock *, SmallVector<SubscriptExpr>>{};

    for (const auto &BB : F) {
      effects[RefVal][&BB] = SmallVector<SubscriptExpr>{};
      for (const Instruction &Inst : BB) {
        if (isa<StoreInst>(Inst)) {
          const auto SI = cast<StoreInst>(&Inst);
          const auto Dst = SI->getPointerOperand();
          const auto Val = SI->getValueOperand();
          if (RefVal == Dst) {
            auto SE = SubscriptExpr::evaluate(Val);
            if (SE.i == Dst) {
              effects[RefVal][&BB].push_back(SE);
              VERBOSE_PRINT {
                SI->getDebugLoc().print(llvm::errs());
                llvm::errs() << " : \t";
                BB.printAsOperand(llvm::errs());
                llvm::errs() << ":\t";
                SE.dump(llvm::errs());
                llvm::errs() << " --> ";
                Dst->printAsOperand(llvm::errs());
                llvm::errs() << "\n";
              }
            }
          }
        }
      }
    }
    VERBOSE_PRINT {
      llvm::errs() << "--------------------------------------------------------"
                      "---- \n\n";
    }
  }

  InitializeToEmpty(F, Grouped_C_GEN, ValuesReferencedInBoundCheck);
  for (const auto &CG : C_GEN) {
    for (const auto &BCP : CG.second) {
      Grouped_C_GEN[BCP.getSubscriptIdentity()->second][CG.first].push_back(
          BCP);
    }
  }

  VERBOSE_PRINT {
    llvm::errs()
        << "===================== Grouped C_GEN ===================== \n";
    print(Grouped_C_GEN, llvm::errs());
  }
}

// void BackwardAnalysis(
//     Function &F, CMap &C_IN, CMap &C_OUT, CMap &C_GEN,
//     std::function<EffectOnSubscript(const BasicBlock *, const Value *)>
//         EFFECT) {

//   SmallVector<const BasicBlock *, 32> WorkList{};
//   SmallPtrSet<const BasicBlock *, 32> Visited{};
//   WorkList.push_back(&F.back());

//   auto backward = [&](BoundCheckSetList &C_OUT_B,
//                       const BasicBlock *B) -> BoundCheckSetList {
// #define KILL_CHECK break
//     auto S = BoundCheckSetList{};
//     if (!C_OUT_B.empty()) {
//       const auto *v = C_OUT_B[0].getSubscriptIdentity().value().second;
//       assert(std::all_of(C_OUT_B.begin(), C_OUT_B.end(),
//                          [v](const auto &C) { return C.Subscript.i == v; }));

//       const auto &Effect = EFFECT(B, v);
//       for (const auto &check_C : C_OUT_B) {
//         if (check_C.isIdentityCheck()) {
//           if (check_C.Kind == BoundCheckKind::lb_le_v) {
//             switch (Effect.kind) {
//             case EffectKind::Unchanged:
//             case EffectKind::Decrement:
//               S.push_back(check_C);
//               break;
//             case EffectKind::Increment:
//             case EffectKind::Multiply:
//             case EffectKind::UnknownChanged:
//               KILL_CHECK;
//             }
//             break;
//           } else if (check_C.Kind == BoundCheckKind::v_le_ub) {
//             switch (Effect.kind) {
//             case EffectKind::Unchanged:
//             case EffectKind::Increment:
//             case EffectKind::Multiply:
//               S.push_back(check_C);
//               break;
//             case EffectKind::Decrement:
//             case EffectKind::UnknownChanged:
//               KILL_CHECK;
//             }
//             break;
//           }
//         } else {
//           /** f(v) case */
//           if (check_C.Kind == BoundCheckKind::lb_le_v) {
//             switch (Effect.kind) {
//             case EffectKind::Unchanged:
//               S.push_back(check_C);
//             case EffectKind::Increment:
//             case EffectKind::Multiply:
//               if (check_C.Subscript.decreasesWhenVIncreases()) {
//                 S.push_back(check_C);
//               }
//               break;
//             case EffectKind::Decrement:
//               if (check_C.Subscript.decreasesWhenVDecreases()) {
//                 S.push_back(check_C);
//               }
//               break;
//             case EffectKind::UnknownChanged:
//               KILL_CHECK;
//             }
//           } else if (check_C.Kind == BoundCheckKind::v_le_ub) {
//             switch (Effect.kind) {
//             case EffectKind::Unchanged:
//               S.push_back(check_C);
//               break;
//             case EffectKind::Increment:
//             case EffectKind::Multiply:
//               if (check_C.Subscript.increasesWhenVIncreases()) {
//                 S.push_back(check_C);
//               }
//               break;
//             case EffectKind::Decrement:
//               if (check_C.Subscript.increasesWhenVDecreases()) {
//                 S.push_back(check_C);
//               }
//               break;
//             case EffectKind::UnknownChanged:
//               KILL_CHECK;
//             }
//           }
//         }
//       }
//     }
// #undef KILL_CHECK
//     return S;
//   };

//   while (!WorkList.empty()) {
//     const auto *BB = WorkList.pop_back_val();

//     C_IN[BB] = Union(C_GEN[BB], backward(C_OUT[BB], BB));

//     if (succ_begin(BB) != succ_end(BB)) {
//       BoundCheckList successorPredicts = {};
//       for (const auto *Succ : successors(BB)) {
//         for (auto &BCP : C_IN[Succ]) {
//           successorPredicts.push_back(BCP);
//         }
//       }
//       C_OUT[BB] = SimplifyAndIntersect(successorPredicts);
//       // for (const auto &P : C_GEN[BB]) {
//       //   const auto &Bound = P.UpperBound;
//       //   const auto &Subscript = P.Subscript;
//       //   const auto &E = EFFECT(BB, Subscript.i);
//       //   switch (E.kind) {
//       //   case EffectKind::Unchanged:
//       //     break;
//       //   case EffectKind::Increment:
//       //     break;
//       //   case EffectKind::Decrement:
//       //     break;
//       //   case EffectKind::Multiply:
//       //     break;
//       //   case EffectKind::UnknownChanged:
//       //     break;
//       //   }
//       // }
//     }
//     Visited.insert(BB);
//     for (const auto *Pred : predecessors(BB)) {
//       if (Visited.find(Pred) == Visited.end()) {
//         WorkList.push_back(Pred);
//       }
//     }
//   }
// }

PreservedAnalyses BoundCheckOptimization::run(Function &F,
                                              FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  llvm::errs() << "BoundCheckOptimization\n";

  CMap C_GEN{};
  CMap C_IN{};
  CMap C_OUT{};

  EffectMap Effects{};
  ValuePtrSet ValuesReferencedInSubscript = {};
  ValuePtrSet ValuesReferencedInBound = {};

  ComputeEffects(F, C_GEN, Effects, ValuesReferencedInSubscript);

  auto EFFECT = [&](const BasicBlock *B, const Value *V) -> EffectOnSubscript {
    const auto &SE = Effects[V][B];
    if (SE.empty()) {
      return {EffectKind::Unchanged, std::nullopt};
    }
    const auto &Last = SE.back();
    if (Last.i != nullptr) {
      assert(Last.i == V);
      if (Last.A == 1) {
        if (Last.B == 0) {
          return {EffectKind::Unchanged, std::nullopt};
        } else if (Last.B > 0) {
          return {EffectKind::Increment, Last.B};
        } else {
          return {EffectKind::Decrement, -Last.B};
        }
      } else if (Last.B == 0) {
        if (Last.A > 1)
          return {EffectKind::Multiply, Last.A};
      }
    }
    return {EffectKind::UnknownChanged, std::nullopt};
  };

  InitializeToEmpty(F, C_IN, ValuesReferencedInSubscript);
  InitializeToEmpty(F, C_OUT, ValuesReferencedInSubscript);

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}