#include <stdio.h>
#include <stdint.h>

#include "bitmap.h"
#include "block_store.h"
#include <stdlib.h> //for calloc and free
// include more if you need
#include <string.h> //for memcpy
#include <errno.h>	//for error messages
#include <fcntl.h>    // open, O_* flags
#include <unistd.h>   // write, read, close
#include <sys/stat.h>  // mode constants for permissions like 0666

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
	//allocate stuct
	block_store_t *bs = (block_store_t*)calloc(1, sizeof(block_store_t));
	if(!bs)	//check to see if memory was allocate
	{
		perror("Allocate struct failed");
		return NULL; //memory allocation failed return NULL
	}

	//allocate block storage
	bs->blocks = (uint8_t*)calloc(BLOCK_STORE_NUM_BLOCKS, BLOCK_SIZE_BYTES);
	if(!bs->blocks)	//check to see if blocks allocated
	{
		perror("Allocate storage failed");
		free(bs); //free block struct
		return NULL;
	}

	//overlay bit map inside block storage
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, bs->blocks + (BITMAP_START_BLOCK * BLOCK_SIZE_BYTES));
	if(!bs->fbm)
	{
		perror("Overlay failed");
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
	//if block does not exist, return
	if (!bs)
	{
		return;
	}
	//free all blocks
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
	if(!bs || !bs->fbm)
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
	// check if value is null, if so, return impossible value
	if (!bs || !bs->fbm) return SIZE_MAX;
	// return value of used blocks
	return bitmap_total_set(bs->fbm);
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	// check if value is null, if so, return impossible value
	if (!bs || !bs->fbm) return SIZE_MAX;
	// return value of free blocks
	return (BLOCK_STORE_NUM_BLOCKS - bitmap_total_set(bs->fbm));
}

size_t block_store_get_total_blocks()
{
	return BLOCK_STORE_NUM_BLOCKS;
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	//-------------input validation------------------
	//check to see if all inputs are not null
	if(!bs || !buffer || !bs->blocks || !bs->fbm)
	{
		return 0;	//can not use null inputs
	}
	//check if block_id makes sense
	if(block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return 0; 	//block id was out of bounds
	}
	//check to see if block has been alocated
	if(!bitmap_test(bs->fbm, block_id))
	{
		return 0;	//can not read from non alocated block
	}

	//------------Find data in memory-----------------
	//compute source adress
	uint8_t* src = bs->blocks + (block_id * BLOCK_SIZE_BYTES);

	//now we copy to buffer
	memcpy(buffer, src, BLOCK_SIZE_BYTES);

	return(BLOCK_SIZE_BYTES);

}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	//-------------input validation------------------
	//check to see if all inputs are valid
	if (!bs || !buffer || !bs->blocks || !bs->fbm)
	{
		return 0;
	}

	//check block bounds
	if (block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return 0;
	}

	//check that block is allocated
	if (!bitmap_test(bs->fbm, block_id))
	{
		return 0;
	}

	//compute destination address
	uint8_t *dest = bs->blocks + (block_id * BLOCK_SIZE_BYTES);

	//copy buffer into block storage
	memcpy(dest, buffer, BLOCK_SIZE_BYTES);

	return BLOCK_SIZE_BYTES;
}

block_store_t *block_store_deserialize(const char *const filename)
{
	// validate input
	if (!filename)
	{
		return NULL;
	}

	// open file for reading
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		return NULL;
	}

	// allocate block store struct
	block_store_t *bs = calloc(1, sizeof(block_store_t));
	if (!bs)
	{
		close(fd);
		return NULL;
	}

	// allocate raw block storage
	bs->blocks = calloc(BLOCK_STORE_NUM_BLOCKS, BLOCK_SIZE_BYTES);
	if (!bs->blocks)
	{
		close(fd);
		free(bs);
		return NULL;
	}

	// read all block data from file
	size_t bytes_expected = BLOCK_STORE_NUM_BYTES;
	size_t total_read = 0;

	while (total_read < bytes_expected)
	{
		ssize_t bytes_read = read(fd, bs->blocks + total_read, bytes_expected - total_read);
		if (bytes_read <= 0)
		{
			close(fd);
			free(bs->blocks);
			free(bs);
			return NULL;
		}
		total_read += (size_t)bytes_read;
	}

	close(fd);

	// recreate bitmap overlay using bitmap region inside block storage
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS,
	                         bs->blocks + (BITMAP_START_BLOCK * BLOCK_SIZE_BYTES));
	if (!bs->fbm)
	{
		perror("Overlay failed");
		free(bs->blocks);
		free(bs);
		return NULL;
	}

	return bs;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	//validate inputs
	if(!bs || !bs->blocks || !bs ->fbm || !filename)
	{
		return 0; //inputs were null
	}

	//open file as write only, create if does not exist, overwite,
	//and with file permission = read + write for user, group and other
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

	//check to see if file opened
	if(fd < 0)
	{
		perror("Open failed");
		return 0; //could not open file
	}

	//write to entire device
	ssize_t bytes_written = write(fd, bs->blocks, BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES);

	if (bytes_written < 0 || (size_t)bytes_written != (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES))
	{
		perror("write failed");	//could not write
		close(fd);
		return 0;
	}

	//close file
	close(fd);
	return(bytes_written);
}
