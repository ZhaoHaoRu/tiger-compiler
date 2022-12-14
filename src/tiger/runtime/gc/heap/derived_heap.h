#pragma once

#include "heap.h"
#include <vector>
#include <cassert>
#include <algorithm>

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

  uint64_t heap_size_;
  char *heap_;
  uint64_t *stack_pointer_;

  /*------------------- some helper function -----------------------*/
  char *FindBestfit(uint64_t size);

public:
  uint64_t Used() const override;
  uint64_t MaxFree() const override;
  char *Allocate(uint64_t size) override;
  char *AllocateRecord(uint64_t size, std::string descriptor, uint64_t *sp) override;
  char *AllocateArray(uint64_t size, uint64_t *sp) override;
  void Initialize(uint64_t size) override;
  void GC() override;
};

} // namespace gc

