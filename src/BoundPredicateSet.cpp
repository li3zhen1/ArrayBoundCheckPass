#include "BoundPredicateSet.h"
// #include <variant>

void BoundPredicateSet::addPredicate(const UpperBoundPredicate &P) {
  assert(P.isNormalized() && "Predicate not normalized!");
  auto PI = P.getIdentity();
  if (const auto ID = getIdentity()) {
    assert(PI == ID.value() && "Identity not match!");
  }
  UbPredicates.push_back(P);
}

void BoundPredicateSet::addPredicate(const LowerBoundPredicate &P) {
  assert(P.isNormalized() && "Predicate not normalized!");
  auto PI = P.getIdentity();
  if (const auto ID = getIdentity()) {
    assert(PI == ID.value() && "Identity not match!");
  }
  LbPredicates.push_back(P);
}

void BoundPredicateSet::addPredicate(const BoundPredicate &P) {
  if (const auto *UB = get_if<UpperBoundPredicate>(&P)) {
    addPredicate(*UB);
  } else if (const auto *LB = get_if<LowerBoundPredicate>(&P)) {
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

optional<BoundPredicateIdentity> BoundPredicateSet::getIdentity() const {
  if (!LbPredicates.empty()) {
    return LbPredicates.begin()->getIdentity();
  }
  if (!UbPredicates.empty()) {
    return UbPredicates.begin()->getIdentity();
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
        auto &Last = Result.LbPredicates.back();
        if (LP.getIdentity() == Last.getIdentity()) {
          Last.Bound.B = std::min(Last.Bound.B, LP.Bound.B);
        } else {
          llvm_unreachable("Should not be different identity.");
          Result.LbPredicates.push_back(LP);
        }
      }
    }
    for (const auto &It : S.UbPredicates) {
      if (Result.UbPredicates.empty()) {
        Result.UbPredicates.push_back(It);
      } else {
        auto &Last = Result.UbPredicates.back();
        if (It.getIdentity() == Last.getIdentity()) {
          Last.Bound.B = std::max(Last.Bound.B, It.Bound.B);
        } else {
          llvm_unreachable("Should not be different identity.");
          Result.UbPredicates.push_back(It);
        }
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
        auto &Last = Result.LbPredicates.back();
        if (LP.getIdentity() == Last.getIdentity()) {
          Last.Bound.B = std::max(Last.Bound.B, LP.Bound.B);
        } else {
          llvm_unreachable("Should not be different identity.");
          Result.LbPredicates.push_back(LP);
        }
      }
    }
    for (const auto &It : S.UbPredicates) {
      if (Result.UbPredicates.empty()) {
        Result.UbPredicates.push_back(It);
      } else {
        auto &Last = Result.UbPredicates.back();
        if (It.getIdentity() == Last.getIdentity()) {
          Last.Bound.B = std::min(Last.Bound.B, It.Bound.B);
        } else {
          llvm_unreachable("Should not be different identity.");
          Result.UbPredicates.push_back(It);
        }
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
    It.print(O);
  }
}
