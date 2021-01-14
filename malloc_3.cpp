//
// Created by student on 1/13/21.
//
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h> // TODO remove this after finish

struct MallocMetadata {
    size_t  size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* first_node = NULL;
MallocMetadata* mmap_first_node = NULL;

// this function get a free block with size that is bigger then the size the user ask
// the function will split the block into two parts, the first one will be return the the user
void* _split(MallocMetadata* node, size_t size) {
    if (node->size + sizeof(MallocMetadata) - size > 128) {
        auto* new_block_ptr = (MallocMetadata*) ((long)node + sizeof(MallocMetadata) + size);
        new_block_ptr->is_free = true;
        new_block_ptr->size = node->size - size - sizeof(MallocMetadata);
        new_block_ptr->next = node->next;
        new_block_ptr->prev = node;
        if (node->next)
            node->next->prev = new_block_ptr;
        node->next = new_block_ptr;
    }
    node->is_free = false;
    node->size = size;
    return (void*)((size_t)node+sizeof(MallocMetadata));
}

void _merge_prev(MallocMetadata* node) {
    //TODO remove assert after done
    assert(node->prev);
    assert(node->prev->is_free);

    MallocMetadata* prev = node->prev;
    prev->size+=node->size+sizeof(MallocMetadata);
    prev->next=node->next;
    prev->is_free = false;
    if(node->next)
        node->next->prev=prev;
}


void _merge_next(MallocMetadata* node) {
    //TODO remove assert after done
    assert(node->next);
    assert(node->next->is_free);

    MallocMetadata* next = node->next;
    next->size+=node->size+sizeof(MallocMetadata);
    next->prev=node->prev;
    next->is_free = false;
    if(node->prev)
        node->prev->next=next;
}


void* smalloc(size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    // Challenge 4 (Large allocations)
    if(size>=128*1024){
        auto * res = (MallocMetadata*) mmap(nullptr,size+sizeof(MallocMetadata),PROT_READ|PROT_WRITE,
                       MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
        
        if (res == (void*)(-1)) {
            return NULL;
        }
        
        MallocMetadata* curr_node=(MallocMetadata*)res;
        curr_node->is_free=false;
        curr_node->size=size;

        if(mmap_first_node){
            mmap_first_node->prev=curr_node;
        }

        curr_node->next = mmap_first_node;
        curr_node->prev = NULL;
        mmap_first_node = curr_node;
        return (void*)((size_t)curr_node+(size_t)sizeof(MallocMetadata));
    }

    // looking for free block available
    MallocMetadata* curr_node = first_node;
    MallocMetadata* prev_node = NULL;
    while (curr_node) {
        if (curr_node->is_free && curr_node->size >= size) {
            _split(curr_node, size);
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }

    // wildrness - last allocated block is free but to small
    if (prev_node !=  NULL and prev_node->is_free) {
        void* res = sbrk(size - prev_node->size);
        if (res == (void*)(-1)) {
            return NULL;
        }
        prev_node->size += size;
        prev_node->is_free = false;
        return (void*)((size_t)prev_node+sizeof(MallocMetadata));
    }

    // no available block found - creating new one
    void* res = sbrk(sizeof(MallocMetadata)+size);
    if (res == (void*)(-1)) {
        return NULL;
    }
    curr_node = (MallocMetadata*) res;
    curr_node->is_free = false;
    curr_node->size = size;
    curr_node->next = NULL;
    curr_node->prev = prev_node;
    if (prev_node) {
        prev_node->next = curr_node;
    }
    else {
        first_node = curr_node;
        first_node->prev = NULL;
    }
    return (void*)((size_t)res+sizeof(MallocMetadata));
}


void* scalloc(size_t num, size_t size) {
    if (num>0) { // rest of checks are done when calling to smalloc
        void* res = smalloc(size*num);
        if (res) {
            memset(res,0,size*num);
        }
        return res;
    }
    return NULL;
}


void sfree(void* p) {
    if (!p)
        return;

    auto *mm = (MallocMetadata *) ((size_t) p - sizeof(MallocMetadata));
    MallocMetadata* prev=mm->prev;
    MallocMetadata* next=mm->next;

    //Challenge 4 (Large allocations)
    if(mm->size>=128*1024) { //mmap
        int res = munmap(mm,mm->size+sizeof(MallocMetadata));

        // TODO - is this the right way to check of failed
        if (res == -1)
            return;

        if(prev){
            prev->next = next;
        } else{
            mmap_first_node = next;
        }
        if(next)
            next->prev=prev;

    //Challenge 2
    } else {

        bool prev_free = prev and prev->is_free;
        bool next_free = next and next->is_free;

        if (prev_free && next_free) {
            _merge_next(mm);
            _merge_prev(mm);
        } else if (prev_free) {
            _merge_prev(mm);
        } else if (next_free) {
            _merge_next(mm);
        } else {
            mm->is_free = true;
        }
    }
}


void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 1e8) {  // TODO
        return NULL;
    }

    if (!oldp) {
        return smalloc(size);
    }

    auto *mm = (MallocMetadata *) ((size_t) oldp - sizeof(MallocMetadata));
    // option 1 - using same block
    if (size <= mm->size) {
        mm->is_free = false; // TODO is it necessary ? maybe we should check in advance
        return _split(mm, size);
    }

    // option 2 - merging with prev
    if (mm->prev != nullptr and mm->prev->is_free and size <= mm->size + mm->prev->size + sizeof(MallocMetadata)) {
        MallocMetadata* prev = mm->prev;
        _merge_prev(mm);
        memcpy((void*) ((size_t)prev + sizeof(MallocMetadata)), oldp , mm->size);
        return _split(prev, size);
    }

    //option 3 - merge with next
    if (mm->next != nullptr and mm->next->is_free and size <= mm->size + mm->next->size + sizeof(MallocMetadata)) {
        _merge_next(mm);
        return _split(mm, size);
    }


    //option 4 - merge with next && prev
    if (mm->next != nullptr and mm->prev != nullptr and mm->next->is_free and mm->prev->is_free and size <= mm->size + mm->next->size + mm->prev->size + 2 * sizeof(MallocMetadata)) {
        MallocMetadata* prev = mm->prev;
        _merge_next(prev);
        _merge_next(prev); // now it will be three of them
        memcpy( (void*)((size_t)prev + sizeof(MallocMetadata)), oldp , mm->size);
        return _split(prev, size);
    }

    //option 5 - try finding another block fits
    // TODO - should we merge block?
    // TODO - if so can we over ride data
//    bool is_prev_merged = false;
//    if (mm->next and mm->next->is_free) {
//        _merge_next(mm);
//    }
//    if (mm->prev and mm->prev->is_free) {
//        _merge_prev(mm);
//        oldp = (void*)((size_t)mm->prev + sizeof(MallocMetadata)); // we have new ptr
//        is_prev_merged = true;
//    }

    void* res = smalloc(size);
    if (res) {
        memcpy(res, oldp, mm->size);
        sfree(oldp);
    // if smalloc failed and we merge with prev that a problem - we un marge
    }
//    else if (is_prev_merged){
//        mm->prev->is_free = true;
//        mm->prev->next = mm;
//        mm->prev->size -= (mm->size + sizeof(MallocMetadata));
//        if(mm->next)
//            mm->next->prev=mm;
//    }
    return res;
}


size_t _num_free_blocks() {
    size_t counter = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        if(curr_node->is_free) {
            ++counter;
        }
        curr_node = curr_node->next;
    }
    return counter;
}


size_t _num_free_bytes() {
    size_t sum = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        if(curr_node->is_free) {
            sum += curr_node->size;
        }
        curr_node = curr_node->next;
    }
    return sum;
}


size_t _num_allocated_blocks() {
    size_t counter = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        ++counter;
        curr_node = curr_node->next;
    }

    MallocMetadata* mmap_curr_node = mmap_first_node;
    while (mmap_curr_node) {
        ++counter;
        mmap_curr_node = mmap_curr_node->next;
    }
    return counter;
}


size_t _num_allocated_bytes() {
    size_t sum = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        sum += curr_node->size;
        curr_node = curr_node->next;
    }

    MallocMetadata* mmap_curr_node = mmap_first_node;
    while (mmap_curr_node) {
        sum += mmap_curr_node->size;
        mmap_curr_node = mmap_curr_node->next;
    }

    return sum;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks()*sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
