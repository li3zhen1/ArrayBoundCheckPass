#include "BoundCheckOptimization.h"
#include "BoundPredicate.h"
#include "BoundPredicateSet.h"
#include "CommonDef.h"
#include "Effect.h"
#include "SubscriptExpr.h"
#include "llvm/IR/Dominators.h"
#include <utility>

using namespace llvm;

void RecurrsivelyClearAllInstructionsUsedOnlyBy(Value *V) {
  // llvm::errs() << "@@@ Trying to clear ";
  // V->print(llvm::errs());
  // llvm::errs() << "\n";
  if (isa<Instruction>(V)) {
    auto *I = cast<Instruction>(V);
    SmallVector<Value *, 4> ops;
    auto opNum = I->getNumOperands();
    for (unsigned i = 0; i < opNum; i++) {
      ops.push_back(I->getOperand(i));
    }
    if (I->use_empty()) {
      I->eraseFromParent();
      for (auto &U : ops) {
        RecurrsivelyClearAllInstructionsUsedOnlyBy(U);
      }
    }
  }
}

Value *createValueForSubExpr(IRBuilder<> &IRB, Instruction *point,
                             const SubscriptExpr &SE) {
  IRB.SetInsertPoint(point);
  if (SE.isConstant()) {
    return IRB.getInt64(SE.B);
  } else {
    Value *V = (Value *)SE.i;

    auto VTy = V->getType();
    if (VTy->isPointerTy()) {
      // https://llvm.org/docs/OpaquePointers.html

      /**
       * For loads, use getType().
        For stores, use getValueOperand()->getType().
        Use getLoadStoreType() to handle both of the above in one call.
        For getelementptr instructions, use getSourceElementType().
        For calls, use getFunctionType().
        For allocas, use getAllocatedType().
        For globals, use getValueType().
        For consistency assertions, use
       PointerType::isOpaqueOrPointeeTypeEquals().
       *
       */
      Type *baseTy = nullptr;
      // do {
      if (isa<AllocaInst>(V)) {
        baseTy = cast<AllocaInst>(V)->getAllocatedType();
      } else if (isa<LoadInst>(V)) {
        baseTy = cast<LoadInst>(V)->getType();
      } else if (isa<StoreInst>(V)) {
        baseTy = cast<StoreInst>(V)->getValueOperand()->getType();
      } else if (isa<GetElementPtrInst>(V)) {
        baseTy = cast<GetElementPtrInst>(V)->getSourceElementType();
      } else if (isa<CallInst>(V)) {
        baseTy = cast<CallInst>(V)->getFunctionType();
      } else if (isa<GlobalValue>(V)) {
        baseTy = cast<GlobalValue>(V)->getValueType();
      } else if (isa<Argument>(V)) {
        baseTy = cast<Argument>(V)->getType();
      } else if (isa<Constant>(V)) {
        baseTy = cast<Constant>(V)->getType();
      } else {
        (V)->print(llvm::errs());
        llvm_unreachable("Unsupported pointer type while creating value for "
                         "subscript expression");
      }
      // } while (baseTy->isPointerTy());

      VTy = baseTy;

      if (VTy->isIntegerTy()) {
        auto I = IRB.CreateLoad(VTy, V);
        // I->setMetadata("BoundCheckOpt", MDNode::get(V->getContext(), {}));
        V = I;
      } else {
        llvm_unreachable("Unsupported pointer type while creating value for "
                         "subscript expression");
      }
    }

    if (VTy->isIntegerTy(64)) {
      V = V;
    } else if (VTy->isIntegerTy()) {
      V = IRB.CreateIntCast(V, IRB.getInt64Ty(), true);
    } else {
      llvm_unreachable("Unsupported type while creating value for subscript "
                       "expression");
    }

    if (SE.A != 1) {
      V = IRB.CreateMul(V, IRB.getInt64(SE.A));
    }
    if (SE.B != 0) {
      V = IRB.CreateAdd(V, IRB.getInt64(SE.B));
    }
    return V;
  }
};

CallInst *createCheckCall(IRBuilder<> &IRB, Instruction *point,
                          FunctionCallee Check, Value *bound, Value *subscript,
                          Constant *file) {
  Value *ln;
  if (const auto Loc = point->getDebugLoc()) {
    ln = IRB.getInt64(Loc.getLine());
  } else {
    ln = IRB.getInt64(0);
  }
  IRB.SetInsertPoint(point);
  return IRB.CreateCall(Check, {bound, subscript, file, ln});
};

void RecomputeC_GEN(Function &F, CMap &Grouped_C_GEN,
                    const ValuePtrVector &ValuesReferencedInBoundCheck,
                    ValueEvaluationCache &Evaluated) {

  InitializeToEmpty(F, Grouped_C_GEN, ValuesReferencedInBoundCheck);

  auto getOrEvaluateSubExpr = [&](const Value *V) -> SubscriptExpr {
    if (Evaluated.find(V) != Evaluated.end()) {
      return Evaluated[V];
    } else {
      auto SE = SubscriptExpr::evaluate(V);
      Evaluated[V] = SE;
      return SE;
    }
  };

  for (auto *V : ValuesReferencedInBoundCheck) {

    SmallDenseMap<const BasicBlock *, BoundPredicateSet> C_GEN{};

    for (auto &BB : F) {

      for (Instruction &Inst : BB) {
        if (isa<CallInst>(Inst)) {

          const auto CB = cast<CallInst>(&Inst);
          const auto F = CB->getCalledFunction();
          auto FName = F->getName();

          if (FName != CHECK_LB && FName != CHECK_UB)
            continue;
          const Value *bound = CB->getArgOperand(0);
          const Value *checked = CB->getArgOperand(1);

          const SubscriptExpr BoundExpr = getOrEvaluateSubExpr(bound);
          const SubscriptExpr SubExpr = getOrEvaluateSubExpr(checked);

          if (SubExpr.i != V) {
            continue;
          }

          if (F->getName() == CHECK_UB) {
            auto UBP = UpperBoundPredicate{BoundExpr, SubExpr};
            UBP.normalize();
            C_GEN[&BB].addPredicate(UBP);
          } else if (F->getName() == CHECK_LB) {
            auto LBP = LowerBoundPredicate{BoundExpr, SubExpr};
            LBP.normalize();
            C_GEN[&BB].addPredicate(LBP);
          }
        }
      }
    }
    for (const auto &[B, CG] : C_GEN) {
      Grouped_C_GEN[V][B] = CG;
    }
  }

  VERBOSE_PRINT {
    BLUE(llvm::errs()) << "===================== C_GEN After modification "
                          "===================== \n";
    print(Grouped_C_GEN, (llvm::errs()), ValuesReferencedInBoundCheck);
  }
}

void ComputeEffects(Function &F, CMap &Grouped_C_GEN, EffectMap &effects,
                    ValuePtrVector &ValuesReferencedInBoundCheck,
                    ValuePtrVector &_ValuesReferencedInBound,
                    ValueEvaluationCache &Evaluated, Constant *file) {

  LLVMContext &Context = F.getContext();
  IRBuilder<> IRB(Context);

  using EarliestUBCheckMap =
      SmallDenseMap<SubscriptIndentity,
                    SmallDenseMap<SubscriptIndentity,
                                  std::pair<CallInst *, UpperBoundPredicate>>>;

  using EarliestLBCheckMap =
      SmallDenseMap<SubscriptIndentity,
                    SmallDenseMap<SubscriptIndentity,
                                  std::pair<CallInst *, LowerBoundPredicate>>>;

  const auto findEarliestMergableUBCheck =
      [](EarliestUBCheckMap &ECM, const SubscriptIndentity &IndexIdentity,
         const SubscriptIndentity &BoundIdentity)
      -> std::pair<CallInst *, UpperBoundPredicate> * {
    auto Iter = ECM.find(IndexIdentity);
    if (Iter == ECM.end()) {
      return nullptr;
    }
    auto Iter2 = Iter->second.find(BoundIdentity);
    if (Iter2 == Iter->second.end()) {
      return nullptr;
    }
    return &Iter2->second;
  };

  const auto findEarliestMergableLBCheck =
      [](EarliestLBCheckMap &ECM, const SubscriptIndentity &IndexIdentity,
         const SubscriptIndentity &BoundIdentity)
      -> std::pair<CallInst *, LowerBoundPredicate> * {
    auto Iter = ECM.find(IndexIdentity);
    if (Iter == ECM.end()) {
      return nullptr;
    }
    auto Iter2 = Iter->second.find(BoundIdentity);
    if (Iter2 == Iter->second.end()) {
      return nullptr;
    }
    return &Iter2->second;
  };

  SmallVector<CallInst *, 32> obsoleteChecks;

  for (auto &BB : F) {
    for (Instruction &Inst : BB) {

      if (isa<CallInst>(Inst)) {
        // Inst.print(llvm::errs(), true);
        // llvm::errs() << "\n";
        const auto CB = cast<CallInst>(&Inst);
        const auto F = CB->getCalledFunction();

        if (F->getName() != CHECK_LB && F->getName() != CHECK_UB)
          continue;

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

        if (!BoundExpr.isConstant()) {
          if (llvm::is_contained(_ValuesReferencedInBound, BoundExpr.i) ==
              false)
            _ValuesReferencedInBound.push_back(BoundExpr.i);
        }
      }
    }
  }

  InitializeToEmpty(F, Grouped_C_GEN, ValuesReferencedInBoundCheck);

  for (auto *V : ValuesReferencedInBoundCheck) {

    SmallDenseMap<const BasicBlock *, BoundPredicateSet> C_GEN{};

    for (auto &BB : F) {
      EarliestLBCheckMap EarliestLB{};
      EarliestUBCheckMap EarliestUB{};
      bool isKept = false;
      for (Instruction &Inst : BB) {
        if (isa<CallInst>(Inst)) {
          // Inst.print(llvm::errs(), true);
          // llvm::errs() << "\n";
          const auto CB = cast<CallInst>(&Inst);
          const auto F = CB->getCalledFunction();

          if (F->getName() != CHECK_LB && F->getName() != CHECK_UB)
            continue;
          const Value *bound = CB->getArgOperand(0);
          const Value *checked = CB->getArgOperand(1);

          const SubscriptExpr BoundExpr = Evaluated[bound];
          const SubscriptExpr SubExpr = Evaluated[checked];

          if (SubExpr.i != V) {
            continue;
          }

          if (F->getName() == CHECK_UB) {
            auto UBP = UpperBoundPredicate{BoundExpr, SubExpr};
            UBP.normalize();
            C_GEN[&BB].addPredicate(UBP);

            // llvm::errs() << "UBP: ";
            // UBP.print(llvm::errs());
            // llvm::errs() << "\n";

            // CallInst *uppermostCheckInst = nullptr;
            // if (auto *E = findEarliestMergableUBCheck(
            //         EarliestUB, UBP.Index.getIdentity(),
            //         UBP.Bound.getIdentity())) {
            //   // llvm::errs() << "Found earliest mergable UB check\n";

            //   auto &&uppermostCheck = E->second;
            //   uppermostCheckInst = E->first;

            //   if (!uppermostCheck.subsumes(UBP)) {
            //     // replace the earliest check with the new one

            //     auto &&thisCheck = UBP;
            //     auto constantDiffOfIndex =
            //         thisCheck.Index.getConstantDifference(uppermostCheck.Index);
            //     auto constantDiffOfBound =
            //         thisCheck.Bound.getConstantDifference(uppermostCheck.Bound);

            //     constantDiffOfBound -= constantDiffOfIndex;

            //     // always reuse the index value

            //     // modify the earliest check's bound to be the tightest bound
            //     // We only need to add the constant
            //     if (constantDiffOfBound != 0) {
            //       IRB.SetInsertPoint(CB);
            //       Value *newBoundValue = IRB.CreateAdd(
            //           CB->getArgOperand(0),
            //           IRB.getInt64(constantDiffOfBound));
            //       uppermostCheckInst->setArgOperand(0, newBoundValue);

            //       uppermostCheckInst->addAnnotationMetadata("Lifted");

            //       // BLUE(llvm::errs()) << "LIFTED\n";
            //     }
            //   }
            //   // obsoleteChecks.push_back(CB);
            // } else {
            //   uppermostCheckInst = CB;
            //   isKept = true;
            // }
            // if (isKept) {
            //   auto Iter1 = EarliestUB.find(UBP.Index.getIdentity());
            //   std::pair<CallInst *, UpperBoundPredicate> newEntry = {
            //       uppermostCheckInst, UBP};

            //   if (Iter1 != EarliestUB.end()) {
            //     Iter1->getSecond().insert({UBP.Bound.getIdentity(),
            //     newEntry});
            //   } else {
            //     EarliestUB[UBP.Index.getIdentity()] = {
            //         {UBP.Bound.getIdentity(), newEntry}};
            //     // llvm::errs() << "Inserting new entry\n";
            //   }
            //   C_GEN[&BB].addPredicate(UBP);
            // }

          } else if (F->getName() == CHECK_LB) {
            auto LBP = LowerBoundPredicate{BoundExpr, SubExpr};
            LBP.normalize();
            C_GEN[&BB].addPredicate(LBP);

            // llvm::errs() << "LBP: ";
            // LBP.print(llvm::errs());
            // llvm::errs() << "\n";

            // CallInst *uppermostCheckInst = nullptr;

            // if (auto *E = findEarliestMergableLBCheck(
            //         EarliestLB, LBP.Index.getIdentity(),
            //         LBP.Bound.getIdentity())) {

            //   // llvm::errs() << "Found earliest mergable LB check\n";
            //   auto &&uppermostCheck = E->second;
            //   uppermostCheckInst = E->first;

            //   if (!uppermostCheck.subsumes(LBP)) {
            //     auto &&thisCheck = LBP;
            //     auto constantDiffOfIndex =
            //         thisCheck.Index.getConstantDifference(uppermostCheck.Index);
            //     auto constantDiffOfBound =
            //         thisCheck.Bound.getConstantDifference(uppermostCheck.Bound);

            //     constantDiffOfBound -= constantDiffOfIndex;

            //     // always reuse the index value

            //     // modify the earliest check's bound to be the tightest bound
            //     // We only need to add the constant
            //     if (constantDiffOfBound != 0) {
            //       IRB.SetInsertPoint(CB);
            //       Value *newBoundValue = IRB.CreateAdd(
            //           CB->getArgOperand(0),
            //           IRB.getInt64(constantDiffOfBound));
            //       uppermostCheckInst->setArgOperand(0, newBoundValue);

            //       uppermostCheckInst->addAnnotationMetadata("Lifted");

            //       // BLUE(llvm::errs()) << "LIFTED\n";
            //     }
            //   }
            //   // obsoleteChecks.push_back(CB);
            // } else {
            //   uppermostCheckInst = CB;
            //   isKept = true;
            // }

            // if (isKept) {

            //   auto Iter1 = EarliestLB.find(LBP.Index.getIdentity());
            //   std::pair<CallInst *, LowerBoundPredicate> newEntry = {
            //       uppermostCheckInst, LBP};

            //   if (Iter1 != EarliestLB.end()) {
            //     Iter1->getSecond().insert({LBP.Bound.getIdentity(),
            //     newEntry});
            //   } else {
            //     EarliestLB[LBP.Index.getIdentity()] = {
            //         {LBP.Bound.getIdentity(), newEntry}};
            //   }

            //   // addPredicateToCGenLB(checked, &BB, LBP);
            //   C_GEN[&BB].addPredicate(LBP);
            // }
          }
        }
      }
    }
    for (const auto &[B, CG] : C_GEN) {
      Grouped_C_GEN[V][B] = CG;
    }
  }

  for (auto *CI : obsoleteChecks) {
    CI->eraseFromParent();
  }

  VERBOSE_PRINT {
    llvm::errs() << "\n\n===================== Bound checks normalized "
                    "===================== \n\n";
    // for (const auto &CG : C_GEN) {
    //   llvm::errs() << "----- BoundCheckSet(s) in ";
    //   CG.first->printAsOperand(llvm::errs());
    //   llvm::errs() << "----- \n";
    //   CG.second.print(llvm::errs());
    //   llvm::errs() << "---------------------------------------- \n\n\n";
    // }

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

  VERBOSE_PRINT {
    llvm::errs()
        << "===================== Grouped C_GEN ===================== \n";
    print(Grouped_C_GEN, (llvm::errs()), ValuesReferencedInBoundCheck);
  }

  VERBOSE_PRINT {
    BLUE(llvm::errs())
        << "===================== Effects ===================== \n";
    for (const auto &[V, BB2SE] : effects) {
      V->printAsOperand(llvm::errs());
      llvm::errs() << "\n";
      for (const auto &[BB, SE] : BB2SE) {
        if (SE.empty())
          continue;
        BB->printAsOperand(llvm::errs());
        llvm::errs() << ": ";
        for (const auto &E : SE) {
          E.dump(llvm::errs());
        }
        llvm::errs() << "\n";
      }
    }
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

        YELLOW(llvm::errs()) << "\n\n\n=========== Iterating over ";
        V->printAsOperand(llvm::errs());
        llvm::errs() << "  " << round << " round\n"
                     << "\n";
      }

      SmallVector<const BasicBlock *, 32> WorkList{};
      SmallPtrSet<const BasicBlock *, 32> Visited{};
      WorkList.push_back(&F.back());

      while (!WorkList.empty()) {

        const auto *BB = WorkList.pop_back_val();

        auto bkwd = backward(V, C_OUT[V][BB], BB);
        ONFLIGHT_PRINT {
          BB->printAsOperand(llvm::errs());
          llvm::errs() << "------------------\n";
          C_IN[V][BB].print(llvm::errs());
          llvm::errs() << "V ";
          bkwd.print(llvm::errs());
          llvm::errs() << "= ";
        }

        assignIfChanged(C_IN[V][BB],
                        BoundPredicateSet::Or({C_GEN[V][BB], bkwd}));

        ONFLIGHT_PRINT {
          C_IN[V][BB].print(llvm::errs());
          llvm::errs() << "\n";
        }

        SmallVector<BoundPredicateSet, 4> successorPredicts = {};

        for (const auto *Succ : successors(BB)) {
          successorPredicts.push_back(C_IN[V][Succ]);
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
    BLUE(llvm::errs()) << "===================== Modification C_OUT "
                          "===================== \n";
    print(C_OUT, (llvm::errs()), ValuesReferencedInSubscript);
  }
}

void ApplyModification(Function &F, CMap &Grouped_C_OUT, CMap &C_GEN,
                       ValuePtrVector &ValuesReferencedInSubscript,
                       ValueEvaluationCache &Evaluated, Constant *file,
                       DominatorTree &DTA) {

  VERBOSE_PRINT {
    BLUE(llvm::errs()) << "===================== Apply Modification "
                          "===================== \n";
  }
  LLVMContext &Context = F.getContext();
  Instruction *InsertPoint = F.getEntryBlock().getFirstNonPHI();
  IRBuilder<> IRB(InsertPoint);
  AttributeList Attr;
  FunctionCallee CheckLower = F.getParent()->getOrInsertFunction(
      CHECK_LB, Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
      IRB.getPtrTy(), IRB.getInt64Ty());

  FunctionCallee CheckUpper = F.getParent()->getOrInsertFunction(
      CHECK_UB, Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
      IRB.getPtrTy(), IRB.getInt64Ty());

  auto getOrEvaluateSubExpr = [&](Value *V) -> std::pair<SubscriptExpr, bool> {
    if (Evaluated.find(V) != Evaluated.end()) {
      return {Evaluated[V], false};
    } else {
      auto SE = SubscriptExpr::evaluate(V);
      Evaluated[V] = SE;
      return {SE, true};
    }
  };

  for (const auto *V : ValuesReferencedInSubscript) {
    auto &&C_OUT = Grouped_C_OUT[V];
    for (auto &BB : F) {
      if (C_OUT[&BB].isEmpty()) {
        continue;
      }

      C_GEN[V][&BB].addPredicateSet(C_OUT[&BB]); // Update C_GEN

      SmallDenseMap<std::pair<int64_t, int64_t>, Value *> ReusableValue{};

      bool hasUpperBound = false;
      bool hasLowerBound = false;

      for (auto &Inst : BB.getInstList()) {
        if (isa<CallInst>(Inst)) {
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();
          auto FName = F->getName();

          if (FName != CHECK_UB && FName != CHECK_LB)
            continue;

          Value *checked = CB->getArgOperand(1);
          auto &&SE = getOrEvaluateSubExpr(checked);

          if (SE.first.i != V)
            continue;

          if (FName == CHECK_UB) {
            auto BSE = getOrEvaluateSubExpr(CB->getArgOperand(0));
            auto UBP = UpperBoundPredicate{BSE.first, SE.first};
            UBP.normalize();

            // if C_OUT subsumes UBP, we can narrow down the bound

            hasUpperBound = true;

            auto tighterBoundIfExists =
                llvm::find_if(C_OUT[&BB].UbPredicates,
                              [&](const auto &It) { return It.subsumes(UBP); });

            if (tighterBoundIfExists != C_OUT[&BB].UbPredicates.end()) {
              auto tighterBound = *tighterBoundIfExists;
              auto constantDiffOfIndex =
                  tighterBound.Index.getConstantDifference(UBP.Index);
              auto constantDiffOfBound =
                  tighterBound.Bound.getConstantDifference(UBP.Bound);

              // always reuse the index value
              constantDiffOfBound -= constantDiffOfIndex;

              if (constantDiffOfBound != 0) {

                VERBOSE_PRINT {
                  llvm::errs() << "Modify check based on tighter C_OUT: \n\t";
                  CB->print(llvm::errs());
                  llvm::errs() << " (";
                  UBP.print(llvm::errs());
                  llvm::errs() << ")\n";
                }

                IRB.SetInsertPoint(CB);
                Value *newBoundValue = IRB.CreateAdd(
                    CB->getArgOperand(0), IRB.getInt64(constantDiffOfBound));
                CB->setArgOperand(0, newBoundValue);

                VERBOSE_PRINT {
                  YELLOW(llvm::errs()) << "\t  => ";
                  CB->print(llvm::errs());
                  llvm::errs() << " (";
                  tighterBound.print(llvm::errs());
                  llvm::errs() << ")\n";
                }
              }
            }

          } else {
            auto BSE = getOrEvaluateSubExpr(CB->getArgOperand(0));
            auto LBP = LowerBoundPredicate{BSE.first, SE.first};
            LBP.normalize();

            hasLowerBound = true;

            auto tighterBoundIfExists =
                llvm::find_if(C_OUT[&BB].LbPredicates,
                              [&](const auto &It) { return It.subsumes(LBP); });

            if (tighterBoundIfExists != C_OUT[&BB].LbPredicates.end()) {
              auto &tighterBound = *tighterBoundIfExists;
              auto constantDiffOfIndex =
                  tighterBound.Index.getConstantDifference(LBP.Index);
              auto constantDiffOfBound =
                  tighterBound.Bound.getConstantDifference(LBP.Bound);

              constantDiffOfIndex -= constantDiffOfBound;

              // always reuse the index value

              if (constantDiffOfBound != 0) {
                VERBOSE_PRINT {
                  llvm::errs() << "Modify check based on tighter C_OUT: \n\t";
                  CB->print(llvm::errs());
                  llvm::errs() << " (";
                  LBP.print(llvm::errs());
                  llvm::errs() << ")\n";
                }

                IRB.SetInsertPoint(CB);
                Value *newBoundValue = IRB.CreateAdd(
                    CB->getArgOperand(0), IRB.getInt64(constantDiffOfBound));
                CB->setArgOperand(0, newBoundValue);

                VERBOSE_PRINT {
                  YELLOW(llvm::errs()) << "\t  => ";
                  CB->print(llvm::errs());
                  llvm::errs() << " (";
                  tighterBound.print(llvm::errs());
                  llvm::errs() << ")\n";
                }
              }
            }
          }
        }
      }

      auto *trailingInsertPoint = BB.getTerminator(); //->getPrevNode();

      VERBOSE_PRINT {
        if ((hasUpperBound || hasLowerBound) && trailingInsertPoint) {

          llvm::errs() << "Modify check because this block has no checks but "
                          "C_OUT has some\n\t";
          trailingInsertPoint->print(llvm::errs());
          llvm::errs() << "\n";
          // if (llvm::any_of(BB,
          //                  [&](Instruction &I) { return isa<PHINode>(I); }))
          //                  {
          //   llvm::errs() << "PHI NODES\n";
          //   BB.print(llvm::errs());
          // }
          llvm::errs() << "\n";
          llvm::errs() << "\n";
        }
      }
      if (trailingInsertPoint) {
        if (!hasUpperBound) {
          for (auto &UBP : C_OUT[&BB].UbPredicates) {
            if (!DTA.dominates((UBP.Bound.i), trailingInsertPoint) ||
                !DTA.dominates((UBP.Index.i), trailingInsertPoint)) {
              continue;
            }
            Value *bound =
                createValueForSubExpr(IRB, trailingInsertPoint, UBP.Bound);
            Value *subscript =
                createValueForSubExpr(IRB, trailingInsertPoint, (UBP.Index));
            createCheckCall(IRB, trailingInsertPoint, CheckUpper, bound,
                            subscript, file);
          }
        }
        if (!hasLowerBound) {
          for (auto &LBP : C_OUT[&BB].LbPredicates) {
            if (!DTA.dominates((LBP.Bound.i), trailingInsertPoint) ||
                !DTA.dominates((LBP.Index.i), trailingInsertPoint)) {
              continue;
            }
            Value *bound =
                createValueForSubExpr(IRB, trailingInsertPoint, LBP.Bound);
            Value *subscript =
                createValueForSubExpr(IRB, trailingInsertPoint, (LBP.Index));
            createCheckCall(IRB, trailingInsertPoint, CheckLower, bound,
                            subscript, file);
          }
        }
      }

      // const auto reuseOrCreate = [&](const SubscriptExpr &SE) -> Value * {
      //   auto Iter = ReusableValue.find({SE.A, SE.B});
      //   if (Iter == ReusableValue.end()) {
      //     return createValueForSubExpr(IRB, firstOldCheckInst, SE);
      //   } else {
      //     return Iter->second;
      //   }
      // };

      // for (auto *CI : CI) {
      //   // remove all single use
      //   // SmallVector<Instruction> WorkList{};
      //   // TODO: DFS to remove all single use
      //   // for (auto& U : CI->uses()) {
      //   //   if (U->hasOneUser() && isa<Instruction>(U)) {
      //   //     cast<Instruction>(U)->eraseFromParent();
      //   //   }
      //   // }
      //   CI->eraseFromParent();
      // }
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

  BLUE(llvm::errs())
      << "===================== Apply Elimination ===================== \n";

  SmallVector<CallInst *, 32> RedundantCheck = {};
  for (const auto *V : ValuesReferencedInSubscript) {
    auto &&C_IN = Grouped_C_IN[V];
    for (auto &BB : F) {
      if (C_IN[&BB].isEmpty()) {
        continue;
      }

      for (auto &Inst : BB.getInstList()) {
        if (isa<CallInst>(Inst)) {
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();
          auto FName = F->getName();

          if (FName == CHECK_LB) {
            EXTRACT_VALUE {
              auto LBP = LowerBoundPredicate{BoundExpr, IndexExpr};
              LBP.normalize();
              LBP.print(llvm::errs(), true);

              if (llvm::any_of(C_IN[&BB].LbPredicates,
                               [&](const LowerBoundPredicate &p) {
                                 return p.subsumes(LBP);
                               })) {
                VERBOSE_PRINT {
                  llvm::errs() << "Redundant check at ";
                  BB.printAsOperand(errs());
                  llvm::errs() << " : ";
                  LBP.print(errs());
                  llvm::errs() << "\t(";
                  Inst.print(llvm::errs());
                  llvm::errs() << ")\n";
                }

                RedundantCheck.push_back(CB);
              }
            }
          } else if (FName == CHECK_UB) {
            EXTRACT_VALUE {
              auto UBP = UpperBoundPredicate{BoundExpr, IndexExpr};
              UBP.normalize();
              UBP.print(llvm::errs(), true);

              if (llvm::any_of(C_IN[&BB].UbPredicates,
                               [&](const UpperBoundPredicate &p) {
                                 return p.subsumes(UBP);
                               })) {
                VERBOSE_PRINT {
                  llvm::errs() << "Redundant check at ";
                  BB.printAsOperand(errs());
                  llvm::errs() << " : ";
                  UBP.print(errs());
                  llvm::errs() << "\t(";
                  Inst.getDebugLoc().print(llvm::errs());
                  Inst.print(llvm::errs());
                  llvm::errs() << ")\n";
                }
                RedundantCheck.push_back(CB);
              }
            }
          }
        }
      }
    }
  }
  for (auto *CI : RedundantCheck) {
    Value *BoundValue = CI->getArgOperand(0);
    Value *IndexValue = CI->getArgOperand(1);
    CI->eraseFromParent();
    RecurrsivelyClearAllInstructionsUsedOnlyBy(BoundValue);
    RecurrsivelyClearAllInstructionsUsedOnlyBy(IndexValue);
  }
#undef EXTRACT_VALUE
}

enum CandidateKind {
  NotCandidate,
  Invariant,
  IncreasingValuesWithLB,
  DecreasingValuesWithUB,
  LoopsWithDeltaOne,
};

// Step 1: Identify candidates for propagation
auto IdentifyCandidatesForPropagation(EffectMap &Effects, Loop *L,
                                      CallInst *CheckCall) -> CandidateKind {
  Value *bound = CheckCall->getArgOperand(0);
  Value *subscript = CheckCall->getArgOperand(1);

  // (i) Invariants.
  if (L->isLoopInvariant(subscript)) {
    YELLOW(llvm::errs()) << "self invariant";
    return CandidateKind::Invariant;
  }
  const SubscriptExpr CandidateSE = SubscriptExpr::evaluate(subscript);
  if (CandidateSE.isConstant()) {
    YELLOW(llvm::errs()) << "self constant";
    return CandidateKind::Invariant;
  }
  /** ->isLoopInvariant actually return true for a mutated pointer! */

  // for (ii)/(iii)/(iv) there must be an effect on i
  auto EffectsOnDependencyIter = Effects.find(CandidateSE.i);
  if (EffectsOnDependencyIter == Effects.end()) {
    return CandidateKind::Invariant;
  }

  auto &EffectsOnDependency = EffectsOnDependencyIter->second;
  auto FName = CheckCall->getCalledFunction()->getName();

  // (iv) check loop with inc/decrement of one first
  // otherwise we fall into (ii)/(iii)
  {
    if (llvm::all_of(L->getBlocks(), [&](auto *BB) {
          return llvm::all_of(EffectsOnDependency[BB], [&](auto &SE) {
            return SE.A == 1 && (SE.B == 1 || SE.B == -1);
          });
        })) {
      return CandidateKind::LoopsWithDeltaOne;
    }
  }

  // (ii) Increasing values
  if (FName == CHECK_LB) {
    if (llvm::all_of(L->getBlocks(), [&](auto *BB) {
          return llvm::all_of(EffectsOnDependency[BB], [&](auto &SE) {
            // assert(SE.i == CandidateSE.i);
            // i <- i + c, i <- c + i
            if (SE.A == 1 && SE.B >= 0) {
              return true;
            }
            // i <- c * i
            if (SE.A >= 1 && SE.B == 0) {
              return true;
            }
            return false;
          });
        })) {
      return CandidateKind::IncreasingValuesWithLB;
    }
  }

  // (iii) Decreasing values
  if (FName == CHECK_UB) {
    bool hasNoneOneDelta = false;
    if (llvm::all_of(L->getBlocks(), [&](auto *BB) {
          return llvm::all_of(EffectsOnDependency[BB], [&](auto &SE) {
            // assert(SE.i == CandidateSE.i);
            if (SE.A == 1 && SE.B <= 0) {
              return true;
            }
            return false;
          });
        })) {
      return CandidateKind::DecreasingValuesWithUB;
    }
  }

  return CandidateKind::NotCandidate;
};

void LoopCheckPropagation(Function &F,
                          ValuePtrVector &ValuesReferencedInSubscript,
                          Constant *file, EffectMap &Effects, LoopInfo &LI,
                          DominatorTree &DT) {
  VERBOSE_PRINT {
    BLUE(llvm::errs()) << "===================== Loop Check Propagation "
                          "===================== \n";
  }

  IRBuilder<> IRB(F.getEntryBlock().getFirstNonPHI());
  AttributeList Attr;
  FunctionCallee CheckLower = F.getParent()->getOrInsertFunction(
      CHECK_LB, Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
      IRB.getPtrTy(), IRB.getInt64Ty());

  FunctionCallee CheckUpper = F.getParent()->getOrInsertFunction(
      CHECK_UB, Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
      IRB.getPtrTy(), IRB.getInt64Ty());

  for (auto *ValueWeCareAbout : ValuesReferencedInSubscript) {
    VERBOSE_PRINT {
      YELLOW(llvm::errs()) << "=========== Checking for Subscript Value ";
      ValueWeCareAbout->printAsOperand(llvm::errs());
      YELLOW(llvm::errs()) << " ===========\n";
    }

    // Step 2: Check hoisting.
    for (auto *L : LI) {
      SmallVector<BasicBlock *> exitBlocks;
      L->getExitBlocks(exitBlocks);

      SmallPtrSet<BasicBlock *, 32> ND{};
      // SmallDenseMap<BasicBlock *, SmallVector<CallInst *>> C_n{};

      for (auto *BB : L->blocks()) {
        if (llvm::any_of(exitBlocks, [&](auto *exitBlock) {
              return !DT.dominates(BB, exitBlock);
            })) {
          ND.insert(BB);
        }
      }

      auto getCnFor = [&](BasicBlock *BB) -> SmallVector<CallInst *> {
        auto Result = SmallVector<CallInst *>{};
        /** for eachblock n do
            C(n) = (c: at the entry to n we can asser that candidate check c
            will be executed in n od
         */
        for (auto &Inst : *BB) {
          if (!isa<CallInst>(Inst))
            continue;
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();
          auto FName = F->getName();
          if (FName != CHECK_UB && FName != CHECK_LB)
            continue;

          SubscriptExpr CandidateSE =
              SubscriptExpr::evaluate(CB->getArgOperand(1));
          if (CandidateSE.i != ValueWeCareAbout)
            continue;

          auto candidateKind = IdentifyCandidatesForPropagation(Effects, L, CB);
          if (candidateKind != CandidateKind::NotCandidate) {
            CB->print(llvm::errs());
            llvm::errs() << "\n";
            Result.push_back(CB);
          }
        }

        VERBOSE_PRINT {
          llvm::errs() << "C(n) for";
          BB->printAsOperand(llvm::errs());
          llvm::errs() << " : \n";
          for (auto *CB : Result) {
            CB->print(llvm::errs());
            llvm::errs() << "\n";
          }
          llvm::errs() << "\n";
        }
        return Result;
      };

      VERBOSE_PRINT {
        llvm::errs() << "ND for ";
        L->print(llvm::errs());
        llvm::errs() << " { ";
        for (auto *BB : ND) {
          BB->printAsOperand(llvm::errs());
          llvm::errs() << ", ";
        }
        llvm::errs() << " }\n";
      }

      bool change = true;
      while (change) {
        change = false;
        for (auto *n : L->blocks()) {

          SmallVector<BasicBlock *> SuccInsideLoop{};

          for (auto *Succ : successors(n)) {
            if (L->contains(Succ)) {
              SuccInsideLoop.push_back(Succ);
            }
          }

          // Succ & ND != empty
          if (!llvm::any_of(SuccInsideLoop,
                            [&](auto *Succ) { return ND.contains(Succ); })) {
            continue;
          }

          // all successors of n have a single predecessor n
          if (!llvm::all_of(SuccInsideLoop, [&](auto *Succ) {
                return Succ->getSinglePredecessor() == n;
              })) {
            continue;
          }

          VERBOSE_PRINT {
            llvm::errs() << "Hoisting for ";
            n->printAsOperand(llvm::errs());
            llvm::errs() << "\n";
          }

          SmallVector<BoundPredicateSet, 4> C_Ss;
          for (auto *Succ : SuccInsideLoop) {
            VERBOSE_PRINT {
              llvm::errs() << "> Succ of ";
              n->printAsOperand(llvm::errs());
              llvm::errs() << " : ";
              Succ->printAsOperand(llvm::errs());
              llvm::errs() << "\n";
            }
            BoundPredicateSet C_S = {};
            for (auto *CB : getCnFor(Succ)) {
              SubscriptExpr CandidateSE =
                  SubscriptExpr::evaluate(CB->getArgOperand(1));
              SubscriptExpr BoundSE =
                  SubscriptExpr::evaluate(CB->getArgOperand(0));

              assert(CandidateSE.i == ValueWeCareAbout);

              if (CB->getCalledFunction()->getName() == CHECK_LB) {
                C_S.LbPredicates.push_back(
                    LowerBoundPredicate{BoundSE, CandidateSE});
              } else {
                C_S.UbPredicates.push_back(
                    UpperBoundPredicate{BoundSE, CandidateSE});
              }
            }

            C_S.print(llvm::errs());
            C_Ss.push_back(C_S);
          }
          BoundPredicateSet prop = BoundPredicateSet::And(C_Ss);

          if (prop.isEmpty()) {
            continue;
          }

          llvm::errs() << "Propagating prop ";
          prop.print(llvm::errs());
          llvm::errs() << " to n ";
          n->printAsOperand(llvm::errs());
          llvm::errs() << "\n";

          // hoist checks in prop to n
          auto *InsertPoint = n->getTerminator();

          if (DT.dominates(prop.getSubscriptIdentity()->second, InsertPoint)) {
            change = true;
            IRB.SetInsertPoint(InsertPoint);
            for (auto &LBP : prop.LbPredicates) {
              Value *bound = createValueForSubExpr(IRB, InsertPoint, LBP.Bound);
              Value *subscript =
                  createValueForSubExpr(IRB, InsertPoint, LBP.Index);
              createCheckCall(IRB, InsertPoint, CheckLower, bound, subscript,
                              file);
            }
            for (auto &UBP : prop.UbPredicates) {
              Value *bound = createValueForSubExpr(IRB, InsertPoint, UBP.Bound);
              Value *subscript =
                  createValueForSubExpr(IRB, InsertPoint, UBP.Index);
              createCheckCall(IRB, InsertPoint, CheckUpper, bound, subscript,
                              file);
            }

            // remove checks from successors
            // TODO: clean unused values
            for (auto *Succ : SuccInsideLoop) {
              for (auto *CB : getCnFor(Succ)) {
                Value *Bound = CB->getArgOperand(0);
                Value *Index = CB->getArgOperand(1);
                CB->eraseFromParent();
                RecurrsivelyClearAllInstructionsUsedOnlyBy(Bound);
                RecurrsivelyClearAllInstructionsUsedOnlyBy(Index);
              }
            }
          }
        }
      }

      // Step 3: Propagate checks out of the loop.
      // Propagate all candidate checks from blocks that dominate all loop
      // exits. In accordance with the rules described in Step 1, some checks
      // are modified in this process, whereas others are propagated unchanged.
      for (auto *BlockThatDominatesAllExits : L->getBlocks()) {
        if (!llvm::all_of(exitBlocks, [&](auto *exitBlock) {
              return DT.dominates(BlockThatDominatesAllExits, exitBlock);
            })) {
          continue;
        }

        VERBOSE_PRINT {
          YELLOW(llvm::errs())
              << "================== Step3: Block That Dominates All Exits ";
          BlockThatDominatesAllExits->printAsOperand(llvm::errs());
          YELLOW(llvm::errs()) << " ================== \n";
        }
        SmallVector<CallInst *, 4> obsoleteChecks{};
        for (auto &Inst : *BlockThatDominatesAllExits) {
          if (!isa<CallInst>(Inst))
            continue;
          auto CB = cast<CallInst>(&Inst);
          auto F = CB->getCalledFunction();
          auto FName = F->getName();
          if (FName != CHECK_UB && FName != CHECK_LB)
            continue;

          SubscriptExpr CandidateSE =
              SubscriptExpr::evaluate(CB->getArgOperand(1));
          if (CandidateSE.i != ValueWeCareAbout)
            continue;

          auto candidateKind = IdentifyCandidatesForPropagation(Effects, L, CB);
          if (candidateKind == CandidateKind::NotCandidate) {
            continue;
          }

          if (candidateKind == CandidateKind::LoopsWithDeltaOne) {
            // replaced by the check “lb < min op c, max op c < ub” outside the
            // loop.
            llvm::errs() << "--- Replace by the check “lb < min op c, max op c "
                            "< ub” outside the loop.\n";
            auto *Term = BlockThatDominatesAllExits->getTerminator();
            if (isa<BranchInst>(Term)) {
              auto *BI = cast<BranchInst>(Term);
              if (BI->isConditional()) {
                auto *Cond = BI->getCondition();
                auto *BBTrue = BI->getSuccessor(0);
                auto *BBFalse = BI->getSuccessor(1);

                if (isa<ICmpInst>(Cond)) {
                  auto *ICmp = cast<ICmpInst>(Cond);
                  auto ICmpKind = ICmp->getPredicate();
                  auto lhsVal = ICmp->getOperand(0);
                  auto rhsVal = ICmp->getOperand(1);

                  auto lhsSubExpr = SubscriptExpr::evaluate(lhsVal);
                  auto rhsSubExpr = SubscriptExpr::evaluate(rhsVal);

                  bool lhsIsSubscript = lhsSubExpr.i == ValueWeCareAbout;
                  bool rhsIsSubscript = rhsSubExpr.i == ValueWeCareAbout;

                  if (!lhsIsSubscript && !rhsIsSubscript)
                    continue;

                  enum class BoundKind : bool {
                    MAX,
                    MIN,
                  };

                  BoundKind BK = BoundKind::MAX;

                  switch (ICmpKind) {
                  case CmpInst::Predicate::ICMP_SGT:
                  case CmpInst::Predicate::ICMP_UGT:
                    // a>b   =>     a >= b + 1
                    rhsSubExpr.B += 1;
                  case CmpInst::Predicate::ICMP_SGE:
                  case CmpInst::Predicate::ICMP_UGE:
                    if (lhsIsSubscript) {
                      BK = BoundKind::MIN;
                    } else {
                      BK = BoundKind::MAX;
                    }
                    break;

                  case CmpInst::Predicate::ICMP_SLT:
                  case CmpInst::Predicate::ICMP_ULT:
                    // a<b   =>     a <= b - 1
                    rhsSubExpr.B -= 1;
                  case CmpInst::Predicate::ICMP_SLE:
                  case CmpInst::Predicate::ICMP_ULE:
                    if (lhsIsSubscript) {
                      BK = BoundKind::MAX;
                    } else {
                      BK = BoundKind::MIN;
                    }
                    break;

                  default:
                    llvm_unreachable("Unimplemented ICmpKind!!");
                  }

                  SubscriptExpr Subscript =
                      lhsIsSubscript ? lhsSubExpr : rhsSubExpr;
                  SubscriptExpr Bound =
                      lhsIsSubscript ? rhsSubExpr : lhsSubExpr;

                  auto IDom = DT.getNode(BlockThatDominatesAllExits)
                                  ->getIDom()
                                  ->getBlock();

                  assert(DT.dominates(Subscript.i, IDom->getTerminator()));
                  assert(DT.dominates(Bound.i, IDom->getTerminator()));

                  IRB.SetInsertPoint(IDom->getTerminator());

                  if (BK == BoundKind::MAX && FName == CHECK_UB) {
                    createCheckCall(IRB, IDom->getTerminator(), CheckUpper,
                                    createValueForSubExpr(
                                        IRB, IDom->getTerminator(), Bound),
                                    createValueForSubExpr(
                                        IRB, IDom->getTerminator(), Subscript),
                                    file);
                  } else if (BK == BoundKind::MIN && FName == CHECK_LB) {
                    createCheckCall(IRB, IDom->getTerminator(), CheckLower,
                                    createValueForSubExpr(
                                        IRB, IDom->getTerminator(), Bound),
                                    createValueForSubExpr(
                                        IRB, IDom->getTerminator(), Subscript),
                                    file);
                  } else {
                    // just move the check unchanged
                  }
                }
              }
            }
          } else {
            // just propagate the check
            llvm::errs() << "--- Hoisting without modification.\n";
          }

          obsoleteChecks.push_back(CB);
        }
        for (auto *CB : obsoleteChecks) {
          // cleanup
          Value *Bound = CB->getArgOperand(0);
          Value *Index = CB->getArgOperand(1);
          CB->eraseFromParent();
          RecurrsivelyClearAllInstructionsUsedOnlyBy(Bound);
          RecurrsivelyClearAllInstructionsUsedOnlyBy(Index);
        }
      }

    } // end of loop
  }   // end of value
};

PreservedAnalyses BoundCheckOptimization::run(Function &F,
                                              FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }

  llvm::errs() << "BoundCheckOptimization\n";

  auto &Context = F.getContext();
  auto InsertPoint = F.getEntryBlock().getFirstNonPHI();
  IRBuilder<> IRB(InsertPoint);
  auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  auto &LI = FAM.getResult<LoopAnalysis>(F);
  SourceFileName = F.getParent()->getNamedGlobal(SOURCE_FILE_NAME);

  CMap C_GEN{};
  EffectMap Effects{};
  ValueEvaluationCache Evaluated{};
  ValuePtrVector ValuesReferencedInSubscript = {};
  ValuePtrVector ValuesReferencedInBound = {};

  /** Compute C_GEN, Effects, ValuesReferencedInSubscript,
   * ValuesReferencedInBound */
  ComputeEffects(F, C_GEN, Effects, ValuesReferencedInSubscript,
                 ValuesReferencedInBound, Evaluated, SourceFileName);

  /** Modification Analysis */
  if (MODIFICATION) {
    CMap C_IN{};
    CMap C_OUT{};
    InitializeToEmpty(F, C_IN, ValuesReferencedInSubscript);
    InitializeToEmpty(F, C_OUT, ValuesReferencedInSubscript);

    RunModificationAnalysis(F, C_IN, C_OUT, C_GEN, Effects,
                            ValuesReferencedInSubscript);

    ApplyModification(F, C_OUT, C_GEN, ValuesReferencedInSubscript, Evaluated,
                      SourceFileName, DT);
  }

  { /** Update C_GEN */
    C_GEN.clear();
    RecomputeC_GEN(F, C_GEN, ValuesReferencedInSubscript, Evaluated);
  }

  /** Elimination Analysis */
  if (ELIMINATION) {
    CMap C_IN{};
    CMap C_OUT{};
    InitializeToEmpty(F, C_IN, ValuesReferencedInSubscript);
    InitializeToEmpty(F, C_OUT, ValuesReferencedInSubscript);

    RunEliminationAnalysis(F, C_IN, C_OUT, C_GEN, Effects,
                           ValuesReferencedInSubscript);

    ApplyElimination(F, C_IN, C_GEN, ValuesReferencedInSubscript);
  }

  if (LOOP_PROPAGATION) {
    LoopCheckPropagation(F, ValuesReferencedInSubscript, SourceFileName,
                         Effects, LI, DT);
  }

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}