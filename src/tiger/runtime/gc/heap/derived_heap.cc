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

        // get the pointer map from assem output
        GetPointerMaps();
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


    /*------------------------- for GC ---------------------- */
    void DerivedHeap::Coalesce() {
        std::vector<FreeFrag> new_free_frags;
        int n = free_frags.size();
        std::sort(free_frags.begin(), free_frags.end(), PosComp);

        if (n == 0) {
            return;
        }
        new_free_frags.emplace_back(free_frags[0]);

        for (int i = 1; i < n ; ++i) {
            int m = new_free_frags.size();
            assert(m != 0);
            uint64_t prev_end_pos = (uint64_t)new_free_frags[m - 1].start_pos + new_free_frags[m - 1].frag_size;
            // coalesce
            if (prev_end_pos == (uint64_t)free_frags[i].start_pos) {
                new_free_frags[m - 1].frag_size += free_frags[i].frag_size;
            } else {
                new_free_frags.emplace_back(free_frags[i]);
            }
        }

        free_frags = new_free_frags;
    }


    void DerivedHeap::MarkAddress(uint64_t address, std::vector<bool> &active_records, std::vector<bool> &active_arrays) {
        int m = active_records.size(), n = active_arrays.size();
        uint64_t word_size = 8;

        // mark address-related records
        for (int i = 0; i < m; ++i) {
            auto record = records_info[i];
            int64_t bias = (int64_t)(address) - (int64_t)(record.start_pos);
            if (bias >= 0 && bias < record.record_size) {
                // avoid duplication
                if (active_records[i]) {
                    return;
                }

                uint64_t begin_addr = (uint64_t)record.start_pos;
                // scan every field in the record
                for (int j = 0; j < record.descriptor_size; ++j) {
                    if (record.descriptor[j] == '1') {  // the field is a pointer
                        uint64_t field_addr = *((uint64_t *)(begin_addr + word_size * j));
                        MarkAddress(field_addr, active_records, active_arrays);
                        active_records[i] = true;
                        return;
                    }
                }
            }
        }
        
        // mark address-related arrays
        for (int i = 0; i < n; ++i) {
            auto array = arrays_info[i];
            int64_t bias = (int64_t)(address) - (int64_t)(array.start_pos);
            if (bias >= 0 && bias < array.array_size) {
                active_arrays[i] = true;
                return;
            }
        }
    }


    void DerivedHeap::GC() {
        /*----------------- mark ---------------------*/
        int m = records_info.size(), n = arrays_info.size();
        std::vector<bool> active_records(m, false);
        std::vector<bool> active_arrays(n, false);

        // the addresses for mark
        std::vector<uint64_t> addresses = RootFinding();
        for (auto address : addresses) {
            MarkAddress(address, active_records, active_arrays);
        }

        /* ---------------- sweep -------------------*/
        std::vector<RecordInfo> new_record_info;
        std::vector<ArrayInfo> new_array_info;
        // sweep the unused record
        for (int i = 0; i < m; ++i) {
            if (active_records[i]) {
                new_record_info.emplace_back(records_info[i]);
            } else {
                FreeFrag new_free_frag;
                new_free_frag.start_pos = records_info[i].start_pos;
                new_free_frag.frag_size = records_info[i].record_size;
                free_frags.emplace_back(new_free_frag);
            }
        }

        // sweep the unused array
        for (int i = 0; i < n; ++i) {
            if (active_arrays[i]) {
                new_array_info.emplace_back(arrays_info[i]);
            } else {
                FreeFrag new_free_frag;
                new_free_frag.start_pos = arrays_info[i].start_pos;
                new_free_frag.frag_size = arrays_info[i].array_size;
                free_frags.emplace_back(new_free_frag);
            }
        }

        records_info = new_record_info;
        arrays_info = new_array_info;
        Coalesce();
    }

    /*------------------ for root finding --------------------*/
    void DerivedHeap::GetPointerMaps() {
        uint64_t *global_root = &GLOBAL_GC_ROOTS;
        uint64_t *cur = global_root;

        while(true) {
            GCPointerMap new_pointer_map;
            uint64_t return_address = *cur;
            new_pointer_map.return_addr = return_address;
            
            cur += 1;
            uint64_t next_address = *cur;
            new_pointer_map.next_addr = next_address;
            
            cur += 1;
            uint64_t frame_size = *cur;
            new_pointer_map.frame_size = frame_size;

            cur += 1;
            uint64_t is_main = *cur;
            new_pointer_map.is_main = is_main;

            std::vector<int64_t> offsets;
            while(true) {
                cur += 1;
                int64_t offset = *cur;
                if (offset == -1) {     // the end label
                    break;
                }
                offsets.emplace_back(offset);
            }

            new_pointer_map.offsets = offsets;
            assert(!ptrmap_info.count(return_address));
            ptrmap_info[return_address] = new_pointer_map;

            if (new_pointer_map.next_addr == 0) {
                break;
            }
        }
    }

    ///@brief find the ptrmap's according stack pos
    std::vector<uint64_t> DerivedHeap::RootFinding() {
        uint64_t *sp = stack_pointer_;  
        std::vector<uint64_t> result;
        uint64_t word_size = 8;

        bool is_main = false;
        while (!is_main) {
            uint64_t return_addr = *(sp - 1);
            assert(ptrmap_info.count(return_addr));
            if (ptrmap_info.count(return_addr)) {
                auto wanted_ptr_map = ptrmap_info[return_addr];
                for (int64_t offset : wanted_ptr_map.offsets) {
                    uint64_t *pos_in_stack = (uint64_t*)(offset + (int64_t)sp + (int64_t)wanted_ptr_map.frame_size);
                    result.emplace_back(*pos_in_stack);
                }

                // find the next frame
                sp = sp + (wanted_ptr_map.frame_size / word_size) + 1;
                is_main = wanted_ptr_map.is_main;
            }
        }
        return result;
    }
} // namespace gc

