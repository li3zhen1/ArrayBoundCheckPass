#include "BoundCheckOptimization.h"
#include "BoundPredicate.h"
#include "BoundPredicateSet.h"
#include "CommonDef.h"
#include "Effect.h"
#include "SubscriptExpr.h"
#include <cstddef>

using namespace llvm;

using BoundCheckSetList = SmallVector<BoundPredicateSet>;

using CMap =
    DenseMap<const Value *, DenseMap<const BasicBlock *, BoundPredicateSet>>;

using ValuePtrVector = SmallVector<const Value *, 32>;

using EffectMap =
    DenseMap<const Value *,
             DenseMap<const BasicBlock *, SmallVector<SubscriptExpr>>>;

using ValueEvaluationCache = DenseMap<const Value *, SubscriptExpr>;

void print(CMap &C, raw_ostream &O, const ValuePtrVector &ValueKeys) {

  for (const auto *V : ValueKeys) {
    O << "----------------- Value: ";
    V->printAsOperand(O);
    O << "----------------- \n";
    for (const auto &BB2SE : C[V]) {
      BB2SE.first->printAsOperand(O);
      O << "\n";
      if (!BB2SE.second.isEmpty()) {
        BB2SE.second.print(O);
      }
    }
    O << "\n";
  }

  // for (auto &[V, BB2SE] : C) {
  //   O << "----------------- Value: ";
  //   V->printAsOperand(O);
  //   O << "----------------- \n";
  //   for (auto &[BB, SE] : BB2SE) {
  //     BB->printAsOperand(O);
  //     O << "\n";
  //     if (!SE.isEmpty()) {
  //       SE.print(O);
  //     }
  //   }
  //   O << "\n";
  // }
}

void InitializeToEmpty(Function &F, CMap &C, const ValuePtrVector &ValueKeys) {
  for (const auto *V : ValueKeys) {
    C[V] = DenseMap<const BasicBlock *, BoundPredicateSet>{};
    for (const auto &BB : F) {
      C[V][&BB] = {};
    }
  }
}

void ComputeEffects(Function &F, CMap &Grouped_C_GEN, EffectMap &effects,
                    ValuePtrVector &ValuesReferencedInBoundCheck,
                    ValueEvaluationCache &Evaluated) {

  SmallDenseMap<const BasicBlock *, BoundPredicateSet> C_GEN{};

  for (auto &BB : F) {
    BoundPredicateSet predicts = {};
    for (Instruction &Inst : BB) {
      if (isa<CallInst>(Inst)) {
        const auto CB = cast<CallInst>(&Inst);
        const auto F = CB->getCalledFunction();
        if (F->getName() == "checkBound") {
          const Value *bound = CB->getArgOperand(0);
          const Value *checked = CB->getArgOperand(1);

          const SubscriptExpr BoundExpr = SubscriptExpr::evaluate(bound);
          const SubscriptExpr SubExpr = SubscriptExpr::evaluate(checked);

          Evaluated[bound] = BoundExpr;
          Evaluated[checked] = SubExpr;

          if (!SubExpr.isConstant()) {
            if (llvm::is_contained(ValuesReferencedInBoundCheck, SubExpr.i) ==
                false)
              ValuesReferencedInBoundCheck.push_back(SubExpr.i);
          }

          predicts.addPredicate(UpperBoundPredicate{BoundExpr - 1, SubExpr});
          predicts.addPredicate(
              LowerBoundPredicate{SubscriptExpr::getZero(), SubExpr});
        }
      }
    }
    C_GEN[&BB] = predicts;
  }

  VERBOSE_PRINT {
    llvm::errs() << "\n\n===================== Bound checks normalized "
                    "===================== \n\n";
    for (const auto &CG : C_GEN) {
      llvm::errs() << "----- BoundCheckSet(s) in ";
      CG.first->printAsOperand(llvm::errs());
      llvm::errs() << "----- \n";
      CG.second.print(llvm::errs());
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
            Evaluated[Val] = SE;
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

  for (const auto &[B, CG] : C_GEN) {
    Grouped_C_GEN[CG.getSubscriptIdentity()->second][B] = CG;
  }

  VERBOSE_PRINT {
    llvm::errs()
        << "===================== Grouped C_GEN ===================== \n";
    print(Grouped_C_GEN, (llvm::errs()), ValuesReferencedInBoundCheck);
  }
}

__attribute__((always_inline)) inline EffectOnSubscript
getEffect(const SmallVector<SubscriptExpr> &SE, const Value *V) {
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
}

void RunModificationAnalysis(Function &F, CMap &C_IN, CMap &C_OUT, CMap &C_GEN,
                             EffectMap &Effects,
                             ValuePtrVector &ValuesReferencedInSubscript) {

  auto EFFECT = [&](const BasicBlock *B, const Value *V) -> EffectOnSubscript {
    return getEffect(Effects[V][B], V);
  };

  auto backward = [&](const Value *V, BoundPredicateSet &C_OUT_B,
                      const BasicBlock *B) -> BoundPredicateSet {
#define KILL_CHECK break
    BoundPredicateSet S{};

    const auto &Effect = EFFECT(B, V);

    for (auto &LBP : C_OUT_B.LbPredicates) {
      if (LBP.isIdentityCheck()) {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
        case EffectKind::Decrement:
          S.addPredicate(LBP);
          break;
        case EffectKind::Increment:
        case EffectKind::Multiply:
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
        break;
      } else {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
          S.addPredicate(LBP);
        case EffectKind::Increment:
        case EffectKind::Multiply:
          if (LBP.Index.decreasesWhenVIncreases()) {
            S.addPredicate(LBP);
          }
          break;
        case EffectKind::Decrement:
          if (LBP.Index.decreasesWhenVDecreases()) {
            S.addPredicate(LBP);
          }
          break;
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
      }

      for (auto &UBP : C_OUT_B.UbPredicates) {
        if (UBP.isIdentityCheck()) {
          switch (Effect.kind) {
          case EffectKind::Unchanged:
          case EffectKind::Increment:
          case EffectKind::Multiply:
            S.addPredicate(UBP);
            break;
          case EffectKind::Decrement:
          case EffectKind::UnknownChanged:
            KILL_CHECK;
          }
          break;
        } else {
          switch (Effect.kind) {
          case EffectKind::Unchanged:
            S.addPredicate(UBP);
            break;
          case EffectKind::Increment:
          case EffectKind::Multiply:
            if (UBP.Index.increasesWhenVIncreases()) {
              S.addPredicate(UBP);
            }
            break;
          case EffectKind::Decrement:
            if (LBP.Index.increasesWhenVDecreases()) {
              S.addPredicate(UBP);
            }
            break;
          case EffectKind::UnknownChanged:
            KILL_CHECK;
          }
        }
      }
    }

#undef KILL_CHECK
    return S;
  };
#define ONFLIGHT_PRINT if (false)
  for (const Value *V : ValuesReferencedInSubscript) {
    bool stable = false;
    int round = 0;
    auto assignIfChanged = [&stable](auto &A, auto B) {
      if (A != B) {
        A = B;
        stable = false;
      }
    };
    do {
      stable = true;
      round++;
      ONFLIGHT_PRINT {
        llvm::errs() << "Iterating over ";
        V->printAsOperand(llvm::errs());
        llvm::errs() << "\n";
      }

      SmallVector<const BasicBlock *, 32> WorkList{};
      SmallPtrSet<const BasicBlock *, 32> Visited{};
      WorkList.push_back(&F.back());

      while (!WorkList.empty()) {

        const auto *BB = WorkList.pop_back_val();

        ONFLIGHT_PRINT {
          BB->printAsOperand(BLUE(llvm::errs()));
          llvm::errs() << "\n";
        }

        assignIfChanged(C_IN[V][BB],
                        BoundPredicateSet::Or(
                            {C_GEN[V][BB], backward(V, C_OUT[V][BB], BB)}));

        ONFLIGHT_PRINT {
          llvm::errs() << "\tC_IN\t";
          C_IN[V][BB].print(llvm::errs());
          llvm::errs() << "\n";
        }

        SmallVector<BoundPredicateSet, 4> successorPredicts = {};

        for (const auto *Succ : successors(BB)) {
          ONFLIGHT_PRINT {
            Succ->printAsOperand(YELLOW(llvm::errs()));
            llvm::errs() << "\n";
          }
          successorPredicts.push_back(C_IN[V][Succ]);
        }
        ONFLIGHT_PRINT {
          llvm::errs() << "Successor predicts: " << successorPredicts.size()
                       << "\n";
          for (const auto &SP : successorPredicts) {
            SP.print(llvm::errs());
          }
        }
        assignIfChanged(C_OUT[V][BB],
                        BoundPredicateSet::And(successorPredicts));

        Visited.insert(BB);
        for (const auto *Pred : predecessors(BB)) {
          if (Visited.find(Pred) == Visited.end()) {
            WorkList.push_back(Pred);
          }
        }
      }
    } while (!stable);

    llvm::errs() << "Stable after " << round << " rounds (bkwd)\n";
  }

  VERBOSE_PRINT {
    BLUE(llvm::errs())
        << "===================== Modification C_IN ===================== \n";
    print(C_IN, (llvm::errs()), ValuesReferencedInSubscript);
    BLUE(llvm::errs())
        << "===================== Modification C_OUT ===================== \n";
    print(C_OUT, (llvm::errs()), ValuesReferencedInSubscript);
  }
}

void ApplyModification(Function &F, CMap &Grouped_C_OUT, CMap &C_GEN,
                       ValuePtrVector &ValuesReferencedInSubscript,
                       ValueEvaluationCache &Evaluated) {

  LLVMContext &Context = F.getContext();
  Instruction *InsertPoint = F.getEntryBlock().getFirstNonPHI();
  IRBuilder<> IRB(InsertPoint);
  AttributeList Attr;
  FunctionCallee CheckLower = F.getParent()->getOrInsertFunction(
      "checkLowerBound", Attr, IRB.getVoidTy(), IRB.getInt64Ty(),
      IRB.getInt64Ty(), IRB.getPtrTy(), IRB.getInt64Ty());

  FunctionCallee CheckUpper = F.getParent()->getOrInsertFunction(
      "checkUpperBound", Attr, IRB.getVoidTy(), IRB.getInt64Ty(),
      IRB.getInt64Ty(), IRB.getPtrTy(), IRB.getInt64Ty());

  // TODO: cache the file name
  const auto file =
      IRB.CreateGlobalStringPtr(F.getParent()->getSourceFileName());

  auto createValueForSubExpr = [&](Instruction *point,
                                   const SubscriptExpr &SE) -> Value * {
    IRB.SetInsertPoint(point);
    if (SE.isConstant()) {
      return IRB.getInt64(SE.B);
    } else if (SE.A == 1 && SE.B == 0) {
      return IRB.CreateLoad(SE.i->getType(), (Value *)SE.i);
    } else {
      Value *V = IRB.CreateLoad(SE.i->getType(), (Value *)SE.i);
      if (SE.A != 1) {
        V = IRB.CreateMul(V, IRB.getInt64(SE.A));
      }
      if (SE.B != 0) {
        V = IRB.CreateAdd(V, IRB.getInt64(SE.B));
      }
      return V;
    }
  };

  auto createCheckCall = [&](Instruction *point, Value *bound, Value *subscript,
                             FunctionCallee Check) {
    Value *ln;
    if (const auto Loc = point->getDebugLoc()) {
      ln = IRB.getInt64(Loc.getLine());
    } else {
      ln = IRB.getInt64(0);
    }
    IRB.SetInsertPoint(point->getNextNode());

    auto CI = IRB.CreateCall(Check, {bound, subscript, file, ln});
    return CI;
  };

  for (const auto *V : ValuesReferencedInSubscript) {
    auto &&C_OUT = Grouped_C_OUT[V];
    for (auto &BB : F) {
      if (C_OUT[&BB].isEmpty()) {
        // rewrite checkBound to checkLowerBound and checkUpperBound
        continue;
      } else {
        // remove all old checks
        // insert new check at the end of block or before the first check
      }
      C_GEN[V][&BB] = C_OUT[&BB]; // Update C_GEN

      SmallVector<Instruction *> CI = {};
      Instruction *point = nullptr;
      for (auto &Inst : BB.getInstList()) {
        if (isa<CallInst>(Inst)) {
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();

          if (F->getName() == "checkBound") {
            Value *checked = CB->getArgOperand(1);

            SubscriptExpr SubExpr{0, nullptr, 0};

            if (Evaluated.find(checked) != Evaluated.end()) {
              SubExpr = Evaluated[checked];
            } else {
              SubExpr = SubscriptExpr::evaluate(checked);
              Evaluated[checked] = SubExpr;
            }

            CI.push_back(&Inst);
            if (SubExpr.i == V) {
              if (C_OUT[&BB].isEmpty()) {
                Value *originalBound = CB->getArgOperand(0);
                auto& BoundSE = Evaluated[originalBound];
                BoundSE.B -= 1;
                Value *tempBound = createValueForSubExpr(&Inst, BoundSE);
                createCheckCall(&Inst, tempBound, checked, CheckUpper);
                createCheckCall(&Inst, IRB.getInt64(0), checked, CheckLower);
              } else {
                for (auto &UBP : C_OUT[&BB].UbPredicates) {
                  Value *tempBound = createValueForSubExpr(&Inst, UBP.Bound);
                  createCheckCall(&Inst, tempBound, checked, CheckUpper);
                }
                for (auto &LBP : C_OUT[&BB].LbPredicates) {
                  Value *tempBound = createValueForSubExpr(&Inst, LBP.Bound);
                  createCheckCall(&Inst, tempBound, checked, CheckLower);
                }
              }
            }
          }
        }
      }

      for (auto *CI : CI) {
        CI->eraseFromParent();
      }
    }
  }
}

void RunEliminationAnalysis(Function &F, CMap &C_IN, CMap &C_OUT, CMap &C_GEN,
                            EffectMap &Effects,
                            ValuePtrVector &ValuesReferencedInSubscript) {
  auto EFFECT = [&](const BasicBlock *B, const Value *V) -> EffectOnSubscript {
    return getEffect(Effects[V][B], V);
  };

  auto forward = [&](const Value *V, BoundPredicateSet &C_IN_B,
                     const BasicBlock *B) -> BoundPredicateSet {
#define KILL_CHECK break
    BoundPredicateSet S{};

    const auto &Effect = EFFECT(B, V);

    for (auto &LBP : C_IN_B.LbPredicates) {
      if (LBP.isIdentityCheck()) {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
        case EffectKind::Increment:
        case EffectKind::Multiply:
          S.addPredicate(LBP);
          break;
        case EffectKind::Decrement:
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
      } else {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
          S.addPredicate(LBP);
        case EffectKind::Decrement:
          if (LBP.Index.increasesWhenVDecreases()) {
            S.addPredicate(LBP);
          }
          break;
        case EffectKind::Increment:
        case EffectKind::Multiply:
          if (LBP.Index.increasesWhenVIncreases()) {
            S.addPredicate(LBP);
          }
          break;
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
      }
    }

    for (auto &UBP : C_IN_B.UbPredicates) {
      if (UBP.isIdentityCheck()) {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
        case EffectKind::Decrement:
          S.addPredicate(UBP);
          break;
        case EffectKind::Increment:
        case EffectKind::Multiply:
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
      } else {
        switch (Effect.kind) {
        case EffectKind::Unchanged:
          S.addPredicate(UBP);
        case EffectKind::Increment:
        case EffectKind::Multiply:
          if (UBP.Index.decreasesWhenVIncreases()) {
            S.addPredicate(UBP);
          }
          break;
        case EffectKind::Decrement:
          if (UBP.Index.decreasesWhenVDecreases()) {
            S.addPredicate(UBP);
          }
          break;
        case EffectKind::UnknownChanged:
          KILL_CHECK;
        }
      }
    }
#undef KILL_CHECK
    return S;
  };

  for (const Value *V : ValuesReferencedInSubscript) {
    bool stable = false;
    int round = 0;
    auto assignIfChanged = [&stable](auto &A, auto B) {
      if (A != B) {
        A = B;
        stable = false;
      }
    };
    do {
      stable = true;
      round++;

      SmallVector<const BasicBlock *, 32> WorkList{};
      SmallPtrSet<const BasicBlock *, 32> Visited{};
      WorkList.push_back(&F.getEntryBlock());

      while (!WorkList.empty()) {

        const auto *BB = WorkList.pop_back_val();

        const auto newCOUT =
            BoundPredicateSet::Or({C_GEN[V][BB], forward(V, C_IN[V][BB], BB)});

        assignIfChanged(C_OUT[V][BB], newCOUT);

        SmallVector<BoundPredicateSet, 4> predecessorPredicts = {};
        for (const auto *Pred : predecessors(BB)) {
          predecessorPredicts.push_back(C_OUT[V][Pred]);
        }
        assignIfChanged(C_IN[V][BB],
                        BoundPredicateSet::And(predecessorPredicts));

        Visited.insert(BB);
        for (const auto *Succ : successors(BB)) {
          if (Visited.find(Succ) == Visited.end()) {
            WorkList.push_back(Succ);
          }
        }
      }

    } while (!stable);

    llvm::errs() << "Stable after " << round << " rounds (fwd)\n";
  }

  VERBOSE_PRINT {
    BLUE(llvm::errs())
        << "===================== Elimination C_IN ===================== \n";
    print(C_IN, (llvm::errs()), ValuesReferencedInSubscript);

    BLUE(llvm::errs())
        << "===================== Elimination C_OUT ===================== \n";
    print(C_OUT, (llvm::errs()), ValuesReferencedInSubscript);
  }
}

void ApplyElimination(Function &F, CMap &Grouped_C_IN, CMap &C_GEN,
                      ValuePtrVector &ValuesReferencedInSubscript) {

#define EXTRACT_VALUE                                                          \
  Value *BoundValue = CB->getArgOperand(0);                                    \
  Value *IndexValue = CB->getArgOperand(1);                                    \
  SubscriptExpr BoundExpr = SubscriptExpr::evaluate(BoundValue);               \
  SubscriptExpr IndexExpr = SubscriptExpr::evaluate(IndexValue);               \
  if (IndexExpr.i == V)

  for (const auto *V : ValuesReferencedInSubscript) {
    auto &&C_IN = Grouped_C_IN[V];
    for (const auto &BB : F) {
      if (C_IN[&BB].isEmpty()) {
        continue;
      }
      for (auto &Inst : BB.getInstList()) {
        if (isa<CallInst>(Inst)) {
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();
          auto FName = F->getName();

          std::optional<LowerBoundPredicate> LBP = std::nullopt;
          std::optional<UpperBoundPredicate> UBP = std::nullopt;

          if (FName == "checkBound") {
            EXTRACT_VALUE {
              LBP = LowerBoundPredicate{SubscriptExpr::getZero(), IndexExpr};
              UBP = UpperBoundPredicate{BoundExpr - 1, IndexExpr};
            }
          } else if (FName == "checkLowerBound") {
            EXTRACT_VALUE {
              LBP = LowerBoundPredicate{SubscriptExpr::getZero(), IndexExpr};
            }
          } else if (FName == "checkUpperBound") {
            EXTRACT_VALUE {
              UBP = UpperBoundPredicate{BoundExpr - 1, IndexExpr};
            }
          }

          if (LBP.has_value()) {
            if (llvm::any_of(C_IN[&BB].LbPredicates,
                             [&](const LowerBoundPredicate &p) {
                               return p.subsumes(*LBP);
                             })) {
              llvm::errs() << "Redundant check at ";
              BB.printAsOperand(errs());
              llvm::errs() << " : ";
              LBP->print(errs());
              llvm::errs() << "\n";
            }
          }
          if (UBP.has_value()) {
            if (llvm::any_of(C_IN[&BB].UbPredicates,
                             [&](const UpperBoundPredicate &p) {
                               return p.subsumes(*UBP);
                             })) {
              llvm::errs() << "Redundant check at ";
              BB.printAsOperand(errs());
              llvm::errs() << " : ";
              UBP->print(errs());
              llvm::errs() << "\n";
            }
          }
        }
      }
    }
  }
#undef EXTRACT_VALUE
}

PreservedAnalyses BoundCheckOptimization::run(Function &F,
                                              FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  llvm::errs() << "BoundCheckOptimization\n";

  CMap C_GEN{};

  EffectMap Effects{};

  ValueEvaluationCache Evaluated{};

  ValuePtrVector ValuesReferencedInSubscript = {};

  ComputeEffects(F, C_GEN, Effects, ValuesReferencedInSubscript, Evaluated);

  VERBOSE_PRINT {
    BLUE(llvm::errs())
        << "===================== Effects ===================== \n";
    for (const auto &[V, BB2SE] : Effects) {
      V->printAsOperand(llvm::errs());
      llvm::errs() << "\n";
      for (const auto &[BB, SE] : BB2SE) {
        BB->printAsOperand(llvm::errs());
        llvm::errs() << "\n";
        for (const auto &E : SE) {
          E.dump(llvm::errs());
        }
      }
      llvm::errs() << "\n";
    }
  }

  {
    /**
     * @brief Modification Analysis
     *
     */
    CMap C_IN{};
    CMap C_OUT{};
    InitializeToEmpty(F, C_IN, ValuesReferencedInSubscript);
    InitializeToEmpty(F, C_OUT, ValuesReferencedInSubscript);

    RunModificationAnalysis(F, C_IN, C_OUT, C_GEN, Effects,
                            ValuesReferencedInSubscript);

    ApplyModification(F, C_OUT, C_GEN, ValuesReferencedInSubscript, Evaluated);
  }

  VERBOSE_PRINT {
    BLUE(llvm::errs()) << "===================== C_GEN After modification "
                          "===================== \n";
    print(C_GEN, (llvm::errs()), ValuesReferencedInSubscript);
  }

  {
    /**
     * @brief Elimination Analysis
     *
     */
    CMap C_IN{};
    CMap C_OUT{};
    InitializeToEmpty(F, C_IN, ValuesReferencedInSubscript);
    InitializeToEmpty(F, C_OUT, ValuesReferencedInSubscript);

    RunEliminationAnalysis(F, C_IN, C_OUT, C_GEN, Effects,
                           ValuesReferencedInSubscript);

    ApplyElimination(F, C_IN, C_GEN, ValuesReferencedInSubscript);
  }

  // F.viewCFGOnly();

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}