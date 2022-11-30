#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  auto instrs = instr_list_->GetList();
  std::unordered_map<assem::Instr*, graph::Node<assem::Instr>*> instr_node_map;
  std::unordered_map<temp::Label*, graph::Node<assem::Instr>*> label_node_map;
  
  graph::Node<assem::Instr> *prev = nullptr;
  for(auto instr : instrs) {
    graph::Node<assem::Instr> *cur = flowgraph_->NewNode(instr);
    instr_node_map[instr] = cur;

    if(prev) {
      flowgraph_->AddEdge(prev, cur);
    }

    if(typeid(*instr) == typeid(assem::LabelInstr)) {
      assem::LabelInstr *label_instr = static_cast<assem::LabelInstr*>(instr);
      label_node_map[label_instr->label_] = cur;
    }

    // if jmp instr and jump target not empty
    if(typeid(*instr) == typeid(assem::OperInstr)) {
      assem::OperInstr *oper_instr = static_cast<assem::OperInstr*>(instr);

      if(oper_instr->assem_.find("jmp") != std::string::npos) {
        prev = nullptr;
      } else {
        prev = cur;
      }
    }
  }

  // associate jump with jump target
  for(auto instr : instrs) {
    if(typeid(*instr) == typeid(assem::OperInstr)) {
      assem::OperInstr *oper_instr = static_cast<assem::OperInstr*>(instr);

      if(oper_instr->jumps_) {
        std::vector<temp::Label *> labels = *oper_instr->jumps_->labels_; 
        for(auto label : labels) {
          assert(label_node_map.count(label));
          assert(instr_node_map.count(instr));

          flowgraph_->AddEdge(instr_node_map[instr], label_node_map[label]);
        } 
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ != nullptr? dst_ : new temp::TempList();
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ != nullptr? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ != nullptr? src_ : new temp::TempList();
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ != nullptr? src_ : new temp::TempList();
}
} // namespace assem
