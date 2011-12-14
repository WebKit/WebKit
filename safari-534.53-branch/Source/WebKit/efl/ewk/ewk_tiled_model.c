/*
    Copyright (C) 2009-2010 Samsung Electronics
    Copyright (C) 2009-2010 ProFUSION embedded systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_tiled_model.h"

#define _GNU_SOURCE
#include "ewk_tiled_backing_store.h"
#include "ewk_tiled_private.h"
#include <Eina.h>
#include <eina_safety_checks.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h> // XXX REMOVE ME LATER
#include <stdlib.h>
#include <string.h>

#ifdef TILE_STATS_ACCOUNT_RENDER_TIME
#include <sys/time.h>
#endif

#ifndef CAIRO_FORMAT_RGB16_565
#define CAIRO_FORMAT_RGB16_565 4
#endif

#define IDX(col, row, rowspan) (col + (row * rowspan))
#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)

#ifdef DEBUG_MEM_LEAKS
static uint64_t tiles_allocated = 0;
static uint64_t tiles_freed = 0;
static uint64_t bytes_allocated = 0;
static uint64_t bytes_freed = 0;

struct tile_account {
    Evas_Coord size;
    struct {
        uint64_t allocated;
        uint64_t freed;
    } tiles, bytes;
};

static size_t accounting_len = 0;
static struct tile_account *accounting = NULL;

static inline struct tile_account *_ewk_tile_account_get(const Ewk_Tile *t)
{
    struct tile_account *acc;
    size_t i;

    for (i = 0; i < accounting_len; i++) {
        if (accounting[i].size == t->w)
            return accounting + i;
    }

    i = (accounting_len + 1) * sizeof(struct tile_account);
    REALLOC_OR_OOM_RET(accounting, i, NULL);

    acc = accounting + accounting_len;
    acc->size = t->w;
    acc->tiles.allocated = 0;
    acc->tiles.freed = 0;
    acc->bytes.allocated = 0;
    acc->bytes.freed = 0;

    accounting_len++;

    return acc;
}

static inline void _ewk_tile_account_allocated(const Ewk_Tile *t)
{
    struct tile_account *acc = _ewk_tile_account_get(t);
    if (!acc)
        return;
    acc->bytes.allocated += t->bytes;
    acc->tiles.allocated++;

    bytes_allocated += t->bytes;
    tiles_allocated++;
}

static inline void _ewk_tile_account_freed(const Ewk_Tile *t)
{
    struct tile_account *acc = _ewk_tile_account_get(t);
    if (!acc)
        return;

    acc->bytes.freed += t->bytes;
    acc->tiles.freed++;

    bytes_freed += t->bytes;
    tiles_freed++;
}

void ewk_tile_accounting_dbg(void)
{
    struct tile_account *acc;
    struct tile_account *acc_end;

    printf("TILE BALANCE: tiles[+%"PRIu64",-%"PRIu64":%"PRIu64"] "
           "bytes[+%"PRIu64",-%"PRIu64":%"PRIu64"]\n",
            tiles_allocated, tiles_freed, tiles_allocated - tiles_freed,
            bytes_allocated, bytes_freed, bytes_allocated - bytes_freed);

    if (!accounting_len)
        return;

    acc = accounting;
    acc_end = acc + accounting_len;
    printf("BEGIN: TILE BALANCE DETAILS (TO THIS MOMENT!):\n");
    for (; acc < acc_end; acc++) {
        uint64_t tiles, bytes;

        tiles = acc->tiles.allocated - acc->tiles.freed;
        bytes = acc->bytes.allocated - acc->bytes.freed;

        printf("   %4d: tiles[+%4"PRIu64",-%4"PRIu64":%4"PRIu64"] "
               "bytes[+%8"PRIu64",-%8"PRIu64":%8"PRIu64"]%s\n",
               acc->size,
               acc->tiles.allocated, acc->tiles.freed, tiles,
               acc->bytes.allocated, acc->bytes.freed, bytes,
               (bytes || tiles) ? " POSSIBLE LEAK" : "");
    }
    printf("END: TILE BALANCE DETAILS (TO THIS MOMENT!):\n");
}
#else

static inline void _ewk_tile_account_allocated(const Ewk_Tile *t) { }
static inline void _ewk_tile_account_freed(const Ewk_Tile *t) { }

void ewk_tile_accounting_dbg(void)
{
    printf("compile webkit with DEBUG_MEM_LEAKS defined!\n");
}
#endif

static inline void _ewk_tile_paint_rgb888(Ewk_Tile *t, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t *dst32, *dst32_end, c1;
    uint64_t *dst64, *dst64_end, c2;

    c1 = 0xff000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    c2 = ((uint64_t)c1 << 32) | c1;

    dst64 = (uint64_t *)t->pixels;
    dst64_end = dst64 + ((t->bytes / 8) & ~7);
    for (; dst64 < dst64_end; dst64 += 8) {
        /* TODO: ARM add pld or NEON instructions */
        dst64[0] = c2;
        dst64[1] = c2;
        dst64[2] = c2;
        dst64[3] = c2;
        dst64[4] = c2;
        dst64[5] = c2;
        dst64[6] = c2;
        dst64[7] = c2;
    }

    dst32 = (uint32_t *)dst64_end;
    dst32_end = (uint32_t *)(t->pixels + t->bytes);
    for (; dst32 < dst32_end; dst32++)
        *dst32 = c1;
}

static inline void _ewk_tile_paint_rgb565(Ewk_Tile *t, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t *dst16, *dst16_end, c1;
    uint64_t *dst64, *dst64_end, c2;

    c1 = ((((r >> 3) & 0x1f) << 11) |
          (((g >> 2) & 0x3f) << 5) |
          ((b >> 3) & 0x1f));

    c2 = (((uint64_t)c1 << 48) | ((uint64_t)c1 << 32) |
          ((uint64_t)c1 << 16) | c1);

    dst64 = (uint64_t *)t->pixels;
    dst64_end = dst64 + ((t->bytes / 8) & ~7);
    for (; dst64 < dst64_end; dst64 += 8) {
        /* TODO: ARM add pld or NEON instructions */
        dst64[0] = c2;
        dst64[1] = c2;
        dst64[2] = c2;
        dst64[3] = c2;
        dst64[4] = c2;
        dst64[5] = c2;
        dst64[6] = c2;
        dst64[7] = c2;
    }

    dst16 = (uint16_t *)dst16_end;
    dst16_end = (uint16_t *)(t->pixels + t->bytes);
    for (; dst16 < dst16_end; dst16++)
        *dst16 = c1;
}

static inline void _ewk_tile_paint(Ewk_Tile *t, uint8_t r, uint8_t g, uint8_t b)
{
    if (t->cspace == EVAS_COLORSPACE_ARGB8888)
        _ewk_tile_paint_rgb888(t, r, g, b);
    else if (t->cspace == EVAS_COLORSPACE_RGB565_A5P)
        _ewk_tile_paint_rgb565(t, r, g, b);
    else
        ERR("unknown color space: %d", t->cspace);
}

/**
 * Create a new tile of given size, zoom level and colorspace.
 *
 * After created these properties are immutable as they're the basic
 * characteristic of the tile and any change will lead to invalid
 * memory access.
 *
 * Other members are of free-access and no getters/setters are
 * provided in orderr to avoid expensive operations on those, however
 * some are better manipulated with provided functions, such as
 * ewk_tile_show() and ewk_tile_hide() to change
 * @c visible or ewk_tile_update_full(), ewk_tile_update_area(),
 * ewk_tile_updates_clear() to change @c stats.misses,
 * @c stats.full_update and @c updates.
 */
Ewk_Tile *ewk_tile_new(Evas *evas, Evas_Coord w, Evas_Coord h, float zoom, Evas_Colorspace cspace)
{
    Eina_Inlist *l;
    Evas_Coord *ec;
    Evas_Colorspace *ecs;
    float *f;
    size_t *s;
    Ewk_Tile *t;
    unsigned int area;
    size_t bytes;
    cairo_format_t format;
    cairo_status_t status;
    int stride;

    area = w * h;

    if (cspace == EVAS_COLORSPACE_ARGB8888) {
        bytes = area * 4;
        stride = w * 4;
        format = CAIRO_FORMAT_RGB24;
    } else if (cspace == EVAS_COLORSPACE_RGB565_A5P) {
        bytes = area * 2;
        stride = w * 2;
        format = CAIRO_FORMAT_RGB16_565;
    } else {
        ERR("unknown color space: %d", cspace);
        return NULL;
    }

    DBG("size: %dx%d (%d), zoom: %f, cspace=%d",
        w, h, area, (double)zoom, cspace);

    MALLOC_OR_OOM_RET(t, sizeof(Ewk_Tile), NULL);
    t->image = evas_object_image_add(evas);

    l = EINA_INLIST_GET(t);
    l->prev = NULL;
    l->next = NULL;

    t->visible = 0;
    t->updates = NULL;

    memset(&t->stats, 0, sizeof(Ewk_Tile_Stats));
    t->stats.area = area;

    /* ugly, but let's avoid at all costs having users to modify those */
    ec = (Evas_Coord *)&t->w;
    *ec = w;

    ec = (Evas_Coord *)&t->h;
    *ec = h;

    ecs = (Evas_Colorspace *)&t->cspace;
    *ecs = cspace;

    f = (float *)&t->zoom;
    *f = zoom;

    s = (size_t *)&t->bytes;
    *s = bytes;

    evas_object_image_size_set(t->image, t->w, t->h);
    evas_object_image_colorspace_set(t->image, t->cspace);
    t->pixels = evas_object_image_data_get(t->image, EINA_TRUE);
    t->surface = cairo_image_surface_create_for_data
        (t->pixels, format, w, h, stride);
    status = cairo_surface_status(t->surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("failed to create cairo surface: %s",
            cairo_status_to_string(status));
        free(t);
        return NULL;
    }

    t->cairo = cairo_create(t->surface);
    status = cairo_status(t->cairo);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("failed to create cairo: %s", cairo_status_to_string(status));
        cairo_surface_destroy(t->surface);
        evas_object_del(t->image);
        free(t);
        return NULL;
    }

    _ewk_tile_account_allocated(t);

    return t;
}

/**
 * Free tile memory.
 */
void ewk_tile_free(Ewk_Tile *t)
{
    _ewk_tile_account_freed(t);

    if (t->updates)
        eina_tiler_free(t->updates);

    cairo_surface_destroy(t->surface);
    cairo_destroy(t->cairo);
    evas_object_del(t->image);
    free(t);
}

/**
 * Make the tile visible, incrementing its counter.
 */
void ewk_tile_show(Ewk_Tile *t)
{
    t->visible++;
    evas_object_show(t->image);
}

/**
 * Decrement the visibility counter, making it invisible if necessary.
 */
void ewk_tile_hide(Ewk_Tile *t)
{
    t->visible--;
    if (!t->visible)
        evas_object_hide(t->image);
}

/**
 * Returns EINA_TRUE if the tile is visible, EINA_FALSE otherwise.
 */
Eina_Bool ewk_tile_visible_get(Ewk_Tile *t)
{
    return !!t->visible;
}

/**
 * Mark whole tile as dirty and requiring update.
 */
void ewk_tile_update_full(Ewk_Tile *t)
{
    /* TODO: list of tiles pending updates? */
    t->stats.misses++;

    if (!t->stats.full_update) {
        t->stats.full_update = EINA_TRUE;
        if (t->updates) {
            eina_tiler_free(t->updates);
            t->updates = NULL;
        }
    }
}

/**
 * Mark the specific subarea as dirty and requiring update.
 */
void ewk_tile_update_area(Ewk_Tile *t, const Eina_Rectangle *r)
{
    /* TODO: list of tiles pending updates? */
    t->stats.misses++;

    if (t->stats.full_update)
        return;

    if (!r->x && !r->y && r->w == t->w && r->h == t->h) {
        t->stats.full_update = EINA_TRUE;
        if (t->updates) {
            eina_tiler_free(t->updates);
            t->updates = NULL;
        }
        return;
    }

    if (!t->updates) {
        t->updates = eina_tiler_new(t->w, t->h);
        if (!t->updates) {
            CRITICAL("could not create eina_tiler %dx%d.", t->w, t->h);
            return;
        }
    }

    eina_tiler_rect_add(t->updates, r);
}

/**
 * For each updated region, call the given function.
 *
 * This will not change the tile statistics or clear the processed
 * updates, use ewk_tile_updates_clear() for that.
 */
void ewk_tile_updates_process(Ewk_Tile *t, void (*cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *update), const void *data)
{
    if (t->stats.full_update) {
        Eina_Rectangle r;
        r.x = 0;
        r.y = 0;
        r.w = t->w;
        r.h = t->h;
#ifdef TILE_STATS_ACCOUNT_RENDER_TIME
        struct timeval timev;
        double render_start;
        gettimeofday(&timev, NULL);
        render_start = (double)timev.tv_sec +
            (((double)timev.tv_usec) / 1000000);
#endif
        cb((void *)data, t, &r);
#ifdef TILE_STATS_ACCOUNT_RENDER_TIME
        gettimeofday(&timev, NULL);
        t->stats.render_time = (double)timev.tv_sec +
            (((double)timev.tv_usec) / 1000000) - render_start;
#endif
    } else if (t->updates) {
        Eina_Iterator *itr = eina_tiler_iterator_new(t->updates);
        Eina_Rectangle *r;
        if (!itr) {
            CRITICAL("could not create tiler iterator!");
            return;
        }
        EINA_ITERATOR_FOREACH(itr, r)
            cb((void *)data, t, r);
        eina_iterator_free(itr);
    }
}

/**
 * Clear all updates in region, if any.
 *
 * This will change the tile statistics, specially zero stat.misses
 * and unset stats.full_update. If t->updates existed, then it will be
 * destroyed.
 *
 * This function is usually called after ewk_tile_updates_process() is
 * called.
 */
void ewk_tile_updates_clear(Ewk_Tile *t)
{
    /* TODO: remove from list of pending updates? */
    t->stats.misses = 0;

    if (t->stats.full_update)
        t->stats.full_update = 0;
    else if (t->updates) {
        eina_tiler_free(t->updates);
        t->updates = NULL;
    }
}

typedef struct _Ewk_Tile_Unused_Cache_Entry Ewk_Tile_Unused_Cache_Entry;
struct _Ewk_Tile_Unused_Cache_Entry {
    Ewk_Tile *tile;
    int weight;
    struct {
        void (*cb)(void *data, Ewk_Tile *t);
        void *data;
    } tile_free;
};

struct _Ewk_Tile_Unused_Cache {
    struct {
        Eina_List *list;
        size_t count;
        size_t allocated;
    } entries;
    struct {
        size_t max;  /**< watermark (in bytes) to start freeing tiles */
        size_t used; /**< in bytes, maybe more than max. */
    } memory;
    struct {
        Evas_Coord x, y, w, h;
        float zoom;
        Eina_Bool locked;
    } locked;
    int references;
    unsigned int frozen;
    Eina_Bool dirty:1;
};

static const size_t TILE_UNUSED_CACHE_ALLOCATE_INITIAL = 128;
static const size_t TILE_UNUSED_CACHE_ALLOCATE_STEP = 16;
static const size_t TILE_UNUSED_CACHE_MAX_FREE = 32;

/**
 * Cache of unused tiles (those that are not visible).
 *
 * The cache of unused tiles.
 *
 * @param max cache size in bytes.
 *
 * @return newly allocated cache of unused tiles, use
 *         ewk_tile_unused_cache_free() to release resources. If not
 *         possible to allocate memory, @c NULL is returned.
 */
Ewk_Tile_Unused_Cache *ewk_tile_unused_cache_new(size_t max)
{
    Ewk_Tile_Unused_Cache *tuc;

    CALLOC_OR_OOM_RET(tuc, sizeof(Ewk_Tile_Unused_Cache), NULL);

    DBG("tuc=%p", tuc);
    tuc->memory.max = max;
    tuc->references = 1;
    return tuc;
}

void ewk_tile_unused_cache_lock_area(Ewk_Tile_Unused_Cache *tuc, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);

    tuc->locked.locked = EINA_TRUE;
    tuc->locked.x = x;
    tuc->locked.y = y;
    tuc->locked.w = w;
    tuc->locked.h = h;
    tuc->locked.zoom = zoom;
}

void ewk_tile_unused_cache_unlock_area(Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);

    tuc->locked.locked = EINA_FALSE;
}

/**
 * Free cache of unused tiles.
 *
 * This function should be only called by ewk_tile_unused_cache_unref
 * function. Calling this function without considering reference counting
 * may lead to unknown results.
 *
 * Those tiles that are still visible will remain live. The unused
 * tiles will be freed.
 *
 * @see ewk_tile_unused_cache_unref()
 */
static void _ewk_tile_unused_cache_free(Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);

    DBG("tuc=%p, "
        "entries=(count:%zd, allocated:%zd), "
        "memory=(max:%zd, used:%zd)",
        tuc, tuc->entries.count, tuc->entries.allocated,
        tuc->memory.max, tuc->memory.used);

    ewk_tile_unused_cache_clear(tuc);
    free(tuc);
}

/**
 * Clear cache of unused tiles.
 *
 * Any tiles that are in the cache are freed. The only tiles that are
 * kept are those that aren't in the cache (i.e. that are visible).
 */
void ewk_tile_unused_cache_clear(Ewk_Tile_Unused_Cache *tuc)
{
    Ewk_Tile_Unused_Cache_Entry *itr;
    EINA_SAFETY_ON_NULL_RETURN(tuc);

    if (!tuc->entries.count)
        return;

    EINA_LIST_FREE(tuc->entries.list, itr) {
        itr->tile_free.cb(itr->tile_free.data, itr->tile);
        free(itr);
    }

    tuc->memory.used = 0;
    tuc->entries.count = 0;
    tuc->dirty = EINA_FALSE;
}

/**
 * Hold reference to cache.
 *
 * @return same pointer as taken.
 *
 * @see ewk_tile_unused_cache_unref()
 */
Ewk_Tile_Unused_Cache *ewk_tile_unused_cache_ref(Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(tuc, NULL);
    tuc->references++;
    return tuc;
}

/**
 * Release cache reference, freeing it if it drops to zero.
 *
 * @see ewk_tile_unused_cache_ref()
 * @see ewk_tile_unused_cache_free()
 */
void ewk_tile_unused_cache_unref(Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);
    tuc->references--;
    if (!tuc->references)
        _ewk_tile_unused_cache_free(tuc);
}

/**
 * Change cache capacity, in bytes.
 *
 * This will not flush cache, use ewk_tile_unused_cache_flush() or
 * ewk_tile_unused_cache_auto_flush() to do so.
 */
void ewk_tile_unused_cache_max_set(Ewk_Tile_Unused_Cache *tuc, size_t max)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);
    tuc->memory.max = max;
}

/**
 * Retrieve maximum cache capacity, in bytes.
 */
size_t ewk_tile_unused_cache_max_get(const Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(tuc, 0);
    return tuc->memory.max;
}

/**
 * Retrieve the used cache capacity, in bytes.
 */
size_t ewk_tile_unused_cache_used_get(const Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(tuc, 0);
    return tuc->memory.used;
}

/**
 * Flush given amount of bytes from cache.
 *
 * After calling this function, near @a bytes are freed from cache. It
 * may be less if cache did not contain that amount of bytes (ie: an
 * empty cache has nothing to free!) or more if the cache just
 * contained objects that were larger than the requested amount (this
 * is usually the case).
 *
 * @param tuc cache of unused tiles.
 * @param bytes amount to free.
 *
 * @return amount really freed.
 *
 * @see ewk_tile_unused_cache_used_get()
 */
size_t ewk_tile_unused_cache_flush(Ewk_Tile_Unused_Cache *tuc, size_t bytes)
{
    Ewk_Tile_Unused_Cache_Entry *itr;
    Eina_List *l, *l_next;
    EINA_SAFETY_ON_NULL_RETURN_VAL(tuc, 0);
    size_t done;
    unsigned int count;

    if (!tuc->entries.count)
        return 0;
    if (bytes < 1)
        return 0;

    /*
     * NOTE: the cache is a FIFO queue currently.
     * Don't need to sort any more.
     */

    if (tuc->dirty)
        tuc->dirty = EINA_FALSE;

    done = 0;
    count = 0;
    EINA_LIST_FOREACH_SAFE(tuc->entries.list, l, l_next, itr) {
        Ewk_Tile *t = itr->tile;
        if (done > bytes)
            break;
        if (tuc->locked.locked
            && t->x + t->w > tuc->locked.x
            && t->y + t->h > tuc->locked.y
            && t->x < tuc->locked.x + tuc->locked.w
            && t->y < tuc->locked.y + tuc->locked.h
            && t->zoom == tuc->locked.zoom) {
            continue;
        }
        done += sizeof(Ewk_Tile) + itr->tile->bytes;
        itr->tile_free.cb(itr->tile_free.data, itr->tile);
        tuc->entries.list = eina_list_remove_list(tuc->entries.list, l);
        free(itr);
        count++;
    }

    tuc->memory.used -= done;
    tuc->entries.count -= count;

    return done;
}

/**
 * Flush enough bytes to make cache usage lower than maximum.
 *
 * Just like ewk_tile_unused_cache_flush(), but this will make the cache
 * free enough tiles to respect maximum cache size as defined with
 * ewk_tile_unused_cache_max_set().
 *
 * This function is usually called when system becomes idle. This way
 * we keep memory low but do not impact performance when
 * creating/deleting tiles.
 */
void ewk_tile_unused_cache_auto_flush(Ewk_Tile_Unused_Cache *tuc)
{
    EINA_SAFETY_ON_NULL_RETURN(tuc);
    if (tuc->memory.used <= tuc->memory.max)
        return;
    ewk_tile_unused_cache_flush(tuc, tuc->memory.used - tuc->memory.max);
    if (tuc->memory.used > tuc->memory.max)
        CRITICAL("Cache still using too much memory: %zd KB; max: %zd KB",
                 tuc->memory.used, tuc->memory.max);
}

/**
 * Flag cache as dirty.
 *
 * If cache is dirty then next flush operations will have to recompute
 * weight and sort again to find the best tiles to expire.
 *
 * One must call this function when tile properties that may change
 * likeness of tile to be flushed change, like Tile::stats.
 */
void ewk_tile_unused_cache_dirty(Ewk_Tile_Unused_Cache *tuc)
{
    tuc->dirty = EINA_TRUE;
}

/**
 * Freeze cache to not do maintenance tasks.
 *
 * Maintenance tasks optimize cache usage, but maybe we know we should
 * hold on them until we do the last operation, in this case we freeze
 * while operating and then thaw when we're done.
 *
 * @see ewk_tile_unused_cache_thaw()
 */
void ewk_tile_unused_cache_freeze(Ewk_Tile_Unused_Cache *tuc)
{
    tuc->frozen++;
}

/**
 * Unfreezes maintenance tasks.
 *
 * If this is the last counterpart of freeze, then maintenance tasks
 * will run immediately.
 */
void ewk_tile_unused_cache_thaw(Ewk_Tile_Unused_Cache *tuc)
{
    if (!tuc->frozen) {
        ERR("thawing more than freezing!");
        return;
    }

    tuc->frozen--;
}

/**
 * Get tile from cache of unused tiles, removing it from the cache.
 *
 * If the tile is used, then it's not in cache of unused tiles, so it
 * is removed from the cache and may be given back with
 * ewk_tile_unused_cache_tile_put().
 *
 * @param tuc cache of unused tiles
 * @param t the tile to be removed from Ewk_Tile_Unused_Cache.
 *
 * @return #EINA_TRUE on success, #EINA_FALSE otherwise.
 */
Eina_Bool ewk_tile_unused_cache_tile_get(Ewk_Tile_Unused_Cache *tuc, Ewk_Tile *t)
{
    Ewk_Tile_Unused_Cache_Entry *entry;
    Eina_List *e, *l;

    e = NULL;
    EINA_LIST_FOREACH(tuc->entries.list, l, entry)
    {
        if (entry->tile == t) {
            e = l;
            break;
        }
    }
    if (!e) {
        ERR("tile %p not found in cache %p", t, tuc);
        return EINA_FALSE;
    }

    tuc->entries.count--;
    tuc->memory.used -= sizeof(Ewk_Tile) + t->bytes;
    tuc->entries.list = eina_list_remove_list(tuc->entries.list, e);
    free(entry);
    // TODO assume dirty for now, but may it's not,
    // if the item was at the beginning of the queue
    tuc->dirty = EINA_TRUE;

    return EINA_TRUE;
}

/**
 * Put tile into cache of unused tiles, adding it to the cache.
 *
 * This should be called when @c t->visible is @c 0 and no objects are
 * using the tile anymore, making it available to be expired and have
 * its memory replaced.
 *
 * Note that tiles are not automatically deleted if cache is full,
 * instead the cache will have more bytes used than maximum and one
 * can call ewk_tile_unused_cache_auto_flush() to free them. This is done
 * because usually we want a lazy operation for better performance.
 *
 * @param tuc cache of unused tiles
 * @param t tile to be added to cache.
 * @param tile_free_cb function used to free tiles.
 * @param data context to give back to @a tile_free_cb as first argument.
 *
 * @return #EINA_TRUE on success, #EINA_FALSE otherwise. If @c t->visible
 *         is not #EINA_FALSE, then it will return #EINA_FALSE.
 *
 * @see ewk_tile_unused_cache_auto_flush()
 */
Eina_Bool ewk_tile_unused_cache_tile_put(Ewk_Tile_Unused_Cache *tuc, Ewk_Tile *t, void (*tile_free_cb)(void *data, Ewk_Tile *t), const void *data)
{
    Ewk_Tile_Unused_Cache_Entry *e;

    if (t->visible) {
        ERR("tile=%p is not unused (visible=%d)", t, t->visible);
        return EINA_FALSE;
    }

    MALLOC_OR_OOM_RET(e, sizeof(Ewk_Tile_Unused_Cache_Entry), EINA_FALSE);
    tuc->entries.list = eina_list_append(tuc->entries.list, e);
    if (eina_error_get()) {
        ERR("List allocation failed");
        return EINA_FALSE;
    }

    e->tile = t;
    e->weight = 0; /* calculated just before sort */
    e->tile_free.cb = tile_free_cb;
    e->tile_free.data = (void *)data;

    tuc->entries.count++;
    tuc->memory.used += sizeof(Ewk_Tile) + t->bytes;
    tuc->dirty = EINA_TRUE;

    return EINA_TRUE;
}

void ewk_tile_unused_cache_dbg(const Ewk_Tile_Unused_Cache *tuc)
{
    Ewk_Tile_Unused_Cache_Entry *itr;
    Eina_List *l;
    int count = 0;
    printf("Cache of unused tiles: entries: %zu/%zu, memory: %zu/%zu\n",
           tuc->entries.count, tuc->entries.allocated,
           tuc->memory.used, tuc->memory.max);

    EINA_LIST_FOREACH(tuc->entries.list, l, itr) {
        const Ewk_Tile *t = itr->tile;
        printf(" [%3lu,%3lu + %dx%d @ %0.3f]%c",
               t->col, t->row, t->w, t->h, t->zoom,
               t->visible ? '*': ' ');

        if (!(count % 4))
            printf("\n");
    }

    printf("\n");
}
