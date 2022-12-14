#include "derived_heap.h"
#include <stdio.h>
#include <stack>
#include <cstring>

namespace gc {
// TODO(lab7): You need to implement those interfaces inherited from TigerHeap correctly according to your design.
    void DerivedHeap::Initialize(uint64_t size) {
        heap_ = new char[size]{' '};
        heap_size_ = size;

        FreeFrag new_free_frag;
        new_free_frag.start_pos = heap_;
        new_free_frag.frag_size = heap_size_;
        free_frags.emplace_back(new_free_frag);
    }

    uint64_t DerivedHeap::Used() const {
        uint64_t total_free = 0;
        for (auto frag : free_frags) {
            total_free += frag.frag_size;
        }

        assert(total_free <= heap_size_);
        return heap_size_ - total_free;
    }

    uint64_t DerivedHeap::MaxFree() const {
        uint64_t max_size = 0;
        for (auto frag : free_frags) {
            max_size = std::max(max_size, frag.frag_size);
        }
        return max_size;
    }

    char *DerivedHeap::FindBestfit(uint64_t size) {
        std::sort(free_frags.begin(), free_frags.end(), SizeComp);
        // find the first fragment which size bigger than the wanted size
        FreeFrag wanted;
        wanted.start_pos = nullptr;
        auto it = free_frags.begin();
        while (it != free_frags.end()) {
            if ((*it).frag_size >= size) {
                wanted = *it;
                free_frags.erase(it);
                break;
            }
            ++it;
        }

        // not found
        if (!wanted.start_pos) {
            return nullptr;
        }

        // if the found fragment is too bigger, cut into two pieces
        if (wanted.frag_size > size) {
            FreeFrag new_frag;
            new_frag.start_pos = wanted.start_pos + size;
            new_frag.frag_size = wanted.frag_size - size;
            free_frags.emplace_back(new_frag);
        }

        return wanted.start_pos;
    }

    char *DerivedHeap::Allocate(uint64_t size) {
        uint64_t max_size = MaxFree();
        if (max_size < size) {
            return nullptr;
        }

        return FindBestfit(size);
    }

    char *DerivedHeap::AllocateRecord(uint64_t size, std::string descriptor, uint64_t *sp) {
        char *start_pos = Allocate(size);
        if (!start_pos) {
            return nullptr;
        }

        // TODO: what stack pointer do?
        stack_pointer_ = sp;

        RecordInfo new_record_info;
        new_record_info.start_pos = start_pos;
        new_record_info.record_size = size;
        new_record_info.descriptor = descriptor;
        new_record_info.descriptor_size = descriptor.size();
        records_info.emplace_back(new_record_info);
        return start_pos;
    }

    char *DerivedHeap::AllocateArray(uint64_t size, uint64_t *sp) {
        char *start_pos = Allocate(size);
        if (!start_pos) {
            return nullptr;
        }

        stack_pointer_ = sp;

        ArrayInfo new_array_info;
        new_array_info.start_pos = start_pos;
        new_array_info.array_size = size;
        arrays_info.emplace_back(new_array_info);
        return start_pos;
    }
} // namespace gc

