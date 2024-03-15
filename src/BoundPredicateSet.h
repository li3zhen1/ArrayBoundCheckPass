#pragma once
#include "BoundPredicate.h"
#include <initializer_list>

using namespace llvm;
using namespace std;
struct BoundPredicateSet {
  
  SmallVector<LowerBoundPredicate> LbPredicates;
  SmallVector<UpperBoundPredicate> UbPredicates;

  BoundPredicateSet() {};

  void addPredicate(const LowerBoundPredicate &P);
  void addPredicate(const UpperBoundPredicate &P);
  void addPredicate(const BoundPredicate &P);
  
  SmallVector<BoundPredicate> getAllPredicates() const;

  static BoundPredicateSet
  Or(std::initializer_list<const BoundPredicateSet> Sets);

  static BoundPredicateSet
  And(std::initializer_list<const BoundPredicateSet> Sets);

  optional<BoundPredicateIdentity> getIdentity() const;

  void print(raw_ostream &O) const;
};
