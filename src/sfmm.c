#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "mem.h"
#include "heap.h"

void *sf_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size += HEADER_SIZE; // Account for header

    // Make sure it is 64 bit aligned
    if (size % ALIGNMENT_SIZE != 0)
        size += ALIGNMENT_SIZE - (size % ALIGNMENT_SIZE);
    
    // If requested size is less than 64, set it to 64
    size = (size < ALIGNMENT_SIZE) ? ALIGNMENT_SIZE : size;

    if (sf_mem_start() == sf_mem_end())
    {
        if (init_heap() == -1)
        {
            return NULL;
        }
    }

    sf_block *raw_block = find_block(size);

    if (raw_block == NULL)
    {
        return NULL;
    }
    size_t allocated_size = raw_block->header | THIS_BLOCK_ALLOCATED;
    raw_block->header = allocated_size;
    set_footer(raw_block, allocated_size);

    split(raw_block, size);
    return &raw_block->body.payload;
}

void sf_free(void *pp)
{
    // Pointer comes from payload so we need to go to the beginning
    pp -= (2 * HEADER_SIZE);

    if (!validate_block(pp)) {
        abort();
    }
    sf_block *block = (sf_block*)pp;
    // Update current allocated bit and next block's prev_alloc bit
    free_block(block);

    coalesce(block);
    return;
}

void *sf_realloc(void *pp, size_t rsize)
{
    if (rsize == 0) {
        sf_free(pp);
        return NULL;
    }
    // Get to beginning of block
    pp -= 2 * HEADER_SIZE;
    if (!validate_block(pp)) {
        abort();
    }

    size_t block_size = get_size(pp);

    // Decrease size
    if (rsize < block_size) {
        rsize += HEADER_SIZE; // Account for header

        // Make sure it is 64 bit aligned
        if (rsize % ALIGNMENT_SIZE != 0)
            rsize += ALIGNMENT_SIZE - (rsize % ALIGNMENT_SIZE);
        
        rsize = (rsize < ALIGNMENT_SIZE) ? ALIGNMENT_SIZE : rsize;
        split(pp, rsize);
        return (pp + (2 * HEADER_SIZE));
    }
    else if (rsize == block_size){
        return pp + (2 * HEADER_SIZE);
    }

    // Increase size
    void *increased_block = sf_malloc(rsize);
    pp += (2 * HEADER_SIZE);               // Get to payload
    memcpy(increased_block, pp, block_size);
    sf_free(pp);
    return increased_block;
}
