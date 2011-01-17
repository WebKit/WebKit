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

#ifndef ewk_tiled_backing_store_h
#define ewk_tiled_backing_store_h

#include "EWebKit.h"

/* Enable accounting of render time in tile statistics */
// #define TILE_STATS_ACCOUNT_RENDER_TIME


/* If define ewk will do more accounting to check for memory leaks
 * try "kill -USR1 $PID" to get instantaneous debug
 * try "kill -USR2 $PID" to get instantaneous debug and force flush of cache
 */
#define DEBUG_MEM_LEAKS 1

#define TILE_W (256)
#define TILE_H (256)

#define ZOOM_STEP_MIN (0.01)

#define TILE_SIZE_AT_ZOOM(SIZE, ZOOM) ((int)roundf((SIZE) * (ZOOM)))
#define TILE_W_ZOOM_AT_SIZE(SIZE) ((float)SIZE / (float)TILE_W)
#define TILE_H_ZOOM_AT_SIZE(SIZE) ((float)SIZE / (float)TILE_H)

#ifdef __cplusplus
extern "C" {
#endif

#include <Evas.h>
#include <cairo.h>

typedef struct _Ewk_Tile                     Ewk_Tile;
typedef struct _Ewk_Tile_Stats               Ewk_Tile_Stats;
typedef struct _Ewk_Tile_Matrix              Ewk_Tile_Matrix;

struct _Ewk_Tile_Stats {
    double last_used;        /**< time of last use */
#ifdef TILE_STATS_ACCOUNT_RENDER_TIME
    double render_time;      /**< amount of time this tile took to render */
#endif
    unsigned int area;       /**< cache for (w * h) */
    unsigned int misses;     /**< number of times it became dirty but not
                              * repainted at all since it was not visible.
                              */
    Eina_Bool full_update:1; /**< tile requires full size update */
};

struct _Ewk_Tile {
    EINA_INLIST;            /**< sibling tiles at same (i, j) but other zoom */
    Eina_Tiler *updates;    /**< updated/dirty areas */
    Ewk_Tile_Stats stats;       /**< tile usage statistics */
    unsigned long col, row; /**< tile tile position */
    Evas_Coord x, y;        /**< tile coordinate position */

    /* TODO: does it worth to keep those or create on demand? */
    cairo_surface_t *surface;
    cairo_t *cairo;

    /** Never ever change those after tile is created (respect const!) */
    const Evas_Coord w, h;        /**< tile size (see TILE_SIZE_AT_ZOOM()) */
    const Evas_Colorspace cspace; /**< colorspace */
    const float zoom;             /**< zoom level contents were rendered at */
    const size_t bytes;           /**< bytes used in pixels. keep
                                   * before pixels to guarantee
                                   * alignement!
                                   */
    int visible;                  /**< visibility counter of this tile */
    Evas_Object *image;           /**< Evas Image, the tile to be rendered */
    uint8_t *pixels;
};

#include "ewk_tiled_matrix.h"
#include "ewk_tiled_model.h"

/* view */
EAPI Evas_Object *ewk_tiled_backing_store_add(Evas *e);

EAPI void ewk_tiled_backing_store_render_cb_set(Evas_Object *o, Eina_Bool (*cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *area), const void *data);

EAPI Eina_Bool    ewk_tiled_backing_store_scroll_full_offset_set(Evas_Object *o, Evas_Coord x, Evas_Coord y);
EAPI Eina_Bool    ewk_tiled_backing_store_scroll_full_offset_add(Evas_Object *o, Evas_Coord dx, Evas_Coord dy);
EAPI Eina_Bool    ewk_tiled_backing_store_scroll_inner_offset_add(Evas_Object *o, Evas_Coord dx, Evas_Coord dy, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h);

EAPI Eina_Bool    ewk_tiled_backing_store_zoom_set(Evas_Object *o, float *zoom, Evas_Coord cx, Evas_Coord cy, Evas_Coord *offx, Evas_Coord *offy);
EAPI Eina_Bool    ewk_tiled_backing_store_zoom_weak_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy);
EAPI void ewk_tiled_backing_store_fix_offsets(Evas_Object *o, Evas_Coord w, Evas_Coord h);
EAPI void ewk_tiled_backing_store_zoom_weak_smooth_scale_set(Evas_Object *o, Eina_Bool smooth_scale);
EAPI Eina_Bool    ewk_tiled_backing_store_update(Evas_Object *o, const Eina_Rectangle *update);
EAPI void ewk_tiled_backing_store_updates_process_pre_set(Evas_Object *o, void *(*cb)(void *data, Evas_Object *o), const void *data);
EAPI void ewk_tiled_backing_store_updates_process_post_set(Evas_Object *o, void *(*cb)(void *data, void *pre_data, Evas_Object *o), const void *data);
EAPI void ewk_tiled_backing_store_process_entire_queue_set(Evas_Object *o, Eina_Bool value);
EAPI void ewk_tiled_backing_store_updates_process(Evas_Object *o);
EAPI void ewk_tiled_backing_store_updates_clear(Evas_Object *o);
EAPI void ewk_tiled_backing_store_contents_resize(Evas_Object *o, Evas_Coord width, Evas_Coord height);
EAPI void ewk_tiled_backing_store_disabled_update_set(Evas_Object *o, Eina_Bool value);
EAPI void ewk_tiled_backing_store_flush(Evas_Object *o);
EAPI void ewk_tiled_backing_store_enable_scale_set(Evas_Object *o, Eina_Bool value);

EAPI Ewk_Tile_Unused_Cache *ewk_tiled_backing_store_tile_unused_cache_get(const Evas_Object *o);
EAPI void                   ewk_tiled_backing_store_tile_unused_cache_set(Evas_Object *o, Ewk_Tile_Unused_Cache *tuc);

EAPI Eina_Bool ewk_tiled_backing_store_pre_render_region(Evas_Object *o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom);
EAPI Eina_Bool ewk_tiled_backing_store_pre_render_relative_radius(Evas_Object *o, unsigned int n, float zoom);
EAPI void ewk_tiled_backing_store_pre_render_cancel(Evas_Object *o);

EAPI Eina_Bool ewk_tiled_backing_store_disable_render(Evas_Object *o);
EAPI Eina_Bool ewk_tiled_backing_store_enable_render(Evas_Object *o);
#ifdef __cplusplus
}
#endif
#endif // ewk_tiled_backing_store_h
