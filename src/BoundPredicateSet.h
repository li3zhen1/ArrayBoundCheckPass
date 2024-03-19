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
  void addPredicateSet(BoundPredicateSet &P);

  SmallVector<BoundPredicate> getAllPredicates() const;

  bool isIdentityCheck() const;

  static BoundPredicateSet Or(SmallVector<BoundPredicateSet, 4> Sets);

  static BoundPredicateSet And(SmallVector<BoundPredicateSet, 4> Sets);

  optional<SubscriptIndentity> getSubscriptIdentity() const;

  void print(raw_ostream &O) const;

  bool operator==(const BoundPredicateSet &Other) const;

  bool isEmpty() const;

  bool subsumes(const LowerBoundPredicate &Other) const;
  bool subsumes(const UpperBoundPredicate &Other) const;

private:
  BoundPredicate getFirstItem() const;
};

using BoundCheckSetList = SmallVector<BoundPredicateSet>;

using CMap =
    DenseMap<const Value *, DenseMap<const BasicBlock *, BoundPredicateSet>>;

using ValuePtrVector = SmallVector<const Value *, 32>;

using EffectMap =
    DenseMap<const Value *,
             DenseMap<const BasicBlock *, SmallVector<SubscriptExpr>>>;

using ValueEvaluationCache = DenseMap<const Value *, SubscriptExpr>;

void print(CMap &C, raw_ostream &O, const ValuePtrVector &ValueKeys);
void InitializeToEmpty(Function &F, CMap &C, const ValuePtrVector &ValueKeys);