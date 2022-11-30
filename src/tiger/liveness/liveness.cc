#include "tiger/liveness/liveness.h"

extern frame::RegManager *reg_manager;

namespace live {

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

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  graph::Table<assem::Instr, temp::TempList> tmp_in, tmp_out;
  int total_count = 0;

  for(auto node : flowgraph_->Nodes()->GetList()) {
    in_->Enter(node, new temp::TempList());
    tmp_in.Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
    tmp_out.Enter(node, new temp::TempList());
    ++total_count;
  }

  int counter = 0;
  while(true) {
    for(auto node : flowgraph_->Nodes()->GetList()) {
      temp::TempList *prev_in = in_->Look(node);
      temp::TempList *prev_out = out_->Look(node);
      temp::TempList *use = node->NodeInfo()->Use();
      temp::TempList *def = node->NodeInfo()->Def();

      // out[n] - def[n]
      temp::TempList *mid = prev_in->Diff(def);
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
      out_->Set(node, new_out);

      // check whether reach fix point for this node
      if(new_in->Equal(prev_in) && new_out->Equal(prev_out)) {
        ++counter;
      }
    }

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
    temp::TempList *out_live = out_->Look(node);
    temp::TempList *def = node->NodeInfo()->Def();
    auto def_list = def->GetList();

    if(typeid(*node->NodeInfo()) != typeid(assem::MoveInstr)) {
      // add edge between live out regs and defs
      // TODO: why not live_out and def's union?
      auto out_live_list = out_live->GetList();
      for(auto live_reg : out_live_list) {
        for(auto def_reg : def_list) {
          if(live_reg == reg_manager->StackPointer() || def_reg == reg_manager->StackPointer()
              || live_reg == def_reg) {
            continue;
          }

          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(live_reg), temp_node_map_->Look(def_reg));
          live_graph_.interf_graph->AddEdge(temp_node_map_->Look(def_reg), temp_node_map_->Look(live_reg));
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

        // add edge between def and live
        temp::TempList *live_sub_use = out_live->Diff(node->NodeInfo()->Use());
        auto live_sub_use_list = live_sub_use->GetList();
        for(auto live_reg : live_sub_use_list) {
          auto live_reg_node = temp_node_map_->Look(live_reg);
          assert(live_reg_node != nullptr);

          live_graph_.interf_graph->AddEdge(def_reg_node, live_reg_node);
          live_graph_.interf_graph->AddEdge(live_reg_node, def_reg_node);
        }
      }
    }
    // TODO: the change of live?
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
