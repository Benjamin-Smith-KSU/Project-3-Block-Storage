#include <stdio.h>
#include <stdint.h>

#include "bitmap.h"
#include "block_store.h"
// include more if you need


// You might find this handy. I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

//this is the implentaion of block_store that was defined in block_store.h
struct block_store
{
	uint8_t *blocks; //this is the raw block storage
	bitmap_t *fbm;	//this is the free block map (this will exist in the block data as overlay)
};


block_store_t *block_store_create()
{
	//This is old that ben has fixed
	// block_store_t * new_block = calloc(1, sizeof(block_store_t));
	// if (new_block == NULL) return NULL;
	// new_block->fbm = bitmap_overlay(BITMAP_SIZE_BITS, (void * const)BITMAP_START_BLOCK);
	// block_store_request(new_block, BITMAP_START_BLOCK);
	// return new_block;

	//allocate stuct
	block_store_t *bs = (block_store_t*)calloc(1, sizeof(block_store_t));
	if(!bs)	//check to see if memory was allocate
	{
		return NULL; //memory allocation failed return NULL
	}

	//allocate block storage
	bs->blocks = (uint8_t*)calloc(BLOCK_STORE_NUM_BLOCKS, BLOCK_SIZE_BYTES);
	if(!bs->blocks)	//check to see if blocks allocated
	{
		free(bs); //free block struct
		return NULL;
	}

	//overlay bit map inside block storage
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, bs->blocks + (BITMAP_START_BLOCK * BLOCK_SIZE_BYTES));
	if(!bs->fbm)
	{
		free(bs->blocks);
		free(bs);
		return NULL;
	}

	//we now how the memory
	//Now we must set the bit map as used
	size_t bitmap_blocks = (BITMAP_SIZE_BYTES + BLOCK_SIZE_BYTES - 1) / BLOCK_SIZE_BYTES;
	for(size_t i = 0; i < bitmap_blocks; i++)
	{
		bitmap_set(bs->fbm, BITMAP_START_BLOCK + i);
	}
	return(bs);
}

void block_store_destroy(block_store_t *const bs)
{
	if (!bs)
	{
		return;
	}

	bitmap_destroy(bs->fbm);
	free(bs->blocks);
	free(bs);
}

size_t block_store_allocate(block_store_t *const bs)
{
	if(!bs || !bs->fbm)
	{
		return SIZE_MAX;
	}

	//look for first free block
	for (size_t i = 0; i < BLOCK_STORE_NUM_BLOCKS; i++)
    {
        //check if block is free
        if (!bitmap_test(bs->fbm, i))
        {
            //mark as used
            bitmap_set(bs->fbm, i);

            //check to see if the set worked
            if (bitmap_test(bs->fbm, i))
            {
                return i;
            }
            else
            {
                return SIZE_MAX;
            }
        }
    }

	//no free block
	return SIZE_MAX;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
	//verify inputs
	if(!bs || !bs->fbm || !block_id)
	{
		return false;
	}

	//check the bounds
    if (block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return false;
	}        

	//check to see if already allocated
    if (bitmap_test(bs->fbm, block_id))
        return false;

    //mark as used
    bitmap_set(bs->fbm, block_id);

    //verify
    return bitmap_test(bs->fbm, block_id);
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
	if (!bs || !bs->fbm)
	{
		return;
	}

	if (block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return;
	}

	if (block_id >= BITMAP_START_BLOCK && block_id < (BITMAP_START_BLOCK + BITMAP_NUM_BLOCKS))
	{
		return;
	}

	bitmap_reset(bs->fbm, block_id);
}

size_t block_store_get_used_blocks(const block_store_t *const bs)
{
	if (!bs || !bs->fbm) return -1;
	return bitmap_total_set(bs->fbm);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	if (!bs || !bs->fbm) return -1;
	return (BLOCK_STORE_NUM_BLOCKS - bitmap_total_set(bs->fbm));
}

size_t block_store_get_total_blocks()
{
	return BLOCK_STORE_NUM_BLOCKS;
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

block_store_t *block_store_deserialize(const char *const filename)
{
	UNUSED(filename);
	return NULL;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	UNUSED(bs);
	UNUSED(filename);
	return 0;
}
