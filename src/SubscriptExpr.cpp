
#include "SubscriptExpr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include <cassert>
#include <cstdint>

using namespace llvm;

void SubscriptExpr::mutatingAdd(int64_t c) { B += c; }

void SubscriptExpr::mutatingSub(int64_t c) { B -= c; }

void SubscriptExpr::mutatingMul(int64_t c) {
  A *= c;
  B *= c;
}

void SubscriptExpr::dump(raw_ostream &O) const {
  O << "<";
  if (isConstant()) {
    O << B;
  } else {
    if (A != 1) {
      O << A << " * ";
    }
    O << "(load ";
    i->printAsOperand(O, false);
    O << ")";
    if (B != 0) {
      if (B < 0) {
        O << " - " << -B;
      } else {
        O << " + " << B;
      }
    }
  }
  O << ">";
}

/**
 * @brief Traceback until find A * (load ptr) + B, A = 0 if it is a constant
 *        or stop as 1*(?)+0 if cannot continue
 *
 * @param v %v = A * (load ptr) + B
 * @return SubscriptExpr
 */
SubscriptExpr SubscriptExpr::evaluate(const Value *v) {
  if (isa<SExtInst>(v)) {
    return evaluate(cast<SExtInst>(v)->getOperand(0));
  } else if (isa<ZExtInst>(v)) {
    return evaluate(cast<ZExtInst>(v)->getOperand(0));
  } else if (isa<LoadInst>(v)) {
    const auto LI = cast<LoadInst>(v);
    const auto Ptr = LI->getPointerOperand();
    return {1, Ptr, 0};
  } else if (isa<AddOperator>(v)) {
    const auto AO = cast<AddOperator>(v);
    const auto Op1 = AO->getOperand(0);
    const auto Op2 = AO->getOperand(1);

    SubscriptExpr s1 = evaluate(Op1);
    SubscriptExpr s2 = evaluate(Op2);

    if (s1.isConstant()) {
      return s2 + s1.B;
    } else if (s2.isConstant()) {
      return s1 + s2.B;
    } else if (s1.i != s2.i) {
      return {1, v, 0};
    } else {
      return s1 + s2;
    }

  } else if (isa<SubOperator>(v)) {
    const auto AO = cast<SubOperator>(v);
    const auto Op1 = AO->getOperand(0);
    const auto Op2 = AO->getOperand(1);

    SubscriptExpr s1 = evaluate(Op1);
    SubscriptExpr s2 = evaluate(Op2);

    if (s1.isConstant()) {
      return s2 - s1.B;
    } else if (s2.isConstant()) {
      return s1 - s2.B;
    } else if (s1.i != s2.i) {
      return {1, v, 0};
    } else {
      return s1 - s2;
    }

  } else if (isa<MulOperator>(v)) {
    const auto MO = cast<MulOperator>(v);
    const auto Op1 = MO->getOperand(0);
    const auto Op2 = MO->getOperand(1);

    SubscriptExpr s1 = evaluate(Op1);
    SubscriptExpr s2 = evaluate(Op2);

    if (s1.isConstant() && s2.isConstant()) {
      return {0, nullptr, s1.B * s2.B};
    } else if (s1.isConstant()) {
      return s2 * s1.B;
    } else if (s2.isConstant()) {
      return s1 * s2.B;
    } else {
      return {1, v, 0};
    }
  } else if (isa<ConstantInt>(v)) {
    int64_t B = cast<ConstantInt>(v)->getSExtValue();
    return {1, nullptr, B};
  } else {
    llvm::errs() << " ===== Unexpected instruction ====== \n";
    cast<Instruction>(v)->print(errs());
    llvm_unreachable("===== Unexpected instruction ====== \n");
    // return {1, v, 0};
  }
}

bool SubscriptExpr::isConstant() const { return i == nullptr || A == 0; }

bool SubscriptExpr::operator==(const SubscriptExpr &Other) {
  return A == Other.A && i == Other.i && B == Other.B;
}

SubscriptExpr SubscriptExpr::operator+(const SubscriptExpr &Other) const {
  assert(isConstant() || Other.isConstant() || i == Other.i);
  //   if ()
  return {A + Other.A, i, B + Other.B};
}

SubscriptExpr SubscriptExpr::operator-(const SubscriptExpr &Other) const {
  assert(isConstant() || Other.isConstant() || i == Other.i);
  return {A - Other.A, i, B - Other.B};
}

SubscriptExpr SubscriptExpr::operator*(int64_t c) const {
  return {A * c, i, B * c};
}

SubscriptExpr SubscriptExpr::operator+(int64_t c) const {
  return {A, i, B + c};
}

SubscriptExpr SubscriptExpr::operator-(int64_t c) const {
  return {A, i, B - c};
}