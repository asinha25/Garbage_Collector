#include <unistd.h>

typedef struct chunk {
	size_t size;
	struct chunk* next;
	struct chunk* prev;
} chunk;


void init_gc();
void gc();

size_t* is_pointer(size_t* ptr);
void walk_region_and_mark(void* start, void* end);
void sweep();
void build_heap_index();

void mark(size_t* chunk);


