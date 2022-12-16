#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <list>
#include <memory>
// TODO: add this lib
#include <vector>
#include <cassert>
#include <string>

#include "tiger/frame/temp.h"
#include "tiger/translate/tree.h"
#include "tiger/codegen/assem.h"


namespace frame {

class RegManager {
public:
  RegManager() : temp_map_(temp::Map::Empty()) {}

  temp::Temp *GetRegister(int regno) { return regs_[regno]; }

  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  [[nodiscard]] virtual temp::TempList *Registers() = 0;

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  [[nodiscard]] virtual temp::TempList *ArgRegs() = 0;

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CallerSaves() = 0;

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  [[nodiscard]] virtual temp::TempList *CalleeSaves() = 0;

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  [[nodiscard]] virtual temp::TempList *ReturnSink() = 0;

  /**
   * Get word size
   */
  [[nodiscard]] virtual int WordSize() = 0;

  [[nodiscard]] virtual temp::Temp *FramePointer() = 0;

  [[nodiscard]] virtual temp::Temp *StackPointer() = 0;

  [[nodiscard]] virtual temp::Temp *ReturnValue() = 0;

  /**
   * add these
   */ 
  [[nodiscard]] virtual temp::Temp *NthRegister(int n) = 0;

  [[nodiscard]] virtual std::string *NthRegisterName(int n) = 0;

  [[nodiscard]] virtual std::string *GetRegisterName(temp::Temp *temp) = 0;

  temp::Map *temp_map_;
protected:
  std::vector<temp::Temp *> regs_;
};

class Access {
public:
  /* TODO: Put your lab5 code here */
  enum Kind {IN_REG, IN_FRAME};
  Kind kind_;
  ///@note add for lab7
  bool store_pointer_{false};

  Access(Kind kind): kind_(kind), store_pointer_(false) {}

  virtual tree::Exp *ToExp(tree::Exp *framePtr) const = 0;

  virtual ~Access() = default;
  
  virtual void SetStorePointer() = 0;
};

class Frame {
  /* TODO: Put your lab5 code here */
public:
  std::list<frame::Access *> formals_;
  std::list<frame::Access *> locals_;
  temp::Label *label_;
  tree::Stm *view_shift_;
  int s_offset;

  // TODO: perhaps need to supply
  ///@brief for view shift
  virtual void newFrame(std::list<bool> formals) = 0;
  virtual Access* AllocLocal(bool escape) = 0;
  virtual std::string GetLabel() = 0;
};

/**
 * Fragments
 */

class Frag {
public:
  virtual ~Frag() = default;

  enum OutputPhase {
    Proc,
    String,
  };

  /**
   *Generate assembly for main program
   * @param out FILE object for output assembly file
   */
  virtual void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const = 0;
};

class StringFrag : public Frag {
public:
  temp::Label *label_;
  std::string str_;

  StringFrag(temp::Label *label, std::string str)
      : label_(label), str_(std::move(str)) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class ProcFrag : public Frag {
public:
  tree::Stm *body_;
  Frame *frame_;

  ProcFrag(tree::Stm *body, Frame *frame) : body_(body), frame_(frame) {}

  void OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const override;
};

class Frags {
public:
  Frags() = default;
  void PushBack(Frag *frag) { frags_.emplace_back(frag); }
  const std::list<Frag*> &GetList() { return frags_; }



private:
  std::list<Frag*> frags_;
};

/* TODO: Put your lab5 code here */
tree::Exp* ExternalCall(std::string s, tree::ExpList* args);
tree::Stm* ProcEntryExit1(Frame *frame, tree::Stm *stm);
assem::InstrList* ProcEntryExit2(assem::InstrList* body);
assem::Proc* ProcEntryExit3(frame::Frame* frame, assem::InstrList* body);
} // namespace frame

#endif