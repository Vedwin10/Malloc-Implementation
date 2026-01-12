#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// metadata struct
typedef struct metadata {
    size_t size;        // size of block
    int free;           // is free or not
    int padding;        // ensures alignment
    struct metadata* next;   // next ptr
    struct metadata* prev;   // prev ptr
} metadata_t;

typedef struct footer {
    size_t size;        // size of block
} footer_t;

// global vars
static metadata_t* free_list_head = NULL;
static void* heap_top = NULL;
static void* heap_start = NULL;

// makes sure user-requested size is aligned to 8 bytes
size_t aligned_size(size_t size) {
    if (size % 8 == 0) return size;
    else return size + (8 - (size % 8));
}

void set_footer(metadata_t* block) {
    footer_t* footer = (footer_t*)((char*)block + sizeof(metadata_t) + block->size);
    footer->size = block->size;
}

// returns ptr if found, NULL otherwise
metadata_t* find_free_block(size_t size) {
    if (!free_list_head) return NULL;

    metadata_t* curr = free_list_head;
    while (curr) {
        if (curr->size >= size) return curr;
        curr = curr->next;
    }

    return NULL;
}

// adds to free list and sets block->free = 1
void add_to_free_list(metadata_t* block) {
    block->free = 1;

    if (!free_list_head) {
        free_list_head = block;
        block->next = NULL;
        block->prev = NULL;
        return;
    }

    block->prev = NULL;
    block->next = free_list_head;
    free_list_head->prev = block;
    free_list_head = block;
}

// removes from free list and sets block->free = 0
void remove_from_free_list(metadata_t* block) {
    metadata_t* curr = free_list_head;
    while (curr) {
        if (curr == block) {
            block->free = 0;
            if (curr == free_list_head) free_list_head = curr->next;
            // Ensure the doubly linked list is intact before writing to memory.
            // If P->next->prev != P or P->prev->next != P, the heap is corrupted.
            if (curr->next && curr->next->prev != curr) {
                fprintf(stderr, "Corrupted heap detected: Next block's prev pointer does not point back to current block.\n");
                abort(); // Crash immediately to prevent exploit execution
            }
            if (curr->prev && curr->prev->next != curr) {
                fprintf(stderr, "Corrupted heap detected: Prev block's next pointer does not point back to current block.\n");
                abort(); // Crash immediately
            }

            // Proceed with standard unlink
            if (curr->next) curr->next->prev = curr->prev;
            if (curr->prev) curr->prev->next = curr->next;
            curr->next = NULL;
            curr->prev = NULL;
            return;
        }
        curr = curr->next;
    }
}

// check for coalesce (and do so if valid) with only the next adjacent block
void coalesce_next(metadata_t* block) {
    metadata_t* next_block = (void*)((char*)block + sizeof(metadata_t) + block->size + sizeof(footer_t));
    if ((void*)next_block == heap_top) return;

    if (next_block->free) {
        remove_from_free_list(block);
        remove_from_free_list(next_block);

        block->size = block->size + sizeof(footer_t) + sizeof(metadata_t) + next_block->size;
        set_footer(block);

        add_to_free_list(block);
    }
}

// check for coalesce (and do so if valid) with only the prev adjacent block
void coalesce_prev(metadata_t* block) {
    if ((void*)block == heap_start) return;
    footer_t* prev_footer = (footer_t*)((char*)block - sizeof(footer_t));
    size_t prev_size = prev_footer->size;
    metadata_t* prev_block = (void*)((char*)block - sizeof(footer_t) - prev_size - sizeof(metadata_t));

    if (prev_block->free) {
        remove_from_free_list(block);
        remove_from_free_list(prev_block);

        prev_block->size = prev_block->size + sizeof(footer_t) + sizeof(metadata_t) + block->size;
        set_footer(prev_block);

        add_to_free_list(prev_block);
    }
}

void coalesce(metadata_t* block) {
    coalesce_next(block);
    coalesce_prev(block);
}

void split_block(metadata_t* block, size_t size) {
    // check for at least 8 bytes of usable space
    size_t leftover = block->size - size;
    if (leftover < sizeof(metadata_t) + sizeof(footer_t) + 8) {
        remove_from_free_list(block);
        block->free = 0;
        return;
    }

    remove_from_free_list(block);
    block->size = size;
    block->free = 0;
    set_footer(block);

    metadata_t* new_block = (metadata_t*)((char*)block + sizeof(metadata_t) + block->size + sizeof(footer_t));
    new_block->size = leftover - sizeof(metadata_t) - sizeof(footer_t);
    new_block->free = 1;
    new_block->next = NULL;
    new_block->prev = NULL;
    set_footer(new_block);

    add_to_free_list(new_block);

    coalesce_next(new_block);
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    // implement calloc!
    if (num == 0 || size == 0) return NULL;

    size_t total_size = num * size;
    if (total_size / num != size) return NULL;

    void* ptr = malloc(total_size);
    if (!ptr) return NULL;

    memset(ptr, 0, total_size);
    return ptr;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */
void *malloc(size_t size) {
    // implement malloc!
    if (!size) return NULL;

    // initialize heap if needed
    if (!heap_top) {
        heap_start = sbrk(0);
        heap_top = heap_start;
    }
    if (heap_top == (void*)-1) return NULL; // sbrk failed

    // full block size (aligned to 8 bytes)
    size_t full_size = aligned_size(size);

    // if free block exists with enough space use and split, else expand heap
    metadata_t* new_block = find_free_block(full_size);
    if (new_block) {
        split_block(new_block, full_size);
    } else {
        new_block = (metadata_t*)sbrk(full_size + sizeof(metadata_t) + sizeof(footer_t));
        if ((void*)new_block == (void*)-1) return NULL; // sbrk failed
        heap_top = sbrk(0);
        new_block->size = full_size;
        new_block->free = 0;
        new_block->next = NULL;
        new_block->prev = NULL;
        set_footer(new_block);
    }

    return (void*)(new_block + 1);
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    // implement free!
    if (!ptr) return;
    metadata_t* block = ((metadata_t*)ptr) - 1;
    add_to_free_list(block);
    coalesce(block);
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */
void *realloc(void *ptr, size_t size) {
    // implement realloc!
    if (!ptr) return malloc(size);
    if (!size) {
        free(ptr);
        return NULL;
    }

    metadata_t* block = ((metadata_t*)ptr) - 1;
    size_t old_size = block->size;
    size_t new_size = aligned_size(size);

    if (new_size <= old_size) return ptr;   // could do block split for optimize
    
    metadata_t* next = (metadata_t*)((char*)block + sizeof(metadata_t) + old_size + sizeof(footer_t));
    if ((void*)next < heap_top && next->free) {
        size_t combined = old_size + sizeof(metadata_t) + next->size + sizeof(footer_t);
        
        if (combined >= new_size) {
            remove_from_free_list(next);
            block->size = combined;
            set_footer(block);
            return ptr;
        }
    }
    
    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, old_size);
    free(ptr);
    
    return new_ptr;
}
