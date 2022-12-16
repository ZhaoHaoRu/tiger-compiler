#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

TempListPtr SmartUnion(TempListPtr list1, TempListPtr list2) {
    auto res = std::make_shared<temp::TempList>();
    std::list<temp::Temp*> temp_list = list1->GetList();

    for(auto temp : temp_list) {
      res->Append(temp);
    }

    if(list2) {
      auto temps = list2->GetList();
  
      for(auto temp : temps) {
        if(!res->Contain(temp)) {
          res->Append(temp);
        }
      }
    }
  
  return res;
}


TempListPtr SmartDiff(TempListPtr list1, TempListPtr list2) {
  auto res = std::make_shared<temp::TempList>();
  std::list<temp::Temp*> temp_list = list1->GetList();
  for(auto temp : temp_list) {
    if(!list2->Contain(temp)) {
      res->Append(temp);
    }
  }
  return res;
}


bool SmartEqual(TempListPtr list1, TempListPtr list2) {
  auto temp_list = list1->GetList();
  auto another_temp_list = list2->GetList();

  if(temp_list.size() != another_temp_list.size()) {
    return false;
  }

  auto it1 = temp_list.begin();
  auto it2 = another_temp_list.begin();

  for(auto elem : temp_list) {
    if(!list2->Contain(elem)) {
      return false;
    }
  }
  return true;
}


bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Diff(MoveList *list) {
  auto *res = new MoveList();
  for(auto move : move_list_) {
    if(!list->Contain(move.first, move.second)) {
      res->move_list_.push_back(move);
    }
  }
  return res;
}

///@note modify for lab7, use smart pointer to manage memory
void LiveGraphFactory::SmartLiveMap() {
  std::unordered_map<fg::FNodePtr, TempListPtr> in, out;
  int total_count = 0;
  for(auto node : flowgraph_->Nodes()->GetList()) {
    in[node] = std::make_shared<temp::TempList>();
    out[node] = std::make_shared<temp::TempList>();
    ++total_count;
  }

  int counter = 0;
  while(true) {
    auto flowgraph_node_list = flowgraph_->Nodes()->GetList();

    for(auto node : flowgraph_->Nodes()->GetList()) {
      TempListPtr prev_in = in[node];
      TempListPtr prev_out = out[node];
      TempListPtr use = node->NodeInfo()->SmartUse();
      TempListPtr def = node->NodeInfo()->SmartDef();

      // out[n] - def[n]
      assert(prev_out);
      TempListPtr mid = SmartDiff(prev_out, def);
      // use[n] U (out[n] - def[n])
      TempListPtr new_in = SmartUnion(mid, use);
      in[node] = new_in;

      // out[n] = U in[s]
      TempListPtr new_out = nullptr;
      auto succ_list = node->Succ()->GetList();

      for(auto succ : succ_list) {
        TempListPtr succ_in = in[succ];
        if(new_out == nullptr) {
          new_out = SmartUnion(succ_in, prev_out);
        } else {
          new_out = SmartUnion(new_out, succ_in);
        }
      }

      if(new_out == nullptr) {
        new_out = prev_out;
      }

      assert(new_out != nullptr);
      out[node] = new_out;

      // check whether reach fix point for this node
      if(new_in && prev_in && new_out && prev_out && SmartEqual(new_in, prev_in) && SmartEqual(new_out, prev_out)) {
        ++counter;
      }
    }

    printf("the counter: %d, the total count: %d\n", counter, total_count);
    if(counter == total_count) {
      break;
    } else {
      counter = 0;
    }
  }

  // put into in_ and out_
  for(auto node : flowgraph_->Nodes()->GetList()) {
    // assert(!in[node]->CheckEmpty());
    temp::TempList *new_in = new temp::TempList();
    temp::TempList *new_out = new temp::TempList();
    for (auto temp : in[node]->GetList()) {
      new_in->Append(temp);
    }
    for (auto temp : out[node]->GetList()) {
      new_out->Append(temp);
    }
    in_->Enter(node, new_in);
    // assert(!out[node]->CheckEmpty());
    out_->Enter(node, new_out);
  }

  printf("finish SmartLiveMap\n");
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  int total_count = 0;
  for(auto node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
    ++total_count;
  }

  int counter = 0;
  while(true) {
    auto flowgraph_node_list = flowgraph_->Nodes()->GetList();

    for(auto node : flowgraph_->Nodes()->GetList()) {
      temp::TempList *prev_in = in_->Look(node);
      temp::TempList *prev_out = out_->Look(node);
      temp::TempList *use = node->NodeInfo()->Use();
      temp::TempList *def = node->NodeInfo()->Def();

      // out[n] - def[n]
      assert(prev_out);
      temp::TempList *mid = prev_out->Diff(def);
      // use[n] U (out[n] - def[n])
      temp::TempList *new_in = mid->Union(use);
      in_->Set(node, new_in);

      // out[n] = U in[s]
      temp::TempList *new_out = nullptr;
      auto succ_list = node->Succ()->GetList();
      for(auto succ : succ_list) {
        temp::TempList *succ_in = in_->Look(succ);

        if(new_out == nullptr) {
          new_out = succ_in->Union(prev_out);
        } else {
          new_out = new_out->Union(succ_in);
        }
      }

      if(new_out == nullptr) {
        new_out = prev_out;
      }
      out_->Set(node, new_out);

      // check whether reach fix point for this node
      if(new_in && prev_in && new_out && prev_out && new_in->Equal(prev_in) && new_out->Equal(prev_out)) {
        ++counter;
      }
    }

    printf("the counter: %d, the total count: %d\n", counter, total_count);
    if(counter == total_count) {
      break;
    } else {
      counter = 0;
    }
  }
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  // add the precolored register
  auto precolored_list = reg_manager->Registers()->GetList(); // skip rsp
  for(auto temp : precolored_list) {
    live::INode *new_node = live_graph_.interf_graph->NewNode(temp);
    temp_node_map_->Enter(temp, new_node);
  }

  // add edges bewteen every precolored register pair
  assert(!precolored_list.empty());
  for(auto it = precolored_list.begin(); it != precolored_list.end(); ++it) {
    for(auto other_it = std::next(it); other_it != precolored_list.end(); ++other_it) {
      // add for debug
      assert(temp_node_map_->Look(*it) != nullptr);
      assert(temp_node_map_->Look(*other_it) != nullptr);

      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(*it), temp_node_map_->Look(*other_it));
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(*other_it), temp_node_map_->Look(*it));
    }
  }

  // add all node in flowgraph(not repeat and not rsp) to live graph
  auto instr_node_list = flowgraph_->Nodes()->GetList();
  for(auto node : instr_node_list) {
    temp::TempList *node_temp_list = node->NodeInfo()->Def()->Union(node->NodeInfo()->Use());
    for(auto temp : node_temp_list->GetList()) {
      if(temp == reg_manager->StackPointer()) {
        continue;
      }
      if(temp_node_map_->Look(temp) != nullptr) {
        continue;
      }
      live::INode *new_node = live_graph_.interf_graph->NewNode(temp);
      temp_node_map_->Enter(temp, new_node);
    }
  }

  // travel every instruction and add edge
  for(auto node : instr_node_list) {
    assert(node != nullptr);
    temp::TempList *out_live = out_->Look(node);
    temp::TempList *def = node->NodeInfo()->Def();
    auto def_list = def->GetList();

    if(typeid(*node->NodeInfo()) != typeid(assem::MoveInstr)) {
      // add edge between live out regs and defs
      auto out_live_list = out_live->GetList();

      for(auto live_reg : out_live_list) {
      // for(auto live_reg : live_and_def_list) {
        for(auto def_reg : def_list) {
          if(live_reg == reg_manager->StackPointer() || def_reg == reg_manager->StackPointer()
              || live_reg == def_reg) {
            continue;
          }

          auto live_node = temp_node_map_->Look(live_reg), def_node = temp_node_map_->Look(def_reg);
          assert(live_node != nullptr);
          assert(def_node != nullptr);

          live_graph_.interf_graph->AddEdge(live_node, def_node);
          live_graph_.interf_graph->AddEdge(def_node, live_node);
        }
      }

    } else {
      temp::TempList *use = node->NodeInfo()->Use();
      auto use_list = use->GetList();
      for(auto def_reg : def_list) {
        // add all node in def_list or use_list to movelist
        auto def_reg_node = temp_node_map_->Look(def_reg);
        for(auto use_reg : use_list) {
          auto use_reg_node = temp_node_map_->Look(use_reg);

          assert(def_reg_node != nullptr);
          assert(use_reg_node != nullptr);
          if(live_graph_.moves->Contain(use_reg_node, def_reg_node)) {
            continue;
          }

          if(def_reg == reg_manager->StackPointer() || use_reg == reg_manager->StackPointer() || def_reg == use_reg) {
            continue;
          }

          live_graph_.moves->Append(use_reg_node, def_reg_node);
        }

        // add edge between def and out live
        temp::TempList *live_sub_use = out_live->Diff(node->NodeInfo()->Use());
        auto live_sub_use_list = live_sub_use->GetList();

        for(auto live_reg : live_sub_use_list) {
          assert(live_reg != nullptr);
          if(live_reg == reg_manager->StackPointer() || def_reg == reg_manager->StackPointer() || def_reg == live_reg) {
            continue;
          }

          auto live_reg_node = temp_node_map_->Look(live_reg);
          assert(live_reg_node != nullptr);

          live_graph_.interf_graph->AddEdge(def_reg_node, live_reg_node);
          live_graph_.interf_graph->AddEdge(live_reg_node, def_reg_node);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  // LiveMap();
  SmartLiveMap();
  printf("begin InterfGraph\n");
  InterfGraph();
  printf("finish InterfGraph\n");
}

} // namespace live
