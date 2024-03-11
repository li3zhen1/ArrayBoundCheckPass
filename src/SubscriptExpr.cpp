
#include "SubscriptExpr.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include <cassert>

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
    i->printAsOperand(O, false);
    if (B != 0) {
      O << " + " << B;
    }
  }
  O << ">";
}

/**
 * @brief Traceback until find A * (load ptr) + B, A = 0 if it is a constant
 *
 * @param v %v = A * (load ptr) + B
 * @return SubscriptExpr
 */
SubscriptExpr SubscriptExpr::traceback(const Value *v) {
  const Value *curr = v;
  SubscriptExpr base(1, nullptr, 0);
  do {
    if (isa<SExtInst>(curr)) {
      const auto SI = cast<SExtInst>(curr);
      const auto V = SI->getOperand(0);
      curr = V;
    } else if (isa<ZExtInst>(curr)) {
      const auto ZI = cast<ZExtInst>(curr);
      const auto V = ZI->getOperand(0);
      curr = V;
    } else if (isa<LoadInst>(curr)) {
      const auto LI = cast<LoadInst>(curr);
      const auto Ptr = LI->getPointerOperand();
      curr = Ptr;
      break;
    } else if (isa<AddOperator>(curr)) {
      const auto AO = cast<AddOperator>(curr);
      const auto Op1 = AO->getOperand(0);
      const auto Op2 = AO->getOperand(1);

      if (isa<ConstantInt>(Op1) && isa<ConstantInt>(Op2)) {
        int64_t evaluated = cast<ConstantInt>(Op1)->getSExtValue() +
                            cast<ConstantInt>(Op2)->getSExtValue();
        base.mutatingAdd(evaluated * base.A);

      } else if (isa<ConstantInt>(Op1)) {
        const auto A = cast<ConstantInt>(Op1);
        base.mutatingAdd(A->getSExtValue());
        curr = Op2;
      } else if (isa<ConstantInt>(Op2)) {
        const auto B = cast<ConstantInt>(Op2);
        base.mutatingAdd(B->getSExtValue());
        curr = Op1;
      } else {
        SubscriptExpr s1 = SubscriptExpr::traceback(Op1);
        SubscriptExpr s2 = SubscriptExpr::traceback(Op2);
        assert(s1.i == s2.i && "Different index in AddOperator");
        base.A = base.A * (s1.A + s2.A);
        base.B = base.B + base.A * (s1.B + s2.B);
        base.i = s1.i;
        return base;
      }
    } else if (isa<MulOperator>(curr)) {
      const auto MO = cast<MulOperator>(curr);
      const auto Op1 = MO->getOperand(0);
      const auto Op2 = MO->getOperand(1);

      if (isa<ConstantInt>(Op1) && isa<ConstantInt>(Op2)) {
        int64_t evaluated = cast<ConstantInt>(Op1)->getSExtValue() *
                            cast<ConstantInt>(Op2)->getSExtValue();
        base.mutatingMul(evaluated);
      } else if (isa<ConstantInt>(Op1)) {
        const auto A = cast<ConstantInt>(Op1);
        base.mutatingMul(A->getSExtValue());
        curr = Op2;
      } else if (isa<ConstantInt>(Op2)) {
        const auto B = cast<ConstantInt>(Op2);
        base.mutatingMul(B->getSExtValue());
        curr = Op1;
      } else {
        SubscriptExpr s1 = SubscriptExpr::traceback(Op1);
        SubscriptExpr s2 = SubscriptExpr::traceback(Op2);
        if (s1.i == nullptr) {
          // s1 is a constant
          base.A = base.A * s1.A;
          base.B = base.B * s1.A;
          base.i = s2.i;
          return base;
        } else if (s2.i == nullptr) {
          // s1 is a constant
          base.A = base.A * s2.A;
          base.B = base.B * s2.A;
          base.i = s1.i;
          return base;
        } else {
          assert(s1.i == nullptr && s2.i == nullptr &&
                 "Different index in MulOperator");
          // s1 and s2 are constants
          base.B += base.A * s1.B * s2.B;
          return base;
        }
      }
    } else {
      curr->print(errs());
      llvm_unreachable("Unexpected instruction");
    }
  } while (curr);

  base.i = curr;
  return base;
}

bool SubscriptExpr::isConstant() const { return i == nullptr; }

SubscriptExpr SubscriptExpr::tracebackWithin(const Value *v,
                                             const BasicBlock *BB) {
  const Value *curr = v;
  SubscriptExpr base(1, nullptr, 0);

  if (isa<Instruction>(curr)) {
    const auto I = dyn_cast<Instruction>(curr);
    if (I->getParent() != BB) {
      return {1, curr, 0};
    }
  }

  return base;
}

bool SubscriptExpr::operator==(const SubscriptExpr &Other) {
  return A == Other.A && i == Other.i && B == Other.B;
}