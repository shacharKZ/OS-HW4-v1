//
// Created by student on 1/13/21.
//
#include <unistd.h>
//#include <stdio.h>
#include <string.h>

struct MallocMetadata {
    size_t  size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
};

MallocMetadata* first_node = NULL;

void* smalloc(size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }

    MallocMetadata* curr_node = first_node;
    MallocMetadata* prev_node = NULL;
    while (curr_node) {
        if (curr_node->is_free && curr_node->size >= size) {
            curr_node->is_free = false;
            return (void*)((size_t)curr_node+sizeof(MallocMetadata));
        }
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
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
    if (p) {
        auto *mm = (MallocMetadata *) ((size_t) p - sizeof(MallocMetadata));
        mm->is_free = true;
    }
}


// TODO - should we free before?
// TODO we need to make sure is_free = false?
void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 1e8) {  // TODO
        return NULL;
    }
    if (!oldp) {
        return smalloc(size);
    }

    auto *mm = (MallocMetadata *) ((size_t) oldp - sizeof(MallocMetadata));
    if (size <= mm->size) {
        mm->is_free = false;
        // mm.size = size //TODO - should we add it?
        return oldp;
    }

    void* res = smalloc(size);
    if (res) {
        memcpy(res, oldp, mm->size);
        sfree(oldp);
    }
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
    return counter;
}


size_t _num_allocated_bytes() {
    size_t sum = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        sum += curr_node->size;
        curr_node = curr_node->next;
    }
    return sum;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks()*sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}
