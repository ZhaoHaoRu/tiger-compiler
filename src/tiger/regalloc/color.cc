#include "tiger/regalloc/color.h"

extern frame::RegManager *reg_manager;

namespace col {
/* TODO: Put your lab6 code here */
/*-------------------------- auxiliary functions ----------------------------------*/
    live::INodeListPtr Color::Adjacent(live::INodePtr n) {
        assert(n);
        printf("adjacent node count: %d\n", n->NodeInfo()->Int());
        assert(adj_list.count(n));
        live::INodeListPtr node_list = adj_list[n];

        // calculate adjList[n] \ selectStack U coalescedNodes
        return node_list->Diff(select_stack->Union(coalesced_nodes));
    }

    live::MoveList* Color::NodeMoves(live::INodePtr n) {
        assert(n);
        if(!move_list.count(n)) {
            return new live::MoveList();
        }
        live::MoveList *move_list_n = move_list[n];

        // calculate moveList[n] ∩ (activeMoves ∪ worklistMoves)
        return move_list_n->Intersect(active_moves->Union(worklist_moves));
    }

    bool Color::MoveRelated(live::INodePtr n) {
        return !NodeMoves(n)->GetList().empty();
    }

    void Color::Decrement(live::INodePtr m) {
        assert(m);
        assert(degrees.count(m));

        // if precolored, its degree is infinite, don't need to decrement
        if(precolored_regs.count(m->NodeInfo())) {
            return;
        }

        int d = degrees[m];
        degrees[m] = d - 1;
        if(d == col::PRECOLORED_COUNT) {
            printf("decrement adjacent\n");
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
        assert(nodes);
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
        assert(n);
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
        assert(t);
        assert(r);
        assert(degrees.count(t));
        std::pair<live::INodePtr, live::INodePtr> new_pair(t, r);
        return degrees[t] < col::PRECOLORED_COUNT || precolored_regs.count(t->NodeInfo()) || adj_set.count(new_pair);
    }

    bool Color::AllOK(live::INodeListPtr list, live::INodePtr u) {
        auto node_list = list->GetList();
        bool result = true;
        for(auto t : node_list) {
            if(!OK(t, u)) {
                return false;
            }
        }
        return true;
    }

    bool Color::Conservertive(live::INodeListPtr nodes) {
        int k = 0;
        auto node_list = nodes->GetList();

        for(auto n : node_list) {
            assert(degrees.count(n));
            if(degrees[n] >= col::PRECOLORED_COUNT || precolored_regs.count(n->NodeInfo())) {
                k += 1;
            }
        }
        return k < col::PRECOLORED_COUNT;
    }

    void Color::Combine(live::INodePtr u, live::INodePtr v) {
        assert(u);
        assert(v);
        if(freeze_worklist->Contain(v)) {
            freeze_worklist->DeleteNode(v);
        } else {
            spill_worklist->DeleteNode(v);
        }
        printf("get line 117\n");
        coalesced_nodes->Append(v);
        alias[v] = u;

        assert(move_list.count(v));
        assert(move_list.count(u));
        move_list[u] = move_list[u]->Union(move_list[v]);
        printf("get line 121\n");
        // TODO: whether it is available?
        live::INodeListPtr param_list = new live::INodeList();
        param_list->Append(v);
        EnableMoves(param_list);
        delete param_list;

        printf("get line 124\n");
        printf("combine adjacent\n");
        auto adj_list = Adjacent(v)->GetList();
        for(auto t : adj_list) {
            AddEdge(t, u);
            Decrement(t);
        }
        printf("get line 131\n");
        if(degrees[u] >= PRECOLORED_COUNT && freeze_worklist->Contain(u)) {
            freeze_worklist->DeleteNode(u);
            spill_worklist->Append(u);
        }
        printf("get line 136\n");
    }

    void Color::FreezeMoves(live::INodePtr u) {
        assert(u);
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

            if(!precolored_regs.count(v->NodeInfo()) && NodeMoves(v)->GetList().empty() && degrees[v] < col::PRECOLORED_COUNT) {
                assert(v);
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
            printf("the precolored: %d\n", reg->Int());
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

    Color::~Color() {
        delete simplify_worklist;
        delete freeze_worklist;
        delete spill_worklist;
        delete spill_nodes;
        delete select_stack;
        delete coalesced_nodes;
        delete colored_nodes;
        delete worklist_moves;
        delete active_moves;
        delete coalesce_moves;
        delete constrained_moves;
        delete frozen_moves;
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

            assert(src);
            assert(dst);

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
            if(!precolored_regs.count(node->NodeInfo())) {
                adj_list[node] = new live::INodeList();
            }
        }

        // addedges
        for(auto node : node_list) {
            auto adj_list = node->Adj()->GetList();
            for(auto adj_node : adj_list) {
                AddEdge(node, adj_node);
            }
        }

        // add the precolor node to color map
        int i = 0;
        auto precolor_temp_list = reg_manager->Registers()->GetList();
        for(auto temp : precolor_temp_list) {
            if(i == 7) {
                ++i;
            }
            color[temp] = i;
            ++i;
        }
    }

    void Color::AddEdge(live::INode *u, live::INode *v) {
        assert(u);
        assert(v);
        std::pair<live::INodePtr, live::INodePtr> node_pair(u, v), reverse_node_pair(v, u);
        if(u != v && !adj_set.count(node_pair)) {
            adj_set.insert(node_pair);
            adj_set.insert(reverse_node_pair);

            // FIXME: ignore precolored in make worklist
            if(!precolored_regs.count(u->NodeInfo())) {
                adj_list[u]->Append(v);
                degrees[u] += 1;
            }

            if(!precolored_regs.count(v->NodeInfo())) {
                adj_list[v]->Append(u);
                degrees[v] += 1;
            }
        }
    }

    void Color::MakeWorkList() {
        auto node_list = initial_->GetList();
        for(auto node : node_list) {
            ///@note skip the precolored nodes
            if(precolored_regs.count(node->NodeInfo())) {
                continue;
            }
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
        auto node = simlify_worklist_nodes.front();
        select_stack->Prepend(node);    // simulate the stack
        
        printf("simpilify adjacent\n");
        live::INodeListPtr adj_nodes = Adjacent(node);
        auto adj_nodes_list = adj_nodes->GetList();
        for(auto adj_node : adj_nodes_list) {
            Decrement(adj_node);
        }
        assert(node);
        simplify_worklist->DeleteNode(node);
        printf("finish simplify\n");
    }

    void Color::Coalesce() {
        auto m = worklist_moves->GetList().front();
        std::pair<live::INodePtr, live::INodePtr> new_pair(m); 
        live::INodePtr x = GetAlias(m.first);
        live::INodePtr y = GetAlias(m.second);
        assert(x);
        assert(y);
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
        bool satisfy_george = false;
        if(precolored_regs.count(new_pair.first->NodeInfo()) && !precolored_regs.count(new_pair.second->NodeInfo())) {
            printf("george adjacent\n");
            auto adj_list = Adjacent(new_pair.second)->GetList();
            for(auto t : adj_list) {
                if(!OK(t, new_pair.first)) {
                    satisfy_george = false;
                    break;
                }
            }
        } 
       

        // briggs
        bool satisfy_briggs = false;
        if(!precolored_regs.count(new_pair.second->NodeInfo()) && !precolored_regs.count(new_pair.first->NodeInfo())) {
            printf("briggs\n");
            satisfy_briggs = Conservertive(Adjacent(new_pair.first)->Union(Adjacent(new_pair.second)));
        }

        if(x == y) {
            printf("get line 369\n");
            coalesce_moves->Append(m.first, m.second);
            AddWorkList(new_pair.first);
            printf("get line 372\n");
        } else if(precolored_regs.count(new_pair.second->NodeInfo()) || adj_set.count(new_pair)) {
            printf("get line 374\n");
            constrained_moves->Append(m.first, m.second);
            printf("get line 376\n");
            AddWorkList(new_pair.first);
            AddWorkList(new_pair.second);
            printf("get line 378\n");
        } else if((precolored_regs.count(new_pair.first->NodeInfo()) && satisfy_george) || (!precolored_regs.count(new_pair.first->NodeInfo()) && satisfy_briggs)) {
            printf("get line 381\n");
            coalesce_moves->Append(m.first, m.second);
            printf("get line 383\n");
            Combine(new_pair.first, new_pair.second);
            printf("get line 385\n");
            AddWorkList(new_pair.first);
            printf("get line 387\n");
        } else {
            printf("get line 388\n");
            active_moves->Append(m.first, m.second);
        }
        printf("finish Coalesce\n");
    }

    void Color::Freeze() {
        auto u = freeze_worklist->GetList().front();
        assert(u);
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

        assert(m);
        spill_worklist->DeleteNode(m);
        simplify_worklist->Append(m);
        FreezeMoves(m);
    }

    void Color::AssignColor() {
        // generate okColors
        printf("generate okColors\n");
        std::set<int> ok_colors;

        // TODO:
        // use caller-save register as more as possible

        while(!select_stack->GetList().empty()) {
            // n = pop(SelectStack)
            auto n = select_stack->GetList().front();
            assert(n);
            select_stack->DeleteNode(n);
            InitColors(ok_colors);

            assert(adj_list.count(n));
            auto adj_list_nodes = adj_list[n]->GetList();
            for(auto w : adj_list_nodes) {
                assert(w);
                auto this_alias = GetAlias(w);
                if(colored_nodes->Contain(this_alias) || precolored_regs.count(this_alias->NodeInfo())) {
                    assert(color.count(this_alias->NodeInfo()));
                    int alias_color = color[this_alias->NodeInfo()];
                    assert(alias_color >= 0);
                    assert(alias_color <= PRECOLORED_COUNT);
                    ok_colors.erase(color[this_alias->NodeInfo()]);
                }
            }

            if(ok_colors.empty()) {
                spill_nodes->Append(n); // it is wrong in slides
            } else {
                colored_nodes->Append(n);
                int set_size = ok_colors.size();
                int random = rand() % set_size;
                int i = 0;
                int c = *ok_colors.begin();
                for(auto elem : ok_colors) {
                    if(i >= random) {
                        c = elem;
                        break;
                    }
                }
                color[n->NodeInfo()] = c;
            }
        }

        // assign color to the coalesced nodes
        auto coalesced_nodes_list = coalesced_nodes->GetList();
        for(auto node : coalesced_nodes_list) {
            auto this_alias = GetAlias(node);
            // printf("the alias: %d", this_alias->NodeInfo()->Int());
            assert(color.count(this_alias->NodeInfo()));
            color[node->NodeInfo()] = color[this_alias->NodeInfo()];
        }
    }

    void Color::ColorMain() {
        Build();
        MakeWorkList();
        do {
            if(!simplify_worklist->GetList().empty()) {
                printf("simplify\n");
                Simplify();
            } else if(!worklist_moves->GetList().empty()) {
                printf("caolesce\n");
                Coalesce();
            } else if(!freeze_worklist->GetList().empty()) {
                printf("freeze\n");
                Freeze();
            } else if(!spill_worklist->GetList().empty()) {
                printf("selectspill\n");
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
        printf("finish color main\n");
    }

    col::Result Color::getResult() {
        return color_result;
    }
} // namespace col
