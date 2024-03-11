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

  static SubscriptExpr traceback(const Value *v);

  static SubscriptExpr tracebackWithin(const Value *v, const BasicBlock *BB);

  bool isConstant() const;

  bool operator==(const SubscriptExpr &Other);

};