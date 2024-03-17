#include "BoundPredicateSet.h"
#include "BoundPredicate.h"
#include "SubscriptExpr.h"
// #include <variant>

void BoundPredicateSet::addPredicate(UpperBoundPredicate &P) {
  P.normalize();
  assert(P.isNormalized() && "Predicate not normalized!");
  auto PID = P.Index.getIdentity();
  if (const auto ID = getSubscriptIdentity()) {
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

 LowerBoundPredicate * findFirstMergablePredicate(SmallVector<LowerBoundPredicate> &S,
                                const SubscriptIndentity ID) {
  const auto Ptr = llvm::find_if(
      S, [&](const auto &It) { return It.Index.getIdentity() == ID; });

  if (Ptr != S.end())
    return Ptr;

  return nullptr;
}

 UpperBoundPredicate * findFirstMergablePredicate(SmallVector<UpperBoundPredicate> &S,
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
  return Result;
}

BoundPredicateSet
BoundPredicateSet::And(SmallVector<BoundPredicateSet, 4> Sets) {
  BoundPredicateSet Result;
  for (const auto &S : Sets) {
    for (const auto &LP : S.LbPredicates) {
      if (const auto _LP = findFirstMergablePredicate(Result.LbPredicates,
                                                      LP.Index.getIdentity())) {
        _LP->Bound.B = std::min(_LP->Bound.B, LP.Bound.B);
      } else {
        Result.LbPredicates.push_back(LP);
      }
    }
    for (const auto &It : S.UbPredicates) {
      if (const auto _UP = findFirstMergablePredicate(Result.UbPredicates,
                                                      It.Index.getIdentity())) {
        _UP->Bound.B = std::max(_UP->Bound.B, It.Bound.B);
      } else {
        Result.UbPredicates.push_back(It);
      }
    }
  }
  return Result;
}

void BoundPredicateSet::print(raw_ostream &O) const {
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