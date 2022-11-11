//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
public:
  X64RegManager();

  // Get general-purpose registers except RSI
  [[nodiscard]] temp::TempList *Registers() override;

  // Get registers which can be used to hold arguments
  [[nodiscard]] temp::TempList *ArgRegs() override;

  // Get caller-saved registers
  [[nodiscard]] temp::TempList *CallerSaves() override;

  // Get callee-saved registers
  [[nodiscard]] temp::TempList *CalleeSaves() override;

  // Get return-sink registers
  [[nodiscard]] temp::TempList *ReturnSink() override;

  // Get word size
  [[nodiscard]] int WordSize() override;

  [[nodiscard]] temp::Temp *FramePointer() override;

  [[nodiscard]] temp::Temp *StackPointer() override;

  [[nodiscard]] temp::Temp *ReturnValue() override;
};


class X64Frame : public frame::Frame {
public:
  /* TODO: Put your lab5 code here */
  X64Frame(temp::Label *name, std::list<bool> formals);

  // TODO: maybe have some problem
  void newFrame(std::list<bool> formals) override;

  Access* AllocLocal(bool escape) override;
};


class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : Access(IN_FRAME), offset(offset) {}

  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *frame_ptr) const override {
    return new tree::BinopExp(tree::BinOp::PLUS_OP, frame_ptr, new tree::ConstExp(offset));
  }
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : Access(IN_REG), reg(reg) {}

  /* TODO: Put your lab5 code here */
  tree::Exp *ToExp(tree::Exp *framePtr) const override {
    return new tree::TempExp(reg);
  }
};

} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
