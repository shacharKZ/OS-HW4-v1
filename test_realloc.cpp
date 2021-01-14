//Made By Daniel Keren

#include <cstdio>
#include <cassert>

#define MALLOC_VERSION 2

#if (MALLOC_VERSION == 2)
    #include "malloc_2.cpp"
#else
    #include "malloc_3.cpp"
#endif

size_t valid_free_blocks = 0;
size_t valid_free_bytes = 0;
size_t valid_allocated_blocks = 0;
size_t valid_allocated_bytes = 0;
size_t valid_meta_data_bytes = 0;

#define TEST_MALLOC() do {\
    assert(_num_free_blocks() == valid_free_blocks);\
    assert(_num_free_bytes() == valid_free_bytes);\
    assert(_num_allocated_blocks() == valid_allocated_blocks);\
    assert(_num_allocated_bytes() == valid_allocated_bytes);\
    assert(_num_meta_data_bytes() == valid_meta_data_bytes);\
    } while (0)


void remalloc_2_test() {
//Reuse a memory block if the size requested is smaller than the old block
    void* first_ptr = malloc(8);
    assert(first_ptr);
    void* second_ptr = realloc(first_ptr, 4);
    assert(second_ptr);
    assert(first_ptr == second_ptr);
    valid_allocated_blocks++;
    valid_allocated_bytes+=8;
    valid_meta_data_bytes+=_size_meta_data();
    TEST_MALLOC();

    //Check that oldp is not freed if realloc fails
    void* third_ptr = realloc(second_ptr, 0);   //Fails because size==0
    assert(!third_ptr);
    assert(second_ptr);
    TEST_MALLOC();

    //Check everything still makes sense
    free(second_ptr);
    valid_free_blocks++;
    valid_free_bytes+=8;
    TEST_MALLOC();


    /*******************************************
    This part onwards differs from malloc_3:
    ********************************************/

    //Test realloc for a large block
    int* large_malloc = (int*)malloc(40*sizeof(int));
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        large_malloc[i] = i;
    }
    valid_allocated_blocks++;
    valid_allocated_bytes+=40*sizeof(int);
    valid_meta_data_bytes+=_size_meta_data();
    TEST_MALLOC();
    
    //Should create a new block and free the old one - without wilderness increase
    int* old_large_malloc = large_malloc;
    large_malloc = (int*)realloc(large_malloc, 100*sizeof(int));
    assert(large_malloc);
    assert (large_malloc != old_large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    valid_free_blocks++;
    valid_free_bytes+=40*sizeof(int);
    valid_allocated_blocks++;
    valid_allocated_bytes+=100*sizeof(int);
    valid_meta_data_bytes+=_size_meta_data();
    TEST_MALLOC();

    //Check that failure for large block realloc dosen't make any changes (no free)
    int* failed_realloc = (int*)realloc(large_malloc, 0);
    assert(!failed_realloc);
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    TEST_MALLOC();
    
    //Should not split any blocks but still reuse the block
    large_malloc = (int*)realloc(large_malloc, 60*sizeof(int));
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    TEST_MALLOC();
}

void remalloc_3_test() {
    //Reuse a memory block if the size requested is smaller than the old block
    void* first_ptr = malloc(8);
    assert(first_ptr);
    void* second_ptr = realloc(first_ptr, 4);
    assert(second_ptr);
    assert(first_ptr == second_ptr);
    valid_allocated_blocks++;
    valid_allocated_bytes+=8;
    valid_meta_data_bytes+=_size_meta_data();
    TEST_MALLOC();

    //Check that oldp is not freed if realloc fails
    void* third_ptr = realloc(second_ptr, 0);   //Fails because size==0
    assert(!third_ptr);
    assert(second_ptr);
    TEST_MALLOC();

    //Check everything still makes sense
    free(second_ptr);
    valid_free_blocks++;
    valid_free_bytes+=8;
    TEST_MALLOC();


    /*******************************************
    This part onwards differs from malloc_2:
    ********************************************/

    //Test realloc for a large block
    int* large_malloc = (int*)malloc(40*sizeof(int));   //Increases wilderness
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        large_malloc[i] = i;
    }
    valid_free_blocks--;
    valid_free_bytes-=8;
    valid_allocated_bytes += (40*sizeof(int)-8);
    TEST_MALLOC();
    
    //Should increase the wilderness further
    int* old_large_malloc = large_malloc;
    large_malloc = (int*)realloc(large_malloc, 100*sizeof(int));
    assert(large_malloc);
    assert (large_malloc == old_large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    valid_allocated_bytes = 100*sizeof(int);
    TEST_MALLOC();

    //Check that failure for large block realloc dosen't make any changes (Without uniting any blocks)
    int* failed_realloc = (int*)realloc(large_malloc, 0);
    assert(!failed_realloc);
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    TEST_MALLOC();
    
    //Should split the block
    large_malloc = (int*)realloc(large_malloc, 60*sizeof(int));
    assert(large_malloc);
    for (int i = 0; i < 40; i++) {
        assert(large_malloc[i] == i);
    }
    valid_free_blocks++;
    valid_free_bytes += 40*sizeof(int)-_size_meta_data();
    valid_allocated_blocks++;
    valid_allocated_bytes-=_size_meta_data();
    valid_meta_data_bytes+=_size_meta_data();
    TEST_MALLOC();
}

int main() {
        switch (MALLOC_VERSION) {
        case 2:
            remalloc_2_test();
            break;
        case 3:
            remalloc_3_test();
            break;
        default:
            printf("Invalid Malloc Version!");
            return 1;
    }

    printf("Success for remalloc test: %d.\n", MALLOC_VERSION);
    return 0;
}

