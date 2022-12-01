#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include <memory>
#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result();
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
private:
  frame::Frame *frame_;
  std::unique_ptr<cg::AssemInstr> instr_list_;
  temp::Map *coloring;
  ra::Result regalloc_result;

public:
  RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> instr_list): frame_(frame),
    instr_list_(std::move(instr_list)) {}
  
  void RegAlloc();
  void MergeMoves();
  std::unique_ptr<ra::Result> TransferResult();
};

} // namespace ra

#endif