#include "mem.h"
#include "heap.h"
#include "sfmm.h"
#include "debug.h"

/**
 * @brief Initializes the heap. Initializes the
 * freelists, prologue, epilogue and free block.
 * 
 * @return int  0 if successful 
 *             -1 if unsuccessful
 */
int init_heap()
{
    int *status = sf_mem_grow();

    /* Check if sf_mem_grow was successful */
    if (status == NULL)
        return -1;

    init_freelists();
    init_prologue();
    init_epilogue();
    sf_block *remaining_block = get_remaining();
    add_to_freelist(remaining_block);

    return 0;
}

/**
 * @brief Grows the heap by a page amount
 * 
 * @return int  0 if successful
 *              -1 if unsuccessful
 */
int grow_heap()
{
    //Get the old epilogue
    size_t *old_epilogue = sf_mem_end();
    old_epilogue--;

    int *status = sf_mem_grow();
    if (status == NULL)
        return -1;

    size_t *new_epilogue = sf_mem_end();
    new_epilogue--;
    *new_epilogue = 0 | THIS_BLOCK_ALLOCATED;

    sf_block *left_over_block = NULL;
    size_t block_size;

    if (is_prev_allocd((sf_block *)(old_epilogue - 1)))
    {
        old_epilogue--; // now at the footer of the block before the old epilogue
        block_size = *old_epilogue & SIZE_MASK;

        left_over_block = (void *)old_epilogue - block_size;
        //Start from previous block end at the new epilogue
        block_size = (void *)new_epilogue - (void *)left_over_block - 8;
        left_over_block->header = block_size;
        set_footer(left_over_block, block_size);
        // When coalescing, we don't need to add the freed block into the
        // free  lists because all we're doing is extending the block
        // that's already there
    }
    else
    {
        block_size = (void *)new_epilogue - (void *)old_epilogue;
        // Start from old epilogue end at new epilogue
        old_epilogue--;
        if (is_prev_allocd((void *)old_epilogue))
        {
            block_size |= PREV_BLOCK_ALLOCATED;
        }
        left_over_block = (void *)old_epilogue;
        left_over_block->header = block_size;
        set_footer(left_over_block, block_size);
        coalesce(left_over_block);
    }
    return 0;
}

/**
 * @brief Get the remaining object
 * 
 * @return sf_block* 
 */
sf_block *get_remaining()
{
    // Get end of prologue
    size_t *heap_start = sf_mem_start();
    heap_start += 6;
    sf_block *remaining_block = (void *)heap_start + get_size((sf_block *)heap_start);

    // Get beginning of epilogue
    size_t *heap_end = sf_mem_end();
    heap_end -= 2; // Skip from epilogue prev_footer to header

    // Set size to end of prologue - beginning of epilogue
    size_t block_size = (void *)heap_end - (void *)remaining_block;
    block_size |= PREV_BLOCK_ALLOCATED; // Remaining block is going to follow the prologue which is allocated
    remaining_block->header = block_size;
    set_footer(remaining_block, block_size);

    return remaining_block;
}

/**
 * @brief Initializes the prologue with a 64 byte size 
 * and the allocated bit.
 * 
 */
void init_prologue()
{
    size_t *heap_start = sf_mem_start();
    heap_start += 6; // Size of size_t is 8 so adding 6 to it is the same as doing += 6*8
    size_t size = ALIGNMENT_SIZE | THIS_BLOCK_ALLOCATED;
    *(heap_start + 1) = size; // Set the allocated bit
}

/**
 * @brief Initializes the epilogue by
 * going to the end of the heap and go
 * upwards by a memory row.
 * 
 */
void init_epilogue()
{
    size_t *heap_end = sf_mem_end();
    heap_end--;
    *heap_end = 0 | THIS_BLOCK_ALLOCATED;
}

/**
 * @brief Initializes the freelists by making all the sentinel
 * nodes point to themselves.
 * 
 */
void init_freelists()
{
    for (int i = 0; i < NUM_FREE_LISTS; i++)
    {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

/**
 * @brief Adds a block to the appropriate free list size class
 * 
 * @param block block to be inserted
 */
void add_to_freelist(sf_block *block)
{
    if (!is_free(block))
        return;
    size_t block_size = get_size(block);
    int fib_arr[NUM_FREE_LISTS];
    int *class_sizes = fib_vals(fib_arr, NUM_FREE_LISTS);
    int i;

    /* Traverse through the class_sizes and find the block that is greater or equal to the size of the block */
    for (i = 0; i < NUM_FREE_LISTS-1; i++)
    {
        if ((class_sizes[i] * ALIGNMENT_SIZE) >= block_size)
            break;
    }

    sf_block *first = sf_free_list_heads[i].body.links.next;
    first->body.links.prev = block;
    block->body.links.next = first;
    block->body.links.prev = &sf_free_list_heads[i];
    sf_free_list_heads[i].body.links.next = block;
}

/**
 * @brief Looks through all the free lists to find a block
 * whose size is greater than or equal to the block_size
 * 
 * @param block_size size of the block
 * @return sf_block* instance of the free block
 */
sf_block *find_block(size_t block_size)
{
    while (true) {
        for (int i = 0; i < NUM_FREE_LISTS; i++)
        {
            sf_block *start_of_class_size = sf_free_list_heads[i].body.links.next;

            while (start_of_class_size != &sf_free_list_heads[i])
            {
                if (get_size(start_of_class_size) >= block_size)
                {
                    remove_from_freelist(start_of_class_size);
                    return start_of_class_size;
                }
                start_of_class_size = start_of_class_size->body.links.next;
            }
        }
        // Once the program has made it here, it means we could not find a block with an adequate size
        // so we will need to extend the heap and call the function again.
        if (grow_heap() == -1) {
            return NULL;
        }
    }
    return NULL;
}

/**
 * @brief (bear with me here) Sets the current block's previous's next field
 * to current blocks' next field.
 * And sets current block's next's previous field to current block's previous
 * field.
 * 
 * @param block 
 */
void remove_from_freelist(sf_block *block)
{
    (block->body.links.prev)->body.links.next = block->body.links.next;
    (block->body.links.next)->body.links.prev = block->body.links.prev;
}

/**
 * @brief Attempts to coalesce the block with any free block that immediately
 * precedes or follows it in the heap.
 * 
 * @param block
 */
void coalesce(sf_block *block)
{
    size_t prev_size, cur_size, next_size, header_size;
    if (!is_prev_allocd(block))
    {
        sf_block *prev = get_prev_block(block);
        remove_from_freelist(prev);
        prev_size = get_size(prev);
        cur_size = get_size(block);
        header_size = prev_size + cur_size;

        if (is_prev_allocd(prev))
            header_size |= PREV_BLOCK_ALLOCATED;
        prev->header = header_size; // Block is now size of prev block plus itself
        set_footer(block, header_size);
        block = prev; // set the start of the current block as the prev
    }
    sf_block *next = get_next_block(block);
    if (is_free(next))
    {
        cur_size = get_size(block);
        cur_size |= (is_prev_allocd(block)) ? PREV_BLOCK_ALLOCATED : cur_size;
        next_size = get_size(next);
        remove_from_freelist(next);
        block->header = (cur_size + next_size);
        set_footer(next, next_size + cur_size);
    }
    add_to_freelist(block);
}

/**
 * @brief Attempts to split the block into a specified chunk.
 * If successful, that remainder block gets coalesced into
 * the free lists. Success is determined if the remainder 
 * block is larger than 64 bytes.
 * 
 * @param block 
 * @param size 
 */
void split(sf_block *block, size_t size)
{
    size = (size < ALIGNMENT_SIZE) ? ALIGNMENT_SIZE : size;
    size_t total_block_size = get_size(block);

    if (total_block_size - size >= ALIGNMENT_SIZE)
    {

        size_t new_size = total_block_size - size;

        // We know the current block is going to be allocated,
        // so the remainder has to have prev_alloc bit set.
        new_size |= PREV_BLOCK_ALLOCATED;

        // If the previous block is allocated, set last 2 bits to 
        // 1s (the current block is allocated and the previous is)
        // if not, set the last bit 1 (the current block is allocated)
        size |= (is_prev_allocd(block)) ? (THIS_BLOCK_ALLOCATED + PREV_BLOCK_ALLOCATED) : THIS_BLOCK_ALLOCATED;
        block->header = size;
        set_footer(block, block->header);
        sf_block *remainder = (void*)block + get_size(block);
        remainder->header = new_size;
        set_footer(remainder, remainder->header);

        free_block(remainder);
        
        coalesce(remainder);
    }
}

int* fib_vals(int *a, int fib_count) {
    int i;

    a[0] = 1; a[1] = 2;

    for (i = 2; i  <= fib_count; i++) {
        a[i] = a[i-1] + a[i-2];
    }

    return a;
}