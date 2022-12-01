#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
    void RegAllocator::RegAlloc() {
        bool end = false;
        while(true) {
            // liveness analysis
            // flow graph
            fg::FlowGraphFactory flowgraph_factory(instr_list_->GetInstrList());
            flowgraph_factory.AssemFlowGraph();
            fg::FGraphPtr flow_graph = flowgraph_factory.GetFlowGraph();

            // live graph
            live::LiveGraphFactory livegraph_factory(flow_graph);
            livegraph_factory.Liveness();
            live::LiveGraph live_graph = livegraph_factory.GetLiveGraph();

            // color
            col::Color color_tool(live_graph);
            color_tool.ColorMain();
            col::Result color_result = color_tool.getResult();
            coloring = color_result.coloring;
            spills = color_result.spills;
            
            // rewrite the program
            if(!spills || spills->GetList().empty()) {
                break;
            }
            new_temps.clear();
            auto new_instr_list = RewriteProgram(new_temps);
            instr_list_.reset(new cg::AssemInstr(new_instr_list));
            // TODO: how new temps use?
        }

        MergeMoves();

        // generate the result
        regalloc_result->coloring_ = coloring;
        regalloc_result->il_ = instr_list_->GetInstrList();
    }

    assem::InstrList *RegAllocator::RewriteProgram(std::list<temp::Temp*> &new_temps) {
        assem::InstrList *new_instrs = new assem::InstrList();
        auto spill_node_list = spills->GetList();
        std::list<assem::Instr*> tmp_instr_list;
        std::list<assem::Instr*> prev_instr_list = instr_list_->GetInstrList()->GetList();
        
        temp::Temp *rsp = reg_manager->StackPointer();
        int wordsize = reg_manager->WordSize();
        std::string assem;
        temp::TempList *dst, *src;
        bool def_change = false;

        for(auto spill_node : spill_node_list) {
            temp::Temp *spill_temp = spill_node->NodeInfo();
            frame_->s_offset -= wordsize;

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
                    assem = "movq (" + frame_->label_->Name() + "_framesize" + std::to_string(frame_->s_offset) 
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
                    assem = "movq (`s0), (" + frame_->label_->Name() + "_framesize" + std::to_string(frame_->s_offset) 
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
        
        // generate new temp's according 
        // TODO: where to do these?
        // initial_ = colored_nodes->Union(coalesced_nodes->Union(new_temps));
        // spill_nodes->Clear();
        // colored_nodes->Clear();
        // coalesced_nodes->Clear();
    }

    void RegAllocator::MergeMoves() {
        std::list<assem::Instr*> instrs = instr_list_->GetInstrList()->GetList();
        assem::InstrList *simplified = new assem::InstrList();
        for(auto instr : instrs) {
            if(typeid(*instr) == typeid(assem::MoveInstr) && !instr->Def()->GetList().empty() 
                && !instr->Use()->GetList().empty()) {
                auto dst = instr->Def()->GetList().front();
                auto src = instr->Use()->GetList().front();
                if(coloring->Look(dst) == coloring->Look(src)) {   
                    // the same register, skip
                    continue;
                }
            }
            simplified->Append(instr);
        }
        instr_list_.reset(new cg::AssemInstr(simplified));
    }

    std::unique_ptr<ra::Result> RegAllocator::TransferResult() {
        return std::make_unique<ra::Result>(regalloc_result);
    }
} // namespace ra