#include "alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 16 /**< The alignment of the memory blocks */

static free_block *HEAD = NULL; /**< Pointer to the first element of the free list */
// Worked with Ayden W, Luke Z and Ethan T throughout project

/**
 * Split a free block into two blocks
 *
 * @param block The block to split
 * @param size The size of the first new split block
 * @return A pointer to the first block or NULL if the block cannot be split
 */
void *split(free_block *block, int size) {
    if((block->size < size + sizeof(free_block))) {
        return NULL;
    }

    void *split_pnt = (char *)block + size + sizeof(free_block);
    free_block *new_block = (free_block *) split_pnt;

    new_block->size = block->size - size - sizeof(free_block);
    new_block->next = block->next;

    block->size = size;

    return block;
}

/**
 * Find the previous neighbor of a block
 *
 * @param block The block to find the previous neighbor of
 * @return A pointer to the previous neighbor or NULL if there is none
 */
free_block *find_prev(free_block *block) {
    free_block *curr = HEAD;
    while(curr != NULL) {
        char *next = (char *)curr + curr->size + sizeof(free_block);
        if(next == (char *)block)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Find the next neighbor of a block
 *
 * @param block The block to find the next neighbor of
 * @return A pointer to the next neighbor or NULL if there is none
 */
free_block *find_next(free_block *block) {
    char *block_end = (char*)block + block->size + sizeof(free_block);
    free_block *curr = HEAD;

    while(curr != NULL) {
        if((char *)curr == block_end)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * Remove a block from the free list
 *
 * @param block The block to remove
 */
void remove_free_block(free_block *block) {
    free_block *curr = HEAD;
    if(curr == block) {
        HEAD = block->next;
        return;
    }
    while(curr != NULL) {
        if(curr->next == block) {
            curr->next = block->next;
            return;
        }
        curr = curr->next;
    }
}

/**
 * Coalesce neighboring free blocks
 *
 * @param block The block to coalesce
 * @return A pointer to the first block of the coalesced blocks
 */
void *coalesce(free_block *block) {
    if (block == NULL) {
        return NULL;
    }

    free_block *prev = find_prev(block);
    free_block *next = find_next(block);

    // Coalesce with previous block if it is contiguous.
    if (prev != NULL) {
        char *end_of_prev = (char *)prev + prev->size + sizeof(free_block);
        if (end_of_prev == (char *)block) {
            prev->size += block->size + sizeof(free_block);

            // Ensure prev->next is updated to skip over 'block', only if 'block' is directly next to 'prev'.
            if (prev->next == block) {
                prev->next = block->next;
            }
            block = prev; // Update block to point to the new coalesced block.
        }
    }

    // Coalesce with next block if it is contiguous.
    if (next != NULL) {
        char *end_of_block = (char *)block + block->size + sizeof(free_block);
        if (end_of_block == (char *)next) {
            block->size += next->size + sizeof(free_block);

            // Ensure block->next is updated to skip over 'next'.
            block->next = next->next;
        }
    }

    return block;
}

/**
 * Call sbrk to get memory from the OS
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the allocated memory
 */
void *do_alloc(size_t size) {
    // stores currnt program break in void pointer p
    //Checks current position in heap before requesting new memory
    void *p = sbrk(0);
    intptr_t align = (intptr_t)p&(ALIGNMENT-1);
    intptr_t ag = (align == 0)?0:ALIGNMENT - align;
    
    //requests memory from system
    void *mem = sbrk(size + ag + sizeof(header));

    // sbrk fails return NULL
    if (mem == (void *)-1) {
        return NULL; 
    }
    //adjusts returned address for alignment
    void* head = (void*)((intptr_t)mem + ag);

    header *block = (header*)head;
    block->size = size;
    block->magic = 0x01234567;
    
    return head + sizeof(header);
}



/**
 * Allocates memory for the end user
 *
 * @param size The amount of memory to allocate
 * @return A pointer to the requested block of memory
 */
void *tumalloc(size_t size) {
    //Checks if there are any free blocks availbale
    if (HEAD == NULL) {
        void *ptr = do_alloc(size);

        //returns the allocated memory pointer
        return ptr;
        }
    else{
        //points to the head of the free list
        free_block* Fhead = HEAD;
        
        //goes through each block in the free list until it hits NUL
        while (Fhead != NULL) {
            //checks if overall size is less than current free block size
            if (size+sizeof(header) <= Fhead-> size){
                /*splits free block into two parts one for requested memory and the other to get added back to free list
                ptr gets address of newly allocated memory */
                void *ptr = split( Fhead, size+sizeof(header));
                //removes free block from free list
                remove_free_block(ptr);
                
                header *vars = (header*)ptr;
                //sets size to requested value
                vars -> size = size;
                //helps detect memory corruption errors
                vars -> magic = 0x01234567;
                
                

                return ptr + sizeof(header);
            }
            
            //goes to next block in the free list
            Fhead = Fhead->next;

        }
        //if no blocks could fit the requested memory do_alloc is called 
        void *ptr = do_alloc(size);

        return ptr;
    }
}



/**
 * Allocates and initializes a list of elements for the end user
 *
 * @param num How many elements to allocate
 * @param size The size of each element
 * @return A pointer to the requested block of initialized memory
 */
void *tucalloc(size_t num, size_t size) {
    void *ptr = tumalloc(size*num);

    //checks if tumalloc failed to allocate memory
    if (ptr == NULL) {
        return NULL;
    }

    else {
        //used to initialize allocated memory to 0
        memset(ptr, 0, size*num);
        return ptr;
    }
}

/**
 * Reallocates a chunk of memory with a bigger size
 *
 * @param ptr A pointer to an already allocated piece of memory
 * @param new_size The new requested size to allocate
 * @return A new pointer containing the contents of ptr, but with the new_size
 */
void *turealloc(void *ptr, size_t new_size) {

    void *re = tumalloc(new_size);
    //checks if tumalloc failed
    if (re == NULL) {
        return NULL;
    }
    //calculates pointer header associated with original memory block
    header *headr = (header *)(ptr - sizeof(header));
    
    if (headr -> magic == 0x01234567) {
        //copies original data to new block
        memcpy(re, ptr, headr -> size);
        tufree(ptr);
        return re;
    }
    else {
        printf("Error with Magic Number (turealloc)");
        return NULL;
    }
}

/**
 * Removes used chunk of memory and returns it to the free list
 *
 * @param ptr Pointer to the allocated piece of memory
 */
void tufree(void *ptr) {
    //moves ptr to start of header
    header *headr = (header *)ptr - 1;

    if (headr -> magic == 0x01234567) {
        //represents a block that can be added back to free list
        free_block *block = (free_block *)headr;
        block -> size = headr -> size;
        block -> next = HEAD;
        //
        HEAD = coalesce(block);
    }
    else {
        return;
    }
}
