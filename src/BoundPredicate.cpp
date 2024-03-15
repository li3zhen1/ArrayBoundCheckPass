#include "BoundPredicate.h"

BoundPredicateIdentity BoundPredicateBase::getIdentity() const {
  return {Bound.getIdentity(), Index.getIdentity()};
}

bool BoundPredicateIdentity::operator==(const BoundPredicateIdentity &Other) {
  return BoundIdentity == Other.BoundIdentity &&
         IndexIdentity == Other.IndexIdentity;
}

void BoundPredicateBase::normalize() {
  Bound.B -= Index.B;
  Index.B = 0;
}

bool BoundPredicateBase::isNormalized() const { return Index.B == 0; }

void LowerBoundPredicate::print(raw_ostream &O) const {
  O << "[";
  Bound.dump(O);
  O << " >= ";
  Index.dump(O);
  O << "]";
}

void UpperBoundPredicate::print(raw_ostream &O) const {
  O << "[";
  Bound.dump(O);
  O << " <= ";
  Index.dump(O);
  O << "]";
}