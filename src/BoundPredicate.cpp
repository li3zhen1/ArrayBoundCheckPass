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

bool BoundPredicateBase::operator==(const BoundPredicateBase &Other) const {
  return Bound == Other.Bound && Index == Other.Index;
}

#pragma region subsumes

bool UpperBoundPredicate::subsumes(const UpperBoundPredicate &Other) const {
  if (Index != Other.Index) {
    return false;
  }

  if (Bound.isConstant() && Other.Bound.isConstant()) {
    return Bound.B <= Other.Bound.B;
  }

  if (!Bound.isConstant() && !Other.Bound.isConstant()) {
    assert(Bound.i == Other.Bound.i);
    if (Bound.A == Other.Bound.A && Bound.B <= Other.Bound.B) {
      return true;
    }
    llvm_unreachable("Unimplemented subsume case");
  }

  llvm_unreachable("Unimplemented subsume case");
}

bool UpperBoundPredicate::subsumes(const LowerBoundPredicate &Other) const {
  return false;
}

bool LowerBoundPredicate::subsumes(const UpperBoundPredicate &Other) const {
  return false;
}

bool LowerBoundPredicate::subsumes(const LowerBoundPredicate &Other) const {
  if (Index != Other.Index) {
    return false;
  }

  if (Bound.isConstant() && Other.Bound.isConstant()) {
    return Bound.B >= Other.Bound.B;
  }

  if (!Bound.isConstant() && !Other.Bound.isConstant()) {
    assert(Bound.i == Other.Bound.i);
    if (Bound.A == Other.Bound.A && Bound.B >= Other.Bound.B) {
      return true;
    }
    llvm_unreachable("Unimplemented subsume case");
  }

  llvm_unreachable("Unimplemented subsume case");
}

#pragma endregion
