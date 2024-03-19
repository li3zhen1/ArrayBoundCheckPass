#include "BoundCheckOptimization.h"
#include "BoundPredicate.h"
#include "BoundPredicateSet.h"
#include "CommonDef.h"
#include "Effect.h"
#include "SubscriptExpr.h"
#include "llvm/IR/Dominators.h"
#include <utility>

using namespace llvm;

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
        V = IRB.CreateLoad(VTy, V);
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
          //                  [&](Instruction &I) { return isa<PHINode>(I); })) {
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

  SmallVector<Instruction *, 32> RedundantCheck = {};
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

                RedundantCheck.push_back(&Inst);
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
                RedundantCheck.push_back(&Inst);
              }
            }
          }
        }
      }
    }
  }
  for (auto *RC : RedundantCheck) {
    RC->eraseFromParent();
  }
#undef EXTRACT_VALUE
}

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
  SourceFileName = F.getParent()->getNamedGlobal(SOURCE_FILE_NAME);

  CMap C_GEN{};
  EffectMap Effects{};
  ValueEvaluationCache Evaluated{};
  ValuePtrVector ValuesReferencedInSubscript = {};
  ValuePtrVector ValuesReferencedInBound = {};


  /** Compute C_GEN, Effects, ValuesReferencedInSubscript, ValuesReferencedInBound */
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

  return PreservedAnalyses::none();
}

BoundCheckOptimization::~BoundCheckOptimization() {}