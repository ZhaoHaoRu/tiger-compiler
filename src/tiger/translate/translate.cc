#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  assert(level != nullptr);
  return new Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), t, f);
    tr::PatchList trues(std::list<temp::Label **>{&cjump_stm->true_label_}), falses(std::list<temp::Label **>{&cjump_stm->false_label_});
    Cx new_cx(trues, falses, cjump_stm);
    return new_cx;
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override { 
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    std::cerr << "For UnCX, should never expect to see a tr::NxExp kind" << std::endl;
    
    PatchList trues;
    PatchList falses;
    tree::Stm *stm = nullptr;
    return Cx(trues, falses, stm);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    // tr::PatchList true_list(std::list<temp::Label **>{&t});
    // tr::PatchList false_list(std::list<temp::Label **>{&f});

    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);

    // PatchList::JoinPatch(cx_.trues_, true_list);
    // PatchList::JoinPatch(cx_.falses_, false_list);

    return new tree::EseqExp(
      new tree::MoveStm(
        new tree::TempExp(r), new tree::ConstExp(1)),
          new tree::EseqExp(cx_.stm_,
            new tree::EseqExp(new tree::LabelStm(f),
              new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
                  new tree::EseqExp(new tree::LabelStm(t), new tree::TempExp(r)))))
    );
  }

  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(UnEx());
  }

  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { 
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};


void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  // add the base symbol for var env and type env
  FillBaseTEnv();
  FillBaseVEnv();


  temp::Label *new_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *result = absyn_tree_->Translate(venv_.get(), tenv_.get(), 
                          main_level_.get(), main_level_->frame_->label_, errormsg_.get());

  // TODO:  need to save
  frags->PushBack(new frame::ProcFrag(result->exp_->UnNx(), main_level_->frame_));
}

} // namespace tr

namespace absyn {
// for static link
tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());

  // if(!current || !current->parent_) {
  //   return nullptr;
  // }

  while(current != target) {
    static_link = new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, static_link, new tree::ConstExp(reg_manager->WordSize())));
    current = current->parent_;
  }

  return static_link;
}

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if(!entry) {
    errormsg->Error(pos_, "cannot find such symbol in environment\n");
  }

  env::VarEntry *var_entry = static_cast<env::VarEntry*>(entry);

  tree::Exp *static_link = StaticLink(level, var_entry->access_->level_);
  static_link = var_entry->access_->access_->ToExp(static_link);

  type::Ty *ty = var_entry->ty_->ActualTy();
  tr::Exp *exp = new tr::ExExp(static_link);

  return new tr::ExpAndTy(exp, ty);
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *var_ty = var_info->ty_->ActualTy();

  if(typeid(*var_ty) != typeid(type::RecordTy)) {
    errormsg->Error(pos_, "in field var, the var should be record type\n");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  } 

  type::RecordTy *record_ty = static_cast<type::RecordTy*>(var_ty);
  std::list<type::Field *> field_list = record_ty->fields_->GetList();

  int i = 0;
  for(auto field : field_list) {
    if(field->name_ == sym_) {
      // generate the exp
      tree::Exp *raw_exp = new tree::BinopExp(tree::PLUS_OP, 
          var_info->exp_->UnEx(), new tree::ConstExp(i * reg_manager->WordSize()));
      tr::Exp *exp = new tr::ExExp(new tree::MemExp(raw_exp));
      return new tr::ExpAndTy(exp, field->ty_->ActualTy());
    }
    ++i;
  }

  errormsg->Error(pos_, "in field var, cannot find the field want\n");
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *var_ty = var_info->ty_->ActualTy();

  if(typeid(*var_ty) != typeid(type::ArrayTy)) {
    errormsg->Error(pos_, "in subscript var, the var should be array type\n");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }

  tr::ExpAndTy *subscript_info = subscript_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *subscript_ty = subscript_info->ty_->ActualTy();

  if(typeid(*subscript_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "the array index should be integer\n");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }

  tree::Exp *offset = new tree::BinopExp(tree::MUL_OP, subscript_info->exp_->UnEx(), new tree::ConstExp(reg_manager->WordSize()));
  tree::Exp *raw_exp = new tree::MemExp(
                    new tree::BinopExp(tree::PLUS_OP, var_info->exp_->UnEx(), offset)
                  );
  tr::Exp *exp = new tr::ExExp(raw_exp);

  return new tr::ExpAndTy(exp, static_cast<type::ArrayTy*>(var_ty)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::Exp *exp = new tr::ExExp(new tree::ConstExp(0));
  return new tr::ExpAndTy(exp, type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::Exp *exp = new tr::ExExp(new tree::ConstExp(val_));
  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *str_label = temp::LabelFactory::NewLabel();
  frags->PushBack(new frame::StringFrag(str_label, str_));
  tr::Exp *exp = new tr::ExExp(new tree::NameExp(str_label));
  return new tr::ExpAndTy(exp, type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // find the function in environment
  env::EnvEntry *entry = venv->Look(func_);
  if(!entry) {
    errormsg->Error(pos_, "cannot find the function name in the environment\n");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }

  if(entry->kind_ != env::EnvEntry::Kind::FUNC) {
    errormsg->Error(pos_, "the function name's type should be function\n");
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }

  env::FunEntry *fun_entry = static_cast<env::FunEntry*>(entry);

  // get the static link
  printf("the function name: %s\n", func_->Name().c_str());

  auto exp_list = new tree::ExpList();

  tree::Exp *static_link = StaticLink(level, fun_entry->level_->parent_);
  // to avoid segmentation fault
  if(static_link) {
    exp_list->Append(static_link);
  } else {
    printf("error: don't have static link!\n");
  }
  

  // translate the arguments
  std::list<absyn::Exp*> raw_arg_list = args_->GetList();
  for(auto arg : raw_arg_list) {
    tr::ExpAndTy *result = arg->Translate(venv, tenv, level, label, errormsg);
    exp_list->Append(result->exp_->UnEx());
  }

  // judge the return type
  type::Ty *result_ty = type::VoidTy::Instance();
  if(fun_entry->result_) {
    result_ty = fun_entry->result_->ActualTy();
  } 
  
  tr::Exp *exp;
  // it is outermost or it is a external call
  if(!fun_entry->level_->parent_) {
    exp = new tr::ExExp(frame::ExternalCall(temp::LabelFactory::LabelString(func_), exp_list));
  } else {
    exp = new tr::ExExp(new tree::CallExp(new tree::NameExp(func_), exp_list));
  }

  return new tr::ExpAndTy(exp, result_ty);
}

void CheckBinop(int pos, tr::ExpAndTy *left, tr::ExpAndTy *right, err::ErrorMsg *errormsg) {
  if(typeid(*left->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(pos, "the left value in binop exp should be integer\n");
  }
  if(typeid(*right->ty_->ActualTy()) != typeid(type::IntTy)) {
    errormsg->Error(pos, "the right value in binop exp should be integer\n");
  }
}

void CheckCjump(int pos, tr::ExpAndTy *left, tr::ExpAndTy *right, err::ErrorMsg *errormsg) {
  type::Ty *left_ty = left->ty_->ActualTy();
  type::Ty *right_ty = right->ty_->ActualTy();

  if(typeid(*left_ty) != typeid(*right_ty) && typeid(*right_ty) != typeid(type::NilTy)) {
    errormsg->Error(pos, "the left value and right value in cjump condition mismatch\n");
  } 
  // if(typeid(*left_ty) != typeid(type::IntTy) && typeid(*right_ty) != typeid(type::StringTy)) {
  //   errormsg->Error(pos, "the value in cjump condition should be integer or string\n");
  // }
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // translate left exp and right exp
  tr::ExpAndTy *l_ret = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *r_ret = right_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp;
  tree::CjumpStm *stm;
  tr::PatchList *trues = new tr::PatchList(), *falses = new tr::PatchList();

  switch(oper_) {
    case absyn::PLUS_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::PLUS_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::MINUS_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::MINUS_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::TIMES_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::MUL_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::DIVIDE_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::DIV_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::AND_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::AND_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::OR_OP:
      CheckBinop(pos_, l_ret, r_ret, errormsg);
      exp = new tr::ExExp(new tree::BinopExp(tree::OR_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx()));
      break;
    case absyn::EQ_OP:
    case absyn::NEQ_OP:
      // for string compare
      CheckCjump(pos_, l_ret, r_ret, errormsg);
      if(typeid(*l_ret->ty_->ActualTy()) == typeid(type::StringTy) && typeid(*r_ret->ty_->ActualTy()) == typeid(type::StringTy)) {
        auto exp_list = new tree::ExpList();
        exp_list->Append(l_ret->exp_->UnEx());
        exp_list->Append(r_ret->exp_->UnEx());
        auto ret = frame::ExternalCall("string_equal", exp_list);
        if(oper_ == absyn::EQ_OP)
          stm = new tree::CjumpStm(tree::EQ_OP, ret, new tree::ConstExp(1), nullptr, nullptr);
        else
          stm = new tree::CjumpStm(tree::NE_OP, ret, new tree::ConstExp(1), nullptr, nullptr);
        trues->AddPatch(&stm->true_label_);
        falses->AddPatch(&stm->false_label_);
        exp = new tr::CxExp(*trues, *falses, stm);
      } else {
        if(oper_ == absyn::EQ_OP)
          stm = new tree::CjumpStm(tree::EQ_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
        else
          stm = new tree::CjumpStm(tree::NE_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
        trues->AddPatch(&stm->true_label_);
        falses->AddPatch(&stm->false_label_);
        exp = new tr::CxExp(*trues, *falses, stm);
      }
      break;
    case absyn::LT_OP:
      CheckCjump(pos_, l_ret, r_ret, errormsg);
      stm = new tree::CjumpStm(tree::LT_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
      trues->AddPatch(&stm->true_label_);
      falses->AddPatch(&stm->false_label_);
      exp = new tr::CxExp(*trues, *falses, stm);
      break;
    case absyn::LE_OP:
      CheckCjump(pos_, l_ret, r_ret, errormsg);
      stm = new tree::CjumpStm(tree::LE_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
      trues->AddPatch(&stm->true_label_);
      falses->AddPatch(&stm->false_label_);
      exp = new tr::CxExp(*trues, *falses, stm);
      break;
    case absyn::GT_OP:
      CheckCjump(pos_, l_ret, r_ret, errormsg);
      stm = new tree::CjumpStm(tree::GT_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
      trues->AddPatch(&stm->true_label_);
      falses->AddPatch(&stm->false_label_);
      exp = new tr::CxExp(*trues, *falses, stm);
      break;
    case absyn::GE_OP:
      CheckCjump(pos_, l_ret, r_ret, errormsg);
      stm = new tree::CjumpStm(tree::GE_OP, l_ret->exp_->UnEx(), r_ret->exp_->UnEx(), nullptr, nullptr);
      trues->AddPatch(&stm->true_label_);
      falses->AddPatch(&stm->false_label_);
      exp = new tr::CxExp(*trues, *falses, stm);
      break;
    default:
      errormsg->Error(pos_, "unknown op type\n");
  }

  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(typ_);

  // check the type
  if(!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().data());
    ty = type::IntTy::Instance();
  } else {
    // remember to convert to the actual type
    ty = ty->ActualTy();
    if(typeid(*ty) != typeid(type::RecordTy)) {
      errormsg->Error(pos_, "not a record type");
      ty = type::IntTy::Instance();
    }
  }

  auto exp_list = new tree::ExpList();

  // translate every field in the field list
  std::list<EField *> field_list = fields_->GetList();
  for(auto field : field_list) {
    tr::ExpAndTy *ret = field->exp_->Translate(venv, tenv, level, label, errormsg);
    assert(ret != nullptr);
    exp_list->Append(ret->exp_->UnEx());
  }

  // alloc a space to store the pointer
  temp::Temp *head = temp::TempFactory::NewTemp();
  auto args = new tree::ExpList();
  args->Append(new tree::ConstExp(reg_manager->WordSize() * field_list.size()));
  tree::Stm *stm = new tree::MoveStm(new tree::TempExp(head), frame::ExternalCall("alloc_record", args));

  // handle the records, move to heap
  int count = 0;
  std::list<tree::Exp*> list = exp_list->GetList();

  for(auto exp : list) {
    tree::Exp *dst = new tree::MemExp(
                      new tree::BinopExp(tree::PLUS_OP, 
                        new tree::TempExp(head), 
                        new tree::ConstExp(count * reg_manager->WordSize())
                      )
                    );
    tree::Stm *move_stm = new tree::MoveStm(dst, exp);
    // TODO: add for debug
    assert(stm);
    stm = new tree::SeqStm(stm, move_stm);
    ++count;
  }

  tr::Exp *exp = new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(head)));
  return new tr::ExpAndTy(exp, ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // check whether it is empty
  if(!seq_) {
    errormsg->Error(pos_, "the seq exp is empty\n");
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }

  // translate every exp in the sequence (step1)
  std::list<tr::Exp*> exp_list = {};
  std::list<absyn::Exp*> raw_exp_list = seq_->GetList();
  tr::ExpAndTy *ret;
  for(auto exp : raw_exp_list) {
    ret = exp->Translate(venv, tenv, level, label, errormsg);
    exp_list.emplace_back(ret->exp_);
  }

  // translate every exp in the sequence (step2)
  tr::Exp *res = new tr::ExExp(new tree::ConstExp(0));
  for(auto &exp : exp_list) {
    if(exp) {
      tree::Stm *stm_former = res->UnNx();
      tree::Exp *exp_new = exp->UnEx();
      res = new tr::ExExp(new tree::EseqExp(stm_former, exp_new));
    } else {
      res = new tr::ExExp(new tree::EseqExp(res->UnNx(), new tree::ConstExp(0)));
    }
  }

  return new tr::ExpAndTy(res, ret->ty_);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp_info = exp_->Translate(venv, tenv, level, label, errormsg);

  if(typeid(*var_info->ty_->ActualTy()) != typeid(*exp_info->ty_->ActualTy())) {
    errormsg->Error(pos_, "assign type mismatch\n");
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }

  tr::Exp *exp = new tr::NxExp(new tree::MoveStm(var_info->exp_->UnEx(), exp_info->exp_->UnEx()));
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  tr::ExpAndTy *test_info = nullptr, *then_info = nullptr, *else_info = nullptr;
  // translate the three part
  test_info = test_->Translate(venv, tenv, level, label, errormsg);
  then_info = then_->Translate(venv, tenv, level, label, errormsg);
  if(elsee_) {
    else_info = elsee_->Translate(venv, tenv, level, label, errormsg);
  }

  // return pos
  temp::Temp *r = temp::TempFactory::NewTemp(); 
  temp::Label *t = temp::LabelFactory::NewLabel(); 
  temp::Label *f = temp::LabelFactory::NewLabel(); 
  temp::Label *done = temp::LabelFactory::NewLabel();

  auto label_list = new std::vector<temp::Label*>();
  label_list->emplace_back(done);

  tr::Exp *exp;
  tr::Cx test_cx = test_info->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(t);
  test_cx.falses_.DoPatch(f);

  if(else_info) {
    exp = new tr::ExExp(
            new tree::EseqExp(test_cx.stm_,
              new tree::EseqExp(new tree::LabelStm(t), // if true
                new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), then_info->exp_->UnEx()), // r <- then
                  new tree::EseqExp(new tree::JumpStm(new tree::NameExp(done), label_list), // goto done
                    new tree::EseqExp(new tree::LabelStm(f),  // if false
                      new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), else_info->exp_->UnEx()), // r<- else
                        new tree::EseqExp(new tree::JumpStm(new tree::NameExp(done), label_list), // goto done
                          new tree::EseqExp(new tree::LabelStm(done),
                            new tree::TempExp(r)))))))))  // return r
          );
  } else {
    // TODO: add for debug
    assert(test_cx.stm_);
    assert(then_info->exp_->UnNx());
    printf("get line 618!\n");
    exp = new tr::NxExp(
            new tree::SeqStm(test_cx.stm_,
              new tree::SeqStm(new tree::LabelStm(t),
                new tree::SeqStm(then_info->exp_->UnNx(),
                  new tree::LabelStm(f))))
          );
    // TODO: add for debug
    printf("get line 625!\n");
  }
  return new tr::ExpAndTy(exp, then_info->ty_);
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  temp::Label *done = temp::LabelFactory::NewLabel();
  temp::Label *body = temp::LabelFactory::NewLabel();
  temp::Label *test = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *test_info = nullptr, *body_info = nullptr;
  test_info = test_->Translate(venv, tenv, level, label, errormsg);
  body_info = body_->Translate(venv, tenv, level, done, errormsg);

  // type checking 
  if(typeid(*test_info->ty_) != typeid(type::IntTy)){
    errormsg->Error(pos_, "integer required");
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  if(typeid(*body_info->ty_) != typeid(type::VoidTy)){
    errormsg->Error(pos_, "while body must produce no value");
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }

  tr::Exp *exp;
  tr::Cx test_cx = test_info->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(body);
  test_cx.falses_.DoPatch(done);

  auto label_list = new std::vector<temp::Label*>();
  label_list->emplace_back(test);

  // TODO: add for debug
  assert(test_cx.stm_);
  assert(body_info->exp_->UnNx());

  printf("get line 664!\n");
  exp = new tr::NxExp(
          new tree::SeqStm(new tree::LabelStm(test),
            new tree::SeqStm(test_cx.stm_,
              new tree::SeqStm(new tree::LabelStm(body),
                new tree::SeqStm(body_info->exp_->UnNx(),
                // after execute body, jump to test
                  new tree::SeqStm(new tree::JumpStm(new tree::NameExp(test), label_list),
                    new tree::LabelStm(done))))))
        );

  printf("get line 674!\n");
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

/**
 *  if i > limit goto done
 *  body
 *  goto test
 * Loop: 
 *  i := i + 1
 *  body
 * test:
 *  if i < limit goto Loop
   done:
*/
tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // translate the edge var
  tr::ExpAndTy *lo_info = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi_info = hi_->Translate(venv, tenv, level, label, errormsg);

  venv->BeginScope();

  // alloc var
  tr::Access *access = tr::Access::AllocLocal(level, this->escape_);
  env::EnvEntry *new_var_entry = new env::VarEntry(access, lo_info->ty_, true);
  venv->Enter(var_, new_var_entry);

  // new labels
  temp::Label *done = temp::LabelFactory::NewLabel();
  temp::Label *body = temp::LabelFactory::NewLabel();
  temp::Label *incr = temp::LabelFactory::NewLabel();

  // translate the body
  tr::ExpAndTy *body_info = body_->Translate(venv, tenv, level, done, errormsg);

  auto label_list = new std::vector<temp::Label*>();
  label_list->emplace_back(body);

  // get i and limit, then assign
  tr::Exp *limit = new tr::ExExp(new tree::TempExp(temp::TempFactory::NewTemp()));
  frame::InRegAccess *inreg = static_cast<frame::InRegAccess*>(access->access_);
  tr::Exp *i = new tr::ExExp(new tree::TempExp(inreg->reg));
  tree::Stm *i_stm = new tree::MoveStm(i->UnEx(), lo_info->exp_->UnEx());
  tree::Stm *limit_stm = new tree::MoveStm(limit->UnEx(), hi_info->exp_->UnEx());

  // test condition: i <= limit
  tree::Stm *first_test_stm = new tree::CjumpStm(tree::LE_OP, i->UnEx(), limit->UnEx(), body, done);

  // i++
  tree::Stm *increase_stm = new tree::MoveStm(i->UnEx(), 
                              new tree::BinopExp(tree::PLUS_OP, i->UnEx(), new tree::ConstExp(1)));
  
  // test stm after the first time
  printf("get line 731!\n");
  tree::Stm *test_stm = new tree::SeqStm(new tree::CjumpStm(tree::LT_OP, i->UnEx(), limit->UnEx(), incr, done),
                          new tree::SeqStm(new tree::LabelStm(incr),
                            new tree::SeqStm(increase_stm,
                              new tree::JumpStm(new tree::NameExp(body), label_list)))
                        );
  
  printf("get line 738!\n");
  tree::Stm *stm = new tree::SeqStm(i_stm,
                    new tree::SeqStm(limit_stm,
                      new tree::SeqStm(first_test_stm,
                        new tree::SeqStm(new tree::LabelStm(body),
                          new tree::SeqStm(body_info->exp_->UnNx(),
                            new tree::SeqStm(test_stm,
                              new tree::LabelStm(done))))))
                  );
  printf("get line 747!\n");
  venv->EndScope();

  return new tr::ExpAndTy(new tr::NxExp(stm), type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  auto label_list = new std::vector<temp::Label*>();
  label_list->emplace_back(label);

  tr::Exp *exp = new tr::NxExp(new tree::JumpStm(new tree::NameExp(label), label_list));

  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // translate all declaration
  // mark the first time enter 
  static bool first = true;
  bool is_main = false;
  if(first) {
    is_main = true;
    first = false;
  }

  venv->BeginScope();
  tenv->BeginScope();

  std::list<absyn::Dec*> raw_dec_list = decs_->GetList();
  std::list<tr::Exp*> dec_list;
  tree::Stm *stm = nullptr;
  tree::Exp *exp = nullptr;

  // translate all declarations
  for(auto dec : raw_dec_list) {
    if(!stm) {
      stm = dec->Translate(venv, tenv, level, label, errormsg)->UnNx();
    } else {
      stm = new tree::SeqStm(stm, dec->Translate(venv, tenv, level, label, errormsg)->UnNx());
    }
  }

  // translate the body
  tr::ExpAndTy *body_info = body_->Translate(venv, tenv, level, label, errormsg);

  venv->EndScope();
  tenv->EndScope();

  if(stm) {
    exp = new tree::EseqExp(stm, body_info->exp_->UnEx());
  } else {
    exp = body_info->exp_->UnEx();
  }

  stm = new tree::ExpStm(exp);

  return new tr::ExpAndTy(new tr::ExExp(exp), body_info->ty_->ActualTy());
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // translate the size and initial val
  tr::ExpAndTy *size_info = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init_info = init_->Translate(venv, tenv, level, label, errormsg);

  auto args = new tree::ExpList();
  args->Append(size_info->exp_->UnEx());
  args->Append(init_info->exp_->UnEx());
  tree::Exp *exp = frame::ExternalCall("init_array", args);

  type::Ty *ty = tenv->Look(typ_)->ActualTy();

  if(!ty) {
    errormsg->Error(pos_, "can't find the array type\n");
  }
  if(typeid(*ty) != typeid(type::ArrayTy)){
    errormsg->Error(pos_, "in array exp , not array type %s\n", typ_->Name());
  } 

  return new tr::ExpAndTy(new tr::ExExp(exp), ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // add the declaration to environments
  std::list<FunDec*> func_dec_list = functions_->GetList();
  for(auto fun_dec : func_dec_list) {
    type::TyList *tylist = fun_dec->params_->MakeFormalTyList(tenv, errormsg);

    // the escape list
    std::list<bool> escapes;
    std::list<absyn::Field*> field_list = fun_dec->params_->GetList();
    for(auto field : field_list) {
      escapes.emplace_back(field->escape_);
    }

    // a label for the function
    temp::Label *func_label = temp::LabelFactory::NamedLabel(fun_dec->name_->Name());
    tr::Level *func_level = tr::Level::newLevel(func_label, escapes, level);

    // add the func's info to environment
    type::Ty *ret_ty = type::VoidTy::Instance();
    if(fun_dec->result_) {
      ret_ty = tenv->Look(fun_dec->result_);
    }

    env::EnvEntry *func_entry = new env::FunEntry(func_level, func_label, tylist, ret_ty);
    venv->Enter(fun_dec->name_, func_entry);
  }

  // translate the function body
  for(auto fun_dec : func_dec_list) {
    type::TyList *formals = fun_dec->params_->MakeFormalTyList(tenv, errormsg);
    env::FunEntry *func_entry = static_cast<env::FunEntry*>(venv->Look(fun_dec->name_));

    venv->BeginScope();

    std::list<absyn::Field *> params_list = fun_dec->params_->GetList();
    std::list<type::Ty *> type_list = formals->GetList();
    std::list<frame::Access *>  access_list = func_entry->level_->frame_->formals_;

    auto type_it = type_list.begin();
    // TODO: does it has the static link?
    auto access_it = access_list.begin();
    if(type_list.size() < access_list.size()) {
      errormsg->Error(pos_, "has the static link\n");
      printf("has static link!\n");
      ++access_it;
    }

    // add the params to the environment
    for(auto param : params_list) {
      tr::Access *new_access = new tr::Access(func_entry->level_, *access_it);
      env::EnvEntry *param_entry = new env::VarEntry(new_access, *type_it);
      venv->Enter(param->name_, param_entry);
      ++access_it;
      ++type_it;
    }
    
    // translate the body
    tr::ExpAndTy *body_info = fun_dec->body_->Translate(venv, tenv, func_entry->level_, func_entry->label_, errormsg);
    venv->EndScope();

    tree::Stm *stm = new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), body_info->exp_->UnEx());
    stm = frame::ProcEntryExit1(func_entry->level_->frame_, stm);

    // save the function frags
    frags->PushBack(new frame::ProcFrag(stm, func_entry->level_->frame_));
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // translate the initial value
  tr::ExpAndTy *init_info = init_->Translate(venv, tenv, level, label, errormsg);
  
  // alloc space
  tr::Access *access = tr::Access::AllocLocal(level, this->escape_);

  // TODO: type checking
  type::Ty *var_ty = nullptr;
  if(this->typ_) {
    var_ty = tenv->Look(typ_)->ActualTy();
  }

  // add to the environment
  type::Ty *init_ty = init_info->ty_->ActualTy();
  if(var_ty) {
    init_ty = var_ty;
  }
  
  venv->Enter(this->var_, new env::VarEntry(access, init_ty));

  // get the var
  tr::Exp *dst_exp = new tr::ExExp(access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer()))); 

  return new tr::NxExp(new tree::MoveStm(dst_exp->UnEx(), init_info->exp_->UnEx()));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // add the types to the environment
  std::list<absyn::NameAndTy *> type_list = types_->GetList();
  for(auto type : type_list) {
    tenv->Enter(type->name_, new type::NameTy(type->name_, nullptr));
  }

  for(auto &type : type_list) {
    type::NameTy *name_ty = static_cast<type::NameTy*>(tenv->Look(type->name_));
    name_ty->ty_ = type->ty_->Translate(tenv, errormsg);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(name_);
  return new type::NameTy(name_, ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::FieldList *field_list = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(field_list);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  type::Ty *ty = tenv->Look(array_);
  return new type::ArrayTy(ty);
}

} // namespace absyn
