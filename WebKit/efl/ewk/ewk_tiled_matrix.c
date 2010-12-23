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
#include "ewk_tiled_matrix.h"

#define _GNU_SOURCE
#include "ewk_tiled_backing_store.h"
#include "ewk_tiled_private.h"
#include <Eina.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h> // XXX remove me later
#include <stdlib.h>
#include <string.h>

static const Evas_Coord TILE_MATRIX_BASE_TILE_SIZE = 256;

struct _Ewk_Tile_Matrix {
    Eina_Matrixsparse *matrix;
    Ewk_Tile_Unused_Cache *tuc;
    Evas_Colorspace cspace;
    struct {
        void (*cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *update);
        void *data;
    } render;
    unsigned int frozen;
    Eina_List *updates;
#ifdef DEBUG_MEM_LEAKS
    struct {
        struct {
            uint64_t allocated, freed;
        } tiles, bytes;
    } stats;
#endif
};

#ifdef DEBUG_MEM_LEAKS
static uint64_t tiles_leaked = 0;
static uint64_t bytes_leaked = 0;
#endif

/* called when matrixsparse is resized or freed */
static void _ewk_tile_matrix_cell_free(void *user_data, void *cell_data)
{
    Ewk_Tile_Matrix *tm = user_data;
    Eina_Inlist *l = cell_data;

    if (!l)
        return;

    ewk_tile_unused_cache_freeze(tm->tuc);

    while (l) {
        Ewk_Tile *t = (Ewk_Tile *)l;
        l = l->next;

        if (t->updates || t->stats.full_update)
            tm->updates = eina_list_remove(tm->updates, t);

        if (t->visible)
            ERR("freeing cell that is visible, leaking tile %p", t);
        else {
            if (!ewk_tile_unused_cache_tile_get(tm->tuc, t))
                ERR("tile %p was not in cache %p? leaking...", t, tm->tuc);
            else {
                DBG("tile cell does not exist anymore, free it %p", t);

#ifdef DEBUG_MEM_LEAKS
                tm->stats.bytes.freed += t->bytes;
                tm->stats.tiles.freed++;
#endif

                ewk_tile_free(t);
            }
        }
    }

    ewk_tile_unused_cache_thaw(tm->tuc);
}

/* called when cache of unused tile is flushed */
static void _ewk_tile_matrix_tile_free(void *data, Ewk_Tile *t)
{
    Ewk_Tile_Matrix *tm = data;
    Eina_Matrixsparse_Cell *cell;
    Eina_Inlist *l, *old;

    if (!eina_matrixsparse_cell_idx_get(tm->matrix, t->row, t->col, &cell)) {
        ERR("removing tile %p that was not in the matrix? Leaking...", t);
        return;
    }

    if (t->updates || t->stats.full_update)
        tm->updates = eina_list_remove(tm->updates, t);

    old = eina_matrixsparse_cell_data_get(cell);
    l = eina_inlist_remove(old, EINA_INLIST_GET(t));
    if (!l) {
        /* set to null to avoid double free */
        eina_matrixsparse_cell_data_replace(cell, NULL, NULL);
        eina_matrixsparse_cell_clear(cell);
    } else if (old != l)
        eina_matrixsparse_cell_data_replace(cell, l, NULL);

    if (EINA_UNLIKELY(!!t->visible)) {
        ERR("cache of unused tiles requesting deletion of used tile %p? "
            "Leaking...", t);
        return;
    }

#ifdef DEBUG_MEM_LEAKS
    tm->stats.bytes.freed += t->bytes;
    tm->stats.tiles.freed++;
#endif

    ewk_tile_free(t);
}

/**
 * Creates a new matrix of tiles.
 *
 * The tile matrix is responsible for keeping tiles around and
 * providing fast access to them. One can use it to retrieve new or
 * existing tiles and give them back, allowing them to be
 * freed/replaced by the cache.
 *
 * @param tuc cache of unused tiles or @c NULL to create one
 *        automatically.
 * @param cols number of columns in the matrix.
 * @param rows number of rows in the matrix.
 * @param cspace the color space used to create tiles in this matrix.
 * @param render_cb function used to render given tile update.
 * @param data context to give back to @a render_cb.
 *
 * @return newly allocated instance on success, @c NULL on failure.
 */
Ewk_Tile_Matrix *ewk_tile_matrix_new(Ewk_Tile_Unused_Cache *tuc, unsigned long cols, unsigned long rows, Evas_Colorspace cspace, void (*render_cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *update), const void *data)
{
    Ewk_Tile_Matrix *tm;

    CALLOC_OR_OOM_RET(tm, sizeof(Ewk_Tile_Matrix), NULL);

    tm->matrix = eina_matrixsparse_new(rows, cols, _ewk_tile_matrix_cell_free, tm);
    if (!tm->matrix) {
        ERR("could not create sparse matrix.");
        free(tm);
        return NULL;
    }

    if (tuc)
        tm->tuc = ewk_tile_unused_cache_ref(tuc);
    else {
        tm->tuc = ewk_tile_unused_cache_new(40960000);
        if (!tm->tuc) {
            ERR("no cache of unused tile!");
            eina_matrixsparse_free(tm->matrix);
            free(tm);
            return NULL;
        }
    }

    tm->cspace = cspace;
    tm->render.cb = render_cb;
    tm->render.data = (void *)data;

    return tm;
}

/**
 * Destroys tiles matrix, releasing its resources.
 *
 * The cache instance is unreferenced, possibly freeing it.
 */
void ewk_tile_matrix_free(Ewk_Tile_Matrix *tm)
{
#ifdef DEBUG_MEM_LEAKS
    uint64_t tiles, bytes;
#endif

    EINA_SAFETY_ON_NULL_RETURN(tm);
    ewk_tile_unused_cache_freeze(tm->tuc);

    eina_matrixsparse_free(tm->matrix);

    ewk_tile_unused_cache_thaw(tm->tuc);
    ewk_tile_unused_cache_unref(tm->tuc);

#ifdef DEBUG_MEM_LEAKS
    tiles = tm->stats.tiles.allocated - tm->stats.tiles.freed;
    bytes = tm->stats.bytes.allocated - tm->stats.bytes.freed;

    tiles_leaked += tiles;
    bytes_leaked += bytes;

    if (tiles || bytes)
        ERR("tiled matrix leaked: tiles[+%"PRIu64",-%"PRIu64":%"PRIu64"] "
           "bytes[+%"PRIu64",-%"PRIu64":%"PRIu64"]",
            tm->stats.tiles.allocated, tm->stats.tiles.freed, tiles,
            tm->stats.bytes.allocated, tm->stats.bytes.freed, bytes);
    else if (tiles_leaked || bytes_leaked)
        WRN("tiled matrix had no leaks: tiles[+%"PRIu64",-%"PRIu64"] "
           "bytes[+%"PRIu64",-%"PRIu64"], but some other leaked "
            "%"PRIu64" tiles (%"PRIu64" bytes)",
            tm->stats.tiles.allocated, tm->stats.tiles.freed,
            tm->stats.bytes.allocated, tm->stats.bytes.freed,
            tiles_leaked, bytes_leaked);
    else
        INF("tiled matrix had no leaks: tiles[+%"PRIu64",-%"PRIu64"] "
           "bytes[+%"PRIu64",-%"PRIu64"]",
            tm->stats.tiles.allocated, tm->stats.tiles.freed,
            tm->stats.bytes.allocated, tm->stats.bytes.freed);
#endif

    free(tm);
}

/**
 * Resize matrix to given number of rows and columns.
 */
void ewk_tile_matrix_resize(Ewk_Tile_Matrix *tm, unsigned long cols, unsigned long rows)
{
    EINA_SAFETY_ON_NULL_RETURN(tm);
    eina_matrixsparse_size_set(tm->matrix, rows, cols);
}

/**
 * Get the cache of unused tiles in use by given matrix.
 *
 * No reference is taken to the cache.
 */
Ewk_Tile_Unused_Cache *ewk_tile_matrix_unused_cache_get(const Ewk_Tile_Matrix *tm)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, NULL);
    return tm->tuc;
}

/**
 * Get the exact tile for the given position and zoom.
 *
 * If the tile was unused then it's removed from the cache.
 *
 * After usage, please give it back using
 * ewk_tile_matrix_tile_put(). If you just want to check if it exists,
 * then use ewk_tile_matrix_tile_exact_exists().
 *
 * @param tm the tile matrix to get tile from.
 * @param col the column number.
 * @param row the row number.
 * @param zoom the exact zoom to use.
 *
 * @return The tile instance or @c NULL if none is found. If the tile
 *         was in the unused cache it will be @b removed (thus
 *         considered used) and one should give it back with
 *         ewk_tile_matrix_tile_put() afterwards.
 *
 * @see ewk_tile_matrix_tile_nearest_get()
 * @see ewk_tile_matrix_tile_exact_get()
 */
Ewk_Tile *ewk_tile_matrix_tile_exact_get(Ewk_Tile_Matrix *tm, unsigned long col, unsigned int row, float zoom)
{
    Ewk_Tile *t, *item, *item_found = NULL;
    Eina_Inlist *inl;

    t = eina_matrixsparse_data_idx_get(tm->matrix, row, col);
    if (!t)
        return NULL;

    if (t->zoom == zoom)
        goto end;

    EINA_INLIST_FOREACH(EINA_INLIST_GET(t), item) {
        if (item->zoom != zoom)
            continue;
        item_found = item;
        break;
    }

    if (!item_found)
        return NULL;

    inl = eina_inlist_promote(EINA_INLIST_GET(t), EINA_INLIST_GET(item_found));
    eina_matrixsparse_data_idx_replace(tm->matrix, row, col, inl, NULL);

  end:
    if (!t->visible) {
        if (!ewk_tile_unused_cache_tile_get(tm->tuc, t))
            WRN("Ewk_Tile was unused but not in cache? bug!");
    }

    return t;
}

/**
 * Checks if tile of given zoom exists in matrix.
 *
 * @param tm the tile matrix to check tile existence.
 * @param col the column number.
 * @param row the row number.
 * @param zoom the exact zoom to query.
 *
 * @return @c EINA_TRUE if found, @c EINA_FALSE otherwise.
 *
 * @see ewk_tile_matrix_tile_exact_get()
 */
Eina_Bool ewk_tile_matrix_tile_exact_exists(Ewk_Tile_Matrix *tm, unsigned long col, unsigned int row, float zoom)
{
    Ewk_Tile *t, *item;

    t = eina_matrixsparse_data_idx_get(tm->matrix, row, col);
    if (!t)
        return EINA_FALSE;

    EINA_INLIST_FOREACH(EINA_INLIST_GET(t), item) {
        if (item->zoom == zoom)
            return EINA_TRUE;
    }

    return EINA_FALSE;
}

/**
 * Get the nearest tile for given position and zoom.
 *
 * The nearest tile is the one at the given position but with the
 * closest zoom, this being the division of the tile zoom by the
 * desired zoom, the closest to 1.0 gets it.
 *
 * If the tile was unused then it's removed from the cache.
 *
 * After usage, please give it back using ewk_tile_matrix_tile_put().
 *
 * @param tm the tile matrix to get tile from.
 * @param col the column number.
 * @param row the row number.
 * @param zoom the exact zoom to use.
 *
 * @return The tile instance or @c NULL if none is found. If the tile
 *         was in the unused cache it will be @b removed (thus
 *         considered used) and one should give it back with
 *         ewk_tile_matrix_tile_put() afterwards.
 */
Ewk_Tile *ewk_tile_matrix_tile_nearest_get(Ewk_Tile_Matrix *tm, unsigned long col, unsigned int row, float zoom)
{
    Ewk_Tile *t, *item, *item_found = NULL;
    Eina_Inlist *inl;
    float zoom_found = 0;

    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, NULL);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(zoom > 0.0, NULL);

    t = eina_matrixsparse_data_idx_get(tm->matrix, row, col);
    if (!t)
        return NULL;

    if (t->zoom == zoom) {
        item_found = t;
        goto end;
    }

    EINA_INLIST_FOREACH(EINA_INLIST_GET(t), item) {
        float cur_zoom = item->zoom;

        if (cur_zoom == zoom) {
            item_found = item;
            break;
        }
        
        if (cur_zoom > zoom)
            cur_zoom = zoom / cur_zoom;
        else
            cur_zoom = cur_zoom / zoom;

        if (cur_zoom > zoom_found) {
            item_found = item;
            zoom_found = cur_zoom;
        }
    }

    if (!item_found)
        return NULL;

    inl = eina_inlist_promote(EINA_INLIST_GET(t), EINA_INLIST_GET(item_found));
    eina_matrixsparse_data_idx_replace(tm->matrix, row, col, inl, NULL);

  end:
    if (!item_found->visible) {
        if (!ewk_tile_unused_cache_tile_get(tm->tuc, item_found))
            WRN("Ewk_Tile was unused but not in cache? bug!");
    }

    return item_found;
}

/**
 * Create a new tile at given position and zoom level in the matrix.
 *
 * The newly created tile is considered in use and not put into cache
 * of unused tiles. After it is used one should call
 * ewk_tile_matrix_tile_put() to give it back to matrix.
 *
 * @param tm the tile matrix to create tile on.
 * @param col the column number.
 * @param row the row number.
 * @param zoom the level to create tile, used to determine tile size.
 */
Ewk_Tile *ewk_tile_matrix_tile_new(Ewk_Tile_Matrix *tm, Evas *evas, unsigned long col, unsigned int row, float zoom)
{
    Evas_Coord s;
    Eina_Inlist *old;
    Ewk_Tile *t;

    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, NULL);
    EINA_SAFETY_ON_FALSE_RETURN_VAL(zoom > 0.0, NULL);

    s = TILE_SIZE_AT_ZOOM(TILE_MATRIX_BASE_TILE_SIZE, zoom);
    zoom = (float)s / (float)TILE_MATRIX_BASE_TILE_SIZE;

    t = ewk_tile_new(evas, s, s, zoom, tm->cspace);
    if (!t) {
        ERR("could not create tile %dx%d at %f, cspace=%d",
            s, s, (double)zoom, tm->cspace);
        return NULL;
    }

    old = eina_matrixsparse_data_idx_get(tm->matrix, row, col);
    old = eina_inlist_prepend(old, EINA_INLIST_GET(t));
    if (!eina_matrixsparse_data_idx_replace(tm->matrix, row, col, t, NULL)) {
        ERR("could not set matrix cell, row/col outside matrix dimensions!");
        ewk_tile_free(t);
        return NULL;
    }

    t->col = col;
    t->row = row;
    t->x = col * s;
    t->y = row * s;

    cairo_translate(t->cairo, -t->x, -t->y);

    t->stats.full_update = EINA_TRUE;
    tm->updates = eina_list_append(tm->updates, t);

#ifdef DEBUG_MEM_LEAKS
    tm->stats.bytes.allocated += t->bytes;
    tm->stats.tiles.allocated++;
#endif

    return t;
}

/**
 * Gives back the tile to the tile matrix.
 *
 * This will report the tile is no longer in use by the one that got
 * it with ewk_tile_matrix_tile_nearest_get() or
 * ewk_tile_matrix_tile_exact_get().
 *
 * Any previous reference to tile should be released
 * (ewk_tile_hide()) before calling this function, so it will
 * be known if it is not visibile anymore and thus can be put into the
 * unused cache.
 *
 * @param tm the tile matrix to return tile to.
 * @param t the tile instance to return, must @b not be @c NULL.
 * @param last_used time in which tile was last used.
 *
 * @return #EINA_TRUE on success or #EINA_FALSE on failure.
 */
Eina_Bool ewk_tile_matrix_tile_put(Ewk_Tile_Matrix *tm, Ewk_Tile *t, double last_used)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(t, EINA_FALSE);

    if (t->visible)
        return EINA_TRUE;

    t->stats.last_used = last_used;
    return ewk_tile_unused_cache_tile_put(tm->tuc, t, _ewk_tile_matrix_tile_free, tm);
}

Eina_Bool ewk_tile_matrix_tile_update(Ewk_Tile_Matrix *tm, unsigned long col, unsigned int row, const Eina_Rectangle *update)
{
    Ewk_Tile *l, *t;
    Eina_Rectangle new_update;
    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(update, EINA_FALSE);

    memcpy(&new_update, update, sizeof(new_update));
    // check update is valid, otherwise return EINA_FALSE
    if (update->x < 0 || update->y < 0 || update->w <= 0 || update->h <= 0) {
        ERR("invalid update region.");
        return EINA_FALSE;
    }

    if (update->x + update->w - 1 >= TILE_MATRIX_BASE_TILE_SIZE)
        new_update.w = TILE_MATRIX_BASE_TILE_SIZE - update->x;
    if (update->y + update->h - 1 >= TILE_MATRIX_BASE_TILE_SIZE)
        new_update.h = TILE_MATRIX_BASE_TILE_SIZE - update->y;

    l = eina_matrixsparse_data_idx_get(tm->matrix, row, col);

    if (!l)
        return EINA_TRUE;

    EINA_INLIST_FOREACH(EINA_INLIST_GET(l), t) {
        if (!t->updates && !t->stats.full_update)
            tm->updates = eina_list_append(tm->updates, t);
        new_update.x = roundf(t->zoom * new_update.x);
        new_update.y = roundf(t->zoom * new_update.y);
        new_update.w = roundf(t->zoom * new_update.w);
        new_update.h = roundf(t->zoom * new_update.h);
        ewk_tile_update_area(t, &new_update);
    }

    return EINA_TRUE;
}

Eina_Bool ewk_tile_matrix_tile_update_full(Ewk_Tile_Matrix *tm, unsigned long col, unsigned int row)
{
    Ewk_Tile *l, *t;
    Eina_Matrixsparse_Cell *cell;
    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, EINA_FALSE);

    if (!eina_matrixsparse_cell_idx_get(tm->matrix, row, col, &cell))
        return EINA_FALSE;

    if (!cell)
        return EINA_TRUE;

    l = eina_matrixsparse_cell_data_get(cell);
    if (!l) {
        CRITICAL("matrix cell with no tile!");
        return EINA_TRUE;
    }

    EINA_INLIST_FOREACH(EINA_INLIST_GET(l), t) {
        if (!t->updates && !t->stats.full_update)
            tm->updates = eina_list_append(tm->updates, t);
        ewk_tile_update_full(t);
    }

    return EINA_TRUE;
}

void ewk_tile_matrix_tile_updates_clear(Ewk_Tile_Matrix *tm, Ewk_Tile *t)
{
    EINA_SAFETY_ON_NULL_RETURN(tm);
    if (!t->updates && !t->stats.full_update)
        return;
    ewk_tile_updates_clear(t);
    tm->updates = eina_list_remove(tm->updates, t);
}

static Eina_Bool _ewk_tile_matrix_slicer_setup(Ewk_Tile_Matrix *tm, const Eina_Rectangle *area, float zoom, Eina_Tile_Grid_Slicer *slicer)
{
    unsigned long rows, cols;
    Evas_Coord x, y, w, h, tw, th;

    if (area->w <= 0 || area->h <= 0) {
        WRN("invalid area region: %d,%d+%dx%d.",
            area->x, area->y, area->w, area->h);
        return EINA_FALSE;
    }

    x = area->x;
    y = area->y;
    w = area->w;
    h = area->h;

    tw = TILE_SIZE_AT_ZOOM(TILE_W, zoom);
    th = TILE_SIZE_AT_ZOOM(TILE_H, zoom);

    // cropping area region to fit matrix
    eina_matrixsparse_size_get(tm->matrix, &rows, &cols);
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }

    if (y + h - 1 > (long)(rows * th))
        h = rows * th - y;
    if (x + w - 1 > (long)(cols * tw))
        w = cols * tw - x;

    return eina_tile_grid_slicer_setup(slicer, x, y, w, h, tw, th);
}


Eina_Bool ewk_tile_matrix_update(Ewk_Tile_Matrix *tm, const Eina_Rectangle *update, float zoom)
{
    const Eina_Tile_Grid_Info *info;
    Eina_Tile_Grid_Slicer slicer;
    EINA_SAFETY_ON_NULL_RETURN_VAL(tm, EINA_FALSE);
    EINA_SAFETY_ON_NULL_RETURN_VAL(update, EINA_FALSE);

    if (update->w < 1 || update->h < 1) {
        DBG("Why we get updates with empty areas? %d,%d+%dx%d at zoom %f",
            update->x, update->y, update->w, update->h, zoom);
        return EINA_TRUE;
    }

    if (!_ewk_tile_matrix_slicer_setup(tm, update, zoom, &slicer)) {
        ERR("Could not setup slicer for update %d,%d+%dx%d at zoom %f",
            update->x, update->y, update->w, update->h, zoom);
        return EINA_FALSE;
    }

    while (eina_tile_grid_slicer_next(&slicer, &info)) {
        unsigned long col, row;
        Ewk_Tile *l, *t;
        col = info->col;
        row = info->row;

        l = eina_matrixsparse_data_idx_get(tm->matrix, row, col);
        if (!l)
            continue;

        EINA_INLIST_FOREACH(EINA_INLIST_GET(l), t) {
            if (!t->updates && !t->stats.full_update)
                tm->updates = eina_list_append(tm->updates, t);
            if (info->full)
                ewk_tile_update_full(t);
            else {
                if (t->zoom != zoom)
                    ewk_tile_update_full(t);
                else
                    ewk_tile_update_area(t, &info->rect);
            }
        }
    }


    return EINA_TRUE;
}

void ewk_tile_matrix_updates_process(Ewk_Tile_Matrix *tm)
{
    Eina_List *l, *l_next;
    Ewk_Tile *t;
    EINA_SAFETY_ON_NULL_RETURN(tm);

    // process updates, unflag tiles
    EINA_LIST_FOREACH_SAFE(tm->updates, l, l_next, t) {
        ewk_tile_updates_process(t, tm->render.cb, tm->render.data);
        if (t->visible) {
            ewk_tile_updates_clear(t);
            tm->updates = eina_list_remove_list(tm->updates, l);
        }
    }
}

void ewk_tile_matrix_updates_clear(Ewk_Tile_Matrix *tm)
{
    Ewk_Tile *t;
    EINA_SAFETY_ON_NULL_RETURN(tm);

    EINA_LIST_FREE(tm->updates, t)
        ewk_tile_updates_clear(t);
    tm->updates = NULL;
}

// remove me later!
void ewk_tile_matrix_dbg(const Ewk_Tile_Matrix *tm)
{
    Eina_Iterator *it = eina_matrixsparse_iterator_complete_new(tm->matrix);
    Eina_Matrixsparse_Cell *cell;
    Eina_Bool last_empty = EINA_FALSE;

#ifdef DEBUG_MEM_LEAKS
    printf("Ewk_Tile Matrix: tiles[+%"PRIu64",-%"PRIu64":%"PRIu64"] "
           "bytes[+%"PRIu64",-%"PRIu64":%"PRIu64"]\n",
           tm->stats.tiles.allocated, tm->stats.tiles.freed,
           tm->stats.tiles.allocated - tm->stats.tiles.freed,
           tm->stats.bytes.allocated, tm->stats.bytes.freed,
           tm->stats.bytes.allocated - tm->stats.bytes.freed);
#else
    printf("Ewk_Tile Matrix:\n");
#endif

    EINA_ITERATOR_FOREACH(it, cell) {
        unsigned long row, col;
        Eina_Inlist *l;
        eina_matrixsparse_cell_position_get(cell, &row, &col);
        l = eina_matrixsparse_cell_data_get(cell);

        if (!l) {
            if (!last_empty) {
                last_empty = EINA_TRUE;
                printf("Empty:");
            }
            printf(" [%lu,%lu]", col, row);
        } else {
            if (last_empty) {
                last_empty = EINA_FALSE;
                printf("\n");
            }
            Ewk_Tile *t;

            printf("%3lu,%3lu %10p:", col, row, l);
            EINA_INLIST_FOREACH(l, t)
                printf(" [%3lu,%3lu + %dx%d @ %0.3f]%c",
                       t->col, t->row, t->w, t->h, t->zoom,
                       t->visible ? '*': ' ');
            printf("\n");
        }
    }
    if (last_empty)
        printf("\n");
    eina_iterator_free(it);

    ewk_tile_unused_cache_dbg(tm->tuc);
}

/**
 * Freeze matrix to not do maintenance tasks.
 *
 * Maintenance tasks optimize usage, but maybe we know we should hold
 * on them until we do the last operation, in this case we freeze
 * while operating and then thaw when we're done.
 *
 * @see ewk_tile_matrix_thaw()
 */
void ewk_tile_matrix_freeze(Ewk_Tile_Matrix *tm)
{
    EINA_SAFETY_ON_NULL_RETURN(tm);
    if (!tm->frozen)
        ewk_tile_unused_cache_freeze(tm->tuc);
    tm->frozen++;
}

/**
 * Unfreezes maintenance tasks.
 *
 * If this is the last counterpart of freeze, then maintenance tasks
 * will run immediately.
 */
void ewk_tile_matrix_thaw(Ewk_Tile_Matrix *tm)
{
    EINA_SAFETY_ON_NULL_RETURN(tm);
    if (!tm->frozen) {
        ERR("thawing more than freezing!");
        return;
    }

    tm->frozen--;
    if (!tm->frozen)
        ewk_tile_unused_cache_thaw(tm->tuc);
}
