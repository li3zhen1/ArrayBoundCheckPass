#include "BoundCheckOptimization.h"
#include "CommonDef.h"
#include "SubscriptExpr.h"
#include <cstdint>
#include <optional>

using namespace llvm;

enum class BoundCheckKind {
  lb_le_v,
  v_le_ub,
};

struct SingleDirectionBoundCheckPredict {
  SubscriptExpr Bound;
  SubscriptExpr Subscript;
  BoundCheckKind Kind;

  SingleDirectionBoundCheckPredict(const SubscriptExpr &Bound,
                                   const SubscriptExpr &Subscript,
                                   BoundCheckKind Kind)
      : Bound(Bound), Subscript(Subscript), Kind(Kind) {}
  bool isIdentityCheck() const {
    return Subscript.i != nullptr && Subscript.A == 1 && Subscript.B == 0;
  }
  void print(raw_ostream &O) const {
    if (Kind == BoundCheckKind::lb_le_v) {
      Subscript.dump(O);
      O << " <= ";
      Bound.dump(O);
    } else {
      Bound.dump(O);
      O << " <= ";
      Subscript.dump(O);
    }
    llvm::errs() << "\n";
  }

  // void simplify() {
  //   if (Subscript.B != 0) {
  //     Bound.B -= Subscript.B;
  //     Subscript.B = 0;
  //   }
  // }
};

using BoundCheckList = SmallVector<SingleDirectionBoundCheckPredict, 4>;

/**
 * @brief LowerBound <= Subscript <= UpperBound
 *
 */
struct BoundCheckPredict {
  BoundCheckKind Kind;

  SubscriptExpr LowerBound;
  SubscriptExpr UpperBound;
  SubscriptExpr Subscript;

  BoundCheckPredict(const SubscriptExpr &UpperBound,
                    const SubscriptExpr &Subscript)
      : UpperBound({UpperBound.A, UpperBound.i, UpperBound.B - 1}),
        Subscript(Subscript), LowerBound({0, nullptr, 0}) {}

  BoundCheckPredict(const SubscriptExpr &LowerBound,
                    const SubscriptExpr &UpperBound,
                    const SubscriptExpr &Subscript)
      : LowerBound(LowerBound), UpperBound(UpperBound), Subscript(Subscript) {}

  bool isIdentityCheck() const {
    return Subscript.i != nullptr && Subscript.A == 1 && Subscript.B == 0;
  }

  void splitInto(BoundCheckList &BCL) const {
    BCL.push_back({LowerBound, Subscript, BoundCheckKind::v_le_ub});
    BCL.push_back({UpperBound, Subscript, BoundCheckKind::lb_le_v});
  }
};

using CEntry = SmallVector<SingleDirectionBoundCheckPredict, 4>;

using CMap = SmallDenseMap<const BasicBlock *, BoundCheckList>;

using EffectMap =
    DenseMap<const Value *,
             DenseMap<const BasicBlock *, SmallVector<SubscriptExpr>>>;

enum class EffectKind {
  Unchanged,
  Increment,
  Decrement,
  Multiply,
  UnknownChanged
};

struct EffectOnSubscript {
  EffectKind kind;
  std::optional<uint64_t> c;
};

// void Simplify(BoundCheckList &BCL) {
//   for (auto &BCP : BCL) {
//     BCP.simplify();
//   }
// }

BoundCheckList SimplifyAndIntersect(BoundCheckList &lhs) {

  DenseMap<int64_t /* A */, BoundCheckList> GroupedByA_lb_le_v{};
  DenseMap<int64_t /* A */, BoundCheckList> GroupedByA_v_le_ub{};

  // for (auto &BCP : rhs) {
  //   BCP.simplify();
  //   auto _A = BCP.Subscript.A;
  //   if (BCP.Kind == BoundCheckKind::lb_le_v) {
  //     if (GroupedByA_lb_le_v.find(_A) == GroupedByA_lb_le_v.end()) {
  //       GroupedByA_lb_le_v[_A] = {};
  //     }
  //     GroupedByA_lb_le_v[_A].push_back(BCP);
  //   } else { // v_le_ub
  //     if (GroupedByA_v_le_ub.find(_A) == GroupedByA_v_le_ub.end()) {
  //       GroupedByA_v_le_ub[_A] = {};
  //     }
  //     GroupedByA_v_le_ub[_A].push_back(BCP);
  //   }
  // }
  for (auto &BCP : lhs) {
    // BCP.simplify();
    auto _A = BCP.Subscript.A;
    if (BCP.Kind == BoundCheckKind::lb_le_v) {
      if (GroupedByA_lb_le_v.find(_A) == GroupedByA_lb_le_v.end()) {
        GroupedByA_lb_le_v[_A] = {};
      }
      GroupedByA_lb_le_v[_A].push_back(BCP);
    } else { // v_le_ub
      if (GroupedByA_v_le_ub.find(_A) == GroupedByA_v_le_ub.end()) {
        GroupedByA_v_le_ub[_A] = {};
      }
      GroupedByA_v_le_ub[_A].push_back(BCP);
    }
  }

  DenseMap<int64_t /* A */, BoundCheckList> Intersected_lb_le_v{};
  DenseMap<int64_t /* A */, BoundCheckList> Intersected_v_le_ub{};
  // after grouping, we only need the maximum UpperBound and minimum LowerBound
  for (const auto &[A, Predict] : GroupedByA_lb_le_v) {
    BoundCheckList MinLowerBounds = {};
    const Value *v = nullptr;
    for (const auto &BCP : Predict) {
      // find first comparable LowerBound
      auto *FirstComparableLowerBound =
          llvm::find_if(MinLowerBounds, [&](const auto &P) {
            return P.Subscript.i == BCP.Subscript.i &&
                   P.Subscript.A == BCP.Subscript.A;
          });
      if (FirstComparableLowerBound != nullptr) {
        FirstComparableLowerBound->Bound.B =
            std::max(FirstComparableLowerBound->Bound.B, BCP.Bound.B);
      } else {
        MinLowerBounds.push_back(BCP);
      }
    }
    Intersected_lb_le_v[A] = MinLowerBounds;
  }

  for (const auto &[A, Predict] : GroupedByA_v_le_ub) {
    BoundCheckList MaxUpperBounds = {};
    const Value *v = nullptr;
    for (const auto &BCP : Predict) {
      // find first comparable UpperBound
      auto *FirstComparableUpperBound =
          llvm::find_if(MaxUpperBounds, [&](const auto &P) {
            return P.Subscript.i == BCP.Subscript.i &&
                   P.Subscript.A == BCP.Subscript.A;
          });
      if (FirstComparableUpperBound != nullptr) {
        FirstComparableUpperBound->Bound.B =
            std::min(FirstComparableUpperBound->Bound.B, BCP.Bound.B);
      } else {
        MaxUpperBounds.push_back(BCP);
      }
    }
    Intersected_v_le_ub[A] = MaxUpperBounds;
  }

  BoundCheckList Result = {};

  for (const auto &[A, Predict] : Intersected_lb_le_v) {
    for (const auto &BCP : Predict) {
      Result.push_back(BCP);
    }
  }

  for (const auto &[A, Predict] : Intersected_v_le_ub) {
    for (const auto &BCP : Predict) {
      Result.push_back(BCP);
    }
  }

  return Result;
}

BoundCheckList Union(BoundCheckList &S1, BoundCheckList &&S2) {
  using namespace std;
  using AandI = pair<int64_t, const Value *>;

  // this should work if Bound always has identical A and i
  DenseMap<pair</*Bound*/ AandI, /*Subscript*/ AandI>, BoundCheckList>
      lbChecks{}, ubChecks{};

  const auto getID = [](const SingleDirectionBoundCheckPredict &BCP) {
    return make_pair(make_pair(BCP.Bound.A, BCP.Bound.i),
                     make_pair(BCP.Subscript.A, BCP.Subscript.i));
  };

  for (auto &BCP : S1) {
    const auto ID = getID(BCP);
    if (BCP.Kind == BoundCheckKind::lb_le_v) {
      if (lbChecks.find(ID) == lbChecks.end()) {
        lbChecks[ID] = {};
      }
      lbChecks[ID].push_back(BCP);
    } else {
      if (ubChecks.find(ID) == ubChecks.end()) {
        ubChecks[ID] = {};
      }
      ubChecks[ID].push_back(BCP);
    }
  }

  for (auto &BCP : S2) {
    if (BCP.Kind == BoundCheckKind::lb_le_v) {
      const auto similarCheck = lbChecks.find(getID(BCP));
      if (similarCheck != lbChecks.end()) {
        // Lowest LowerBound
        BoundCheckList LowestLowerBound = {};
        for (auto &BCP2 : similarCheck->second) {
          if (BCP2.Bound.B < BCP.Bound.B) {
            BCP2.Bound.B = BCP.Bound.B;
          }
        }
      } else {
        lbChecks[getID(BCP)].push_back(BCP);
      }
    } else {
      ubChecks[getID(BCP)].push_back(BCP);
    }
  }

  return {};
}

void ComputeEffects(
    Function &F, CMap &C_GEN, EffectMap &effects,
    SmallPtrSet<const Value *, 4> &ValuesReferencedInBoundCheck) {

  for (const auto &BB : F) {
    BoundCheckList predicts = {};
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
          BoundCheckPredict BCP = {BoundExpr, SubExpr};
          // BCP.clampToInclusive();
          BCP.splitInto(predicts);
        }
      }
    }
    C_GEN[&BB] = predicts;
  }

  VERBOSE_PRINT {
    for (const auto &CG : C_GEN) {
      if (CG.second.empty()) {
        continue;
      }
      llvm::errs() << "--------------- BoundCheckList in ";
      CG.first->printAsOperand(llvm::errs());
      llvm::errs() << " ---------------- \n";
      for (const auto &BCP : CG.second) {
        BCP.print(llvm::errs());
      }
      llvm::errs() << "--------------------------------------------------------"
                      "---- \n\n";
    }
  }

  {
    llvm::errs() << "========== Value referenced in subscript ========== \n";
    for (const auto *RefVal : ValuesReferencedInBoundCheck) {
      llvm::errs() << *RefVal << "\n";
    }
    llvm::errs() << "=================================================== \n\n";
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
}

void FillEmpty(Function &F, CMap &C) {
  for (const auto &BB : F) {
    if (C.find(&BB) == C.end()) {
      C[&BB] = {};
    }
  }
}

void BackwardAnalysis(
    Function &F, CMap &C_IN, CMap &C_OUT, CMap &C_GEN,
    std::function<EffectOnSubscript(const BasicBlock *, const Value *)>
        EFFECT) {

  SmallVector<const BasicBlock *, 32> WorkList{};
  SmallPtrSet<const BasicBlock *, 32> Visited{};
  WorkList.push_back(&F.back());

  auto backward = [&](CEntry &C_OUT_B, const BasicBlock *B) -> BoundCheckList {
#define KILL_CHECK break
    auto S = BoundCheckList{};
    if (!C_OUT_B.empty()) {
      const auto *v = C_OUT_B[0].Subscript.i;
      assert(std::all_of(C_OUT_B.begin(), C_OUT_B.end(),
                         [v](const auto &C) { return C.Subscript.i == v; }));

      const auto &Effect = EFFECT(B, v);
      for (const auto &check_C : C_OUT_B) {
        if (check_C.isIdentityCheck()) {
          if (check_C.Kind == BoundCheckKind::lb_le_v) {
            switch (Effect.kind) {
            case EffectKind::Unchanged:
            case EffectKind::Decrement:
              S.push_back(check_C);
              break;
            case EffectKind::Increment:
            case EffectKind::Multiply:
            case EffectKind::UnknownChanged:
              KILL_CHECK;
            }
            break;
          } else if (check_C.Kind == BoundCheckKind::v_le_ub) {
            switch (Effect.kind) {
            case EffectKind::Unchanged:
            case EffectKind::Increment:
            case EffectKind::Multiply:
              S.push_back(check_C);
              break;
            case EffectKind::Decrement:
            case EffectKind::UnknownChanged:
              KILL_CHECK;
            }
            break;
          }
        } else {
          /** f(v) case */
          if (check_C.Kind == BoundCheckKind::lb_le_v) {
            switch (Effect.kind) {
            case EffectKind::Unchanged:
              S.push_back(check_C);
            case EffectKind::Increment:
            case EffectKind::Multiply:
              if (check_C.Subscript.decreasesWhenVIncreases()) {
                S.push_back(check_C);
              }
              break;
            case EffectKind::Decrement:
              if (check_C.Subscript.decreasesWhenVDecreases()) {
                S.push_back(check_C);
              }
              break;
            case EffectKind::UnknownChanged:
              KILL_CHECK;
            }
          } else if (check_C.Kind == BoundCheckKind::v_le_ub) {
            switch (Effect.kind) {
            case EffectKind::Unchanged:
              S.push_back(check_C);
              break;
            case EffectKind::Increment:
            case EffectKind::Multiply:
              if (check_C.Subscript.increasesWhenVIncreases()) {
                S.push_back(check_C);
              }
              break;
            case EffectKind::Decrement:
              if (check_C.Subscript.increasesWhenVDecreases()) {
                S.push_back(check_C);
              }
              break;
            case EffectKind::UnknownChanged:
              KILL_CHECK;
            }
          }
        }
      }
    }
#undef KILL_CHECK
    return S;
  };

  while (!WorkList.empty()) {
    const auto *BB = WorkList.pop_back_val();

    C_IN[BB] = Union(C_GEN[BB], backward(C_OUT[BB], BB));

    if (succ_begin(BB) != succ_end(BB)) {
      BoundCheckList successorPredicts = {};
      for (const auto *Succ : successors(BB)) {
        for (auto &BCP : C_IN[Succ]) {
          successorPredicts.push_back(BCP);
        }
      }
      C_OUT[BB] = SimplifyAndIntersect(successorPredicts);
      // for (const auto &P : C_GEN[BB]) {
      //   const auto &Bound = P.UpperBound;
      //   const auto &Subscript = P.Subscript;
      //   const auto &E = EFFECT(BB, Subscript.i);
      //   switch (E.kind) {
      //   case EffectKind::Unchanged:
      //     break;
      //   case EffectKind::Increment:
      //     break;
      //   case EffectKind::Decrement:
      //     break;
      //   case EffectKind::Multiply:
      //     break;
      //   case EffectKind::UnknownChanged:
      //     break;
      //   }
      // }
    }
    Visited.insert(BB);
    for (const auto *Pred : predecessors(BB)) {
      if (Visited.find(Pred) == Visited.end()) {
        WorkList.push_back(Pred);
      }
    }
  }
}

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
  SmallPtrSet<const Value *, 4> ValuesReferencedInBoundCheck = {};

  ComputeEffects(F, C_GEN, Effects, ValuesReferencedInBoundCheck);

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

  FillEmpty(F, C_IN);
  FillEmpty(F, C_OUT);

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}