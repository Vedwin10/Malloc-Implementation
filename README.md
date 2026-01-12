# Custom Memory Allocator

A thread-unsafe implementation of dynamic memory allocation functions (`malloc`, `calloc`, `free`, and `realloc`) in C, built from scratch using the `sbrk()` system call.

## Overview

This project implements a custom memory allocator that manages heap memory using explicit free lists and boundary tag coalescing. The allocator provides drop-in replacements for the standard C library memory management functions with additional heap corruption detection.

## Features

### Core Functionality
- **malloc**: Allocates memory blocks with first-fit search strategy
- **calloc**: Allocates zero-initialized memory for arrays with overflow protection
- **free**: Deallocates memory and returns it to the free list
- **realloc**: Resizes allocated memory blocks with in-place expansion optimization

### Memory Management Techniques
- **Free List Management**: Doubly-linked list of free blocks for efficient reuse
- **Block Coalescing**: Automatically merges adjacent free blocks to reduce fragmentation
- **Block Splitting**: Divides large free blocks to minimize wasted space
- **8-Byte Alignment**: Ensures all allocations are properly aligned for performance
- **Boundary Tags**: Header and footer metadata for bidirectional block traversal

### Security Features
- **Heap Corruption Detection**: Validates doubly-linked list integrity during unlink operations
- **Overflow Protection**: Checks for integer overflow in calloc
- **Immediate Crash on Corruption**: Prevents exploit execution when heap corruption is detected

## Implementation Details

### Data Structures

**Metadata Header** (24 bytes):
```c
typedef struct metadata {
    size_t size;           // Size of usable block
    int free;              // Free/allocated flag
    int padding;           // Alignment padding
    struct metadata* next; // Next free block
    struct metadata* prev; // Previous free block
} metadata_t;
```

**Footer** (8 bytes):
```c
typedef struct footer {
    size_t size;  // Size of usable block (for backward traversal)
} footer_t;
```

### Memory Layout
```
[metadata_t][user data (aligned)][footer_t][metadata_t][user data][footer_t]...
```

### Allocation Strategy
- **First-fit**: Searches the free list for the first block large enough to satisfy the request
- **Splitting**: If a free block is significantly larger than needed (>= 8 bytes remaining), it is split
- **Expansion**: If no suitable free block exists, expands the heap using `sbrk()`

### Deallocation Strategy
- Adds the freed block to the head of the free list
- Attempts to coalesce with adjacent blocks (both previous and next)
- Coalescing uses boundary tags (footers) for efficient backward traversal

### Reallocation Optimization
- Returns existing pointer if new size fits within current block
- Attempts in-place expansion by merging with next free block if available
- Falls back to allocate-copy-free if in-place expansion is not possible

## Technical Specifications

- **Alignment**: 8 bytes
- **Minimum Block Size**: 8 bytes of usable space
- **Metadata Overhead**: 32 bytes per block (24-byte header + 8-byte footer)
- **Heap Growth**: Dynamic via `sbrk()` system call
- **Thread Safety**: Not thread-safe (no locking mechanisms)

## Building and Testing

### Compilation
```bash
gcc -o test alloc.c test.c
```

### Usage
Simply include the header and link against the compiled allocator:
```c
#include <stdlib.h>

int main() {
    int* arr = malloc(10 * sizeof(int));
    // Use the allocated memory
    free(arr);
    return 0;
}
```

## Limitations

- **Not thread-safe**: Concurrent access will cause data races
- **No defragmentation**: Only coalesces adjacent free blocks
- **No shrinking**: Heap never returns memory to the OS (no `brk()` reduction)
- **First-fit may be suboptimal**: Can lead to higher fragmentation than best-fit strategies

## Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| malloc    | O(n)           | Linear scan of free list (first-fit) |
| free      | O(n)           | Includes coalescing with adjacent blocks |
| calloc    | O(n + k)       | malloc + memset (k = allocation size) |
| realloc   | O(n + k)       | May require malloc + memcpy + free |

where n = number of free blocks

## Future Improvements

- Implement segregated free lists for O(1) allocation
- Add best-fit or next-fit allocation strategies
- Implement memory pooling for common allocation sizes
- Add thread safety with mutexes
- Return unused memory to OS via `brk()`
- Implement defragmentation/compaction

## License

This project is released for educational and portfolio purposes.

## Acknowledgments

Developed as part of systems programming coursework, demonstrating understanding of:
- Low-level memory management
- Systems programming in C
- Data structure implementation
- Security-conscious coding practices
