#include "tiger/regalloc/color.h"

extern frame::RegManager *reg_manager;

namespace col {
/* TODO: Put your lab6 code here */
/*-------------------------- auxiliary functions ----------------------------------*/
    live::INodeListPtr Color::Adjacent(live::INodePtr n) {
        assert(adj_list.count(n));
        live::INodeListPtr node_list = adj_list[n];

        // calculate adjList[n] \ selectStack U coalescedNodes
        return node_list->Diff(select_stack->Union(coalesced_nodes));
    }

    live::MoveList* Color::NodeMoves(live::INodePtr n) {
        assert(move_list.count(n));
        live::MoveList *move_list_n = move_list[n];

        // calculate moveList[n] ∩ (activeMoves ∪ worklistMoves)
        return move_list_n->Intersect(active_moves->Union(worklist_moves));
    }

    bool Color::MoveRelated(live::INodePtr n) {
        return !NodeMoves(n)->GetList().empty();
    }

    void Color::Decrement(live::INodePtr m) {
        assert(degrees.count(m));

        int d = degrees[m];
        degrees[m] = d - 1;
        if(d == col::PRECOLORED_COUNT) {
            live::INodeListPtr adj_nodes = Adjacent(m);
            adj_nodes->Append(m);
            EnableMoves(adj_nodes);
            spill_worklist->DeleteNode(m);
            if(MoveRelated(m)) {
                freeze_worklist->Append(m);
            } else {
                simplify_worklist->Append(m);
            }
        }
    }

    void Color::EnableMoves(live::INodeListPtr nodes) {
        auto node_list = nodes->GetList();
        for(auto n : node_list) {
            auto node_move = NodeMoves(n);
            assert(node_move != nullptr);
            auto node_move_list = node_move->GetList();

            for(auto m : node_move_list) {
                if(active_moves->Contain(m.first, m.second)) {
                    active_moves->Delete(m.first, m.second);
                    worklist_moves->Append(m.first, m.second);
                }
            }
        }
    }

    live::INodePtr Color::GetAlias(live::INodePtr n) {
        if(coalesced_nodes->Contain(n)) {
            assert(alias.count(n));
            return GetAlias(alias[n]);
        } else {
            return n;
        }
    }

    void Color::AddWorkList(live::INodePtr u) {
        assert(u != nullptr);
        if(!precolored_regs.count(u->NodeInfo()) && !MoveRelated(u) && degrees[u] < col::PRECOLORED_COUNT) {
            freeze_worklist->DeleteNode(u);
            simplify_worklist->Append(u);
        }
    }

    bool Color::OK(live::INodePtr t, live::INodePtr r) {
        assert(degrees.count(t));
        std::pair<live::INodePtr, live::INodePtr> new_pair(t, r);
        return degrees[t] < col::PRECOLORED_COUNT || precolored_regs.count(t->NodeInfo()) || adj_set.count(new_pair);
    }

    bool Color::Conservertive(live::INodeListPtr nodes) {
        int k = 0;
        auto node_list = nodes->GetList();

        for(auto n : node_list) {
            assert(degrees.count(n));
            if(degrees[n] >= col::PRECOLORED_COUNT) {
                k += 1;
            }
        }
        return k < col::PRECOLORED_COUNT;
    }

    void Color::Combine(live::INodePtr u, live::INodePtr v) {
        if(freeze_worklist->Contain(v)) {
            freeze_worklist->DeleteNode(v);
        } else {
            spill_worklist->DeleteNode(v);
        }

        coalesced_nodes->Append(v);
        alias[v] = u;
        move_list[u] = move_list[u]->Union(move_list[v]);

        // TODO: whether it is available?
        EnableMoves(live::INodeListPtr(v));

        auto adj_list = Adjacent(v)->GetList();
        for(auto t : adj_list) {
            AddEdge(t, u);
            Decrement(t);
        }
        
        if(degrees[u] >= PRECOLORED_COUNT && freeze_worklist->Contain(u)) {
            freeze_worklist->DeleteNode(u);
            spill_worklist->Append(u);
        }
    }

    void Color::FreezeMoves(live::INodePtr u) {
        live::MoveList* node_moves = NodeMoves(u);
        assert(node_moves != nullptr);

        auto node_moves_list = node_moves->GetList();
        for(auto m : node_moves_list) {
            auto x = m.first, y = m.second;
            live::INodePtr v;
            if(GetAlias(y) == GetAlias(u)) {
                v = GetAlias(x);
            } else {
                v = GetAlias(y);
            }

            active_moves->Delete(x, y);
            frozen_moves->Append(x, y);

            if(NodeMoves(v)->GetList().empty() && degrees[v] < col::PRECOLORED_COUNT) {
                freeze_worklist->DeleteNode(v);
                simplify_worklist->Append(v);
            }
        }
    }

    void Color::InitColors(std::set<int> &ok_colors) {
        for(int i = 0; i <= PRECOLORED_COUNT; ++i) {
            if(i == 7) {
                continue;
            }
            ok_colors.insert(i);
        }
    }

/*-------------------------- public color functions ----------------------------------*/

    // TODO: maybe need to modify in the future
    Color::Color(live::LiveGraph live_graph): live_graph_(live_graph) {
        // pre-define the precolored regs for convenience
        temp::TempList *regs = reg_manager->Registers();
        auto reg_list = regs->GetList();
        for(auto reg : reg_list) {
            precolored_regs.insert(reg);
        }

        // init the pointers
        simplify_worklist = new live::INodeList();
        freeze_worklist = new live::INodeList();
        spill_worklist = new live::INodeList();
        spill_nodes = new live::INodeList();
        select_stack = new live::INodeList();
        coalesced_nodes = new live::INodeList();
        colored_nodes = new live::INodeList();
        initial_ = live_graph_.interf_graph->Nodes();

        worklist_moves = new live::MoveList();
        active_moves = new live::MoveList();
        coalesce_moves = new live::MoveList();
        constrained_moves = new live::MoveList();
        frozen_moves = new live::MoveList();
    }

    void Color::Build() {
        // TODO: need to do initalize work
        auto node_list = live_graph_.interf_graph->Nodes()->GetList();
        auto liveness_move_list = live_graph_.moves->GetList();

        // produce worklist moves
        worklist_moves = live_graph_.moves;

        // produce move list from liveness move list
        for(auto move : liveness_move_list) {
            live::INodePtr src = move.first;
            live::INodePtr dst = move.second;

            if(!move_list.count(src)) {
                move_list[src] = new live::MoveList();
            } 
            if(!move_list.count(dst)) {
                move_list[dst] = new live::MoveList();
            }
            move_list[src]->Append(src, dst);
            move_list[dst]->Append(src, dst);
        }

        // handle the degrees
        // first initialize the degrees
        for(auto node : node_list) {
            degrees[node] = 0;
        }

        // addedges
        for(auto node : node_list) {
            auto adj_list = node->Adj()->GetList();
            for(auto adj_node : adj_list) {
                AddEdge(node, adj_node);
            }
        }
    }

    void Color::AddEdge(live::INode *u, live::INode *v) {
        std::pair<live::INodePtr, live::INodePtr> node_pair(u, v), reverse_node_pair(v, u);
        if(u != v && !adj_set.count(node_pair)) {
            adj_set.insert(node_pair);
            adj_set.insert(reverse_node_pair);

            if(!precolored_regs.count(u->NodeInfo())) {
                if(!adj_list.count(u)) {
                    adj_list[u] = new live::INodeList();
                }
                adj_list[u]->Append(v);
                degrees[u] += 1;
            }

            if(!precolored_regs.count(v->NodeInfo())) {
                if(!adj_list.count(v)) {
                    adj_list[v] = new live::INodeList();
                }
                adj_list[v]->Append(u);
                degrees[v] += 1;
            }
        }
    }

    void Color::MakeWorkList() {
        auto node_list = initial_->GetList();
        for(auto node : node_list) {
            if(degrees[node] >= col::PRECOLORED_COUNT) {
                spill_worklist->Append(node);
            } else if(MoveRelated(node)) {
                freeze_worklist->Append(node);
            } else {
                simplify_worklist->Append(node);
            }
        }
    }

    void Color::Simplify() {
        auto simlify_worklist_nodes = simplify_worklist->GetList();
        for(auto node : simlify_worklist_nodes) {
            select_stack->Prepend(node);    // simulate the stack
            
            live::INodeListPtr adj_nodes = Adjacent(node);
            auto adj_nodes_list = adj_nodes->GetList();
            for(auto adj_node : adj_nodes_list) {
                Decrement(adj_node);
            }
            simplify_worklist->DeleteNode(node);
        }
    }

    void Color::Coalesce() {
        auto m = worklist_moves->GetList().front();
        std::pair<live::INodePtr, live::INodePtr> new_pair(m); 
        live::INodePtr x = GetAlias(m.first);
        live::INodePtr y = GetAlias(m.second);
        if(precolored_regs.count(y->NodeInfo())) {
            new_pair.first = y;
            new_pair.second = x;
        } else {
            new_pair.first = x;
            new_pair.second = y;
        }

        // TODO: add for debug
        assert(new_pair.second != nullptr); 
        worklist_moves->Delete(m.first, m.second);

        // George
        auto adj_list = Adjacent(new_pair.second)->GetList();
        bool satisfy_george = true;
        for(auto t : adj_list) {
            if(!OK(t, new_pair.first)) {
                satisfy_george = false;
                break;
            }
        }

        // briggs
        bool satisfy_briggs = Conservertive(Adjacent(new_pair.first)->Union(Adjacent(new_pair.second)));

        if(x == y) {
            coalesce_moves->Append(new_pair.first, new_pair.second);
            AddWorkList(new_pair.first);
        } else if(precolored_regs.count(new_pair.second->NodeInfo()) || adj_set.count(new_pair)) {
            constrained_moves->Append(new_pair.first, new_pair.second);
            AddWorkList(new_pair.first);
            AddWorkList(new_pair.second);
        } else if((precolored_regs.count(new_pair.first->NodeInfo()) && satisfy_george) || (!precolored_regs.count(new_pair.second->NodeInfo()) && satisfy_briggs)) {
            coalesce_moves->Append(new_pair.first, new_pair.second);
            Combine(new_pair.first, new_pair.second);
            AddWorkList(new_pair.first);
        } else {
            active_moves->Append(new_pair.first, new_pair.second);
        }
    }

    void Color::Freeze() {
        auto u = freeze_worklist->GetList().front();
        freeze_worklist->DeleteNode(u);
        simplify_worklist->Append(u);
        FreezeMoves(u);
    }

    void Color::SelectSpill() {
        // select the biggest degree
        auto spill_worklist_nodes = spill_worklist->GetList();

        int max_degree = 0;
        live::INodePtr m = nullptr;
        for(auto node : spill_worklist_nodes) {
            // TODO: maybe need to add more constrains
            assert(degrees.count(node));
            if(degrees[node] > max_degree) {
                max_degree = degrees[node];
                m = node;
            }
        }

        // always need to choose one 
        if(m == nullptr) {
            m = spill_worklist_nodes.front();
        }

        spill_worklist->DeleteNode(m);
        simplify_worklist->Append(m);
        FreezeMoves(m);
    }

    void Color::AssignColor() {
        // generate okColors
        std::set<int> ok_colors;

        while(!select_stack->GetList().empty()) {
            // n = pop(SelectStack)
            auto n = select_stack->GetList().front();
            select_stack->DeleteNode(n);
            InitColors(ok_colors);

            assert(adj_list.count(n));
            auto adj_list_nodes = adj_list[n]->GetList();
            for(auto w : adj_list_nodes) {
                auto this_alias = GetAlias(w);
                if(colored_nodes->Contain(this_alias) || precolored_regs.count(this_alias->NodeInfo())) {
                    ok_colors.erase(color[this_alias->NodeInfo()]);

                }
            }

            if(ok_colors.empty()) {
                spill_worklist->Append(n);
            } else {
                colored_nodes->Append(n);
                int c = *ok_colors.begin();
                color[n->NodeInfo()] = c;
                ok_colors.erase(c);
            }
        }
    }

    void Color::ColorMain() {
        Build();
        MakeWorkList();
        do {
            if(!simplify_worklist->GetList().empty()) {
                Simplify();
            } else if(worklist_moves->GetList().empty()) {
                Coalesce();
            } else if(freeze_worklist->GetList().empty()) {
                Freeze();
            } else if(simplify_worklist->GetList().empty()) {
                SelectSpill();
            }
        } while(!simplify_worklist->GetList().empty() || !worklist_moves->GetList().empty()
            || !freeze_worklist->GetList().empty() || !spill_worklist->GetList().empty());
        AssignColor();

        // generate the result
        color_result.coloring = temp::Map::Empty();
        for(auto color_elem : color) {
            color_result.coloring->Enter(color_elem.first, reg_manager->NthRegisterName(color_elem.second));
        }

        color_result.spills = new live::INodeList();
        color_result.spills->CatList(spill_nodes);
    }

    col::Result Color::getResult() {
        return color_result;
    }

    assem::InstrList *Color::RewriteProgram(frame::Frame *frame, assem::InstrList *prev_instrs, std::list<temp::Temp*> &new_temps) {
        assem::InstrList *new_instrs = new assem::InstrList();
        auto spill_node_list = spill_nodes->GetList();
        std::list<assem::Instr*> tmp_instr_list;
        std::list<assem::Instr*> prev_instr_list = prev_instrs->GetList();
        
        temp::Temp *rsp = reg_manager->StackPointer();
        int wordsize = reg_manager->WordSize();
        std::string assem;
        temp::TempList *dst, *src;
        bool def_change = false;

        for(auto spill_node : spill_node_list) {
            temp::Temp *spill_temp = spill_node->NodeInfo();
            frame->s_offset -= wordsize;

            for(auto instr : prev_instr_list) {
                if(typeid(*instr) == typeid(assem::LabelInstr)) {
                    continue;
                }
                dst = instr->Def();
                src = instr->Use();
                def_change = false;

                // check use
                if(src && src->Contain(spill_temp)) {
                    // load
                    assem = "movq (" + frame->label_->Name() + "_framesize" + std::to_string(frame->s_offset) 
                            + ")(%rsp), `d0";
                    temp::Temp *new_temp = temp::TempFactory::NewTemp();
                    assem::Instr *new_instr = new assem::MoveInstr(assem, new temp::TempList({new_temp}), nullptr);
                    
                    tmp_instr_list.push_back(new_instr);
                    new_temps.push_back(new_temp);
                    // TODO: whether it is avaliable?
                    // replace the former reg
                    instr->Use()->ReplaceTemp(spill_temp, new_temp);
                }

                // check def
                if(dst && dst->Contain(spill_temp)) {
                    def_change = true;
                    // store
                    temp::Temp *new_temp = temp::TempFactory::NewTemp();
                    assem = "movq (`s0), (" + frame->label_->Name() + "_framesize" + std::to_string(frame->s_offset) 
                        + ")(%rsp)";
                    assem::Instr *new_instr = new assem::MoveInstr(assem, nullptr, new temp::TempList({new_temp}));
                    // replace the former reg
                    instr->Def()->ReplaceTemp(spill_temp, new_temp);
                    // need to insert the former instruction first
                    tmp_instr_list.push_back(instr);
                    tmp_instr_list.push_back(new_instr);
                    new_temps.push_back(new_temp); 
                }

                // if def part not insert the instr, insert here
                if(!def_change) {
                    tmp_instr_list.push_back(instr);
                }
            }
            prev_instr_list = tmp_instr_list;
        }

        // generate the InstrList
        new_instrs->setContent(prev_instr_list);
        spill_nodes->Clear();
        colored_nodes->Clear();
        coalesced_nodes->Clear();
        // generate new temp's according 
        // TODO: where to do it?
        // initial_ = colored_nodes->Union(coalesced_nodes->Union(new_temps));
    }
} // namespace col
