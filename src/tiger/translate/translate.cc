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
    tree::CjumpStm *cjump_stm = new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    tr::PatchList trues, falses;
    trues.DoPatch(cjump_stm->true_label_);
    falses.DoPatch(cjump_stm->false_label_);
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

    tr::PatchList trues, falses;
    trues.DoPatch(t); 
    falses.DoPatch(f);
    return new tree::EseqExp(
      new tree::MoveStm(
        new tree::TempExp(r), new tree::ConstExp(1)),
        new tree::EseqExp(
          cx_.stm_,
            new tree::EseqExp(
              new tree::LabelStm(f),
              new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r),
                new tree::ConstExp(0)),
                  new tree::EseqExp(new tree::LabelStm(t),
                    new tree::TempExp(r)
                  )
              )
            )
        )
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
                          main_level_.get(), new_label, errormsg_.get());

  // TODO: maybe not need to save
}

} // namespace tr

namespace absyn {
// for static link
tree::Exp *StaticLink(tr::Level *current, tr::Level *target) {
  tree::Exp *static_link = new tree::TempExp(reg_manager->FramePointer());

  while(current != target) {
    if(current == nullptr || !current->parent_) {
      return nullptr;
    }
    static_link = (*current->frame_->formals_.begin())->ToExp(static_link);
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

  tree::Exp *static_link = StaticLink(var_entry->access_->level_, level);
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
      tr::Exp *exp = new tr::ExExp(raw_exp);
      return new tr::ExpAndTy(exp, field->ty_->ActualTy());
    }
  }

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
  tr::Exp *exp = new tr::ExExp(new tree::NameExp(temp::LabelFactory::NamedLabel(str_)));
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
  std::list<tr::Exp*> arg_list;

  // translate the arguments
  std::list<absyn::Exp*> raw_arg_list = args_->GetList();
  for(auto arg : raw_arg_list) {
    tr::ExpAndTy 
  }
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
}

} // namespace absyn
