#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  int depth = 0;
  root_->Traverse(env, depth);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  esc::EscapeEntry* elem = env->Look(sym_);
  if(elem) {
    if(elem->depth_ < depth) {
      *elem->escape_ = true;
    }
  }
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  esc::EscapeEntry* elem = env->Look(sym_);
  if(elem && elem->depth_ < depth) {
    *elem->escape_ = true;
  }
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // check the function label
  // esc::EscapeEntry* elem = env->Look(func_);
  // if(elem && elem->depth_ < depth) {
  //   *elem->escape_ = true;
  // }

  // check the arguments
  std::list<Exp *> exps = args_->GetList();
  for(auto exp : exps) {
    exp->Traverse(env, depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // check the fields
  std::list<EField *> fields = fields_->GetList();
  for(auto field : fields) {
    field->exp_->Traverse(env, depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  std::list<Exp *> exps = seq_->GetList();
  for(auto exp : exps) {
    exp->Traverse(env, depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if(elsee_) {
    then_->Traverse(env, depth);
  }
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);

  if(body_) {
    env->BeginScope();

    escape_ = false;
    env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
    body_->Traverse(env, depth);

    env->EndScope();
  }
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  env->BeginScope();
  std::list<Dec *> dec_list = decs_->GetList();
  for(auto dec : dec_list) {
    dec->Traverse(env, depth);
  }
  body_->Traverse(env, depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  return;
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  std::list<FunDec *> fun_dec_list = functions_->GetList();

  for(auto fun_dec : fun_dec_list) {
    env->BeginScope();
    depth++;

    std::list<Field *> fields = fun_dec->params_->GetList();
    for(auto field : fields) {
      field->escape_ = false;
      env->Enter(field->name_, new esc::EscapeEntry(depth, &field->escape_));
    }

    if(fun_dec->body_) {
      fun_dec->body_->Traverse(env, depth);
    }

    depth--;
    env->EndScope();
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));

  if(init_) {
    init_->Traverse(env, depth);
  }
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  std::list<NameAndTy *> name_and_ty_list = types_->GetList();

  for(auto name_and_ty : name_and_ty_list) {
    if(typeid(*(name_and_ty->ty_)) == typeid(type::RecordTy)) {
      absyn::RecordTy* record_ty = static_cast<absyn::RecordTy*>(name_and_ty->ty_);
      std::list<Field*> field_list = record_ty->record_->GetList();

      // TODO: I don't understand
      // for(auto field : field_list) {
      //   field->escape_ = false;
      //   env->Enter(name, new esc::EscapeEntry())
      // }
    }
  }
}

} // namespace absyn
