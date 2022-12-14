#ifndef TIGER_RUNTIME_GC_ROOTS_H
#define TIGER_RUNTIME_GC_ROOTS_H

#include <iostream>
#include <map>
#include "tiger/liveness/liveness.h"
namespace gc {

const std::string GC_ROOTS = "GLOBAL_GC_ROOTS";


class Roots {
  // TODO(lab7): define some member and methods here to keep track of gc roots;
private:
  assem::InstrList *il_;
  frame::Frame *frame_;
  fg::FGraphPtr flowgraph_;
  std::vector<int> escape_var_;
  temp::Map *color;
  std::map<fg::FNodePtr, temp::TempList*> temp_in_;
   
};

}

#endif // TIGER_RUNTIME_GC_ROOTS_H