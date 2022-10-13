#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  
  if(entry && typeid(*entry) == typeid(env::VarEntry)) {
    return (static_cast<env::VarEntry*>(entry))->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }
  
  return type::IntTy::Instance();
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::RecordTy *record_ty;

  if(!ty->IsSameType(record_ty)) {
    errormsg->Error(pos_, "not record type %s", sym_->Name().data());
  } else {
    std::list<type::Field *> field_list = (static_cast<type::RecordTy*>(ty))->fields_->GetList();
    for(auto elem : field_list) {
      if(elem->name_ == sym_) {
        return elem->ty_->ActualTy();
      }
    }
    errormsg->Error(pos_, "not %s according type", sym_->Name().data());
  }

  return type::IntTy::Instance();
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::ArrayTy *array_ty;

  if(!ty->IsSameType(array_ty)) {
    errormsg->Error(pos_, "not have a array type");
  } else {
    type::Ty *exp_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if(!exp_ty->IsSameType(type::IntTy::Instance())) {
      errormsg->Error(pos_, "subscript not have int type");
    } else {
      return (static_cast<type::ArrayTy*>(ty))->ty_->ActualTy();
    }
  }

  return type::IntTy::Instance();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  return ty;
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if(!(entry && typeid(*entry) == typeid(env::FunEntry))) {
    errormsg->Error(pos_, "call exp not have a correct id type");
  } else {
    std::list<type::Ty *> ty_list = (static_cast<env::FunEntry*>(entry))->formals_->GetList();
    std::list<Exp *> arg_list = args_->GetList();
    
    auto itr_1 = ty_list.begin();
    auto itr_2 = arg_list.begin();

    while(itr_1 != ty_list.end()) {
      // absyn::Exp* arg = *itr_2;
      // type::Ty* param_type = *itr_1;
      if(itr_2 == arg_list.end()) {
        errormsg->Error(pos_, "function call not have enough param");
        break;
      }
      type::Ty *arg_type = (*itr_2)->SemAnalyze(venv, tenv, labelcount, errormsg);
      if(!arg_type->IsSameType(*itr_1)) {
        errormsg->Error(pos_, "function params type not the same as want");
        break;
      }
      ++itr_2;
      ++itr_1;
    }

    if(itr_2 != arg_list.end()) {
      errormsg->Error(pos_, "function call have too much param");
    } else {
      return (static_cast<env::FunEntry*>(entry))->result_->ActualTy();
    }
  } 
  return type::IntTy::Instance();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg)->ActualTy();
  switch (oper_) {
    case absyn::PLUS_OP:
    case absyn::MINUS_OP:
    case absyn::TIMES_OP:
    case absyn::DIVIDE_OP:
    case absyn::AND_OP:
    case absyn::OR_OP:
      if(typeid(*left_ty) != typeid(type::IntTy)) {
        errormsg->Error(left_->pos_, "left integer required");
      } 
      if(typeid(*right_ty) != typeid(type::IntTy)) {
        errormsg->Error(right_->pos_, "right integer required");
      }
      break;
    case absyn::LT_OP:
    case absyn::LE_OP:
    case absyn::GT_OP:
    case absyn::GE_OP:
      if(typeid(*left_ty) != typeid(type::IntTy) && typeid(*left_ty) != typeid(type::StringTy)) {
        errormsg->Error(left_->pos_, "left integer or string required");
      }
      if(typeid(*right_ty) != typeid(type::IntTy) && typeid(*right_ty) != typeid(type::StringTy)) {
        errormsg->Error(right_->pos_, "right integer or string required");
      }
      if(!left_ty->IsSameType(right_ty)) {
        errormsg->Error(pos_, "same type required");
      }
      break;
    default:
      if(!left_ty->IsSameType(right_ty)) {
        errormsg->Error(pos_, "same type required");
      }
      break;
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  type::Ty *ret_ty = ty;
  if(!(ty && typeid(*ty) == typeid(type::RecordTy))) {
    errormsg->Error(pos_, "the type is not correct!");
    ret_ty = type::IntTy::Instance();
  } else {
    std::list<absyn::EField *> efeild_list = fields_->GetList();
    std::list<type::Field *> field_list = (static_cast<type::RecordTy*>(ty))->fields_->GetList();

    auto itr_1 = field_list.begin();
    auto itr_2 = efeild_list.begin();

    while(itr_1 != field_list.end()) {
      // judge whether itr_2 reach the end
      if(itr_2 == efeild_list.end()) {
        errormsg->Error(pos_, "the field list is too short");
        break;
      }

      type::Ty *field_type = (*itr_2)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
      std::string name_1 = (*itr_1)->name_->Name();
      std::string name_2 = (*itr_2)->name_->Name();

      // compare the name
      if(name_1 != name_2) {
        errormsg->Error(pos_, "the name %s in field list is not match %s", name_2, name_1);
        ret_ty = type::IntTy::Instance();
        break;
      }

      // compare the type
      if(!(*itr_1)->ty_->IsSameType(field_type)) {
        errormsg->Error(pos_, "the type in field list is not match");
        ret_ty = type::IntTy::Instance();
        break;
      }
      ++itr_1;
      ++itr_2;
    }

    if(itr_2 != efeild_list.end()) {
      errormsg->Error(pos_, "the field list is too long");
      ret_ty = type::IntTy::Instance();
    }
  }

  return ret_ty;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if(seq_ == nullptr) {
    return type::VoidTy::Instance();
  }

  std::list<absyn::Exp *> exp_list = seq_->GetList();
  type::Ty *ty;
  for(auto elem : exp_list) {
    ty = elem->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  return ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // judge loop
  type::Ty *left_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(typeid(*left_ty) == typeid(SimpleVar)) {
    SimpleVar *sim_var = static_cast<SimpleVar*>(var_);
    env::EnvEntry *entry = venv->Look(sim_var->sym_);
    
    if(entry) {
      errormsg->Error(pos_, "detect loop in assign");
      return type::IntTy::Instance();
    }
  }

  // verify left type equal to right type
  type::Ty *right_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!left_ty->IsSameType(right_ty)) {
    errormsg->Error(var_->pos_, "the left type and right type not match");
    return type::IntTy::Instance();
  }

  return left_ty;
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // judge the test part type
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "the test part's type must be itgeter");
    return type::IntTy::Instance();
  }

  type::Ty *then_ty = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(elsee_ == nullptr) {
    if(typeid(*then_ty) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "the then part's type must be void");
      return type::IntTy::Instance();
    }

    return then_ty;
  } else {
    type::Ty *else_ty = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);

    // then part and else part' type must be same
    if(!then_ty->IsSameType(else_ty)) {
      errormsg->Error(pos_, "the then part's type and else part's type not match");
      return type::IntTy::Instance();
    }

    return then_ty;
  }
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *test_ty = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if(typeid(*test_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "the while test part's type must be integer");
    return type::IntTy::Instance();
  }

  if(typeid(*body_ty) != typeid(type::VoidTy)) {
    errormsg->Error(pos_, "the while body part's type must be void");
    return type::IntTy::Instance();
  }

  return body_ty;
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // verify the Id is constant
  env::EnvEntry *entry = venv->Look(var_);
  if(!(entry && entry->readonly_)) {
    errormsg->Error(pos_, "the id in for loop must be constant");
    return type::IntTy::Instance();
  } else {
    type::Ty *lo_ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *hi_ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);

    // high type and low type must same
    if(!lo_ty->IsSameType(hi_ty)) {
      errormsg->Error(pos_, "the high type and low type must be same");
      return type::IntTy::Instance();
    }

    // the edge's type must be integer
    if(typeid(*lo_ty) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "the edge's type must be integer");
      return type::IntTy::Instance();
    }

    venv->BeginScope();
    tenv->BeginScope();
    venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
    type::Ty *body_ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    // TODO: here I am not sure
    if(typeid(*body_ty) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "the for body's type must be void");
      return type::IntTy::Instance();
    }
    tenv->EndScope();
    venv->EndScope();

    return type::VoidTy::Instance();
  }

}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  for(Dec *dec : decs_->GetList()) {
    dec->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  type::Ty *result;
  if(!body_) {
    result = type::VoidTy::Instance();
  } else {
    result = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  tenv->EndScope();
  venv->EndScope();
  return result;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *array_ty = tenv->Look(typ_)->ActualTy();
  if(array_ty == nullptr) {
    errormsg->Error(pos_, "the array exp's type is undefined");
    return type::IntTy::Instance();
  }

  type::Ty *size_ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(typeid(*size_ty) != typeid(type::IntTy)) {
    errormsg->Error(pos_, "the array size's type must be integer");
    return type::IntTy::Instance();
  }

  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if(!init_ty->IsSameType(array_ty)) {
    errormsg->Error(pos_, "the initial data type must same as array type");
    return type::IntTy::Instance();
  }

  return array_ty;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  std::list<FunDec *> func_dec_list = functions_->GetList();
  for(auto func : func_dec_list) {
    // enter function name, param and value
    if(venv->Look(func->name_) != nullptr) {
      errormsg->Error(func->pos_, "the function name has been declared");
      return;
    } 

    absyn::FieldList *params = func->params_;
    type::Ty *result_ty;
    if(func->result_ != nullptr) 
      result_ty = tenv->Look(func->result_);
    else {
      result_ty = type::VoidTy::Instance();
    }

    type::TyList *formals = params->MakeFormalTyList(tenv, errormsg);
    venv->Enter(func->name_, new env::FunEntry(formals, result_ty));
  }

  // the second pass, enter the params
  for(auto func : func_dec_list) {
    absyn::FieldList *params = func->params_;
    type::TyList *formals = params->MakeFormalTyList(tenv, errormsg);

    venv->BeginScope();
    auto formal_it = formals->GetList().begin();
    auto param_it = params->GetList().begin();
    for(;param_it != params->GetList().end() && formal_it != formals->GetList().end(); param_it++, formal_it++) {
      venv->Enter((*param_it)->name_, new env::VarEntry(*formal_it));
    }
    type::Ty *ty;
    ty = func->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    venv->EndScope();
  }
}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *init_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if(typ_ == nullptr) {
    // the init shouldn't be nil
    if(typeid(*init_ty) == typeid(type::NilTy)) {
      errormsg->Error(init_->pos_, "if the type of exp is nilty, the type id must be a record type");
      return;
    }
    venv->Enter(var_, new env::VarEntry(init_ty));
    return;
  } else {
    type::Ty *dec_ty = tenv->Look(typ_);
    if(dec_ty == nullptr) {
      errormsg->Error(pos_, "the type %s is undefined", typ_->Name().data());
    } else {
      dec_ty = dec_ty->ActualTy();
    }

    // check nil
    if(typeid(*init_ty) == typeid(type::NilTy)) {
      if(typeid(*dec_ty) != typeid(type::RecordTy)) {
        errormsg->Error(init_->pos_, "if the type of exp is nilty, the type id must be a record type");
      } else {
        venv->Enter(var_, new env::VarEntry(init_ty));
      }   
      return;
    }

    // check compatible
    if(typeid(*init_ty) != typeid(*dec_ty)) {
      errormsg->Error(pos_, "type_id and type of exp are compatible");
    } else {
      venv->Enter(var_, new env::VarEntry(init_ty));
    }
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  std::list<NameAndTy *> name_and_ty_list = types_->GetList();
  
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}
} // namespace sem
