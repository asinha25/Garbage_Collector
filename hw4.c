//
// Name:  Aditya Sinha
// NetID: asinha
// CS 361 | Homework #04 - Garbage Collector
//
// Refrences used: *Textbook*
// 				   *Code provided during labs 6, 7, and 8*
//

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct memory_region{
  size_t * start;
  size_t * end;
};

struct memory_region global_mem;
struct memory_region heap_mem;
struct memory_region stack_mem;

void walk_region_and_mark(void* start, void* end);

//how many ptrs into the heap we have
#define INDEX_SIZE 1000
void* heapindex[INDEX_SIZE];


//grabbing the address and size of the global memory region from proc 
void init_global_range(){
  char file[100];
  char * line=NULL;
  size_t n=0;
  size_t read_bytes=0;
  size_t start, end;

  sprintf(file, "/proc/%d/maps", getpid());
  FILE * mapfile  = fopen(file, "r");
  if (mapfile==NULL){
    perror("opening maps file failed\n");
    exit(-1);
  }

  int counter=0;
  while ((read_bytes = getline(&line, &n, mapfile)) != -1) {
    if (strstr(line, "hw4")!=NULL){
      ++counter;
      if (counter==3){
        sscanf(line, "%lx-%lx", &start, &end);
        global_mem.start=(size_t*)start;
        // with a regular address space, our globals spill over into the heap
        global_mem.end=malloc(256);
        free(global_mem.end);
      }
    }
    else if (read_bytes > 0 && counter==3) {
      if(strstr(line,"heap")==NULL) {
        // with a randomized address space, our globals spill over into an unnamed segment directly following the globals
        sscanf(line, "%lx-%lx", &start, &end);
        printf("found an extra segment, ending at %zx\n",end);						
        global_mem.end=(size_t*)end;
      }
      break;
    }
  }
  fclose(mapfile);
}


//marking related operations

int is_marked(size_t* chunk) {
  return ((*chunk) & 0x2) > 0;
}

void mark(size_t* chunk) {
  (*chunk)|=0x2;
}

void clear_mark(size_t* chunk) {
  (*chunk)&=(~0x2);
}

// chunk related operations

#define chunk_size(c)  ((*((size_t*)c))& ~(size_t)3 ) 
void* next_chunk(void* c) { 
  if(chunk_size(c) == 0) {
    printf("Panic, chunk is of zero size.\n");
  }
  if((c+chunk_size(c)) < sbrk(0))
    return ((void*)c+chunk_size(c));
  else 
    return 0;
}
int in_use(void *c) { 
  return (next_chunk(c) && ((*(size_t*)next_chunk(c)) & 1));
}


// index related operations

#define IND_INTERVAL ((sbrk(0) - (void*)(heap_mem.start - 1)) / INDEX_SIZE)
void build_heap_index() {
  // TODO - Optional  
}

// the actual collection code
// 
// This function will sweep through the heap and free anything that is not marked.
// **marked blocks are reachable**
// **unmarked blocks are to be freed** 
void sweep() {
	size_t* current = heap_mem.start - 1;	// points to the header
	current = next_chunk(current);

	// when the current chunk < end of heap and not equal to NULL 
	while (current < (size_t*)sbrk(0) && current != NULL)  {
		// check if pointer is between current and next chunk
		size_t* next = next_chunk(current);	

		// if the chunk is marked, unmark it.
		if(is_marked(current))
			clear_mark(current);
		// if the chunk is unmarked and allocated (descendants of root node), free.
		else if(in_use(current))
			free(current + 1);
		// move to next chunk
		current = next;
	}
}

//
// Helper function that helps with marking
// If number is not an address on the heap, return 0. Else, return a pointer to the 
// chunk that holds the address that the pointer passed in as a parameter to.
//**determine if what "looks" like a pointer actually points to a block in the heap**
size_t * is_pointer(size_t * ptr) {
  // check if ptr is in the range of the heap - sbrk(0) takes care of end of heap
  if(ptr <= heap_mem.start || ptr > (size_t*)sbrk(0)) {
  	return NULL;
  }
  // mem section of current chunk -1 (for size) : [diagram in lab 6]
  size_t* current_chunk = heap_mem.start - 1;
  // when current chunk is not NULL and isn't at the end of heap... 	
  while(current_chunk <  (size_t*)sbrk(0) && current_chunk != NULL)	{
  	// point to the header of current chunk
  	size_t* next = next_chunk(current_chunk);
  	// check if ptr is between next and current chunk
  	if(ptr <= next && current_chunk <= ptr)	{
  		// return current chunk
  		return current_chunk;	
  	}
  	// move to next chunk
  	current_chunk = next;	
  } // end while
  return NULL;
}

// Helper function for walk_region_and_mark
// This recursively takes a start and end address and performs the operations 
// on every pointer wihtin that range
void mark_helper(size_t* p)  {

	size_t* chunk = is_pointer((size_t*)*p);
	// check if chunk is NULL
	if(chunk == NULL)
		return;
	// calling is_marked to check if chunk is marked
	if(is_marked(chunk))
		return;
	// calling mark - checks second bit by ORing (last three bits always 0)
	mark(chunk);
	// recursively repeat for every pointer in range
	for (int i=0; i < chunk_size(chunk)/8; i++)
		// keep going for next chunks
		mark_helper((size_t*)chunk+i);
	return;
}

// This is the function that will go through the data and stack sections, 
// finding pointers and marking the blocks they point to
void walk_region_and_mark(void* start, void* end) {
	size_t* current = start;
	// check if block is in range of heap
	while (current < (size_t*)end)  {
		// call the mark helper function to mark current block
		mark_helper(current);
		// keep going for next block
		current = current+1;
	}
}



// standard initialization 

void init_gc() {
  size_t stack_var;
  init_global_range();
  heap_mem.start=malloc(512);
  //since the heap grows down, the end is found first
  stack_mem.end=(size_t *)&stack_var;
}

void gc() {
  size_t stack_var;
  heap_mem.end=sbrk(0);
  //grows down, so start is a lower address
  stack_mem.start=(size_t *)&stack_var;

  // build the index that makes determining valid ptrs easier
  // implementing this smart function for collecting garbage can get bonus;
  // if you can't figure it out, just comment out this function.
  // walk_region_and_mark and sweep are enough for this project.
  build_heap_index();

  //walk memory regions
  walk_region_and_mark(global_mem.start,global_mem.end);
  walk_region_and_mark(stack_mem.start,stack_mem.end);
  sweep();
}
