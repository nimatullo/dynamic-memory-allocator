#include "sfmm.h"

int init_heap();
void init_prologue();
void init_epilogue();
void init_freelists();

int grow_heap();
void coalesce(sf_block *block);

void add_to_freelist(sf_block *block);
void remove_from_freelist(sf_block *block);
sf_block *get_remaining();

sf_block *find_block(size_t block_size);
void split(sf_block *block, size_t size);


int* fib_vals(int *a, int fib_count);