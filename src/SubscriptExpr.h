#ifndef UTILS_H
#define UTILS_H

#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

typedef std::pair<int64_t, const Value *> SubscriptIndentity;

// hash
namespace std {
template <> struct hash<SubscriptIndentity> {
  std::size_t operator()(const SubscriptIndentity &k) const {
    using std::hash;
    using std::size_t;
    using std::string;

    return (
        (hash<int64_t>()(k.first) ^ (hash<const Value *>()(k.second) << 1)) >>
        1);
  }
};
} // namespace std

struct SubscriptExpr {

  int64_t A;
  const Value *i;
  int64_t B;

  SubscriptExpr(int64_t A, const Value *i, int64_t B) : A(A), i(i), B(B) {}

  SubscriptExpr(): A(0), i(nullptr), B(0) {}

  static SubscriptExpr initEmpty() { return {0, nullptr, 0}; }

  void mutatingAdd(int64_t c);

  void mutatingSub(int64_t c);

  void mutatingMul(int64_t c);

  void dump(raw_ostream &O) const;

  static SubscriptExpr evaluate(const Value *v);

  bool isConstant() const;

  int64_t getConstant() const;

  bool operator==(const SubscriptExpr &Other) const;

  SubscriptExpr operator+(const SubscriptExpr &Other) const;

  SubscriptExpr operator-(const SubscriptExpr &Other) const;

  SubscriptExpr operator+(int64_t c) const;

  SubscriptExpr operator-(int64_t c) const;

  SubscriptExpr operator*(int64_t c) const;

  /**
   * @brief Get the Identity object, {A,i}
   *
   * @return SubscriptIndentity
   */
  SubscriptIndentity getIdentity() const;

  bool decreasesWhenVIncreases() const;
  bool increasesWhenVIncreases() const;
  bool decreasesWhenVDecreases() const;
  bool increasesWhenVDecreases() const;

  // SubscriptExpr operator*(const SubscriptExpr &Other, SubscriptExpr&
  // fallback) const;

  static SubscriptExpr getZero() { return {0, nullptr, 0}; }

  int64_t getConstantDifference(const SubscriptExpr &rhs) const;
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


const Value* findEarliestLoadLike(const Value *v);

#endif