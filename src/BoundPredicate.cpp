#include "BoundPredicate.h"

BoundPredicateIdentity BoundPredicateBase::getIdentity() const {
  return {Bound.getIdentity(), Index.getIdentity()};
}

bool BoundPredicateIdentity::operator==(
    const BoundPredicateIdentity &Other) const {
  return BoundIdentity == Other.BoundIdentity &&
         IndexIdentity == Other.IndexIdentity;
}

void BoundPredicateBase::normalize() {
  Bound.B -= Index.B;
  Index.B = 0;
}

bool BoundPredicateBase::isNormalized() const { return Index.B == 0; }

void LowerBoundPredicate::print(raw_ostream &O) const {
  Bound.dump(GreenO);
  O << " ≤ ";
  Index.dump(RedO);
}

bool BoundPredicateBase::isIdentityCheck() const {
  return Index.A == 1 && Index.B == 0;
}

void UpperBoundPredicate::print(raw_ostream &O) const {

  Index.dump(RedO);
  O << " ≤ ";
  Bound.dump(GreenO);
}
