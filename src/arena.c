#include "arena.h"

#include "benchmark.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

// adds the alignment and then masks the lower bits to get the next multiple of that alignment
#define ALIGN_ROUND_UP(x, b) (((x) + (b) - 1) & (~((b) - 1)))

void arena_alloc(arena_t *arena, uint64_t reserve_size) {

    // NOTE(spencer): this will return page-aligned memory (obviously) so I don't think it is
    // nessecary to make sure that the alignment suffices
    arena->base_ptr = calloc(reserve_size, 1);

    if (arena->base_ptr == NULL) {
        fprintf(stderr, "Failed to allocate arena memory");
        exit(EXT_ERROR_ARENA_ALLOCATION);
    }

    arena->capacity = reserve_size;
    arena->offset = 0;
};

void arena_free(arena_t *arena) {
    free(arena->base_ptr);
    memset(arena, 0, sizeof(*arena));
}

void *arena_push(arena_t *arena, uint64_t size, uint64_t alignment) {
    uint64_t aligned_offset = ALIGN_ROUND_UP(arena->offset, alignment);

    if (aligned_offset + size > arena->capacity) {
        fprintf(stderr, "Not enough memory in arena");
        // TODO(spencer): reallocate with bigger memory?
        return NULL;
    };

    void *ptr = arena->base_ptr + aligned_offset;
    arena->offset = aligned_offset + size;

    return ptr;
}

void arena_clear(arena_t *arena) { arena->offset = 0; }
