#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(stm1->MaxArgs(), stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  return stm2->Interp(stm1->Interp(t));
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *new_table = exp->Interp(t);
  return new_table->t->Update(id, new_table->i);

}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return exps->MaxArgs();
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  IntAndTable *table = exps->Interp(t);
  return table->t;
}

int A::IdExp::MaxArgs() const {
  return 1;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
  int i = t->Lookup(id);
  IntAndTable *result = new IntAndTable(i, t);
  return result;
}

int A::NumExp::MaxArgs() const {
  return 1;
}

IntAndTable *A::NumExp::Interp(Table *t) const {
  IntAndTable *result = new IntAndTable(num, t);
  return result;
}

int A::OpExp::MaxArgs() const {
  return 1;
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  IntAndTable *table1 = left->Interp(t);
  IntAndTable *table2 = right->Interp(table1->t);
  int result = 0;
  if(oper == 0) {
    result = table1->i + table2->i;
  } else if(oper == 1) {
    result = table1->i - table2->i;
  } else if(oper == 2) {
    result = table1->i * table2->i;
  } else if(oper == 3) {
    result = table1->i / table2->i;
  }
  // an error may incur
  IntAndTable *new_table = new IntAndTable(result, table2->t);
  return new_table;
}

int A::EseqExp::MaxArgs() const {
  return std::max(stm->MaxArgs(), exp->MaxArgs());
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  return exp->Interp(stm->Interp(t));
}

int A::PairExpList::MaxArgs() const {
  // refer to the example on book
  return exp->MaxArgs() + tail->MaxArgs();
}

int A::PairExpList::NumExps() {
  return 1 + tail->NumExps();
}

IntAndTable *A::PairExpList::Interp(Table *t) const{
  IntAndTable *table = exp->Interp(t);
  // PRINT!!!
  printf("%d ", table->i);
  return tail->Interp(table->t);
}

int A::LastExpList::MaxArgs() const {
  return exp->MaxArgs();
}

int A::LastExpList::NumExps() {
  return 1;
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
  IntAndTable *table = exp->Interp(t);
  // PRINT!!!
  printf("%d\n", table->i);
  return table;
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}  // namespace A
