#include "BoundPredicateSet.h"
#include "BoundPredicate.h"
#include "CommonDef.h"
#include "SubscriptExpr.h"
// #include <variant>

void BoundPredicateSet::addPredicate(UpperBoundPredicate &P) {
  P.normalize();
  assert(P.isNormalized() && "Predicate not normalized!");
  auto PID = P.Index.getIdentity();
  if (const auto ID = getSubscriptIdentity()) {
    if (PID != ID.value()) {
      print(BLUE(llvm::errs()));
      llvm::errs() << " <=> ";
      P.print(BLUE(llvm::errs()));
    }
    assert(PID == ID.value() && "Identity not match!");
  }
  const auto &ComparableBoundPredict =
      llvm::find_if(UbPredicates, [&](const auto &It) {
        return It.Bound.getIdentity() == P.Bound.getIdentity();
      });
  if (ComparableBoundPredict != UbPredicates.end()) {
    ComparableBoundPredict->Bound.B =
        std::min(ComparableBoundPredict->Bound.B, P.Bound.B);
  } else {
    UbPredicates.push_back(P);
  }
}

void BoundPredicateSet::addPredicate(UpperBoundPredicate &&P) {
  addPredicate(P);
}

void BoundPredicateSet::addPredicate(LowerBoundPredicate &P) {
  P.normalize();
  assert(P.isNormalized() && "Predicate not normalized!");
  auto PID = P.Index.getIdentity();
  if (const auto ID = getSubscriptIdentity()) {
    if (PID != ID.value()) {
      print(BLUE(llvm::errs()));
      llvm::errs() << " <=> ";
      P.print(BLUE(llvm::errs()));
    }
    assert(PID == ID.value() && "Identity not match!");
  }
  const auto &ComparableBoundPredict =
      llvm::find_if(LbPredicates, [&](const auto &It) {
        return It.Bound.getIdentity() == P.Bound.getIdentity();
      });
  if (ComparableBoundPredict != LbPredicates.end()) {
    ComparableBoundPredict->Bound.B =
        std::max(ComparableBoundPredict->Bound.B, P.Bound.B);
  } else {
    LbPredicates.push_back(P);
  }
}

void BoundPredicateSet::addPredicate(LowerBoundPredicate &&P) {
  addPredicate(P);
}

void BoundPredicateSet::addPredicateSet(BoundPredicateSet &P) {
  for (auto &It : P.LbPredicates) {
    addPredicate(It);
  }
  for (auto &It : P.UbPredicates) {
    addPredicate(It);
  }
}

void BoundPredicateSet::addPredicate(BoundPredicate &P) {
  if (auto *UB = get_if<UpperBoundPredicate>(&P)) {
    addPredicate(*UB);
  } else if (auto *LB = get_if<LowerBoundPredicate>(&P)) {
    addPredicate(*LB);
  }
}

SmallVector<BoundPredicate> BoundPredicateSet::getAllPredicates() const {
  SmallVector<BoundPredicate> AllPredicates;
  for (const auto &It : LbPredicates) {
    AllPredicates.push_back(It);
  }
  for (const auto &It : UbPredicates) {
    AllPredicates.push_back(It);
  }
  return AllPredicates;
}

optional<SubscriptIndentity> BoundPredicateSet::getSubscriptIdentity() const {
  if (!LbPredicates.empty()) {
    return LbPredicates.begin()->Index.getIdentity();
  }
  if (!UbPredicates.empty()) {
    return UbPredicates.begin()->Index.getIdentity();
  }
  return nullopt;
}

LowerBoundPredicate *
findFirstMergablePredicate(SmallVector<LowerBoundPredicate> &S,
                           const SubscriptIndentity ID) {
  const auto Ptr = llvm::find_if(
      S, [&](const auto &It) { return It.Index.getIdentity() == ID; });

  if (Ptr != S.end())
    return Ptr;

  return nullptr;
}

UpperBoundPredicate *
findFirstMergablePredicate(SmallVector<UpperBoundPredicate> &S,
                           const SubscriptIndentity ID) {
  const auto Ptr = llvm::find_if(
      S, [&](const auto &It) { return It.Index.getIdentity() == ID; });

  if (Ptr != S.end())
    return Ptr;

  return nullptr;
}

BoundPredicateSet
BoundPredicateSet::Or(SmallVector<BoundPredicateSet, 4> Sets) {
  BoundPredicateSet Result;

  // there should be no other predicate subsumes the predicates in the result

  for (const auto &S : Sets) {
    for (const auto &LP : S.LbPredicates) {
      if (const auto _LP = findFirstMergablePredicate(Result.LbPredicates,
                                                      LP.Index.getIdentity())) {
        _LP->Bound.B = std::max(_LP->Bound.B, LP.Bound.B);
      } else {
        Result.LbPredicates.push_back(LP);
      }
    }
    for (const auto &UP : S.UbPredicates) {
      if (const auto _UP = findFirstMergablePredicate(Result.UbPredicates,
                                                      UP.Index.getIdentity())) {
        _UP->Bound.B = std::min(_UP->Bound.B, UP.Bound.B);
      } else {
        Result.UbPredicates.push_back(UP);
      }
    }
  }

  // Result.print(errs());

  return Result;
}

BoundPredicateSet
BoundPredicateSet::And(SmallVector<BoundPredicateSet, 4> Sets) {
  BoundPredicateSet Result;

  bool isUbOpen = false;
  bool isLbOpen = false;

  // for (const auto &S_i : Sets) {
  //   S_i.print(errs());
  //   llvm::errs() << " AND ";
  // }
  // llvm::errs() << " = ";

  for (const auto &S : Sets) {
    if (S.LbPredicates.empty()) {
      isLbOpen = true;
    }
    for (const auto &LP : S.LbPredicates) {
      if (const auto _LP = findFirstMergablePredicate(Result.LbPredicates,
                                                      LP.Index.getIdentity())) {
        _LP->Bound.B = std::min(_LP->Bound.B, LP.Bound.B);
      } else {
        if (!Result.LbPredicates.empty()) {
          llvm_unreachable("Incomparable LB predicates");
          YELLOW(llvm::errs()) << "Meet incomparable predicates: ";
          LP.print(YELLOW(llvm::errs()));
          llvm::errs() << "\n";
          YELLOW(llvm::errs()) << "Ignoring both";
          isLbOpen = true;
        } else {
          Result.LbPredicates.push_back(LP);
        }
      }
    }

    if (S.UbPredicates.empty()) {
      isUbOpen = true;
    }
    for (const auto &UP : S.UbPredicates) {
      if (const auto _UP = findFirstMergablePredicate(Result.UbPredicates,
                                                      UP.Index.getIdentity())) {
        _UP->Bound.B = std::max(_UP->Bound.B, UP.Bound.B);
      } else {
        if (!Result.UbPredicates.empty()) {
          llvm_unreachable("Incomparable UB predicates");
          YELLOW(llvm::errs()) << "Meet incomparable predicates: ";
          UP.print(YELLOW(llvm::errs()));
          llvm::errs() << "\n";
          YELLOW(llvm::errs()) << "Ignoring both";
          isLbOpen = true;
        } else {
          Result.UbPredicates.push_back(UP);
        }
      }
    }
  }

  if (isUbOpen) {
    Result.UbPredicates.clear();
  }

  if (isLbOpen) {
    Result.LbPredicates.clear();
  }

  return Result;
}

void BoundPredicateSet::print(raw_ostream &O) const {
  if (LbPredicates.size() == 1 && UbPredicates.size() == 1 &&
      LbPredicates.front().Index == UbPredicates.front().Index) {
    LbPredicates.front().print(O);
    O << " ≤ ";
    UbPredicates.front().Bound.dump(GreenO);
    O << "\n";
    return;
  }

  for (const auto &It : LbPredicates) {
    It.print(O);
  }
  errs() << "\n";
  for (const auto &It : UbPredicates) {
    O << "\t";
    It.print(O);
  }
  errs() << "\n";
}

bool BoundPredicateSet::isIdentityCheck() const {
  if (!LbPredicates.empty()) {
    const auto LP = LbPredicates.front();
    return LP.Index.A == 1 && LP.Index.B == 0;
  }
  if (!UbPredicates.empty()) {
    const auto UP = UbPredicates.front();
    return UP.Index.A == 1 && UP.Index.B == 0;
  }
  llvm_unreachable("Empty BoundPredicateSet!");
}

BoundPredicate BoundPredicateSet::getFirstItem() const {
  if (!LbPredicates.empty()) {
    return LbPredicates.front();
  }
  if (!UbPredicates.empty()) {
    return UbPredicates.front();
  }
  llvm_unreachable("Empty BoundPredicateSet!");
}

bool BoundPredicateSet::operator==(const BoundPredicateSet &Other) const {
  bool IsEqual = true;
  if (LbPredicates.size() != Other.LbPredicates.size() ||
      UbPredicates.size() != Other.UbPredicates.size()) {
    return false;
  }

  for (const auto &It : LbPredicates) {
    IsEqual &= llvm::find(Other.LbPredicates, It) != Other.LbPredicates.end();
  }

  for (const auto &It : UbPredicates) {
    IsEqual &= llvm::find(Other.UbPredicates, It) != Other.UbPredicates.end();
  }

  return IsEqual;
}

bool BoundPredicateSet::isEmpty() const {
  return LbPredicates.empty() && UbPredicates.empty();
}

bool BoundPredicateSet::subsumes(const LowerBoundPredicate &Other) const {
  return llvm::any_of(LbPredicates,
                      [&](const auto &It) { return It.subsumes(Other); });
}

bool BoundPredicateSet::subsumes(const UpperBoundPredicate &Other) const {

  return llvm::any_of(UbPredicates,
                      [&](const auto &It) { return It.subsumes(Other); });
}

void print(CMap &C, raw_ostream &O, const ValuePtrVector &ValueKeys) {

  for (const auto *V : ValueKeys) {
    O << "----------------- Value: ";
    V->printAsOperand(O);
    O << "----------------- \n";
    for (const auto &BB2SE : C[V]) {
      if (BB2SE.second.isEmpty()) {
        continue;
      }
      BB2SE.first->printAsOperand(O);
      O << "\n";
      if (!BB2SE.second.isEmpty()) {
        BB2SE.second.print(O);
      }
    }
    O << "\n";
  }
}

void InitializeToEmpty(Function &F, CMap &C, const ValuePtrVector &ValueKeys) {
  for (const auto *V : ValueKeys) {
    C[V] = DenseMap<const BasicBlock *, BoundPredicateSet>{};
    for (const auto &BB : F) {
      C[V][&BB] = {};
    }
  }
}