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
  std::map<assem::Instr*, std::unordered_set<std::string>> valid_temps;

  void CheckDefAndUse(assem::Instr *instr, int  &def, int &use, bool &found_def, bool &found_use) {
    std::string raw_assem = instr->getAssem();
    std::size_t space_pos = raw_assem.find(' ');
    std::size_t comma_pos = raw_assem.find(',');
    if (space_pos == std::string::npos || comma_pos == std::string::npos || (raw_assem.find("movq") == std::string::npos && raw_assem.find("leaq") == std::string::npos)) {
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
        printf("instr: %s, use: %d\n", instr->getAssem().c_str(), use);
      } else if (minus_op_pos != std::string::npos && close_pos != std::string::npos) {
        found_use = true;
        std::string offset = src_str.substr(minus_op_pos, close_pos - minus_op_pos);
        use = stoi(offset);
        printf("instr: %s, use: %d\n", instr->getAssem().c_str(), use);
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


  ///@note merge some move related frame pointer
  void MergeMove() {
    assem::InstrList *new_instr_list = new assem::InstrList();
    std::list<assem::Instr *> instr_list = il_->GetList();
    auto it = instr_list.begin();

    while(it != instr_list.end()) {
      auto instr = *it;
      if (typeid(*instr) == typeid(assem::LabelInstr) || typeid(*instr) == typeid(assem::MoveInstr) || 
        instr->getAssem().find("leaq") == std::string::npos || instr->getAssem().find("framesize") == std::string::npos) {
        new_instr_list->Append(instr);
        ++it;
        continue;
      }

      auto next_it = std::next(it);
      if (next_it == instr_list.end() || typeid(**next_it) != typeid(assem::OperInstr) 
        || (*next_it)->getAssem().find("movq") == std::string::npos || (*next_it)->getAssem().find("$") == std::string::npos) {
        new_instr_list->Append(instr);
        ++it;
        continue;
      }

      auto next_next_it = std::next(it, 2);
      if (next_next_it == instr_list.end() || typeid(**next_next_it) != typeid(assem::OperInstr)
        || (*next_next_it)->getAssem().find("addq") == std::string::npos) {
        new_instr_list->Append(instr);
        ++it;
        continue;
      }

      auto next_instr = static_cast<assem::OperInstr*>(*next_it);
      auto next_next_instr = static_cast<assem::OperInstr*>(*next_next_it);

      ///@note actually it is dangerous, without prechecking
      temp::Temp *dst_reg1 = static_cast<assem::OperInstr*>(instr)->dst_->GetList().front();
      temp::Temp *dst_reg2 = next_instr->dst_->GetList().front();
      temp::Temp *src_reg3 = next_next_instr->src_->GetList().front();
      temp::Temp *dst_reg3 = next_next_instr->dst_->GetList().front();

      std::string *actual_dst1_color = color_->Look(dst_reg1);
      std::string *actual_dst2_color = color_->Look(dst_reg2);
      std::string *acutal_src3_color = color_->Look(src_reg3);
      std::string *actual_dst3_color = color_->Look(dst_reg3);

      if (actual_dst1_color == actual_dst3_color && actual_dst2_color == acutal_src3_color) {
        std::string move_assem = next_instr->getAssem();
        std::string leaq_assem = instr->getAssem();
        std::size_t begin_pos = move_assem.find("$"), end_pos = move_assem.find(",");
        std::string offset_str = move_assem.substr(begin_pos + 1, end_pos - begin_pos - 1);
        if (offset_str[0] != '-') {
          offset_str.insert(offset_str.begin(), '+');
        }

        std::size_t comma_pos = leaq_assem.find("("), space_pos = leaq_assem.find(" ");
        std::string frame_name = leaq_assem.substr(space_pos + 1, comma_pos - space_pos - 1);
        std::string new_assem = leaq_assem.substr(0, space_pos + 1) + "(" + frame_name + offset_str + ')' + leaq_assem.substr(comma_pos);
        // printf("new assem: %s\n", new_assem.c_str());
        auto new_instr = new assem::OperInstr(new_assem, static_cast<assem::OperInstr*>(instr)->dst_,
           static_cast<assem::OperInstr*>(instr)->src_, nullptr);
        
        new_instr_list->Append(new_instr);
        it = std::next(next_next_it);
      } else {
        new_instr_list->Append(instr);
        ++it;
        continue;
      }
    }

    il_ = new_instr_list;

    // update the flowgraph
    fg::FlowGraphFactory flowgraph_factory(il_);
    flowgraph_factory.AssemFlowGraph();
    fg::FGraphPtr flow_graph = flowgraph_factory.GetFlowGraph();
    flowgraph_ = flow_graph;
  }


  bool SetEqual(std::unordered_set<int> &set1, std::unordered_set<int> &set2) {
    if (set1.size() != set2.size()) {
      return false;
    }

    for (auto elem : set1) {
      if (!set2.count(elem)) {
        return false;
      }
    }

    return true;
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

    int node_count = f_nodes.size(), same_count = 0, use = 0, def = 0;
    std::unordered_set<int> prev_in, prev_out; 
    bool found_use = false, found_def = false;
    while (true) {
      same_count = 0;
      for (auto node : f_nodes) {
        assem::Instr *instr = node->NodeInfo();
        if (typeid(*instr) == typeid(assem::LabelInstr)) {
          ++same_count;
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
        if (SetEqual(prev_in, address_in_[node]) && SetEqual(prev_out, address_out_[node])) {
          ++same_count;
        }
      }

      printf("same count: %d, node_count: %d\n", same_count, node_count);
      if (same_count == node_count) {
        ///@note print for debug
        // for (auto node : f_nodes) {
        //   assem::Instr *instr = node->NodeInfo();
        //   if (typeid(*instr) != typeid(assem::LabelInstr)) {
        //     printf("instr: %s\n", instr->getAssem().c_str());
        //     printf("address in: ");
        //     for (auto elem : address_in_[node]) {
        //       printf("%d ", elem);
        //     }
        //     printf("\n");
        //     printf("address out: ");
        //     for (auto elem : address_out_[node]) {
        //       printf("%d ", elem);
        //     }
        //     printf("\n");
        //   }
        // }
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
    std::unordered_set<std::string*> callee_save_set;
    // generate the color set
    for (auto reg : callee_save_list) {
      std::string *name = reg_manager->GetRegisterName(reg);
      callee_save_set.insert(name);
    }

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

        // add pointers in frame
        // for (auto access : frame_->locals_) {
        //   auto in_frame_access = static_cast<frame::InFrameAccess*>(access);
        //   valid_addrs[instr].insert(in_frame_access->offset);
        // }

        valid_temps[instr] = std::unordered_set<std::string>();

        assert(temp_in_.count(node));
        auto temp_list = temp_in_[node]->GetList();
        
        for (auto temp : temp_in_[node]->GetList()) {
          if (temp->store_pointer_) {
            std::string *color = color_->Look(temp);
            if (callee_save_set.count(color)) {
              printf("the pointer in register found: the color %s, the temp %d\n", (*color).c_str(), temp->Int());
              valid_temps[instr].insert(*color);
            }
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
      auto next_next_instr = *std::next(it, 2);

      assert(valid_temps.count(next_instr));
      for (auto temp : valid_temps[next_instr]) {
        int offset = frame_->s_offset;
        frame::Access *new_access = frame_->AllocLocal(true);
        assert(new_access != nullptr);
        new_access->SetStorePointer();

        valid_addrs[next_instr].insert(offset);

        std::string assem = "movq " + temp + ", (" + frame_->label_->Name() + "_framesize" + std::to_string(offset) + ")(%rsp)";
        auto new_instr = new assem::OperInstr(assem, nullptr, nullptr, nullptr);
        new_instrs->Append(new_instr);
      }
    }

    il_ = new_instrs;
  } 


  std::vector<PointerMap> generatePointerMap() {
    printf("merge moves\n");
    // MergeMove();
    printf("begin address liveness\n");
    AddressLiveness();
    printf("begin temp liveness\n");
    TempLiveness();
    printf("begin call related\n");
    HandleCallRelatedPointer();
    printf("begin rewrite the program\n");
    RewriteProgram();
    printf("finish rewrite the program\n");

    std::vector<PointerMap> result;
    bool is_main = false;
    std::string frame_name = frame_->label_->Name();
    if (frame_name == "tigermain") {
      is_main = true;
    }

    printf("get line 378\n");
    for (auto elem : valid_addrs) {
      PointerMap new_map;
      new_map.frame_size = frame_name + "_framesize";
      printf("the assem: %s\n", elem.first->getAssem().c_str());
      printf("the offset: ");
      for (auto offset : elem.second) {
        printf("%d ", offset);
      }
      printf("\n");
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

    printf("get line 402\n");
    // fill next label
    int n = result.size();
    for (int i = 0; i < n - 1; ++i) {
      result[i].next_label = result[i + 1].label;
    }

    printf("get line 409\n");
    if (n != 0) {
      result[n - 1].next_label = "0";
    }
    return result;
  }


  assem::InstrList *getInstrList() {
    return il_;
  }
};

}

#endif // TIGER_RUNTIME_GC_ROOTS_H