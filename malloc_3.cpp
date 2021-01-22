//
// Created by student on 1/13/21.
//
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

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
    if (node->size  >= 128 + sizeof(MallocMetadata) + size) {
        auto* new_block_ptr = (MallocMetadata*) ((size_t)node + sizeof(MallocMetadata) + size);
        new_block_ptr->is_free = true;
        new_block_ptr->size = node->size - size - sizeof(MallocMetadata);
        new_block_ptr->next = node->next;
        new_block_ptr->prev = node;
        if (node->next)
            node->next->prev = new_block_ptr;
        node->next = new_block_ptr;
        node->size = size;
    }
    node->is_free = false;
    return (void*)((size_t)node + sizeof(MallocMetadata));
}

void _merge_prev(MallocMetadata* node) {
    MallocMetadata* prev = node->prev;
    prev->size += node->size + sizeof(MallocMetadata);
    prev->next = node->next;
    if(node->next)
        node->next->prev = prev;
}


void _merge_next(MallocMetadata* node) {
    MallocMetadata* next = node->next;
    node->size += next->size + sizeof(MallocMetadata);
    node->next =next->next;
    if(next->next)
        next->next->prev = node;
}


void* smalloc(size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    // Challenge 4 (Large allocations)
    if(size>=1024  * 128){
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
            return _split(curr_node, size);
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
        prev_node->size = size;
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
    if(mm->size >= 1024 * 128) { //mmap
        int res = munmap(mm,mm->size+sizeof(MallocMetadata));

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
            mm->is_free = true;
        } else {
            mm->is_free = true;
        }
    }
}

void * map_srealloc(MallocMetadata* mm, void* oldp, size_t size) {
    void* newp = smalloc(size);
    if(newp == NULL){
        return NULL;
    }

    if(mm->size < size){
        memmove(newp, oldp, mm->size);
    } else{
        memmove(newp, oldp, size);
    }
    sfree(oldp);
    return newp;
}

void * small_srealloc(MallocMetadata* mm, void* oldp, size_t size) {
    // option 1 - using same block
    if (size <= mm->size) {
        mm->is_free = false;
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
        _merge_next(mm);
        _merge_prev(mm);
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


void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }

    if (!oldp) {
        return smalloc(size);
    }
    auto *mm = (MallocMetadata *) ((size_t) oldp - sizeof(MallocMetadata));

    // first case
    if (mm->size > 1024 * 128) {
        return map_srealloc(mm, oldp, size);
    } else {
        return small_srealloc(mm, oldp, size);
    }
}


size_t _num_free_blocks() {
    size_t counter = 0;
    MallocMetadata* curr_node = first_node;
    while (curr_node) {
        counter += curr_node->is_free;
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


//#define META_SIZE         sizeof(MallocMetadata) // put your meta data name here.
//int main() {
//    void *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11, *p12, *p13, *p14 ,*p15;
//    //check mmap allocation and malloc/calloc fails
//    smalloc(0);
//    smalloc(100000001);
//    scalloc(10000000, 10000000);
//    scalloc(100000001, 1);
//    scalloc(1, 100000001);
//    scalloc(50, 0);
//    scalloc(0, 50);
//
//    p1 = smalloc(100000000);
//    p2 = scalloc(1, 10000000);
//    p3 = scalloc(10000000, 1);
//    p1 = srealloc(p1, 100000000);
//    p3 = srealloc(p3, 20000000);
//    p3 = srealloc(p3, 10000000);
//    p4 = srealloc(nullptr, 10000000);
//    assert(_num_free_blocks() == 0);
//    assert(_num_free_bytes() == 0);
//    assert(_num_allocated_blocks() == 4);
//    assert(_num_allocated_bytes() == 100000000 + 30000000);
//    assert(_num_meta_data_bytes() == 4 * META_SIZE);
//    sfree(p1), sfree(p2), sfree(p3), sfree(p4);
//    assert(_num_free_blocks() == 0);
//    assert(_num_free_bytes() == 0);
//    assert(_num_allocated_blocks() == 0);
//    assert(_num_allocated_bytes() == 0);
//    assert(_num_meta_data_bytes() == 0);
//    p1 = smalloc(1000);
//    p2 = smalloc(1000);
//    p3 = smalloc(1000);
//    p4 = scalloc(500,2);
//    p5 = scalloc(2, 500);
//    assert(_num_free_blocks() == 0);
//    assert(_num_free_bytes() == 0);
//    assert(_num_allocated_blocks() == 5);
//    assert(_num_allocated_bytes() == 5000);
//    assert(_num_meta_data_bytes() == 5 * META_SIZE);
//    //check free, combine and split
//    sfree(p3); sfree(p5); sfree(p4);
//    p3 = smalloc(1000); p4 = smalloc(1000); p5 = smalloc (1000);
//    sfree(p4); sfree(p5);
//    sfree(p1); sfree(p1); sfree(p2);
//    p1 = smalloc(1000); p2 = smalloc(1000);
//    sfree(p2); sfree(p1);
//    assert(_num_free_blocks() == 2);
//    assert(_num_free_bytes() == 4000 + 2*META_SIZE);
//    assert(_num_allocated_blocks() == 3);
//    assert(_num_allocated_bytes() == 5000 + 2*META_SIZE);
//    assert(_num_meta_data_bytes() == 3 * META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p4 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
//    //list condition: FREE OF 2032 -> BLOCK OF 1000 -> FREE OF 2032
//    p1 = smalloc(1000);
//    p2 = smalloc(1000);
//    p4 = scalloc(1500,2);
//    assert(_num_free_blocks() == 0);
//    assert(_num_free_bytes() == 0);
//    assert(_num_allocated_blocks() == 4);
//    assert(_num_allocated_bytes() == 6000);
//    assert(_num_meta_data_bytes() == 4 * META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p4 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
//    sfree(p1); sfree(p2); sfree(p3); sfree(p4);
//    assert(_num_free_blocks() == 1);
//    assert(_num_free_bytes() == 6000+3*META_SIZE);
//    assert(_num_allocated_blocks() == 1);
//    assert(_num_allocated_bytes() == 6000+3*META_SIZE);
//    assert(_num_meta_data_bytes() == META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p1 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
////    assert(mmap_list.head == nullptr);
////    assert(mmap_list.head == nullptr);
//    p1 = smalloc(1000); p2 = scalloc(2,500);
//    p3 = smalloc(1000); p4 = scalloc(10,100);
//    p5 = smalloc(1000); p6 = scalloc(250,4);
//
//    //list condition: SIX BLOCKS, 1000 BYTES EACH
//    //now we'll check realloc
//
//    //check englare
//    sfree(p5);
//    for (int i = 0; i < 250; ++i)
//        *((int*)p6+i) = 2;
//    p6 = srealloc(p6, 2000);
//    for (int i = 0; i < 250; ++i)
//        assert(*((int*)p6+i) == 2);
//    assert(_num_free_blocks() == 1);
//    assert(_num_free_bytes() == 1000);
//    assert(_num_allocated_blocks() == 6);
//    assert(_num_allocated_bytes() == 7000);
//    assert(_num_meta_data_bytes() == 6*META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p6 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
//    p5 = smalloc(1000);
//    p1 = srealloc(p1,500-sizeof(MallocMetadata));
//    sfree(p1);
//    //check case a
//
//    assert(((MallocMetadata*)p5-1)->is_free==false);
//    sfree(p5); sfree(p3);
//    //check case b
//    p3 = srealloc(p4,2000);
//    //LIST CONDITION: 1-> 1000 FREE, 2->1000, 3->2000,
//    // 5-> 1000 FREE, 6->2000 FREE
//    //now i return the list to its state before tha last realloc
//    p3 = srealloc(p3,1000);
//    p4 = ((char*)p3 + 1000 + META_SIZE);
//    p1 = smalloc(1000); p4=smalloc(1000); sfree(p1);
//    sfree(p3);
//    //check case d
//    p3 = srealloc(p4,3000);
//    assert(_num_free_blocks() == 1);
//    assert(_num_free_bytes() == 1000);
//    assert(_num_allocated_blocks() == 4);
//    assert(_num_allocated_bytes() == 7000 + 2*META_SIZE);
//    assert(_num_meta_data_bytes() == 4*META_SIZE);
//    //just change the state of the list
//    p3 = srealloc(p3,500);
//    p1 = smalloc(1000);
//    p4 = smalloc(1500);
//    p5 = smalloc(1000);
//    sfree(p1); sfree(p3); sfree(p5);
//    //check case c
//    p4 = srealloc(p4, 2500);
//    p4 = srealloc(p4, 1000);
//
//    assert(_num_free_blocks() == 3);
//    assert(_num_free_bytes() == 3000);
//    assert(_num_allocated_blocks() == 6);
//    assert(_num_allocated_bytes() == 7000);
//    assert(_num_meta_data_bytes() == 6*META_SIZE);
//    //just change the state of the list
//    p1 = smalloc(1000);
//    p3 = smalloc(500);
//    sfree(p1);
//    //check case e
//    p1 = srealloc(p3, 900);
//    assert(_num_free_blocks() == 2);
//    assert(_num_free_bytes() == 2000);
//    assert(_num_allocated_blocks() == 6);
//    assert(_num_allocated_bytes() == 7000);
//    assert(_num_meta_data_bytes() == 6*META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p6 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
//    //check case f
//    for (int i = 0; i < 250; ++i)
//        *((int*)p1+i) = 2;
//    p7 = srealloc(p1,2000);
//    for (int i = 0; i < 250; ++i)
//        assert(*((int*)p7+i) == 2);
//    assert(_num_free_blocks() == 3);
//    assert(_num_free_bytes() == 3000);
//    assert(_num_allocated_blocks() == 7);
//    assert(_num_allocated_bytes() == 9000);
//    assert(_num_meta_data_bytes() == 7*META_SIZE);
////    assert(block_list.tail == (MallocMetadata*)p7 - 1);
////    assert(block_list.head == (MallocMetadata*)p1 - 1);
//    //block_list.print_list();
//    /*LIST CONDITION:
//     * 1->1000 free
//     * 2->1000
//     * 3->500 free
//     * 4->1000
//     * 5->1500 free
//     * 6->2000
//     * 7->2000
//    */
////    cout << "good job!" << endl;
//    return 0;
//}
