#pragma once
#include "BoundPredicate.h"

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

  static BoundPredicateSet Or(SmallVector<BoundPredicateSet, 4> Sets);

  static BoundPredicateSet And(SmallVector<BoundPredicateSet, 4> Sets);

  optional<SubscriptIndentity> getSubscriptIdentity() const;

  void print(raw_ostream &O) const;

  bool operator==(const BoundPredicateSet &Other) const;\

  bool isEmpty() const;

  bool subsumes(const LowerBoundPredicate &Other) const;
  bool subsumes(const UpperBoundPredicate &Other) const;

private:
  BoundPredicate getFirstItem() const;
};
