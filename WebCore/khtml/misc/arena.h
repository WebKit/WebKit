#ifndef ARENA_H
#define ARENA_H

// This code is a more-or-less direct port of the PLArena code found in the NSPR (the runtime
// used by Gecko).  This version is not thread safe.  It sacrifices thread safety for speed
// (no lock overhead when allocing out of the arena). -dwh

#define ARENA_ALIGN_MASK 3

typedef unsigned long uword;

struct Arena {
    Arena* next; 	// next arena
    uword base;		// aligned base address
    uword limit;	// end of arena (1+last byte)
    uword avail;	// points to next available byte in arena
};

struct ArenaPool {
    Arena first;	// first arena in pool list.
    Arena* current; // current arena.
    unsigned int arenasize;
    uword mask; 	// Mask (power-of-2 - 1)
};

void InitArenaPool(ArenaPool *pool, const char *name, 
                   unsigned int size, unsigned int align);
void FinishArenaPool(ArenaPool *pool);
void FreeArenaPool(ArenaPool *pool);
void* ArenaAllocate(ArenaPool *pool, unsigned int nb);

#define ARENA_ALIGN(pool, n) (((uword)(n) + ARENA_ALIGN_MASK) & ~ARENA_ALIGN_MASK)
#define INIT_ARENA_POOL(pool, name, size) \
        InitArenaPool(pool, name, size, ARENA_ALIGN_MASK + 1)

#define ARENA_ALLOCATE(p, pool, nb) \
        Arena *_a = (pool)->current; \
        unsigned int _nb = ARENA_ALIGN(pool, nb); \
        uword _p = _a->avail; \
        uword _q = _p + _nb; \
        if (_q > _a->limit) \
            _p = (uword)ArenaAllocate(pool, _nb); \
        else \
            _a->avail = _q; \
        p = (void *)_p;

#define ARENA_GROW(p, pool, size, incr) \
        Arena *_a = (pool)->current; \
        unsigned int _incr = ARENA_ALIGN(pool, incr); \
        uword _p = _a->avail; \
        uword _q = _p + _incr; \
        if (_p == (uword)(p) + ARENA_ALIGN(pool, size) && \
            _q <= _a->limit) { \
            _a->avail = _q; \
        } else { \
            p = ArenaGrow(pool, p, size, incr); \
        }

#define ARENA_MARK(pool) ((void *) (pool)->current->avail)
#define UPTRDIFF(p,q) ((uword)(p) - (uword)(q))     

#ifdef DEBUG
#define FREE_PATTERN 0xDA
#define CLEAR_UNUSED(a) (assert((a)->avail <= (a)->limit), \
                           memset((void*)(a)->avail, FREE_PATTERN, \
                            (a)->limit - (a)->avail))
#define CLEAR_ARENA(a)  memset((void*)(a), FREE_PATTERN, \
                            (a)->limit - (PRUword)(a))
#else
#define CLEAR_UNUSED(a)
#define CLEAR_ARENA(a)
#endif

#define ARENA_RELEASE(pool, mark) \
         char *_m = (char *)(mark); \
         Arena *_a = (pool)->current; \
         if (UPTRDIFF(_m, _a->base) <= UPTRDIFF(_a->avail, _a->base)) { \
             _a->avail = (uword)ARENA_ALIGN(pool, _m); \
             CLEAR_UNUSED(_a); \
         } else { \
             ArenaRelease(pool, _m); \
         }

#define ARENA_DESTROY(pool, a, pnext) \
         if ((pool)->current == (a)) (pool)->current = &(pool)->first; \
         *(pnext) = (a)->next; \
         CLEAR_ARENA(a); \
         free(a); \
         (a) = 0;

#endif ARENA_H
