#ifndef TIGER_RUNTIME_GC_ROOTS_H
#define TIGER_RUNTIME_GC_ROOTS_H

#include <iostream>
#include <map>
#include <unordered_set>
#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace gc {

const std::string GC_ROOTS = "GLOBAL_GC_ROOTS";

struct PointerMap {
  std::string label;
  std::string return_label;
  std::string next_label;
  std::string frame_size;
  std::vector<int> offsets;
  bool is_main = false;
};

class Roots {
  // TODO(lab7): define some member and methods here to keep track of gc roots;
private:
  assem::InstrList *il_;
  frame::Frame *frame_;
  fg::FGraphPtr flowgraph_;
  std::vector<int> escape_var_;
  temp::Map *color_;
  ///@note address_in and address_out, whether it is necessary?
  std::map<fg::FNodePtr, std::unordered_set<int>> address_in_;
  std::map<fg::FNodePtr, std::unordered_set<int>> address_out_;
  std::map<fg::FNodePtr, temp::TempList*> temp_in_;
  std::map<assem::Instr*, std::unordered_set<int>> valid_addrs;
  std::map<assem::Instr*, std::unordered_set<temp::Temp*>> valid_temps;

  void CheckDefAndUse(assem::Instr *instr, int  &def, int &use, bool &found_def, bool &found_use) {
    std::string raw_assem = instr->getAssem();
    std::size_t space_pos = raw_assem.find(' ');
    std::size_t comma_pos = raw_assem.find(',');
    if (space_pos == std::string::npos || comma_pos == std::string::npos || raw_assem.find("movq") == std::string::npos) {
      return;
    }

    std::string src_str = raw_assem.substr(space_pos + 1, comma_pos - space_pos - 1);
    std::string dst_str = raw_assem.substr(comma_pos + 2);

    // check use
    if (src_str.find("framesize") != std::string::npos && src_str[0] == '(') {
      std::size_t plus_op_pos = src_str.find('-');
      std::size_t minus_op_pos = src_str.find('+');
      std::size_t close_pos = src_str.find(')');

      if (plus_op_pos != std::string::npos && close_pos != std::string::npos) {
        found_use = true;
        std::string offset = src_str.substr(plus_op_pos + 1, close_pos - plus_op_pos);
        use = stoi(offset);
      } else if (minus_op_pos != std::string::npos && close_pos != std::string::npos) {
        found_use = true;
        std::string offset = src_str.substr(minus_op_pos, close_pos - minus_op_pos);
        use = stoi(offset);
      }

    } else if (dst_str.find("framesize") != std::string::npos && dst_str[0] == '(') {
      // check def 
      std::size_t plus_op_pos = dst_str.find('-');
      std::size_t minus_op_pos = dst_str.find('+');
      std::size_t close_pos = dst_str.find(')');

      if (plus_op_pos != std::string::npos && close_pos != std::string::npos) {
        found_def = true;
        std::string offset = dst_str.substr(plus_op_pos + 1, close_pos - plus_op_pos);
        def = stoi(offset);
      } else if (minus_op_pos != std::string::npos && close_pos != std::string::npos) {
        found_def = true;
        std::string offset = dst_str.substr(minus_op_pos, close_pos - minus_op_pos);
        def = stoi(offset);
      }
    }
  }
   
public:
  Roots(assem::InstrList *il, frame::Frame *frame, fg::FGraphPtr fg, std::vector<int> escapes, temp::Map *color):
    il_(il), frame_(frame), flowgraph_(fg), escape_var_(escapes), color_(color) {}
  
  void AddressLiveness() {
    std::list<fg::FNodePtr> f_nodes = flowgraph_->Nodes()->GetList();

    for (auto node : f_nodes) {
      address_in_[node] = std::unordered_set<int>();
      address_out_[node] = std::unordered_set<int>();
    }

    int node_count = address_in_.size(), same_count = 0, use = 0, def = 0;
    std::unordered_set<int> prev_in, prev_out; 
    bool found_use = false, found_def = false;
    while (true) {
      same_count = 0;
      for (auto node : f_nodes) {
        assem::Instr *instr = node->NodeInfo();
        if (typeid(*instr) == typeid(assem::LabelInstr)) {
          continue;
        }
        prev_in = address_in_[node];
        prev_out = address_out_[node];

        
        CheckDefAndUse(instr, def, use, found_def, found_use);

        address_out_[node] = std::unordered_set<int>();
        // generate address out
        std::list<fg::FNodePtr> succ_list = node->Succ()->GetList();
        for (auto succ : succ_list) {
          assert(address_in_.count(succ));
          address_out_[node].insert(address_in_[succ].begin(), address_in_[succ].end());
        }

        // generate address in
        address_in_[node] = address_out_[node];
        if (found_def) {
          address_in_[node].erase(def);
        }
        if (found_use) {
          address_in_[node].insert(use);
        }

        // check whether change
        if (prev_in == address_in_[node] && prev_out == address_out_[node]) {
          ++same_count;
        }
      }

      if (same_count == node_count) {
        break;
      }
    }
  }
  

  void TempLiveness() {
    // TODO: maybe need to modify
    live::LiveGraphFactory livegraph_factory(flowgraph_);
    livegraph_factory.Liveness();
    auto in_table = livegraph_factory.in_.get();

    std::list<fg::FNodePtr> f_nodes = flowgraph_->Nodes()->GetList();
    for (auto node : f_nodes) {
      temp_in_[node] = in_table->Look(node);
    }
  }


  void HandleCallRelatedPointer() {
    temp::TempList *callee_saves = reg_manager->CalleeSaves();
    std::list<temp::Temp*> callee_save_list = callee_saves->GetList();

    std::list<fg::FNodePtr> f_nodes = flowgraph_->Nodes()->GetList();
    bool is_return_pos = false;
    for (auto node : f_nodes) {
      assem::Instr *instr = node->NodeInfo();
      if (typeid(*instr) == typeid(assem::OperInstr) && instr->getAssem().find("call") != std::string::npos) {
        is_return_pos = true;
        continue;
      } 
      if (is_return_pos) {
        is_return_pos = false;

        valid_addrs[instr] = address_in_[node];
        valid_temps[instr] = std::unordered_set<temp::Temp*>();

        assert(temp_in_.count(node));
        auto temp_list = temp_in_[node]->GetList();
        
        for (auto temp : temp_in_[node]->GetList()) {
          if (temp->store_pointer_ && (std::find(callee_save_list.begin(), callee_save_list.end(), temp) != callee_save_list.end())) {
            valid_temps[instr].insert(temp);
          }
        }
      }
    }
  }


  ///@brief move callee-save register which store pointer to frame
  void RewriteProgram() {
    std::list<assem::Instr*> instr_list = il_->GetList();
    assem::InstrList *new_instrs = new assem::InstrList();

    for (auto it = instr_list.begin(); it != instr_list.end(); ++it) {
      auto instr = *it;
      if (typeid(*instr) == typeid(assem::MoveInstr) || typeid(*instr) == typeid(assem::LabelInstr) || instr->getAssem().find("call") == std::string::npos) {
        new_instrs->Append(instr);
        continue;
      }

      new_instrs->Append(instr);
      if (std::next(it) == instr_list.end()) {
        continue;
      }

      auto next_instr = *std::next(it);

      assert(valid_temps.count(next_instr));
      for (auto temp : valid_temps[next_instr]) {
        int offset = frame_->s_offset;
        frame::Access *new_access = frame_->AllocLocal(true);
        assert(new_access != nullptr);
        new_access->SetStorePointer();

        valid_addrs[next_instr].insert(offset);

        std::string *reg_name = reg_manager->GetRegisterName(temp);
        assert(reg_name != nullptr);

        std::string assem = "movq " + *reg_name + ", (" + frame_->label_->Name() + "_framesize" + std::to_string(offset) + ")(%rsp)";
        auto new_instr = new assem::OperInstr(assem, nullptr, nullptr, nullptr);
        new_instrs->Append(new_instr);
      }
    }

    il_ = new_instrs;
  } 


  std::vector<PointerMap> generatePointerMap() {
    AddressLiveness();
    TempLiveness();
    HandleCallRelatedPointer();
    RewriteProgram();

    std::vector<PointerMap> result;
    bool is_main = false;
    std::string frame_name = frame_->label_->Name();
    if (frame_name == "tigermain") {
      is_main = true;
    }

    for (auto elem : valid_addrs) {
      PointerMap new_map;
      new_map.frame_size = frame_name + "_framesize";
      assert(typeid(*elem.first) == typeid(assem::LabelInstr));
      new_map.return_label = static_cast<assem::LabelInstr*>(elem.first)->label_->Name();
      new_map.label = "Lmap_" + new_map.return_label;

      new_map.is_main = is_main;

      new_map.offsets = std::vector<int>();
      for (auto offset : elem.second) {
        new_map.offsets.emplace_back(offset);
      }

      result.emplace_back(new_map);
    }

    // fill next label
    int n = result.size();
    for (int i = 0; i < n - 1; ++i) {
      result[i].next_label = result[i + 1].label;
    }

    result[n - 1].next_label = "0";
    return result;
  }


  assem::InstrList *getInstrList() {
    return il_;
  }
};

}

#endif // TIGER_RUNTIME_GC_ROOTS_H