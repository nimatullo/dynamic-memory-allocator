#include "sfmm.h"
#include "mem.h"
#include "debug.h"

/**
 * @brief Get the size of a block
 * 
 * @param block 
 * @return int 
 */
int get_size(sf_block *block)
{
    sf_header *header = get_header(block);
    return *(header) & SIZE_MASK;
}

/**
 * @brief check if a block is allocated or free
 * 
 * @param block 
 * @return int 
 */
int is_free(sf_block *block)
{
    return !(block->header & THIS_BLOCK_ALLOCATED) && block->header >= ALIGNMENT_SIZE;
}

/**
 * @brief check if the previous block is free or allocated
 * 
 * @param block 
 * @return int 
 */
int is_prev_allocd(sf_block *block)
{
    return (block->header & PREV_BLOCK_ALLOCATED) >> 1;
}

/**
 * @brief Set the footer of block
 * 
 * @param block 
 * @param footer 
 */
void set_footer(void *block, sf_footer footer)
{
    sf_footer *current_footer = get_footer(block);
    *current_footer = footer;
}

/**
 * @brief Get the footer of block
 * 
 * @param block 
 * @return sf_footer* 
 */
sf_footer *get_footer(sf_block *block)
{
    return &get_next_block(block)->prev_footer;
}

/**
 * @brief Get the header of a block
 * 
 * @param block 
 * @return sf_header* 
 */
sf_header *get_header(sf_block *block)
{
    return &(block->header);
}

/**
 * @brief Get the next block
 * 
 * @param block 
 * @return sf_block* 
 */
sf_block *get_next_block(sf_block *block)
{
    return (void *)block + get_size(block);
}

/**
 * @brief Get the prev block
 * 
 * @param block 
 * @return sf_block* 
 */
sf_block *get_prev_block(sf_block *block)
{
    return (void *)block - (block->prev_footer & SIZE_MASK);
}

void free_block(sf_block *block) {
    size_t freed_size = block->header & ~(THIS_BLOCK_ALLOCATED);
    block->header = freed_size;
    set_footer(block, freed_size);

    sf_block *next = get_next_block(block);
    freed_size = next->header & ~(PREV_BLOCK_ALLOCATED);
    next->header = freed_size;
    if (is_free(next)) {
        set_footer(next, freed_size);
    }
}

int validate_block(void *pointer)
{
    pointer += (2 * HEADER_SIZE);
    if (pointer == NULL)
    {
        return 0;
    }
    if ((size_t)pointer % ALIGNMENT_SIZE != 0)
    {
        return 0;
    }
    if (pointer < sf_mem_start() + (ALIGNMENT_SIZE - (2 * HEADER_SIZE)))
    {
        return 0;
    }
    if (pointer > sf_mem_end() - HEADER_SIZE)
    {
        return 0;
    }
    pointer -= (2 * HEADER_SIZE);
    if (is_free(pointer))
    {
        return 0;
    }
    if (!is_prev_allocd(pointer) && !is_free(get_prev_block(pointer)))
    {
        return 0;
    }
    return 1;
}