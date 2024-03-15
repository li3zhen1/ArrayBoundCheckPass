#pragma once

#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

struct SubscriptExpr {
  int64_t A;
  const Value *i;
  int64_t B;

  SubscriptExpr(int64_t A, const Value *i, int64_t B) : A(A), i(i), B(B) {}

  void mutatingAdd(int64_t c);

  void mutatingSub(int64_t c);

  void mutatingMul(int64_t c);

  void dump(raw_ostream &O) const;

  static SubscriptExpr evaluate(const Value *v);

  bool isConstant() const;

  int64_t getConstant() const;

  bool operator==(const SubscriptExpr &Other);

  SubscriptExpr operator+(const SubscriptExpr &Other) const;

  SubscriptExpr operator-(const SubscriptExpr &Other) const;

  SubscriptExpr operator+(int64_t c) const;

  SubscriptExpr operator-(int64_t c) const;

  SubscriptExpr operator*(int64_t c) const;

  bool decreasesWhenVIncreases() const;
  bool increasesWhenVIncreases() const;
  bool decreasesWhenVDecreases() const;
  bool increasesWhenVDecreases() const;

  // SubscriptExpr operator*(const SubscriptExpr &Other, SubscriptExpr&
  // fallback) const;
};

namespace std {
template <> struct hash<SubscriptExpr> {
  std::size_t operator()(const SubscriptExpr &k) const {
    using std::hash;
    using std::size_t;
    using std::string;

    return ((hash<int64_t>()(k.A) ^ (hash<const Value *>()(k.i) << 1)) >> 1) ^
           (hash<int64_t>()(k.B) << 1);
  }
};
} // namespace std