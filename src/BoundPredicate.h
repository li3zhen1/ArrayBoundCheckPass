#ifndef BOUND_PREDICATE_H
#define BOUND_PREDICATE_H

#include "SubscriptExpr.h"
// #include <variant>

struct BoundPredicateIdentity {
  SubscriptIndentity BoundIdentity;
  SubscriptIndentity IndexIdentity;

  BoundPredicateIdentity(SubscriptIndentity BoundIdentity,
                         SubscriptIndentity IndexIdentity)
      : BoundIdentity(BoundIdentity), IndexIdentity(IndexIdentity) {}

  bool operator==(const BoundPredicateIdentity &Other);
};

namespace std {
template <> struct hash<BoundPredicateIdentity> {
  size_t operator()(const BoundPredicateIdentity &x) const {
    return hash<SubscriptIndentity>()(x.BoundIdentity) ^
           hash<SubscriptIndentity>()(x.IndexIdentity);
  }
};
} // namespace std

struct BoundPredicateBase {

  SubscriptExpr Bound;
  SubscriptExpr Index;

  BoundPredicateBase(SubscriptExpr Bound, SubscriptExpr Index)
      : Bound(Bound), Index(Index) {}

  BoundPredicateIdentity getIdentity() const;

  /**
   * @brief Make subscript.B = 0 by subtracting Index from Bound
   *
   */
  void normalize();

  bool isNormalized() const;

  // void print(raw_ostream &O) const;
};

struct UpperBoundPredicate : public BoundPredicateBase {
  UpperBoundPredicate(SubscriptExpr Bound, SubscriptExpr Index)
      : BoundPredicateBase(Bound, Index) {}

  void print(raw_ostream &O) const;
};

struct LowerBoundPredicate : public BoundPredicateBase {
  LowerBoundPredicate(SubscriptExpr Bound, SubscriptExpr Index)
      : BoundPredicateBase(Bound, Index) {}

  void print(raw_ostream &O) const;
};

typedef std::variant<UpperBoundPredicate, LowerBoundPredicate> BoundPredicate;
#endif