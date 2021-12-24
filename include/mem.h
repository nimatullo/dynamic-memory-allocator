#include "sfmm.h"

#define HEADER_SIZE 8
#define CHUNKSIZE 1<<13
#define ALIGNMENT_SIZE 64
#define SIZE_MASK -4

int get_size(sf_block *block);
int is_free(sf_block *block);
int is_prev_allocd(sf_block *block);

sf_header *get_header(sf_block *block);

void set_footer(void * block, sf_footer footer);
sf_footer *get_footer(sf_block *block);

sf_block *get_next_block(sf_block *block); 
sf_block *get_prev_block(sf_block *block);

void free_block(sf_block *freed_block);

int validate_block(void* pointer);