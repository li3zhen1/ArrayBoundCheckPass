#include "BoundPredicateSet.h"
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

BoundPredicateSet
BoundPredicateSet::Or(std::initializer_list<const BoundPredicateSet> Sets) {
  BoundPredicateSet Result;

  for (const auto &S : Sets) {
    for (const auto &LP : S.LbPredicates) {
      if (Result.LbPredicates.empty()) {
        Result.LbPredicates.push_back(LP);
      } else {
      }
    }
    for (const auto &It : S.UbPredicates) {
      if (Result.UbPredicates.empty()) {
        Result.UbPredicates.push_back(It);
      } else {
      }
    }
  }
  return Result;
}

BoundPredicateSet
BoundPredicateSet::And(std::initializer_list<const BoundPredicateSet> Sets) {
  BoundPredicateSet Result;
  for (const auto &S : Sets) {
    for (const auto &LP : S.LbPredicates) {
      if (Result.LbPredicates.empty()) {
        Result.LbPredicates.push_back(LP);
      } else {
      }
    }
    for (const auto &It : S.UbPredicates) {
      if (Result.UbPredicates.empty()) {
        Result.UbPredicates.push_back(It);
      } else {
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