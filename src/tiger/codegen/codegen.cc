#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>
#include <iostream>


extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;


} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  assem::InstrList *instr_list = new assem::InstrList();

  std::string frame_name = frame_->label_->Name();
  fs_ = frame_name + "_framesize";

  // save callee-save registers
  SaveCalleeSaved(*instr_list, fs_);

  // instruction selection
  tree::StmList *stms = traces_->GetStmList();
  std::list<tree::Stm*> stm_list = stms->GetList();

  for(auto stm : stm_list) {
    stm->Munch(*instr_list, fs_);
  }

  // restore the callee-save registers
  RestoreCalleeSaved(*instr_list, fs_);

  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(instr_list));
  
}


void CodeGen::SaveCalleeSaved(assem::InstrList &instr_list, std::string_view fs) {
  // get all callee-save registers
  temp::TempList *regs = reg_manager->CalleeSaves();
  std::list<temp::Temp*> reg_list = regs->GetList();
  std::string assem = "movq `s0, `d0";

  for(auto reg : reg_list) {
    temp::Temp *new_temp = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({new_temp}), new temp::TempList({reg}), nullptr));
    tmp_save_regs.emplace_back(new_temp);
  }
}


void CodeGen::RestoreCalleeSaved(assem::InstrList &instr_list, std::string_view fs) {
  // get all callee-save registers
  temp::TempList *regs = reg_manager->CalleeSaves();
  std::list<temp::Temp*> reg_list = regs->GetList();
  std::string assem = "movq `s0, `d0";

  int pos = tmp_save_regs.size() - 1;
  // restore the callee-save register in reverse order
  for(auto it = reg_list.rbegin(); it != reg_list.rend(); ++it) {
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({*it}), new temp::TempList({tmp_save_regs[pos]}), nullptr));
    --pos;
  }
  tmp_save_regs.clear();
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList()){
    // TODO: add this for debug
    assert(instr != nullptr);

    // std::string assem_sentence = instr->GetAssem(); 
    // if(assem_sentence == "addq t130, t131") {
    //   int debug = 1;
    // } 

    instr->Print(out, map);
  }
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string assem = label_->Name();
  instr_list.Append(new assem::LabelInstr(assem, label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string jump_label = exp_->name_->Name();
  assem::Targets *jump_target = new assem::Targets(jumps_);
  std::string assem = "jmp `j0";
  
  instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, jump_target));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string jump_type;
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);

  temp::TempList *src_list = new temp::TempList();
  src_list->Append(left_temp);
  src_list->Append(right_temp);
  std::string assem = "cmpq `s1, `s0";

  // add the first compare instruction
  instr_list.Append(new assem::OperInstr(assem, nullptr, src_list, nullptr));


  switch (op_) {
    case tree::EQ_OP:
      jump_type = "je";
      break;
    case tree::NE_OP:
      jump_type = "jne";
      break;
    case tree::LT_OP:
      jump_type = "jl";
      break;
    case tree::GT_OP:
      jump_type = "jg";
      break;
    case tree::LE_OP:
      jump_type = "jle";
      break;
    case tree::GE_OP:
      jump_type = "jge";
      break;
    case tree::ULT_OP:
      jump_type = "jb";
      break;
    case tree::ULE_OP:
      jump_type = "jbe";
      break;
    case tree::UGT_OP:
      jump_type = "ja";
      break;
    case tree::UGE_OP:
      jump_type = "jae";
      break;
    default:
      printf("invalid relation op\n");
      break;
  }

  // get the target label, if cannot decide thw jump-target label, using the first label
  std::string jump_label = "`j0";
  if(true_label_) {
      jump_label = true_label_->Name();
  }
  
  assem = jump_type + " " + jump_label;
  std::vector<temp::Label *> *labels = new std::vector<temp::Label *>{true_label_};
  assem::Targets *jump_target = new assem::Targets(labels);

  instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, jump_target));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  std::string instr_name;
  std::string assem;
  std::string const_val_str;

  if (typeid(*dst_) == typeid(MemExp)) {
    MemExp *dst_mem = static_cast<MemExp*>(dst_);
    if (typeid(*dst_mem->exp_) == typeid(BinopExp)) {
      BinopExp *dst_binop = static_cast<BinopExp *>(dst_mem->exp_);
      if(dst_binop->op_ == tree::PLUS_OP && typeid(*dst_binop->right_) == typeid(ConstExp)) {
        Exp *e1 = dst_binop->left_; 
        Exp *e2 = src_;

        /*MOVE(MEM(e1+i), e2) */
        auto dst_reg = e1->Munch(instr_list, fs);
        auto src_reg = e2->Munch(instr_list, fs);
        const_val_str = std::to_string(static_cast<tree::ConstExp*>(dst_binop->right_)->consti_);
        assem = "movq `s0, " + const_val_str + "(`s1)";

        instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({src_reg, dst_reg}), nullptr));
      
      }else if(dst_binop->op_== PLUS_OP && typeid(*dst_binop->left_) == typeid(ConstExp)) {
        Exp *e1 = dst_binop->right_; 
        Exp *e2 = src_;
        /*MOVE(MEM(i+e1), e2) */
        auto dst_reg = e1->Munch(instr_list, fs);
        auto src_reg = e2->Munch(instr_list, fs);

        const_val_str = std::to_string(static_cast<tree::ConstExp*>(dst_binop->left_)->consti_);
        assem = "movq `s0, " + const_val_str + "(`s1)";

        instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({src_reg, dst_reg}), nullptr));
      }
    } else if(typeid(*src_) == typeid(MemExp)) {
      MemExp *src_mem = static_cast<MemExp*>(src_);
      Exp *e1=dst_mem->exp_, *e2=src_mem->exp_;
      /*MOVE(MEM(e1), MEM(e2)) */
      auto dst_reg = e1->Munch(instr_list, fs);
      auto src_reg = e2->Munch(instr_list, fs);

      // x86-64 don't permit move from memory to memory
      temp::Temp *new_reg = temp::TempFactory::NewTemp();

      assem = "movq (`s0), d0";
      instr_list.Append(new assem::OperInstr(assem, new temp::TempList({new_reg}), new temp::TempList({src_reg}), nullptr));

      assem = "movq s1, (`s0)";
      instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({dst_reg, new_reg}), nullptr));
    
    } else {
      Exp *e1=dst_mem->exp_, *e2 = src_;
      /*MOVE(MEM(e1), e2) */
      auto dst_reg = e1->Munch(instr_list, fs);
      auto src_reg = e2->Munch(instr_list, fs);

      assem = "movq `s0, (`s1)";
      instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({src_reg, dst_reg}), nullptr));
    }

  } else if (typeid(*dst_) == typeid(TempExp)) {
    temp::Temp *dst_temp = static_cast<tree::TempExp*>(dst_)->temp_;
    Exp *e2=src_;
    /*MOVE(TEMP~i, e2) */
    auto src_reg = e2->Munch(instr_list, fs);
    assem = "movq `s0, `d0";
    instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({dst_temp}), new temp::TempList({src_reg})));
  }
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  temp::Temp *left_reg = left_->Munch(instr_list, fs);
  temp::Temp *right_reg = right_->Munch(instr_list, fs);

  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();
  std::string assem;

  assem = "movq `s0, `d0";
  instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({res_reg}), new temp::TempList({left_reg})));

  switch (op_)
  {
  case tree::PLUS_OP:
    assem = "addq `s0, `d0";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList(res_reg), new temp::TempList({right_reg, res_reg}), nullptr));
    break;
  case tree::MINUS_OP:
    assem = "subq `s0, `d0";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList(res_reg), new temp::TempList({right_reg, res_reg}), nullptr));
    break;
  case tree::MUL_OP:
    // move the left value to %rax
    assem = "movq `s0, `d0";
    instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({reg_manager->NthRegister(0)}), new temp::TempList({res_reg})));
    // multiply the right value, save in %rax, %rbx
    assem = "imulq `s0";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({reg_manager->NthRegister(0), reg_manager->NthRegister(3)}), new temp::TempList({right_reg}), nullptr));
    // move the result to res_reg
    assem = "movq `s0, `d0";
    instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({res_reg}), new temp::TempList({reg_manager->NthRegister(0)})));
    break;
  case tree::DIV_OP:
    // move the left value to %rax
    assem = "movq `s0, `d0";
    instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({reg_manager->NthRegister(0)}), new temp::TempList({res_reg})));
    // sign extend
    assem = "cqto";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({reg_manager->NthRegister(0), reg_manager->NthRegister(3)}), new temp::TempList({reg_manager->NthRegister(0)}), nullptr));
    // divide
    assem = "idivq `s0";
    instr_list.Append(new assem::OperInstr(assem, new temp::TempList({reg_manager->NthRegister(0), reg_manager->NthRegister(3)}), new temp::TempList({right_reg}), nullptr));
    // move the result to res_reg
    assem = "movq `s0, `d0";
    instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({res_reg}), new temp::TempList({reg_manager->NthRegister(0)})));
    break;
  default:
    // is illegal
    assert(0);
    break;
  }

  return res_reg;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();
  temp::Temp *src_reg = exp_->Munch(instr_list, fs);

  std::string assem = "movq (`s0), `d0";
  instr_list.Append(new assem::OperInstr(assem, new temp::TempList({res_reg}), new temp::TempList({src_reg}), nullptr));
  return res_reg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  if(temp_ != reg_manager->FramePointer()) {
    return temp_;
  }
  
  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();
  // fs: the frame size??
  std::string assem = "leaq " + std::string(fs) + "(`s0), `d0";

  instr_list.Append(new assem::OperInstr(assem, new temp::TempList({res_reg}), new temp::TempList({reg_manager->StackPointer()}), nullptr));
  return res_reg; 
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();

  std::string assem = "leaq " + name_->Name() + "(%rip), `d0";
  instr_list.Append(new assem::OperInstr(assem, new temp::TempList({res_reg}), nullptr, nullptr));
  return res_reg;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();

  // move the const value to register
  std::string assem = "movq $" + std::to_string(consti_) + ", `d0";
  instr_list.Append(new assem::OperInstr(assem, new temp::TempList({res_reg}), nullptr, nullptr));
  return res_reg;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // register for saving the reuslt
  temp::Temp *res_reg = temp::TempFactory::NewTemp();

  // check the function name
  if(typeid(*fun_) != typeid(tree::NameExp)) {
    // it should be a label
    return nullptr;
  } 

  // handle static link specially, the first arg is static link, handle it first
  std::list<Exp *> raw_args = args_->GetNonConstList();
  int size = raw_args.size();
  tree::Exp *static_link = raw_args.front();
  args_->PopFront();

  temp::Temp *static_link_reg = static_link->Munch(instr_list, fs);

  int after_size = args_->GetList().size();
  printf("the func name: %s, the arg count: %d \n", static_cast<tree::NameExp*>(fun_)->name_->Name().c_str(), after_size);
  assert(after_size == size - 1);
  auto src_arg_reg_list = args_->MunchArgs(instr_list, fs);

  // store the static link on the stack
  std::string assem = "subq $" + std::to_string(reg_manager->WordSize()) + ", %rsp";
  instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, nullptr));
  assem = "movq `s0, (%rsp)";
  instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({static_link_reg}), nullptr));

  // call the function, and move the arguments to the arg-registers
  assem = "callq " + static_cast<tree::NameExp*>(fun_)->name_->Name();

  instr_list.Append(new assem::OperInstr(assem, reg_manager->ArgRegs(), src_arg_reg_list, nullptr));

  // move the function return value to res_reg
  assem = "movq `s0, `d0";
  instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({res_reg}), new temp::TempList({reg_manager->ReturnValue()})));

  // restore the stack pointer
  int word_size = reg_manager->WordSize();
  int offset = word_size;
  if(raw_args.size() > 6) {
    offset += (raw_args.size() - 6) * word_size;
  }
  assem = "addq $" + std::to_string(offset) + ", %rsp";
  instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, nullptr));

  // restore the arglist
  args_->Insert(static_link);
  
  return res_reg;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // get the arg-save register list
  temp::TempList *arg_reg_list = reg_manager->ArgRegs();

  // the return value
  temp::TempList *res_list = new temp::TempList();

  int arg_reg_size = arg_reg_list->GetList().size();
  int arg_count = exp_list_.size();
  auto it = arg_reg_list->GetList().begin();
  int i = 0;
  std::string assem;
  int word_size = reg_manager->WordSize();

  for(auto &exp : exp_list_) {
    temp::Temp *ret_reg = exp->Munch(instr_list, fs);
    if(i < arg_reg_size) {
      // move the argument to specific register 
      assem = "movq `s0, `d0";
      // TODO: for debug
      printf("the ret_reg value: %d\n", ret_reg->Int());

      instr_list.Append(new assem::MoveInstr(assem, new temp::TempList({*it}), new temp::TempList({ret_reg})));

      // add to the result list
      res_list->Append(*it);
      ++it;

    } else {
      // move the argument to stack, the order is arg n,arg n-1...arg7 from high to low address
      int offset = (i - arg_count) * word_size;
      assem = "movq `s0, " + std::to_string(offset) + "(`s1)";
      instr_list.Append(new assem::OperInstr(assem, nullptr, new temp::TempList({ret_reg, reg_manager->StackPointer()}), nullptr));
    }
    ++i;
  } 

  // move the stack pointer to the top of stack, if necessary
  if(arg_count > arg_reg_size) {
    int offset = (arg_count - arg_reg_size) * word_size;
    assem = "subq $" + std::to_string(offset) + ", %rsp";
    instr_list.Append(new assem::OperInstr(assem, nullptr, nullptr, nullptr));
  }
}

} // namespace tree
