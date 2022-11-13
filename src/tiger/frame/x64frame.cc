#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */

const int WORDSIZE = 8;

X64RegManager::X64RegManager():RegManager() {
  temp::Temp *rax = temp::TempFactory::NewTemp();
  temp::Temp *rbx = temp::TempFactory::NewTemp();
  temp::Temp *rcx = temp::TempFactory::NewTemp();
  temp::Temp *rdx = temp::TempFactory::NewTemp();
  temp::Temp *rsi = temp::TempFactory::NewTemp();
  temp::Temp *rdi = temp::TempFactory::NewTemp();
  temp::Temp *rbp = temp::TempFactory::NewTemp();
  temp::Temp *rsp = temp::TempFactory::NewTemp();
  temp::Temp *r8 = temp::TempFactory::NewTemp();
  temp::Temp *r9 = temp::TempFactory::NewTemp();
  temp::Temp *r10 = temp::TempFactory::NewTemp();
  temp::Temp *r11 = temp::TempFactory::NewTemp();
  temp::Temp *r12 = temp::TempFactory::NewTemp();
  temp::Temp *r13 = temp::TempFactory::NewTemp();
  temp::Temp *r14 = temp::TempFactory::NewTemp();
  temp::Temp *r15 = temp::TempFactory::NewTemp();

  // add the register to temp map and reg
  std::string reg_name = "%rax";
  regs_.emplace_back(rax);
  temp_map_->Enter(rax, &reg_name);

  regs_.emplace_back(rbx);
  reg_name = "%rbx";
  temp_map_->Enter(rbx, &reg_name);

  regs_.emplace_back(rcx);
  reg_name = "%rcx";
  temp_map_->Enter(rcx, &reg_name);

  regs_.emplace_back(rdx);
  reg_name = "%rdx";
  temp_map_->Enter(rdx, &reg_name);

  regs_.emplace_back(rsi);
  reg_name = "%rsi";
  temp_map_->Enter(rsi, &reg_name);

  regs_.emplace_back(rdi);
  reg_name = "%rdi";
  temp_map_->Enter(rdi, &reg_name);

  regs_.emplace_back(rbp);
  reg_name = "%rbp";
  temp_map_->Enter(rbp, &reg_name);

  regs_.emplace_back(rsp);
  reg_name = "%rsp";
  temp_map_->Enter(rsp, &reg_name);

  regs_.emplace_back(r8);
  reg_name = "%r8";
  temp_map_->Enter(r8, &reg_name);

  regs_.emplace_back(r9);
  reg_name = "%r9";
  temp_map_->Enter(r9, &reg_name);

  regs_.emplace_back(r10);
  reg_name = "%r10";
  temp_map_->Enter(r10, &reg_name);

  regs_.emplace_back(r11);
  reg_name = "%r11";
  temp_map_->Enter(r11, &reg_name);

  regs_.emplace_back(r12);
  reg_name = "%r12";
  temp_map_->Enter(r12, &reg_name);

  regs_.emplace_back(r13);
  reg_name = "%r13";
  temp_map_->Enter(r13, &reg_name);

  regs_.emplace_back(r14);
  reg_name = "%r14";
  temp_map_->Enter(r14, &reg_name);

  regs_.emplace_back(r15);
  reg_name = "%r15";
  temp_map_->Enter(r15, &reg_name);
}


temp::TempList *X64RegManager::Registers() {
  std::list<temp::Temp *> initial_list(regs_.begin(), regs_.end());
  // delete rsi
  auto it = initial_list.begin();
  for(int i = 0; i < 4; ++i) {
    ++it;
  }
  initial_list.erase(it);

  temp::TempList *result = new temp::TempList();
  std::list<temp::Temp *> dest_list = result->GetList();
  dest_list = initial_list;
  // for debug
  assert(result->GetList().size() == 15);
  return result;
}


temp::TempList *X64RegManager::ArgRegs() {
  temp::TempList *result = new temp::TempList();
  result->Append(regs_[5]);
  result->Append(regs_[4]);
  result->Append(regs_[3]);
  result->Append(regs_[2]);
  result->Append(regs_[8]);
  result->Append(regs_[9]);
  return result;
}


temp::TempList *X64RegManager::CallerSaves() {
  temp::TempList *result = new temp::TempList();
  result->Append(regs_[0]);
  result->Append(regs_[2]);
  result->Append(regs_[3]);
  result->Append(regs_[4]);
  result->Append(regs_[5]);
  result->Append(regs_[8]);
  result->Append(regs_[9]);
  result->Append(regs_[10]);
  result->Append(regs_[10]);
  result->Append(regs_[11]);
  return result;
}


temp::TempList *X64RegManager::CalleeSaves() {
  temp::TempList *result = new temp::TempList();
  result->Append(regs_[1]);
  result->Append(regs_[6]);
  result->Append(regs_[12]);
  result->Append(regs_[13]);
  result->Append(regs_[14]);
  result->Append(regs_[15]);
  return result;
}


temp::TempList *X64RegManager::ReturnSink() {
  // return value, stack pointer and all callee-saved register
  temp::TempList *result = CalleeSaves();
  result->Append(StackPointer());
  result->Append(ReturnValue());
  return result;
}


int X64RegManager::WordSize() {
  return 8;
}


temp::Temp *X64RegManager::FramePointer() {
  return regs_[6];
}


temp::Temp *X64RegManager::StackPointer() {
  return regs_[7];
} 


temp::Temp *X64RegManager::ReturnValue() {
  return regs_[0];
}


temp::Temp *X64RegManager::NthRegister(int n) {
  assert(n >= 0);
  assert(n < regs_.size());

  return regs_[n];
}

/* TODO: Put your lab5 code here */
X64Frame::X64Frame(temp::Label *name, std::list<bool> formals){
  // initialize
  view_shift_ = nullptr;
  label_ = name;

  // alloc register or in frame
  for(auto formal : formals) {
    formals_.emplace_back(AllocLocal(formal));
  }

  // handle view shift
  newFrame(formals);
}


void X64Frame::newFrame(std::list<bool> formals) {
  // get the arg reg list
  std::list<temp::Temp *> arg_reg_list = reg_manager->ArgRegs()->GetList();
  int max_inreg = arg_reg_list.size();
  auto it = arg_reg_list.begin();
  int nth = 0;

  tree::Exp *dst, *src;
  tree::Stm *stm;

  for(auto formal : formals_) {
    dst = new tree::TempExp(reg_manager->FramePointer());

    if(nth < max_inreg) {
      src = new tree::TempExp(*it);
      ++it;
    } else {
      src = new tree::BinopExp(tree::BinOp::PLUS_OP, new tree::TempExp(reg_manager->FramePointer()), new tree::ConstExp((nth - max_inreg + 1) * WORDSIZE));
    }
    ++nth;
  }

  stm = new tree::MoveStm(dst, src);
  if(view_shift_ != nullptr) {
    view_shift_ = new tree::SeqStm(view_shift_, stm);
  } else {
    view_shift_ = stm;
  }
}


Access* X64Frame::AllocLocal(bool escape) {
  Access *access = nullptr;
  if(escape) {
    access = new InFrameAccess(s_offset);
    s_offset -= WORDSIZE;
  } else {
    access = new InRegAccess(temp::TempFactory::NewTemp());
  }
  return access;
}


std::string X64Frame::GetLabel() {
  return label_->Name();
}

tree::Stm* ProcEntryExit1(Frame *frame, tree::Stm *stm) {
  return new tree::SeqStm(frame->view_shift_, stm);
}


assem::InstrList* ProcEntryExit2(assem::InstrList* body) {
  static temp::TempList *sink_list = nullptr;
  if(!sink_list) {
    sink_list = reg_manager->ReturnSink();
  }
  body->Append(new assem::OperInstr("", nullptr, sink_list, nullptr));
  return body;
}


// TODO: maybe need to supply
assem::Proc* ProcEntryExit3(frame::Frame* frame, assem::InstrList* body) {
  std::string prologue, epilogue;

  // prologue
  // ".set xx_framesize, size", e.g., ".set tigermain_framesize, 8"
  prologue = ".set " + frame->label_->Name() + "_framesize, " + std::to_string(-frame->s_offset) + "\n";
  // the function label, e.g., "tigermain"
  prologue += frame->label_->Name() + "\n";
  // stack pointer shift, e.g., "subq $0x8, %rsp";
  prologue += "subq $" + std::to_string(-frame->s_offset) + ", %rsp\n";

  // epilogue
  epilogue = "addq $" + std::to_string(-frame->s_offset) + ", %rsp\n";
  epilogue += "retq\n";

  return new assem::Proc(prologue, body, epilogue);
}


tree::Exp* ExternalCall(std::string s, tree::ExpList* args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args);
}
} // namespace frame
