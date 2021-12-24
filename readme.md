# Custom Dynamic Memory Allocator

Dynamic memory allocator using segregated lists and coalescing.

**Supports**
- `sf_malloc`
- `sf_realloc`
- `sf_free`


## Format of a free memory block
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | unused |prv alloc|  alloc  |
    |                                  (6 LSB's implicitly 0)    |  (0)   |  (0/1)  |   (0)   |
    |                                        (1 row)             | 4 bits |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                Pointer to next free block                               |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         |
    |                               Pointer to previous free block                            |
    |                                        (1 row)                                          |
    +-----------------------------------------------------------------------------------------+
    |                                                                                         | 
    |                                         Unused                                          | 
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- footer
    |                                       block_size           | unused |prv alloc|  alloc  |
    |                                  (6 LSB's implicitly 0)    |  (0)   |  (0/1)  |   (0)   |
    |                                        (1 row)             | 4 bits |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+

    NOTE: For a free block, footer contents must always be identical to header contents.

## Heap

The heap is designed to keep the payload area of each block aligned to an eight-row (64-byte) boundary. The header of a block precedes the payload area, and is only single-row (8-byte) aligned. The first block of the heap starts as soon as possible after the beginning of the heap, subject to the condition that its payload area is two-row aligned.
  
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +-----------------------------------------------------------------------------------------+ <- heap start
    |                                                                                         |    (aligned)
    |                                        Unused                                           |
    |                                       (7 rows)                                          |
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                  minimum block_size (64)   | unused |prv alloc|  alloc  |
    |                                  (6 LSB's implicitly 0)    |  (0)   |   (0)   |   (1)   | prologue block
    |                                        (1 row)             | 4 bits |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Unused Payload Area                                   |
    |                                        (7 rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | unused |prv alloc|  alloc  |
    |                                  (6 LSB's implicitly 0)    |  (0)   |   (1)   |  (0/1)  | first block
    |                                        (1 row)             | 4 bits |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- (aligned)
    |                                                                                         |
    |                                   Payload and Padding                                   |
    |                                        (N rows)                                         |
    |                                                                                         |
    |                                                                                         |
    +--------------------------------------------+------------------------+---------+---------+
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    |                             Additional allocated and free blocks                        |
    |                                                                                         |
    |                                                                                         |
    |                                                                                         |
    +------------------------------------------------------------+--------+---------+---------+ <- header
    |                                       block_size           | unused |prv alloc|  alloc  |
    |                                          (0)               |  (0)   |  (0/1)  |   (1)   | epilogue
    |                                        (1 row)             | 4 bits |  1 bit  |  1 bit  |
    +------------------------------------------------------------+--------+---------+---------+ <- heap end
                                                                                                   (aligned)
