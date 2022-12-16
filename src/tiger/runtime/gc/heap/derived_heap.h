#pragma once

#include "heap.h"
#include <vector>
#include <cassert>
#include <algorithm>
#include <map>

namespace gc {

/*------------------------------- some infomation structure ------------------------------*/

struct FreeFrag {
  char *start_pos;
  uint64_t frag_size;
};

struct RecordInfo {
  char *start_pos;
  uint64_t record_size;
  std::string descriptor;
  uint64_t descriptor_size;
};

struct ArrayInfo {
  char *start_pos;
  uint64_t array_size;
};

struct GCPointerMap {
  uint64_t return_addr;
  uint64_t next_addr;
  uint64_t frame_size;
  uint64_t is_main;

  ///@note offset may less than zero
  std::vector<int64_t> offsets;
};

struct {
  bool operator()(const FreeFrag &p1,const FreeFrag &p2) {
    return p1.frag_size < p2.frag_size;
  }
} SizeComp;

struct {
  bool operator()(const FreeFrag &p1,const FreeFrag &p2) {
    return p1.start_pos < p2.start_pos;
  }
} PosComp;

class DerivedHeap : public TigerHeap {
  // TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.
private:
  std::vector<FreeFrag> free_frags;
  std::vector<RecordInfo> records_info;
  std::vector<ArrayInfo> arrays_info;
  std::map<uint64_t, GCPointerMap> ptrmap_info;
  // std::vector<GCPointerMap> ptrmap_info;

  uint64_t heap_size_;
  char *heap_;
  uint64_t *stack_pointer_;

  /*------------------- some helper function -----------------------*/
  char *FindBestfit(uint64_t size);
  void Coalesce();
  void GetPointerMaps();
  std::vector<uint64_t> RootFinding();
  void MarkAddress(uint64_t address, std::vector<bool> &active_records, std::vector<bool> &active_arrays);

public:
  uint64_t Used() const override;
  uint64_t MaxFree() const override;
  char *Allocate(uint64_t size) override;
  char *AllocateRecord(uint64_t size, int descriptor_length, char *descriptor, uint64_t *sp) override;
  char *AllocateArray(uint64_t size, uint64_t *sp) override;
  void Initialize(uint64_t size) override;
  void GC() override;

  
};

} // namespace gc

