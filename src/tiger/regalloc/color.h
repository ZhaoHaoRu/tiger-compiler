#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include <unordered_map>
#include <set>
#include <unordered_set>
#include <utility>
#include <cassert>
#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"

namespace col {
const int PRECOLORED_COUNT = 15;

struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills;
};

class Color {
  /* TODO: Put your lab6 code here */
private:
  live::LiveGraph live_graph_;
  std::unordered_set<temp::Temp*> precolored_regs;
  live::INodeListPtr simplify_worklist;
  live::INodeListPtr freeze_worklist;
  live::INodeListPtr spill_worklist;
  live::INodeListPtr spill_nodes;
  live::INodeListPtr select_stack;
  live::INodeListPtr coalesced_nodes;

  live::MoveList* worklist_moves;
  live::MoveList* active_moves;
  live::MoveList* coalesce_moves;
  live::MoveList* constrained_moves;
  live::MoveList* frozen_moves;
  std::unordered_map<live::INodePtr, live::MoveList*> move_list;
  std::unordered_map<live::INodePtr, int> degrees;
  std::set<std::pair<live::INodePtr, live::INodePtr>> adj_set;
  std::unordered_map<live::INodePtr, live::INodeListPtr> adj_list;
  std::unordered_map<live::INodePtr, live::INodePtr> alias;
  std::unordered_map<temp::Temp*, std::string> color_map;

  // some auxiliary functions
  live::INodeListPtr Adjacent(live::INodePtr n);
  live::MoveList* NodeMoves(live::INodePtr n);
  bool MoveRelated(live::INodePtr n);
  void Decrement(live::INodePtr m);
  void EnableMoves(live::INodeListPtr nodes);
  live::INodePtr GetAlias(live::INodePtr n);
  void AddWorkList(live::INodePtr u);
  bool OK(live::INodePtr t, live::INodePtr r);
  bool Conservertive(live::INodeListPtr node);
  void Combine(live::INodePtr u, live::INodePtr v);
  void FreezeMoves(live::INodePtr u);

public:
  Color(live::LiveGraph live_graph);
  ~Color();
  void Build();
  void AddEdge(live::INodePtr u, live::INodePtr v);
  void MakeWorkList();
  void Simplify();
  void Coalesce();
  void Freeze();
  void SelectSpill();
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
