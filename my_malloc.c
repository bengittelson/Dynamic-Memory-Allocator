/*
 * CS 2110 Fall 2016
 * Author: BEN GITTELSON
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this for my_sbrk */
#include "my_sbrk.h"
/* we need this for the metadata_t struct definition */
#include "my_malloc.h"

/* You *MUST* use this macro when calling my_sbrk to allocate the
 * appropriate size. Failure to do so may result in an incorrect
 * grading!
 */
#define SBRK_SIZE 2048

/* If you want to use debugging printouts, it is HIGHLY recommended
 * to use this macro or something similar. If you produce output from
 * your code then you may receive a 20 point deduction. You have been
 * warned.
 */
#ifdef DEBUG
#define DEBUG_PRINT(x) printf(x)
#else
#define DEBUG_PRINT(x)
#endif

/* Our freelist structure - this is where the current freelist of
 * blocks will be maintained. failure to maintain the list inside
 * of this structure will result in no credit, as the grader will
 * expect it to be maintained here.
 * DO NOT CHANGE the way this structure is declared
 * or it will break the autograder.
 */
metadata_t* freelist;

void* my_malloc(size_t size) {
    //check request too large
    if (size + sizeof(int) + sizeof(metadata_t) > SBRK_SIZE) {
      ERRNO = SINGLE_REQUEST_TOO_LARGE;
      return NULL;
    }

    metadata_t* best_fit = find_smallest_fit(size);

    /*if best_fit returned null, the free list was null, or there
    wasn't a large enough block for the user*/
    if (best_fit == NULL) {
      best_fit = my_sbrk(SBRK_SIZE);

      //check if my_sbrk failed
      if (best_fit == NULL) {
        ERRNO = OUT_OF_MEMORY;
        return NULL;
      }
      best_fit->block_size = SBRK_SIZE;
      best_fit->request_size = 0;
      best_fit->next = NULL;
      add(best_fit);
      //recursive my_malloc call
      return my_malloc(size);
    }
    else {
      if (should_split(best_fit, size) == 1) {
        ERRNO = NO_ERROR;
        //add 1 so that the user can't write over the beginning metadata
        return split_block(best_fit, size) + 1;
      }
      else {
        /*the lines below happen in split_block if we call it, but we need to
        do them manually here*/
        best_fit->request_size = size;
        best_fit->block_size = size + sizeof(int) + sizeof(metadata_t);
        best_fit->canary = calculate_canary(best_fit);
        int* end_canary = (int*)((char*)best_fit + sizeof(metadata_t) + best_fit->request_size);
        *end_canary = best_fit->canary;
        removeBlock(best_fit);
        ERRNO = NO_ERROR;
        return best_fit + 1;
      }
    }
  }


//add a chunk back to the freelist
void add(metadata_t* add_ptr) {
  printf("ADDPTR: %lu \n", (uintptr_t)add_ptr);
  if (freelist == NULL) {
    freelist = add_ptr;
    freelist->next = NULL;
    return;
  }

  //special case to add to front
  if ((uintptr_t)add_ptr < (uintptr_t)freelist) {
    add_ptr->next = freelist;
    freelist = add_ptr;
    return;
  }

  else {
    //iterate through free list
    metadata_t* addCur = freelist;
    metadata_t* addPrev = NULL;
    //compare memory addresses:
    //the first time memory address of what we want to add is >
    //the current memory address, add it and stop
    while (addCur != NULL && ((uintptr_t)add_ptr > (uintptr_t)addCur)) {
      //check if you're adding an element that's already in the list
      addPrev = addCur;
      addCur = addCur->next;
    }

    add_ptr->next = addCur;
    if (addPrev != NULL) {
        addPrev->next = add_ptr;
    }
  }
}

//remove a block from the freelist
void removeBlock(metadata_t* remove_ptr) {

  //if freelist == NULL, do nothing
  if (freelist == NULL) {
    return;
  }

  //remove at the beginning of the list
  if ((uintptr_t)remove_ptr == (uintptr_t)freelist) {
    freelist = freelist->next;
    return;
  }
  metadata_t* removeCur = freelist;
  metadata_t* removePrev;

  //get to the memory address at which we want to remove
  while (removeCur != NULL && ((uintptr_t)remove_ptr != (uintptr_t)removeCur)) {
    removePrev = removeCur;
    removeCur = removeCur->next;
  }

  //check if removePrev next is NULL to deal with case in which you
  //remove an element toward the end of the list
  if (removePrev->next != NULL) {
    removePrev->next = removePrev->next->next;
  }
}


metadata_t* find_smallest_fit(size_t size) {
  //if freelist == NULL, return NULL
  if (freelist == NULL) {
      return NULL;
  }

  //if user request is too big, set error code and return NULL
  int user_size = size + sizeof(metadata_t) + sizeof(int);
  if (user_size > SBRK_SIZE) {
    ERRNO = SINGLE_REQUEST_TOO_LARGE;
    return NULL;
  }

  metadata_t* cur = freelist;
  metadata_t* best_fit = NULL;

  //iterate through the list
  while (cur != NULL) {
    //check if the block is big enough for what we want
    if (cur->block_size >= user_size) {
      //if the best_fit hasn't already been set, set it to the current node
      if (best_fit == NULL) {
        best_fit = cur;
      }

      //otherwise, check if the node we're on is smaller than the
      //current best_fit
      else {
        if (cur->block_size < best_fit->block_size) {
          best_fit = cur;
        }
      }
    }
    cur = cur -> next;
  }

  //if we never found a node that was big enough, return NULL
  if (best_fit == NULL) {
    return NULL;
  }

  //if the best fit isn't big enough, return NULL;
  if (best_fit->block_size < user_size) {
    return NULL;
  }
  return best_fit;
}

//split method
int should_split (metadata_t* block, size_t size) {
  int size_needed = size + sizeof(int) + sizeof(metadata_t);
  int min_split = sizeof(int) + sizeof(metadata_t);

  //first part checks if the block is bigger than what we need
  //second part checks whether the section that would be left over if we
  //split would be big enough to hold a canary and a metadata
  if (size_needed < block->block_size && (block->block_size - size_needed > min_split)) {
    return 1;
  }

  else {
    return 0;
  }
}

metadata_t* split_block(metadata_t* block, size_t size) {
  //calculate pointer to beginning of new block
  metadata_t* free_block = (metadata_t*)((char*)block + size + sizeof(int) + sizeof(metadata_t));
  free_block->block_size = block->block_size - size - sizeof(int) - sizeof(metadata_t);
  removeBlock(block);
  add(free_block);

  //set block's beginning and end canaries
  block->request_size = size;
  block->block_size = size + sizeof(int) + sizeof(metadata_t);
  block->canary = calculate_canary(block);

  //get the location of the end canary and then dereference it and set its value
  int* end_canary = (int*)((char*)block + sizeof(metadata_t) + block->request_size);
  *end_canary = block->canary;
  return block;
}

unsigned int calculate_canary(metadata_t* block) {
    return ((((int)block->block_size) << 16) | ((int)block->request_size))
            ^ (int)(uintptr_t)block;
}


void merge2(metadata_t* ptr) {
  metadata_t* cur = freelist;

  while (cur->next != NULL) {
    if ((uintptr_t)freelist == (uintptr_t)ptr) {
      break;
    }
    if ((uintptr_t)cur->next == (uintptr_t)ptr) {
      break;
    }
    cur = cur->next;
  }

  //merge all three
  if (cur->next != NULL) {
    //check if memory addresses are adjacent on the left and the right
    if (((char*)ptr + ptr->block_size == (char*)cur->next->next) &&
    ((char*)cur + cur->block_size == (char*)cur->next)) {
      cur->block_size = cur->block_size + cur->next->block_size + cur->next->next->block_size;
      cur->next = cur->next->next->next;
      return;
    }
  }

  //merge left = combine current and ptr
  if ((char*)cur + cur->block_size == (char*)cur->next) {
    cur->block_size += cur->next->block_size;
    cur->next = cur->next->next;
    return;
  }

  //merge right = block and current next
  if (cur->next != NULL && (char*)ptr + ptr->block_size == (char*)cur->next->next) {
    ptr->block_size += cur->next->next->block_size;
    ptr->next = ptr->next->next;
    return;
  }

  else {
    return;
  }
}

void* my_realloc(void* ptr, size_t new_size) {
    if (ptr == NULL) {
      void* retVal =  my_malloc(new_size);

      //check if malloc failed
      if (retVal == NULL) {
        return NULL;
      }
      else {
        return retVal;
      }
    }

    if (new_size == 0) {
      my_free(ptr);
      return NULL;
    }
    else {
      void* new = my_malloc(new_size);
      if (new == NULL) {
        return NULL;
      }

      //copy contents of old node to new one and free the old one
      memcpy(new, ptr, new_size);
      my_free(ptr);
      return new;
    }
    return NULL;
}

void* my_calloc(size_t nmemb, size_t size) {
    void* ptr = my_malloc(size*nmemb);
    if (ptr == NULL) {
      return NULL;
    }
    //zero out memory
    memset(ptr, 0, size*nmemb);
    return ptr;
}

void my_free(void* ptr) {
  if (ptr == NULL) {
    return;
  }

  //subtract 1 since you added 1 when you returned the memory to the user
  metadata_t* free_pointer = ((metadata_t*)ptr) - 1;
  //check whether beginning canary is correct:
  int test_canary = calculate_canary(free_pointer);
  if (test_canary != free_pointer->canary) {
    ERRNO = CANARY_CORRUPTED;
    return;
  }
  //check whether end canary is correct:
  int* end_canary_loc = (int*)((char*)free_pointer + sizeof(metadata_t) + free_pointer->request_size);

  if (*end_canary_loc != free_pointer->canary) {
    ERRNO = CANARY_CORRUPTED;
    return;
  }

  add(free_pointer);
  merge2(free_pointer);
  ERRNO = NO_ERROR;
}
