#include "tiger/output/output.h"

#include <cstdio>

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;
extern frame::Frags *frags;
extern std::vector<gc::PointerMap> root_list;

namespace output {

std::vector<int> GetEscapePointers(frame::Frame *frame) {
  std::vector<int> escapes;

  for (auto elem : frame->locals_) {
    if (typeid(*elem) == typeid(frame::InFrameAccess) && elem->store_pointer_) {
      auto in_frame_access = static_cast<frame::InFrameAccess*>(elem);
      escapes.emplace_back(in_frame_access->offset);
    } 
  }

  return escapes;
}


void outPutPointerMap(FILE *out_) { 
  // the pos of the root
  fprintf(out_, ".global GLOBAL_GC_ROOTS\n");
  fprintf(out_, ".data\n");
  fprintf(out_, "GLOBAL_GC_ROOTS:\n");
  std::string output_str;

  for (auto pointer_map : root_list) {
    output_str = pointer_map.label + ":\n";
    output_str += (".quad " + pointer_map.return_label + "\n");
    output_str += (".quad " + pointer_map.next_label + "\n");
    output_str += (".quad " + pointer_map.frame_size + "\n");
    output_str += (".quad " + std::to_string(pointer_map.is_main) + "\n");
    
    for (auto offset : pointer_map.offsets) {
      output_str += (".quad " + std::to_string(offset) + "\n");
    }
    
    // the end label
    output_str += "-1\n";
    
    fprintf(out_, "%s\n", output_str.c_str());
  }
}


assem::InstrList *AddRootFinding(assem::InstrList *il, frame::Frame *frame, std::vector<int> escapes, temp::Map *color) {
  fg::FlowGraphFactory flowgraph_factory(il);
  flowgraph_factory.AssemFlowGraph();
  fg::FGraphPtr flow_graph = flowgraph_factory.GetFlowGraph();

  gc::Roots *tiger_root = new gc::Roots(il, frame, flow_graph, escapes, color);
  auto new_pointer_maps = tiger_root->generatePointerMap();

  auto new_instr_list = tiger_root->getInstrList();

  // update the last elem's next label 
  if (!root_list.empty() && !new_pointer_maps.empty()) {
    root_list.back().next_label = new_pointer_maps.front().label;
  }
  root_list.insert(root_list.end(), new_pointer_maps.begin(), new_pointer_maps.end());

  return new_instr_list;
}

void AssemGen::GenAssem(bool need_ra) {
  frame::Frag::OutputPhase phase;

  // Output proc
  phase = frame::Frag::Proc;
  fprintf(out_, ".text\n");
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

  // Output string
  phase = frame::Frag::String;
  fprintf(out_, ".section .rodata\n");
  for (auto &&frag : frags->GetList())
    frag->OutputAssem(out_, phase, need_ra);

  // Output pointer map
  outPutPointerMap(out_);
}

} // namespace output

namespace frame {

void ProcFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  std::unique_ptr<canon::Traces> traces;
  std::unique_ptr<cg::AssemInstr> assem_instr;
  std::unique_ptr<ra::Result> allocation;

  // When generating proc fragment, do not output string assembly
  if (phase != Proc)
    return;

  TigerLog("-------====IR tree=====-----\n");
  TigerLog(body_);

  {
    // Canonicalize
    TigerLog("-------====Canonicalize=====-----\n");
    canon::Canon canon(body_);

    // Linearize to generate canonical trees
    TigerLog("-------====Linearlize=====-----\n");
    tree::StmList *stm_linearized = canon.Linearize();
    TigerLog(stm_linearized);

    // Group list into basic blocks
    TigerLog("------====Basic block_=====-------\n");
    canon::StmListList *stm_lists = canon.BasicBlocks();
    // TigerLog(stm_lists);

    // Order basic blocks into traces_
    TigerLog("-------====Trace=====-----\n");
    tree::StmList *stm_traces = canon.TraceSchedule();
    // TigerLog(stm_traces);

    traces = canon.TransferTraces();
  }

  temp::Map *color = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  {
    // Lab 5: code generation
    TigerLog("-------====Code generate=====-----\n");
    cg::CodeGen code_gen(frame_, std::move(traces));
    code_gen.Codegen();
    assem_instr = code_gen.TransferAssemInstr();
    // TigerLog(assem_instr.get(), color);
  }

  // anlysis escape before register allocation
  std::vector<int> escapes = output::GetEscapePointers(frame_);

  assem::InstrList *il = assem_instr.get()->GetInstrList();
  
  if (need_ra) {
    // Lab 6: register allocation
    TigerLog("----====Register allocate====-----\n");
    ra::RegAllocator reg_allocator(frame_, std::move(assem_instr));
    reg_allocator.RegAlloc();
    allocation = reg_allocator.TransferResult();
    il = allocation->il_;
    color = temp::Map::LayerMap(reg_manager->temp_map_, allocation->coloring_);
  }

  // Lab 7: Garbage collection
  il = output::AddRootFinding(il, frame_, escapes, color);
  
  ///@note change for me
  TigerLog("-------====Output assembly for %s=====-----\n",
           frame_->label_->Name().data());  

  assem::Proc *proc = frame::ProcEntryExit3(frame_, il);
  
  std::string proc_name = frame_->GetLabel();

  fprintf(out, ".globl %s\n", proc_name.data());
  fprintf(out, ".type %s, @function\n", proc_name.data());
  // prologue
  fprintf(out, "%s", proc->prolog_.data());
  // body
  proc->body_->Print(out, color);
  // epilog_
  fprintf(out, "%s", proc->epilog_.data());
  fprintf(out, ".size %s, .-%s\n", proc_name.data(), proc_name.data());
}

void StringFrag::OutputAssem(FILE *out, OutputPhase phase, bool need_ra) const {
  // When generating string fragment, do not output proc assembly
  if (phase != String)
    return;

  fprintf(out, "%s:\n", label_->Name().data());
  int length = static_cast<int>(str_.size());
  // It may contain zeros in the middle of string. To keep this work, we need
  // to print all the charactors instead of using fprintf(str)
  fprintf(out, ".long %d\n", length);
  fprintf(out, ".string \"");
  for (int i = 0; i < length; i++) {
    if (str_[i] == '\n') {
      fprintf(out, "\\n");
    } else if (str_[i] == '\t') {
      fprintf(out, "\\t");
    } else if (str_[i] == '\"') {
      fprintf(out, "\\\"");
    } else {
      fprintf(out, "%c", str_[i]);
    }
  }
  fprintf(out, "\"\n");
}
} // namespace frame
