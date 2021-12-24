#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#include "mem.h"
#include "heap.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int index, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & ~0x3f)) {
		cnt++;
		if(size != 0) {
		    cr_assert_eq(index, i, "Block %p (size %ld) is in wrong list for its size "
				 "(expected %d, was %d)",
				 (long *)(bp) + 1, bp->header & ~0x3f, index, i);
		}
	    }
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(8000, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	void *x = sf_malloc(32624);
	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(524288);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 0, 1);
	assert_free_block_count(130944, 8, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(256, 3, 1);
	assert_free_block_count(7680, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(576, 5, 1);
	assert_free_block_count(7360, 8, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 0, 4);
	assert_free_block_count(256, 3, 3);
	assert_free_block_count(5696, 8, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 16,
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 16);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 128,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  *((long *)(bp) + 1) & ~0x3f, 128);

	assert_free_block_count(0, 0, 2);
	assert_free_block_count(64, 0, 1);
	assert_free_block_count(7808, 8, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 128,
		  "Block size (%ld) not what was expected (%ld)!",
	          *((long *)(bp) + 1) & ~0x3f, 128);

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(7936, 8, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)y - 16);
	cr_assert(*((long *)(bp) + 1) & 0x1, "Allocated bit is not set!");
	cr_assert((*((long *)(bp) + 1) & ~0x3f) == 64,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  *((long *)(bp) + 1) & ~0x3f, 64);

	assert_free_block_count(0, 0, 1);
	assert_free_block_count(8000, 8, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

//Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
//}

// TODO: Change the use of malloc to sf_malloc

Test(sfmm_student_suite, get_size_returns_last_2_bits, .timeout = TEST_TIMEOUT) {
	sf_block *p = sf_malloc(100);
	size_t header_size = 0x457;
	sf_header header = (header_size << 2);
	p->header = header;

	int size = get_size(p);
	size >>= 2;

	cr_assert(header_size == size, "Expected %d vs actual %d", header_size, size);
}


Test(sfmm_student_suite, is_free_returns_true, .timeout = TEST_TIMEOUT) {
	sf_block *p = sf_malloc(sizeof(sf_block));
	sf_free(p);

	int free_or_not = is_free(p);

	cr_assert(free_or_not == 1, "Expected %d vs actual %d", 1, free_or_not);
}

Test(sfmm_student_suite, is_prev_allocd_returns_true, .timeout = TEST_TIMEOUT) {
	sf_block *p = sf_malloc(sizeof(sf_block));
	size_t header_size = 0x3;
	sf_header header = header_size;
	p->header = header;
	

	int free_or_not = is_prev_allocd(p);

	cr_assert(free_or_not == 1, "Expected %d vs actual %d", 1, free_or_not);
}

Test(sfmm_student_suite, is_prev_allocd_returns_false, .timeout = TEST_TIMEOUT) {
	sf_block *p = sf_malloc(sizeof(sf_block));
	size_t header_size = 64 | 0x2;
	sf_header header = header_size;
	p->header = header;

	int free_or_not = is_prev_allocd(p);

	cr_assert(1 == free_or_not, "Expected %d vs actual %d", 1, free_or_not);
}

Test(sfmm_student_suite, get_footer_returns_valid_answer, .timeout = TEST_TIMEOUT) {
	sf_block *p = sf_malloc(sizeof(sf_block));
	size_t footer_size = 0xdead;
	sf_footer footer = footer_size;
	p->header = 0xdead;

	set_footer(p, footer_size);


	footer = *get_footer(p);

	cr_assert(footer == footer_size, "Expected %d vs actual %d", footer_size, footer);
}

// Test(sfmm_student_suite, get_prev_block_returns_corrently, .timeout = TEST_TIMEOUT) {
// 	sf_block *p = sf_malloc(sizeof(sf_block));
// 	sf_block *prev = sf_malloc(sizeof(sf_block));

// 	p->body.links.prev = prev;

// 	p->prev_footer = 0xDEAD;

// 	sf_footer *footer = get_footer(p);

// 	cr_assert(*footer == 0xDEAD, "Expected %d vs actual %d", *footer, 0xDEAD);
// }

Test(sfmm_student_suite, prologue_init_correctly, .timeout = TEST_TIMEOUT) {
	init_heap();

	size_t *heap_start = sf_mem_start();
	heap_start += 6;

	int size = get_size((sf_block *) heap_start);


	cr_assert(ALIGNMENT_SIZE == size, "Expected %d vs %d", ALIGNMENT_SIZE, size);
}

Test(sfmm_student_suite, epilogue_init_correctly, .timeout = TEST_TIMEOUT) {
	init_heap();
	int SIZE_OF_EPILOGUE = 1;
	size_t *heap_end = sf_mem_end();
	heap_end--;

	cr_assert(*heap_end == SIZE_OF_EPILOGUE, "Expected %d vs actual %d", *heap_end, SIZE_OF_EPILOGUE);
}

// Test(sfmm_student_suite, add_block_to_freelist_to_first, .timeout = TEST_TIMEOUT) {
// 	init_heap();
//     int class_sizes[9] = {1,2,3,5,8,13,21,34,35};
// 	sf_block *test_block;
// 	size_t block_size;

// 	for (int i = 0; i < 9; i++)
// 	{
// 		test_block = sf_malloc(sizeof(char)* 64);
// 		block_size = class_sizes[i] * MEM_ROW;
// 		test_block->header = block_size;
// 		set_footer(test_block, block_size);
// 		add_to_freelist(test_block);
// 		cr_assert(sf_free_list_heads[i].body.links.next->header == block_size, "expected %d vs actual %d", block_size, sf_free_list_heads[i].body.links.next->header);
// 	}
// }

Test(sfmm_student_suite, remove_free_block, .timeout = TEST_TIMEOUT) {
	init_heap();
	sf_block *free_block = sf_free_list_heads[8].body.links.next;
	remove_from_freelist(free_block);

	cr_assert(sf_free_list_heads[8].body.links.next == &sf_free_list_heads[8],
			"Expected %p vs actual %p",
			sf_free_list_heads[8].body.links.next,
			&sf_free_list_heads[8]);
}

Test(sfmm_student_suite, get_block_returns_correct_block, .timeout = TEST_TIMEOUT) {
	init_heap();

	sf_block *block = find_block(64);
	size_t expected = 8064;
	size_t actual = get_size(block);

	cr_assert(expected == actual, "Expected %d vs Actual %d", expected, actual);

}

Test(sfmm_student_suite, split_places_remainder_in_freelist, .timeout = TEST_TIMEOUT) {
	void *block = sf_malloc(200);

	split((block-16), 64);

	size_t actual = sf_free_list_heads[8].body.links.next->header;
	size_t expected = 8002;

	cr_assert(expected == actual, "Expected %d vs actual %d", expected, actual);
}
