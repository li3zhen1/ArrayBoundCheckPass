#pragma once
#include "BoundPredicate.h"
#include <initializer_list>

using namespace llvm;
using namespace std;
struct BoundPredicateSet {

  SmallVector<LowerBoundPredicate> LbPredicates;
  SmallVector<UpperBoundPredicate> UbPredicates;

  BoundPredicateSet(){};

  void addPredicate(LowerBoundPredicate &P);
  void addPredicate(UpperBoundPredicate &P);
  void addPredicate(LowerBoundPredicate &&P);
  void addPredicate(UpperBoundPredicate &&P);
  void addPredicate(BoundPredicate &P);

  SmallVector<BoundPredicate> getAllPredicates() const;

  bool isIdentityCheck() const;

  static BoundPredicateSet
  Or(std::initializer_list<const BoundPredicateSet> Sets);

  static BoundPredicateSet
  And(std::initializer_list<const BoundPredicateSet> Sets);

  optional<SubscriptIndentity> getSubscriptIdentity() const;

  void print(raw_ostream &O) const;

private:
  BoundPredicate getFirstItem() const;
};
