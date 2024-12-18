#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>

typedef struct {
    void *base_ptr;
    uint64_t capacity;
    uint64_t offset;
} arena_t;

void arena_alloc(arena_t *arena, uint64_t reserve_size);
void arena_free(arena_t *arena);

void *arena_push(arena_t *arena, uint64_t size, uint64_t alignment);
void arena_clear(arena_t *arena);

#endif // ARENA_H
