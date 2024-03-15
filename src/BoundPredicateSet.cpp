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

  return Result;
}

BoundPredicateSet
BoundPredicateSet::And(std::initializer_list<const BoundPredicateSet> Sets) {
  BoundPredicateSet Result;

  return Result;
}


void BoundPredicateSet::print(raw_ostream &O) const {
  for (const auto &It : LbPredicates) {
    It.print(O);
  }
  for (const auto &It : UbPredicates) {
    It.print(O);
  }
}
