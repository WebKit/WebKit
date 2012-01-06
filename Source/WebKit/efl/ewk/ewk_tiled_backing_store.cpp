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
#include "ewk_tiled_backing_store.h"

#include "ewk_private.h"
#include "ewk_tiled_matrix.h"
#include "ewk_tiled_private.h"
#include <Ecore.h>
#include <Eina.h>
#include <algorithm>
#include <errno.h>
#include <math.h>
#include <stdio.h> // XXX REMOVE ME LATER
#include <stdlib.h>
#include <string.h>

#define IDX(col, row, rowspan) (col + (row * rowspan))

typedef struct _Ewk_Tiled_Backing_Store_Data Ewk_Tiled_Backing_Store_Data;
typedef struct _Ewk_Tiled_Backing_Store_Item Ewk_Tiled_Backing_Store_Item;
typedef struct _Ewk_Tiled_Backing_Store_Pre_Render_Request Ewk_Tiled_Backing_Store_Pre_Render_Request;

struct _Ewk_Tiled_Backing_Store_Item {
    EINA_INLIST;
    Ewk_Tile* tile;
    struct {
        Evas_Coord x, y, width, height;
    } geometry;
    bool smooth_scale;
};

struct _Ewk_Tiled_Backing_Store_Pre_Render_Request {
    EINA_INLIST;
    unsigned long column, row;
    float zoom;
};

struct _Ewk_Tiled_Backing_Store_Data {
    Evas_Object_Smart_Clipped_Data base;
    Evas_Object* self;
    Evas_Object* contentsClipper;
    struct {
        Eina_Inlist** items;
        Evas_Coord x, y, width, height;
        long cols, rows;
        struct {
            Evas_Coord width, height;
            float zoom;
            bool zoomWeakSmoothScale : 1;
            bool hasAlpha : 1;
        } tile;
        struct {
            struct {
                Evas_Coord x, y;
            } cur, old, base, zoomCenter;
        } offset;
    } view;
    Evas_Colorspace colorSpace;
    struct {
        Ewk_Tile_Matrix* matrix;
        struct {
            unsigned long column, row;
        } base;
        struct {
            unsigned long columns, rows;
        } current, old;
        Evas_Coord width, height;
    } model;
    struct {
        Eina_Bool (*callback)(void* data, Ewk_Tile* tile, const Eina_Rectangle* area);
        void* data;
        Eina_Inlist* preRenderRequests;
        Ecore_Idler* idler;
        bool disabled;
        bool suspend : 1;
    } render;
    struct {
        void* (*preCallback)(void* data, Evas_Object* ewkBackingStore);
        void* preData;
        void* (*postCallback)(void* data, void* preData, Evas_Object* ewkBackingStore);
        void* postData;
    } process;
    struct {
        bool any : 1;
        bool position : 1;
        bool size : 1;
        bool model : 1;
        bool offset : 1;
    } changed;
#ifdef DEBUG_MEM_LEAKS
    Ecore_Event_Handler* sig_usr;
#endif
};

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;
int _ewk_tiled_log_dom = -1;

#define PRIV_DATA_GET_OR_RETURN(obj, ptr, ...)                       \
    Ewk_Tiled_Backing_Store_Data* ptr = static_cast<Ewk_Tiled_Backing_Store_Data*>(evas_object_smart_data_get(obj)); \
    if (!ptr) {                                                      \
        CRITICAL("no private data in obj=%p", obj);                  \
        return __VA_ARGS__;                                          \
    }

static void _ewk_tiled_backing_store_fill_renderers(Ewk_Tiled_Backing_Store_Data* priv);
static inline void _ewk_tiled_backing_store_view_dbg(const Ewk_Tiled_Backing_Store_Data* priv);
static inline void _ewk_tiled_backing_store_changed(Ewk_Tiled_Backing_Store_Data* priv);

static inline void _ewk_tiled_backing_store_updates_process(Ewk_Tiled_Backing_Store_Data* priv)
{
    void* data = 0;

    /* Do not process updates. Note that we still want to get updates requests
     * in the queue in order to not miss any updates after the render is
     * resumed.
     */
    if (priv->render.suspend || !evas_object_visible_get(priv->self))
        return;

    if (priv->process.preCallback)
        data = priv->process.preCallback(priv->process.preData, priv->self);

    ewk_tile_matrix_updates_process(priv->model.matrix);

    if (priv->process.postCallback)
        priv->process.postCallback(priv->process.postData, data, priv->self);
}

static int _ewk_tiled_backing_store_flush(void* data)
{
    Ewk_Tiled_Backing_Store_Data* priv = static_cast<Ewk_Tiled_Backing_Store_Data*>(data);
    Ewk_Tile_Unused_Cache* tiledUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);

    if (tiledUnusedCache) {
        DBG("flush unused tile cache.");
        ewk_tile_unused_cache_auto_flush(tiledUnusedCache);
    } else
        ERR("no cache?!");

    return 0;
}

static Ewk_Tile* _ewk_tiled_backing_store_tile_new(Ewk_Tiled_Backing_Store_Data* priv, unsigned long column, unsigned long row, float zoom)
{
    Ewk_Tile* tile;
    Evas* evas = evas_object_evas_get(priv->self);
    if (!evas) {
        CRITICAL("evas_object_evas_get failed!");
        return 0;
    }

    tile = ewk_tile_matrix_tile_new
               (priv->model.matrix, evas, column, row, zoom);

    if (!tile) {
        CRITICAL("ewk_tile_matrix_tile_new failed!");
        return 0;
    }

    return tile;
}

static void _ewk_tiled_backing_store_item_move(Ewk_Tiled_Backing_Store_Item* item, Evas_Coord x, Evas_Coord y)
{
    item->geometry.x = x;
    item->geometry.y = y;

    if (item->tile)
        evas_object_move(item->tile->image, x, y);
}

static void _ewk_tiled_backing_store_item_resize(Ewk_Tiled_Backing_Store_Item* item, Evas_Coord width, Evas_Coord height)
{
    item->geometry.width = width;
    item->geometry.height = height;

    if (item->tile) {
        evas_object_resize(item->tile->image, width, height);
        evas_object_image_fill_set(item->tile->image, 0, 0, width, height);
    }
}

static void _ewk_tiled_backing_store_tile_associate(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tile* tile, Ewk_Tiled_Backing_Store_Item* item)
{
    if (item->tile)
        CRITICAL("it->tile=%p, but it should be 0!", item->tile);
    item->tile = tile;
    evas_object_move(item->tile->image, item->geometry.x, item->geometry.y);
    evas_object_resize(item->tile->image, item->geometry.width, item->geometry.height);
    evas_object_image_fill_set
        (item->tile->image, 0, 0, item->geometry.width, item->geometry.height);
    evas_object_image_smooth_scale_set(item->tile->image, item->smooth_scale);
    evas_object_image_alpha_set(item->tile->image, priv->view.tile.hasAlpha);

    if (!ewk_tile_visible_get(tile))
        evas_object_smart_member_add(tile->image, priv->self);

    ewk_tile_show(tile);
}

static void _ewk_tiled_backing_store_tile_dissociate(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tiled_Backing_Store_Item* item, double lastUsed)
{
    Ewk_Tile_Unused_Cache* tiledUnusedCache;
    ewk_tile_hide(item->tile);
    if (!ewk_tile_visible_get(item->tile))
        evas_object_smart_member_del(item->tile->image);
    ewk_tile_matrix_tile_put(priv->model.matrix, item->tile, lastUsed);
    tiledUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_auto_flush(tiledUnusedCache);

    item->tile = 0;
}

static void _ewk_tiled_backing_store_tile_dissociate_all(Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist* list;
    Ewk_Tiled_Backing_Store_Item* item;
    int i;
    double last_used = ecore_loop_time_get();

    for (i = 0; i < priv->view.rows; i++) {
        list = priv->view.items[i];
        EINA_INLIST_FOREACH(list, item)
            if (item->tile)
                _ewk_tiled_backing_store_tile_dissociate(priv, item, last_used);
    }
}

static inline Eina_Bool _ewk_tiled_backing_store_pre_render_request_add(Ewk_Tiled_Backing_Store_Data* priv, unsigned long column, unsigned long row, float zoom)
{
    Ewk_Tiled_Backing_Store_Pre_Render_Request* request;

    request = static_cast<Ewk_Tiled_Backing_Store_Pre_Render_Request*>(malloc(sizeof(*request)));
    if (!request)
        return false;

    priv->render.preRenderRequests = eina_inlist_append
                                           (priv->render.preRenderRequests, EINA_INLIST_GET(request));

    request->column = column;
    request->row = row;
    request->zoom = zoom;

    return true;
}

static inline void _ewk_tiled_backing_store_pre_render_request_del(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tiled_Backing_Store_Pre_Render_Request* request)
{
    priv->render.preRenderRequests = eina_inlist_remove
                                           (priv->render.preRenderRequests, EINA_INLIST_GET(request));
    free(request);
}

static inline Ewk_Tiled_Backing_Store_Pre_Render_Request* _ewk_tiled_backing_store_pre_render_request_first(const Ewk_Tiled_Backing_Store_Data* priv)
{
    return EINA_INLIST_CONTAINER_GET(
               priv->render.preRenderRequests,
               Ewk_Tiled_Backing_Store_Pre_Render_Request);
}

static void _ewk_tiled_backing_store_pre_render_request_flush(Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist** preRenderList = &priv->render.preRenderRequests;
    while (*preRenderList) {
        Ewk_Tiled_Backing_Store_Pre_Render_Request* request;
        request = _ewk_tiled_backing_store_pre_render_request_first(priv);
        *preRenderList = eina_inlist_remove(*preRenderList, *preRenderList);
        free(request);
    }
}

static void _ewk_tiled_backing_store_pre_render_request_clear(Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist** preRenderList = &priv->render.preRenderRequests;
    Eina_Inlist* iter = *preRenderList, *tmp;
    while (iter) {
        Ewk_Tiled_Backing_Store_Pre_Render_Request* request =
            EINA_INLIST_CONTAINER_GET(
                iter, Ewk_Tiled_Backing_Store_Pre_Render_Request);
        tmp = iter->next;
        *preRenderList = eina_inlist_remove(*preRenderList, iter);
        iter = tmp;
        free(request);
    }
}

/* assumes priv->process.pre_cb was called if required! */
static void _ewk_tiled_backing_store_pre_render_request_process_single(Ewk_Tiled_Backing_Store_Data* priv)
{
    Ewk_Tiled_Backing_Store_Pre_Render_Request* request;
    Eina_Rectangle area;
    Ewk_Tile_Matrix* tileMatrix = priv->model.matrix;
    Ewk_Tile* tile;
    Ewk_Tile_Unused_Cache* tiledUnusedCache;
    unsigned long column, row;
    float zoom;
    double last_used = ecore_loop_time_get();

    request = _ewk_tiled_backing_store_pre_render_request_first(priv);
    if (!request)
        return;

    column = request->column;
    row = request->row;
    zoom = request->zoom;

    if (ewk_tile_matrix_tile_exact_exists(tileMatrix, column, row, zoom)) {
        DBG("no pre-render required for tile %lu,%lu @ %f.", column, row, zoom);
        goto end;
    }

    tile = _ewk_tiled_backing_store_tile_new(priv, column, row, zoom);
    if (!tile)
        goto end;

    area.x = 0;
    area.y = 0;
    area.w = priv->view.tile.width;
    area.h = priv->view.tile.height;

    priv->render.callback(priv->render.data, tile, &area);
    evas_object_image_data_update_add(
        tile->image,
        area.x, area.y, area.w, area.h);
    ewk_tile_matrix_tile_updates_clear(tileMatrix, tile);

    ewk_tile_matrix_tile_put(tileMatrix, tile, last_used);

end:
    _ewk_tiled_backing_store_pre_render_request_del(priv, request);
    tiledUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_auto_flush(tiledUnusedCache);
}

static Eina_Bool _ewk_tiled_backing_store_item_process_idler_cb(void* data)
{
    Ewk_Tiled_Backing_Store_Data* priv = static_cast<Ewk_Tiled_Backing_Store_Data*>(data);

    if (priv->process.preCallback)
        data = priv->process.preCallback(priv->process.preData, priv->self);

    _ewk_tiled_backing_store_pre_render_request_process_single(priv);

    if (priv->process.postCallback)
        priv->process.postCallback(priv->process.postData, data, priv->self);

    if (!priv->render.preRenderRequests) {
        priv->render.idler = 0;
        return false;
    }

    return true;
}

static inline void _ewk_tiled_backing_store_item_process_idler_stop(Ewk_Tiled_Backing_Store_Data* priv)
{
    if (!priv->render.idler)
        return;

    ecore_idler_del(priv->render.idler);
    priv->render.idler = 0;
}

static inline void _ewk_tiled_backing_store_item_process_idler_start(Ewk_Tiled_Backing_Store_Data* priv)
{
    if (priv->render.idler || !evas_object_visible_get(priv->self))
        return;
    priv->render.idler = ecore_idler_add(
        _ewk_tiled_backing_store_item_process_idler_cb, priv);
}

static Eina_Bool _ewk_tiled_backing_store_disable_render(Ewk_Tiled_Backing_Store_Data* priv)
{
    if (priv->render.suspend)
        return true;

    priv->render.suspend = true;
    _ewk_tiled_backing_store_item_process_idler_stop(priv);
    return true;
}

static Eina_Bool _ewk_tiled_backing_store_enable_render(Ewk_Tiled_Backing_Store_Data* priv)
{
    if (!priv->render.suspend)
        return true;

    priv->render.suspend = false;

    _ewk_tiled_backing_store_fill_renderers(priv);
    _ewk_tiled_backing_store_item_process_idler_start(priv);

    return true;
}

static inline Eina_Bool _ewk_tiled_backing_store_item_fill(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tiled_Backing_Store_Item* item, unsigned long column, unsigned long row)
{
    if (!evas_object_visible_get(priv->self))
        return false;

    unsigned long currentColumn = priv->model.base.column + column;
    unsigned long currentRow = priv->model.base.row + row;
    double lastUsed = ecore_loop_time_get();

    if (currentColumn >= priv->model.current.columns || currentRow >= priv->model.current.rows) {
        if (item->tile)
            _ewk_tiled_backing_store_tile_dissociate(priv, item, lastUsed);
    } else {
        Ewk_Tile* tile;
        const float zoom = priv->view.tile.zoom;

        if (item->tile) {
            Ewk_Tile* old = item->tile;
            if (old->row != currentRow || old->col != currentColumn || old->zoom != zoom)
                _ewk_tiled_backing_store_tile_dissociate(priv, item, lastUsed);
            else if (old->row == currentRow && old->col == currentColumn && old->zoom == zoom)
                goto end;
        }

        tile = ewk_tile_matrix_tile_exact_get(priv->model.matrix, currentColumn, currentRow, zoom);

        if (!tile) {
            /* NOTE: it never returns 0 if it->tile was set! */
            if (item->tile) {
                CRITICAL("it->tile=%p, but it should be 0!", item->tile);
                _ewk_tiled_backing_store_tile_dissociate(priv, item,
                                                         lastUsed);
            }

            /* Do not add new requests to the render queue */
            if (!priv->render.suspend) {
                tile = _ewk_tiled_backing_store_tile_new(priv, currentColumn, currentRow, zoom);
                if (!tile)
                    return false;
                _ewk_tiled_backing_store_tile_associate(priv, tile, item);
            }
        } else if (tile != item->tile) {
            if (item->tile)
                _ewk_tiled_backing_store_tile_dissociate(priv,
                                                         item, lastUsed);
            _ewk_tiled_backing_store_tile_associate(priv, tile, item);
        }

end:

        return true;
    }

    return true;
}

static Ewk_Tiled_Backing_Store_Item* _ewk_tiled_backing_store_item_add(Ewk_Tiled_Backing_Store_Data* priv, unsigned long column, unsigned long row)
{
    Ewk_Tiled_Backing_Store_Item* item;
    Evas_Coord x, y, tileWidth, tileHeight;

    DBG("ewkBackingStore%p", priv->self);

    item = static_cast<Ewk_Tiled_Backing_Store_Item*>(malloc(sizeof(*item)));
    if (!item)
        return 0;

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;
    x = priv->view.offset.base.x + priv->view.x + tileWidth  *column;
    y = priv->view.offset.base.y + priv->view.y + tileHeight  *row;

    item->tile = 0;

    item->smooth_scale = priv->view.tile.zoomWeakSmoothScale;
    _ewk_tiled_backing_store_item_move(item, x, y);
    _ewk_tiled_backing_store_item_resize(item, tileWidth, tileHeight);
    if (!_ewk_tiled_backing_store_item_fill(priv, item, column, row)) {
        free(item);
        return 0;
    }

    return item;
}

static void _ewk_tiled_backing_store_item_del(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tiled_Backing_Store_Item* item)
{
    if (item->tile) {
        double last_used = ecore_loop_time_get();
        _ewk_tiled_backing_store_tile_dissociate(priv, item, last_used);
    }

    free(item);
}

static void _ewk_tiled_backing_store_item_smooth_scale_set(Ewk_Tiled_Backing_Store_Item* item, Eina_Bool smoothScale)
{
    if (item->smooth_scale == smoothScale)
        return;

    if (item->tile)
        evas_object_image_smooth_scale_set(item->tile->image, smoothScale);
}

static inline void _ewk_tiled_backing_store_changed(Ewk_Tiled_Backing_Store_Data* priv)
{
    if (priv->changed.any)
        return;
    evas_object_smart_changed(priv->self);
    priv->changed.any = true;
}

static void _ewk_tiled_backing_store_view_cols_end_del(Ewk_Tiled_Backing_Store_Data* priv, Eina_Inlist** rowList, unsigned int count)
{
    Eina_Inlist* nextItem;
    unsigned int i;

    if (!count)
        return;

    nextItem = (*rowList)->last;

    for (i = 0; i < count; i++) {
        Ewk_Tiled_Backing_Store_Item* item;
        item = EINA_INLIST_CONTAINER_GET(nextItem, Ewk_Tiled_Backing_Store_Item);
        nextItem = nextItem->prev;
        *rowList = eina_inlist_remove(*rowList, EINA_INLIST_GET(item));
        _ewk_tiled_backing_store_item_del(priv, item);
    }
}

static Eina_Bool _ewk_tiled_backing_store_view_cols_end_add(Ewk_Tiled_Backing_Store_Data* priv, Eina_Inlist** rowList, unsigned int baseColumn, unsigned int count)
{
    unsigned int i, row = rowList - priv->view.items;

    for (i = 0; i < count; i++, baseColumn++) {
        Ewk_Tiled_Backing_Store_Item* it;

        it = _ewk_tiled_backing_store_item_add(priv, baseColumn, row);
        if (!it) {
            CRITICAL("failed to add column %u of %u in row %u.", i, count, row);
            _ewk_tiled_backing_store_view_cols_end_del(priv, rowList, i);
            return false;
        }

        *rowList = eina_inlist_append(*rowList, EINA_INLIST_GET(it));
    }
    return true;
}

static void _ewk_tiled_backing_store_view_row_del(Ewk_Tiled_Backing_Store_Data* priv, Eina_Inlist* row)
{
    while (row) {
        Ewk_Tiled_Backing_Store_Item* item;
        item = EINA_INLIST_CONTAINER_GET(row, Ewk_Tiled_Backing_Store_Item);
        row = row->next;
        _ewk_tiled_backing_store_item_del(priv, item);
    }
}

static void _ewk_tiled_backing_store_view_rows_range_del(Ewk_Tiled_Backing_Store_Data* priv, Eina_Inlist** start, Eina_Inlist** end)
{
    for (; start < end; start++) {
        _ewk_tiled_backing_store_view_row_del(priv, *start);
        *start = 0;
    }
}

static void _ewk_tiled_backing_store_view_rows_all_del(Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist** start;
    Eina_Inlist** end;

    start = priv->view.items;
    end = priv->view.items + priv->view.rows;
    _ewk_tiled_backing_store_view_rows_range_del(priv, start, end);

    free(priv->view.items);
    priv->view.items = 0;
    priv->view.cols = 0;
    priv->view.rows = 0;
}

static void _ewk_tiled_backing_store_render(void* data, Ewk_Tile* tile, const Eina_Rectangle* area)
{
    Ewk_Tiled_Backing_Store_Data* priv = static_cast<Ewk_Tiled_Backing_Store_Data*>(data);

    INF("TODO %p (visible? %d) [%lu,%lu] %d,%d + %dx%d",
        tile, tile->visible, tile->col, tile->row, area->x, area->y, area->w, area->h);

    if (!tile->visible)
        return;

    if (priv->view.tile.width != tile->width || priv->view.tile.height != tile->height)
        return; // todo: remove me later, don't even flag as dirty!

    EINA_SAFETY_ON_NULL_RETURN(priv->render.callback);
    if (!priv->render.callback(priv->render.data, tile, area))
        return;

    evas_object_image_data_update_add(tile->image, area->x, area->y, area->w, area->h);
}

static inline void _ewk_tiled_backing_store_model_matrix_create(Ewk_Tiled_Backing_Store_Data* priv, Ewk_Tile_Unused_Cache* tileUnusedCache)
{
    if (priv->model.matrix) {
        _ewk_tiled_backing_store_view_rows_all_del(priv);

        priv->changed.offset = false;
        priv->changed.size = true;

        ewk_tile_matrix_free(priv->model.matrix);
    }

    priv->model.matrix = ewk_tile_matrix_new(tileUnusedCache, priv->model.current.columns, priv->model.current.rows, priv->view.tile.zoom, priv->colorSpace, _ewk_tiled_backing_store_render, priv);
}

static void _ewk_tiled_backing_store_smart_member_del(Evas_Object* ewkBackingStore, Evas_Object* member)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    if (!priv->contentsClipper)
        return;
    evas_object_clip_unset(member);
    if (!evas_object_clipees_get(priv->contentsClipper))
        evas_object_hide(priv->contentsClipper);
}

static void _ewk_tiled_backing_store_smart_member_add(Evas_Object* ewkBackingStore, Evas_Object* member)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    if (!priv->contentsClipper)
        return;
    evas_object_clip_set(member, priv->contentsClipper);
    if (evas_object_visible_get(ewkBackingStore))
        evas_object_show(priv->contentsClipper);
}

#ifdef DEBUG_MEM_LEAKS
static void _ewk_tiled_backing_store_mem_dbg(Ewk_Tiled_Backing_Store_Data* priv)
{
    static int run = 0;

    run++;

    printf("\n--- BEGIN DEBUG TILED BACKING STORE MEMORY [%d] --\n"
           "t=%0.2f, obj=%p, priv=%p, view.items=%p, matrix=%p\n",
           run, ecore_loop_time_get(),
           priv->self, priv, priv->view.items, priv->model.matrix);

    ewk_tile_matrix_dbg(priv->model.matrix);
    ewk_tile_accounting_dbg();

    printf("--- END DEBUG TILED BACKING STORE MEMORY [%d] --\n\n", run);
}

static Eina_Bool _ewk_tiled_backing_store_sig_usr(void* data, int type, void* event)
{
    Ecore_Event_Signal_User* sig = (Ecore_Event_Signal_User*)event;
    Ewk_Tiled_Backing_Store_Data* priv = (Ewk_Tiled_Backing_Store_Data*)data;

    if (sig->number == 2) {
        Ewk_Tile_Unused_Cache* tilieUnusedCache;
        tilieUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
        ewk_tile_unused_cache_auto_flush(tilieUnusedCache);
    }

    _ewk_tiled_backing_store_view_dbg(priv);
    _ewk_tiled_backing_store_mem_dbg(priv);
    return true;
}
#endif

static void _ewk_tiled_backing_store_smart_add(Evas_Object* ewkBackingStore)
{
    Ewk_Tiled_Backing_Store_Data* priv;

    DBG("ewkBackingStore=%p", ewkBackingStore);

    priv = static_cast<Ewk_Tiled_Backing_Store_Data*>(calloc(1, sizeof(*priv)));
    if (!priv)
        return;

    priv->self = ewkBackingStore;
    priv->view.tile.zoom = 1.0;
    priv->view.tile.width = DEFAULT_TILE_W;
    priv->view.tile.height = DEFAULT_TILE_H;
    priv->view.offset.cur.x = 0;
    priv->view.offset.cur.y = 0;
    priv->view.offset.old.x = 0;
    priv->view.offset.old.y = 0;
    priv->view.offset.base.x = 0;
    priv->view.offset.base.y = 0;

    priv->model.base.column = 0;
    priv->model.base.row = 0;
    priv->model.current.columns = 1;
    priv->model.current.rows = 1;
    priv->model.old.columns = 0;
    priv->model.old.rows = 0;
    priv->model.width = 0;
    priv->model.height = 0;
    priv->render.suspend = false;
    priv->colorSpace = EVAS_COLORSPACE_ARGB8888; // TODO: detect it.

    evas_object_smart_data_set(ewkBackingStore, priv);
    _parent_sc.add(ewkBackingStore);

    priv->contentsClipper = evas_object_rectangle_add(
        evas_object_evas_get(ewkBackingStore));
    evas_object_move(priv->contentsClipper, 0, 0);
    evas_object_resize(priv->contentsClipper,
                       priv->model.width, priv->model.height);
    evas_object_color_set(priv->contentsClipper, 255, 255, 255, 255);
    evas_object_show(priv->contentsClipper);
    evas_object_smart_member_add(priv->contentsClipper, ewkBackingStore);

    _ewk_tiled_backing_store_model_matrix_create(priv, 0);
    evas_object_move(priv->base.clipper, 0, 0);
    evas_object_resize(priv->base.clipper, 0, 0);
    evas_object_clip_set(priv->contentsClipper, priv->base.clipper);

#ifdef DEBUG_MEM_LEAKS
    priv->sig_usr = ecore_event_handler_add
                        (ECORE_EVENT_SIGNAL_USER, _ewk_tiled_backing_store_sig_usr, priv);
#endif
}

static void _ewk_tiled_backing_store_smart_del(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    DBG("ewkBackingStore%p", ewkBackingStore);
    Ewk_Tile_Unused_Cache* tileUnusedCache;

    tileUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_unlock_area(tileUnusedCache);

    _ewk_tiled_backing_store_flush(priv);

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    _ewk_tiled_backing_store_item_process_idler_stop(priv);
    _ewk_tiled_backing_store_view_rows_all_del(priv);

#ifdef DEBUG_MEM_LEAKS
    _ewk_tiled_backing_store_mem_dbg(priv);
    if (priv->sig_usr)
        ecore_event_handler_del(priv->sig_usr);
#endif

    ewk_tile_matrix_free(priv->model.matrix);
    evas_object_smart_member_del(priv->contentsClipper);
    evas_object_del(priv->contentsClipper);

    _parent_sc.del(ewkBackingStore);

#ifdef DEBUG_MEM_LEAKS
    printf("\nIMPORTANT: TILED BACKING STORE DELETED (may be real leaks)\n");
    ewk_tile_accounting_dbg();
#endif
}

static void _ewk_tiled_backing_store_smart_move(Evas_Object* ewkBackingStore, Evas_Coord x, Evas_Coord y)
{
    DBG("ewkBackingStore%p, new pos: %dx%d", ewkBackingStore, x, y);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    if (priv->changed.position)
        return;

    if (priv->view.x == x && priv->view.y == y)
        return;

    priv->changed.position = true;
    _ewk_tiled_backing_store_changed(priv);
}

static void _ewk_tiled_backing_store_smart_resize(Evas_Object* ewkBackingStore, Evas_Coord width, Evas_Coord height)
{
    DBG("ewkBackingStore%p, new size: %dx%d", ewkBackingStore, width, height);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    if (priv->changed.size)
        return;

    if (priv->view.width == width && priv->view.height == height)
        return;

    priv->changed.size = true;
    _ewk_tiled_backing_store_changed(priv);
}

static void _ewk_tiled_backing_store_smart_show(Evas_Object* ewkBackingStore)
{
    ewk_tiled_backing_store_enable_render(ewkBackingStore);
    _parent_sc.show(ewkBackingStore);
}

static void _ewk_tiled_backing_store_smart_hide(Evas_Object* ewkBackingStore)
{
    ewk_tiled_backing_store_disable_render(ewkBackingStore);
    _parent_sc.hide(ewkBackingStore);
}

static void _ewk_tiled_backing_store_recalc_renderers(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord width, Evas_Coord height, Evas_Coord tileWidth, Evas_Coord tileHeight)
{
    long columns, rows, oldRows, oldCols;
    INF("ewkBackingStore%p, new size: %dx%d", priv->self, width, height);

    columns = 1 + static_cast<int>(ceil(width / static_cast<float>(tileWidth)));
    rows = 1 + static_cast<int>(ceil(height / static_cast<float>(tileHeight)));

    INF("ewkBackingStore%p new grid size cols: %ld, rows: %ld, was %ld, %ld",
        priv->self, columns, rows, priv->view.cols, priv->view.rows);

    if (priv->view.cols == columns && priv->view.rows == rows)
        return;

    oldCols = priv->view.cols;
    oldRows = priv->view.rows;

    if (rows < oldRows) {
        Eina_Inlist** start, **end;
        start = priv->view.items + rows;
        end = priv->view.items + oldRows;
        _ewk_tiled_backing_store_view_rows_range_del(priv, start, end);
    }

    void* newItems = realloc(priv->view.items, sizeof(Eina_Inlist*) * rows);
    if (!newItems)
        return;

    priv->view.items = static_cast<Eina_Inlist**>(newItems);
    priv->view.rows = rows;
    priv->view.cols = columns;
    if (rows > oldRows) {
        Eina_Inlist** start = priv->view.items + oldRows;
        Eina_Inlist** end = priv->view.items + rows;
        for (; start < end; start++) {
            *start = 0;
            Eina_Bool r = _ewk_tiled_backing_store_view_cols_end_add(priv, start, 0, columns);
            if (!r) {
                CRITICAL("failed to allocate %ld columns", columns);
                _ewk_tiled_backing_store_view_rows_range_del(priv, priv->view.items + oldRows, start);
                priv->view.rows = oldRows;
                return;
            }
        }
    }

    if (columns != oldCols) {
        int todo = columns - oldCols;
        Eina_Inlist** start = priv->view.items;
        Eina_Inlist** end = start + std::min(oldRows, rows);
        if (todo > 0) {
            for (; start < end; start++) {
                Eina_Bool result = _ewk_tiled_backing_store_view_cols_end_add(priv, start, oldCols, todo);
                if (!result) {
                    CRITICAL("failed to allocate %d columns!", todo);

                    for (start--; start >= priv->view.items; start--)
                        _ewk_tiled_backing_store_view_cols_end_del(priv, start, todo);
                    if (rows > oldRows) {
                        start = priv->view.items + oldRows;
                        end = priv->view.items + rows;
                        for (; start < end; start++)
                            _ewk_tiled_backing_store_view_cols_end_del(priv, start, todo);
                    }
                    return;
                }
            }
        } else if (todo < 0) {
            todo = -todo;
            for (; start < end; start++)
                _ewk_tiled_backing_store_view_cols_end_del(priv, start, todo);
        }
    }

    _ewk_tiled_backing_store_fill_renderers(priv);
}

static void _ewk_tiled_backing_store_smart_calculate_size(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord width, Evas_Coord height)
{
    evas_object_resize(priv->base.clipper, width, height);

    priv->view.width = width;
    priv->view.height = height;

    _ewk_tiled_backing_store_recalc_renderers(
        priv, width, height, priv->view.tile.width, priv->view.tile.height);
}

// TODO: remove me later.
static inline void _ewk_tiled_backing_store_view_dbg(const Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist** start, **end;
    printf("tiles=%2ld,%2ld  model=%2ld,%2ld [%dx%d] base=%+3ld,%+4ld offset=%+4d,%+4d old=%+4d,%+4d base=%+3d,%+3d\n",
           priv->view.cols, priv->view.rows,
           priv->model.current.columns, priv->model.current.rows,
           priv->model.width, priv->model.height,
           priv->model.base.column, priv->model.base.row,
           priv->view.offset.cur.x, priv->view.offset.cur.y,
           priv->view.offset.old.x, priv->view.offset.old.y,
           priv->view.offset.base.x, priv->view.offset.base.y);

    start = priv->view.items;
    end = priv->view.items + priv->view.rows;
    for (; start < end; start++) {
        const Ewk_Tiled_Backing_Store_Item* item;

        EINA_INLIST_FOREACH(*start, item) {
            printf(" %+4d,%+4d ", item->geometry.x, item->geometry.y);

            if (!item->tile)
                printf("            ;");
            else
                printf("%8p %lu,%lu;", item->tile, item->tile->col, item->tile->row);
        }
        printf("\n");
    }
    printf("---\n");
}

/**
 * @internal
 * Move top row down as last.
 *
 * The final result is visually the same, but logically the top that
 * went out of screen is now at bottom and filled with new model items.
 *
 * This is worth just when @a count is smaller than @c
 * priv->view.rows, after that one is refilling the whole matrix so it
 * is better to trigger full refill.
 *
 * @param count the number of times to repeat the process.
 */
static void _ewk_tiled_backing_store_view_wrap_up(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int lastRow = priv->view.rows - 1;
    Evas_Coord tileWidth = priv->view.tile.width;
    Evas_Coord tileHeight = priv->view.tile.height;
    Evas_Coord offsetY = priv->view.offset.base.y + count * tileHeight;
    Evas_Coord tilePositionY = y + (lastRow - count + 1) * tileHeight + offsetY;
    Eina_Inlist** iteratorStart, **iteratorEnd;
    Evas_Coord originX = x + priv->view.offset.base.x;

    iteratorStart = priv->view.items;
    iteratorEnd = iteratorStart + lastRow;

    for (; count > 0; count--) {
        Eina_Inlist** iteratorList;
        Eina_Inlist* temp = *iteratorStart;
        Ewk_Tiled_Backing_Store_Item* item;
        Evas_Coord tilePositionX = originX;
        int count1 = 0;

        for (iteratorList = iteratorStart; iteratorList < iteratorEnd; iteratorList++)
            *iteratorList = *(iteratorList + 1);
        *iteratorList = temp;

        priv->model.base.row++;
        EINA_INLIST_FOREACH(temp, item) {
            _ewk_tiled_backing_store_item_move(item, tilePositionX, tilePositionY);
            tilePositionX += tileWidth;
            _ewk_tiled_backing_store_item_fill(priv, item, count1, lastRow);
            count1++;
        }
        tilePositionY += tileHeight;
    }
    priv->view.offset.base.y = offsetY;
}

/**
 * @internal
 * Move bottom row up as first.
 *
 * The final result is visually the same, but logically the bottom that
 * went out of screen is now at top and filled with new model items.
 *
 * This is worth just when @a count is smaller than @c
 * priv->view.rows, after that one is refilling the whole matrix so it
 * is better to trigger full refill.
 *
 * @param count the number of times to repeat the process.
 */
static void _ewk_tiled_backing_store_view_wrap_down(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    Evas_Coord tileWidth = priv->view.tile.width;
    Evas_Coord tileHeight = priv->view.tile.height;
    Evas_Coord offsetY = priv->view.offset.base.y - count * tileHeight;
    Evas_Coord tilePositionY = y + offsetY + (count - 1) * tileHeight;
    Eina_Inlist** iteratorStart, **iteratorEnd;
    Evas_Coord originX = x + priv->view.offset.base.x;

    iteratorStart = priv->view.items + priv->view.rows - 1;
    iteratorEnd = priv->view.items;

    for (; count > 0; count--) {
        Eina_Inlist** iteratorList;
        Eina_Inlist* temp = *iteratorStart;
        Ewk_Tiled_Backing_Store_Item* item;
        Evas_Coord tilePositionX = originX;
        int count1 = 0;

        for (iteratorList = iteratorStart; iteratorList > iteratorEnd; iteratorList--)
            *iteratorList = *(iteratorList - 1);
        *iteratorList = temp;

        priv->model.base.row--;
        EINA_INLIST_FOREACH(temp, item) {
            _ewk_tiled_backing_store_item_move(item, tilePositionX, tilePositionY);
            tilePositionX += tileWidth;
            _ewk_tiled_backing_store_item_fill(priv, item, count1, 0);
            count1++;
        }
        tilePositionY -= tileHeight;
    }
    priv->view.offset.base.y = offsetY;
}

/**
 * @internal
 * Move left-most (first) column right as last (right-most).
 *
 * The final result is visually the same, but logically the first col that
 * went out of screen is now at last and filled with new model items.
 *
 * This is worth just when @a count is smaller than @c
 * priv->view.cols, after that one is refilling the whole matrix so it
 * is better to trigger full refill.
 *
 * @param count the number of times to repeat the process.
 */
static void _ewk_tiled_backing_store_view_wrap_left(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int row, lastColumn = priv->view.cols - 1;
    Evas_Coord tileWidth = priv->view.tile.width;
    Evas_Coord tileHeight = priv->view.tile.height;
    Evas_Coord offsetX = priv->view.offset.base.x + count * tileWidth;
    Evas_Coord oy = y + priv->view.offset.base.y;
    Eina_Inlist** iterator;
    Eina_Inlist** iteratorEnd;
    unsigned int baseColumn = lastColumn - count + 1;
    Evas_Coord originX = x + baseColumn * tileWidth + offsetX;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;
    row = 0;

    priv->model.base.column += count;

    for (; iterator < iteratorEnd; iterator++, row++) {
        Evas_Coord tilePositionX = originX;
        unsigned int column = baseColumn;

        for (unsigned int i = 0; i < count; i++, column++, tilePositionX += tileWidth) {
            Ewk_Tiled_Backing_Store_Item* it;
            it = EINA_INLIST_CONTAINER_GET(*iterator, Ewk_Tiled_Backing_Store_Item);
            *iterator = eina_inlist_demote(*iterator, *iterator);

            _ewk_tiled_backing_store_item_move(it, tilePositionX, oy);
            _ewk_tiled_backing_store_item_fill(priv, it, column, row);
        }
        oy += tileHeight;
    }

    priv->view.offset.base.x = offsetX;
}

/**
 * @internal
 * Move right-most (last) column left as first (left-most).
 *
 * The final result is visually the same, but logically the last col that
 * went out of screen is now at first and filled with new model items.
 *
 * This is worth just when @a count is smaller than @c
 * priv->view.cols, after that one is refilling the whole matrix so it
 * is better to trigger full refill.
 *
 * @param count the number of times to repeat the process.
 */
static void _ewk_tiled_backing_store_view_wrap_right(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int row;
    Evas_Coord tileWidth = priv->view.tile.width;
    Evas_Coord tileHeight = priv->view.tile.height;
    Evas_Coord offsetX = priv->view.offset.base.x - count * tileWidth;
    Evas_Coord tilePositionY = y + priv->view.offset.base.y;
    Eina_Inlist** iterator, ** iteratorEnd;
    unsigned int baseColumn = count - 1;
    Evas_Coord originX = x + baseColumn * tileWidth + offsetX;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;
    row = 0;

    priv->model.base.column -= count;

    for (; iterator < iteratorEnd; iterator++, row++) {
        Evas_Coord tilePositionX = originX;
        unsigned int column = baseColumn;

        for (unsigned int i = 0; i < count; i++, column--, tilePositionX -= tileWidth) {
            Ewk_Tiled_Backing_Store_Item* item;
            item = EINA_INLIST_CONTAINER_GET((*iterator)->last, Ewk_Tiled_Backing_Store_Item);
            *iterator = eina_inlist_promote(*iterator, (*iterator)->last);

            _ewk_tiled_backing_store_item_move(item, tilePositionX, tilePositionY);
            _ewk_tiled_backing_store_item_fill(priv, item, column, row);
        }
        tilePositionY += tileHeight;
    }

    priv->view.offset.base.x = offsetX;
}

static void _ewk_tiled_backing_store_view_refill(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y, int stepX, int stepY)
{
    Eina_Inlist** iterator, **iteratorEnd;
    Evas_Coord baseTilePositionX, tilePositionY, tileWidth, tileHeight;
    unsigned int r;

    evas_object_move(priv->base.clipper, x, y);

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;

    baseTilePositionX = x + priv->view.offset.base.x;
    tilePositionY = y + priv->view.offset.base.y;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;
    r = 0;

    priv->model.base.column -= stepX;
    priv->model.base.row -= stepY;

    for (; iterator < iteratorEnd; iterator++, r++) {
        Ewk_Tiled_Backing_Store_Item* item;
        Evas_Coord newTilePositionX = baseTilePositionX;
        unsigned int count = 0;
        EINA_INLIST_FOREACH(*iterator, item) {
            _ewk_tiled_backing_store_item_fill(priv, item, count, r);
            _ewk_tiled_backing_store_item_move(item, newTilePositionX, tilePositionY);
            count++;
            newTilePositionX += tileWidth;
        }
        tilePositionY += tileHeight;
    }
}

static void _ewk_tiled_backing_store_view_pos_apply(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y)
{
    Eina_Inlist** iterator, **iteratorEnd;
    Evas_Coord baseTilePositionX, baseTilePositionY, tileWidth, tileHeight;

    evas_object_move(priv->base.clipper, x, y);

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;

    baseTilePositionX = x + priv->view.offset.base.x;
    baseTilePositionY = y + priv->view.offset.base.y;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;
    for (; iterator < iteratorEnd; iterator++) {
        Ewk_Tiled_Backing_Store_Item* it;
        Evas_Coord ox = baseTilePositionX;
        EINA_INLIST_FOREACH(*iterator, it) {
            _ewk_tiled_backing_store_item_move(it, ox, baseTilePositionY);
            ox += tileWidth;
        }
        baseTilePositionY += tileHeight;
    }
}

static void _ewk_tiled_backing_store_smart_calculate_offset_force(Ewk_Tiled_Backing_Store_Data* priv)
{
    Evas_Coord deltaX = priv->view.offset.cur.x - priv->view.offset.old.x;
    Evas_Coord deltaY = priv->view.offset.cur.y - priv->view.offset.old.y;
    Evas_Coord tileWidth, tileHeight;
    int stepY, stepX;

    INF("ewkBackingStore%p, offset: %+4d, %+4d (%+4d, %+4d)",
        priv->self, deltaX, deltaY, priv->view.offset.cur.x, priv->view.offset.cur.y);

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;

    long newColumn = -priv->view.offset.cur.x / tileWidth;
    stepX = priv->model.base.column - newColumn;
    long newRow = -priv->view.offset.cur.y / tileHeight;
    stepY = priv->model.base.row - newRow;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    evas_object_move(
        priv->contentsClipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);

    priv->view.offset.base.x += deltaX - stepX * tileWidth;
    priv->view.offset.base.y += deltaY - stepY * tileHeight;

    _ewk_tiled_backing_store_view_refill
        (priv, priv->view.x, priv->view.y, stepX, stepY);
}

static void _ewk_tiled_backing_store_smart_calculate_offset(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y)
{
    Evas_Coord deltaX = priv->view.offset.cur.x - priv->view.offset.old.x;
    Evas_Coord deltaY = priv->view.offset.cur.y - priv->view.offset.old.y;
    Evas_Coord tileWidth, tileHeight;
    int stepY, stepX;

    INF("ewkBackingStore%p, offset: %+4d, %+4d (%+4d, %+4d)",
        priv->self, deltaX, deltaY, priv->view.offset.cur.x, priv->view.offset.cur.y);

    if (!deltaX && !deltaY)
        return;

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;

    long newCol = -priv->view.offset.cur.x / tileWidth;
    stepX = priv->model.base.column - newCol;
    long newRow = -priv->view.offset.cur.y / tileHeight;
    stepY = priv->model.base.row - newRow;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    evas_object_move(
        priv->contentsClipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);

    if ((stepX < 0 && stepX <= -priv->view.cols)
        || (stepX > 0 && stepX >= priv->view.cols)
        || (stepY < 0 && stepY <= -priv->view.rows)
        || (stepY > 0 && stepY >= priv->view.rows)) {

        priv->view.offset.base.x += deltaX - stepX * tileWidth;
        priv->view.offset.base.y += deltaY - stepY * tileHeight;

        _ewk_tiled_backing_store_view_refill
            (priv, priv->view.x, priv->view.y, stepX, stepY);
        return;
    }

    priv->view.offset.base.x += deltaX;
    priv->view.offset.base.y += deltaY;

    if (stepY < 0)
        _ewk_tiled_backing_store_view_wrap_up(priv, x, y, -stepY);
    else if (stepY > 0)
        _ewk_tiled_backing_store_view_wrap_down(priv, x, y, stepY);

    if (stepX < 0)
        _ewk_tiled_backing_store_view_wrap_left(priv, x, y, -stepX);
    else if (stepX > 0)
        _ewk_tiled_backing_store_view_wrap_right(priv, x, y, stepX);

    _ewk_tiled_backing_store_view_pos_apply(priv, x, y);
}

static void _ewk_tiled_backing_store_smart_calculate_pos(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y)
{
    _ewk_tiled_backing_store_view_pos_apply(priv, x, y);
    priv->view.x = x;
    priv->view.y = y;
    evas_object_move(
        priv->contentsClipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);
}

static void _ewk_tiled_backing_store_fill_renderers(Ewk_Tiled_Backing_Store_Data* priv)
{
    Eina_Inlist* list;
    Ewk_Tiled_Backing_Store_Item* item;
    int i, j;

    for (i = 0; i < priv->view.rows; i++) {
        list = priv->view.items[i];
        j = 0;
        EINA_INLIST_FOREACH(list, item)
            _ewk_tiled_backing_store_item_fill(priv, item, j++, i);
    }
}

static void _ewk_tiled_backing_store_smart_calculate(Evas_Object* ewkBackingStore)
{
    Evas_Coord x, y, width, height;

    evas_object_geometry_get(ewkBackingStore, &x, &y, &width, &height);
    DBG("ewkBackingStore%p at %d,%d + %dx%d", ewkBackingStore, x, y, width, height);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    priv->changed.any = false;

    ewk_tile_matrix_freeze(priv->model.matrix);

    if (priv->changed.model && !priv->changed.size)
        ewk_tile_matrix_invalidate(priv->model.matrix);

    if (!priv->render.suspend && priv->changed.model) {
        unsigned long columns, rows;

        columns = priv->model.width / priv->view.tile.width + 1;
        rows = priv->model.height / priv->view.tile.height + 1;

        priv->model.old.columns = priv->model.current.columns;
        priv->model.old.rows = priv->model.current.rows;
        priv->model.current.columns = columns;
        priv->model.current.rows = rows;
        if (priv->model.old.columns > columns)
            columns = priv->model.old.columns;
        if (priv->model.old.rows > rows)
            rows = priv->model.old.rows;
        ewk_tile_matrix_resize(priv->model.matrix, columns, rows);
    }

    if (priv->changed.position && (priv->view.x != x || priv->view.y != y)) {
        _ewk_tiled_backing_store_smart_calculate_pos(priv, x, y);
        priv->changed.position = false;
    }
    if (priv->changed.offset) {
        _ewk_tiled_backing_store_smart_calculate_offset(priv, x, y);
        priv->changed.offset = false;
    }

    if (priv->changed.size) {
        _ewk_tiled_backing_store_smart_calculate_size(priv, width, height);
        priv->changed.size = false;
    }

    if (!priv->render.suspend && priv->changed.model) {
        _ewk_tiled_backing_store_fill_renderers(priv);
        ewk_tile_matrix_resize(priv->model.matrix,
                               priv->model.current.columns,
                               priv->model.current.rows);
        priv->changed.model = false;
        evas_object_resize(priv->contentsClipper, priv->model.width, priv->model.height);
        _ewk_tiled_backing_store_smart_calculate_offset_force(priv);

        /* Make sure we do not miss any important repaint by
         * repainting the whole viewport */
        const Eina_Rectangle rect =
        { 0, 0, priv->model.width, priv->model.height };
        ewk_tile_matrix_update(priv->model.matrix, &rect,
                               priv->view.tile.zoom);
    }

    ewk_tile_matrix_thaw(priv->model.matrix);

    _ewk_tiled_backing_store_updates_process(priv);

    if (priv->view.offset.base.x > 0
        || priv->view.offset.base.x <= - priv->view.tile.width
        || priv->view.offset.base.y > 0
        || priv->view.offset.base.y <= - priv->view.tile.height)
        ERR("incorrect base offset %+4d,%+4d, tile=%dx%d, cur=%+4d,%+4d\n",
            priv->view.offset.base.x, priv->view.offset.base.y,
            priv->view.tile.width, priv->view.tile.height,
            priv->view.offset.cur.x, priv->view.offset.cur.y);

}

Evas_Object* ewk_tiled_backing_store_add(Evas* canvas)
{
    static Evas_Smart* smart = 0;

    if (_ewk_tiled_log_dom < 0)
        _ewk_tiled_log_dom = eina_log_domain_register("Ewk_Tiled_Backing_Store", 0);

    if (!smart) {
        static Evas_Smart_Class sc =
            EVAS_SMART_CLASS_INIT_NAME_VERSION("Ewk_Tiled_Backing_Store");

        evas_object_smart_clipped_smart_set(&sc);
        _parent_sc = sc;

        sc.add = _ewk_tiled_backing_store_smart_add;
        sc.del = _ewk_tiled_backing_store_smart_del;
        sc.resize = _ewk_tiled_backing_store_smart_resize;
        sc.move = _ewk_tiled_backing_store_smart_move;
        sc.show = _ewk_tiled_backing_store_smart_show;
        sc.hide = _ewk_tiled_backing_store_smart_hide;
        sc.calculate = _ewk_tiled_backing_store_smart_calculate;
        sc.member_add = _ewk_tiled_backing_store_smart_member_add;
        sc.member_del = _ewk_tiled_backing_store_smart_member_del;

        smart = evas_smart_class_new(&sc);
    }

    return evas_object_smart_add(canvas, smart);
}

void ewk_tiled_backing_store_render_cb_set(Evas_Object* ewkBackingStore, Eina_Bool (*callback)(void* data, Ewk_Tile* tile, const Eina_Rectangle* area), const void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(callback);
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    priv->render.callback = callback;
    priv->render.data = (void*)data;
}

Ewk_Tile_Unused_Cache* ewk_tiled_backing_store_tile_unused_cache_get(const Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, 0);
    return ewk_tile_matrix_unused_cache_get(priv->model.matrix);
}

void ewk_tiled_backing_store_tile_unused_cache_set(Evas_Object* ewkBackingStore, Ewk_Tile_Unused_Cache* tileUnusedCache)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    if (ewk_tile_matrix_unused_cache_get(priv->model.matrix) == tileUnusedCache)
        return;

    _ewk_tiled_backing_store_model_matrix_create(priv, tileUnusedCache);
}

static Eina_Bool _ewk_tiled_backing_store_scroll_full_offset_set_internal(Ewk_Tiled_Backing_Store_Data* priv, Evas_Coord x, Evas_Coord y)
{
    /* TODO: check offset go out of bounds, clamp */
    if (priv->render.disabled)
        return false;

    priv->view.offset.cur.x = x;
    priv->view.offset.cur.y = y;

    priv->changed.offset = true;
    _ewk_tiled_backing_store_changed(priv);

    return true;
}

Eina_Bool ewk_tiled_backing_store_scroll_full_offset_set(Evas_Object* ewkBackingStore, Evas_Coord x, Evas_Coord y)
{
    DBG("ewkBackingStore%p, x=%d, y=%d", ewkBackingStore, x, y);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    if (x == priv->view.offset.cur.x && y == priv->view.offset.cur.y)
        return true;

    return _ewk_tiled_backing_store_scroll_full_offset_set_internal(priv, x, y);
}

Eina_Bool ewk_tiled_backing_store_scroll_full_offset_add(Evas_Object* ewkBackingStore, Evas_Coord deltaX, Evas_Coord deltaY)
{
    DBG("ewkBackingStore%p, deltaX=%d, deltaY=%d", ewkBackingStore, deltaX, deltaY);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    if (!deltaX && !deltaY)
        return true;

    return _ewk_tiled_backing_store_scroll_full_offset_set_internal
               (priv, priv->view.offset.cur.x + deltaX, priv->view.offset.cur.y + deltaY);
}

static Eina_Bool _ewk_tiled_backing_store_zoom_set_internal(Ewk_Tiled_Backing_Store_Data* priv, float* zoom, Evas_Coord currentX, Evas_Coord currentY, Evas_Coord* offsetX, Evas_Coord* offsetY)
{
    *offsetX = priv->view.offset.cur.x;
    *offsetY = priv->view.offset.cur.y;

    if (fabsf(priv->view.tile.zoom - *zoom) < ZOOM_STEP_MIN) {
        DBG("ignored as zoom difference is < %f: %f",
            (double)ZOOM_STEP_MIN, fabsf(priv->view.tile.zoom - *zoom));
        return true;
    }

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    Evas_Coord tileWidth, tileHeight;

    *zoom = ROUNDED_ZOOM(priv->view.tile.width, *zoom);

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;

    float scale = *zoom / priv->view.tile.zoom;

    priv->view.tile.zoom = *zoom;
    // todo: check currentX [0, w]...
    priv->view.offset.zoomCenter.x = currentX;
    priv->view.offset.zoomCenter.y = currentY;

    ewk_tile_matrix_zoom_level_set(priv->model.matrix, *zoom);

    if (!priv->view.width || !priv->view.height) {
        priv->view.offset.base.x = 0;
        priv->view.offset.base.y = 0;
        return true;
    }
    Eina_Inlist** iterator, **iteratorEnd;
    Ewk_Tiled_Backing_Store_Item* it;

    Evas_Coord newX = currentX + (priv->view.offset.cur.x - currentX) * scale;
    Evas_Coord newY = currentY + (priv->view.offset.cur.y - currentY) * scale;

    Evas_Coord modelWidth = priv->model.width * scale;
    Evas_Coord modelHeight = priv->model.height * scale;

    if (modelWidth < priv->view.width || newX >= 0)
        newX = 0;
    else if (-newX + priv->view.width >= modelWidth)
        newX = -modelWidth + priv->view.width;

    if (modelHeight < priv->view.height || newY >= 0)
        newY = 0;
    else if (-newY + priv->view.height >= modelHeight)
        newY = -modelHeight + priv->view.height;

    Evas_Coord baseX = newX % tileWidth;
    Evas_Coord baseY = newY % tileHeight;
    priv->model.base.column = -newX / tileWidth;
    priv->model.base.row = -newY / tileHeight;

    priv->changed.size = true;
    priv->changed.model = true;
    _ewk_tiled_backing_store_changed(priv);

    priv->view.offset.cur.x = newX;
    priv->view.offset.cur.y = newY;
    priv->view.offset.base.x = baseX;
    priv->view.offset.base.y = baseY;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    *offsetX = priv->view.offset.cur.x;
    *offsetY = priv->view.offset.cur.y;

    evas_object_move(
        priv->contentsClipper,
        newX + priv->view.x,
        newY + priv->view.y);

    _ewk_tiled_backing_store_fill_renderers(priv);

    Evas_Coord tilePositionY = priv->view.offset.base.y + priv->view.y;
    Evas_Coord baseTilePositionX = priv->view.x + priv->view.offset.base.x;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;

    for (; iterator < iteratorEnd; iterator++) {
        Evas_Coord tilePositionX = baseTilePositionX;
        Eina_Inlist* lst = *iterator;

        EINA_INLIST_FOREACH(lst, it) {
            _ewk_tiled_backing_store_item_move(it, tilePositionX, tilePositionY);
            _ewk_tiled_backing_store_item_resize(it, tileWidth, tileHeight);
            tilePositionX += tileWidth;
        }
        tilePositionY += tileHeight;
    }

    return true;
}

Eina_Bool ewk_tiled_backing_store_zoom_set(Evas_Object* ewkBackingStore, float* zoom, Evas_Coord currentX, Evas_Coord currentY, Evas_Coord* offsetX, Evas_Coord* offsetY)
{
    DBG("ewkBackingStore%p, zoom=%f", ewkBackingStore, *zoom);

    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);

    return _ewk_tiled_backing_store_zoom_set_internal(priv, zoom, currentX, currentY, offsetX, offsetY);
}

Eina_Bool ewk_tiled_backing_store_zoom_weak_set(Evas_Object* ewkBackingStore, float zoom, Evas_Coord currentX, Evas_Coord currentY)
{
    DBG("ewkBackingStore%p, zoom=%f", ewkBackingStore, zoom);
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    if (!priv->view.width || !priv->view.height)
        return false;
    Eina_Inlist** iterator, ** iteratorEnd;
    Ewk_Tiled_Backing_Store_Item* item;
    Evas_Coord tileWidth, tileHeight;
    Eina_Bool reCalculate = false;

    float scale = zoom / priv->view.tile.zoom;

    tileWidth = TILE_SIZE_AT_ZOOM(priv->view.tile.width, scale);
    scale = TILE_ZOOM_AT_SIZE(tileWidth, priv->view.tile.width);
    tileHeight = TILE_SIZE_AT_ZOOM(priv->view.tile.height, scale);
    zoom = scale * priv->view.tile.zoom;

    Evas_Coord modelWidth = priv->model.width * scale;
    Evas_Coord modelHeight = priv->model.height * scale;

    evas_object_resize(priv->contentsClipper, modelWidth, modelHeight);

    int vrows = static_cast<int>(ceil(priv->view.height /static_cast<float>(tileHeight)) + 1);
    int vcols = static_cast<int>(ceil(priv->view.width / static_cast<float>(tileWidth)) + 1);
    Evas_Coord newX = currentX + (priv->view.offset.cur.x - currentX) * scale;
    Evas_Coord newY = currentY + (priv->view.offset.cur.y - currentY) * scale;
    Evas_Coord baseX = newX % tileWidth;
    Evas_Coord baseY = newY % tileHeight;
    unsigned long baseRow = -newY / tileHeight;
    unsigned long baseColumn = -newX / tileWidth;

    if (baseRow != priv->model.base.row || baseColumn != priv->model.base.column) {
        priv->model.base.row = baseRow;
        priv->model.base.column = baseColumn;
        reCalculate = true;
    }

    if (vrows > priv->view.rows || vcols > priv->view.cols)
        reCalculate = true;

    if (reCalculate) {
        Evas_Coord width, height;
        evas_object_geometry_get(ewkBackingStore, 0, 0, &width, &height);
        _ewk_tiled_backing_store_recalc_renderers(priv, width, height, tileWidth, tileHeight);
        _ewk_tiled_backing_store_fill_renderers(priv);
        _ewk_tiled_backing_store_updates_process(priv);
    }

    Evas_Coord baseTilePositionX = baseX + priv->view.x;
    Evas_Coord tilePositionY = baseY + priv->view.y;

    evas_object_move(priv->contentsClipper,
                     newX + priv->view.x,
                     newY + priv->view.y);

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;

    for (; iterator < iteratorEnd; iterator++) {
        Evas_Coord tilePositionX = baseTilePositionX;
        Eina_Inlist* list = *iterator;

        EINA_INLIST_FOREACH(list, item) {
            _ewk_tiled_backing_store_item_move(item, tilePositionX, tilePositionY);
            _ewk_tiled_backing_store_item_resize(item, tileWidth, tileHeight);
            tilePositionX += tileWidth;
        }
        tilePositionY += tileHeight;
    }

    return true;
}

void ewk_tiled_backing_store_fix_offsets(Evas_Object* ewkBackingStore, Evas_Coord width, Evas_Coord height)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    Eina_Inlist** iterator, **iteratorEnd;
    Ewk_Tiled_Backing_Store_Item* item;
    Evas_Coord newX = priv->view.offset.cur.x;
    Evas_Coord newY = priv->view.offset.cur.y;
    Evas_Coord baseX = priv->view.offset.base.x;
    Evas_Coord baseY = priv->view.offset.base.y;
    Evas_Coord tileWidth = priv->view.tile.width;
    Evas_Coord tileHeight = priv->view.tile.height;

    if (-newX > width) {
        newX = -width;
        baseX = newX % tileWidth;
        priv->model.base.column = -newX / tileWidth;
    }

    if (-newY > height) {
        newY = -height;
        baseY = newY % tileHeight;
        priv->model.base.row = -newY / tileHeight;
    }

    if (baseX >= 0 || baseX <= -2 * priv->view.tile.width) {
        baseX = newX % tileWidth;
        priv->model.base.column = -newX / tileWidth;
    }

    if (baseY >= 0 || baseY <= -2 * priv->view.tile.height) {
        baseY = newY % tileHeight;
        priv->model.base.row = -newY / tileHeight;
    }

    priv->view.offset.cur.x = newX;
    priv->view.offset.cur.y = newY;
    priv->view.offset.old.x = newX;
    priv->view.offset.old.y = newY;
    priv->view.offset.base.x = baseX;
    priv->view.offset.base.y = baseY;
    evas_object_move(priv->contentsClipper,
                     newX + priv->view.x,
                     newY + priv->view.y);

    Evas_Coord tilePositionY = priv->view.offset.base.y + priv->view.y;
    Evas_Coord baseTilePositionX = priv->view.x + priv->view.offset.base.x;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;

    for (; iterator < iteratorEnd; iterator++) {
        Evas_Coord tilePositionX = baseTilePositionX;
        Eina_Inlist* lst = *iterator;

        EINA_INLIST_FOREACH(lst, item) {
            _ewk_tiled_backing_store_item_move(item, tilePositionX, tilePositionY);
            _ewk_tiled_backing_store_item_resize(item, tileWidth, tileHeight);
            tilePositionX += tileWidth;
        }
        tilePositionY += tileHeight;
    }
}

void ewk_tiled_backing_store_zoom_weak_smooth_scale_set(Evas_Object* ewkBackingStore, Eina_Bool smoothScale)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    Eina_Inlist** iterator, **iteratorEnd;

    iterator = priv->view.items;
    iteratorEnd = iterator + priv->view.rows;
    priv->view.tile.zoomWeakSmoothScale = smoothScale;

    for (; iterator< iteratorEnd; iterator++) {
        Ewk_Tiled_Backing_Store_Item* item;
        EINA_INLIST_FOREACH(*iterator, item)
            if (item->tile)
                _ewk_tiled_backing_store_item_smooth_scale_set
                    (item, smoothScale);
    }
}

void ewk_tiled_backing_store_alpha_set(Evas_Object* ewkBackingStore, Eina_Bool hasAlpha)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    priv->view.tile.hasAlpha = hasAlpha;
}

Eina_Bool ewk_tiled_backing_store_update(Evas_Object* ewkBackingStore, const Eina_Rectangle* update)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);

    if (priv->render.disabled)
        return false;

    return ewk_tile_matrix_update(priv->model.matrix, update,
                                  priv->view.tile.zoom);
}

void ewk_tiled_backing_store_updates_process_pre_set(Evas_Object* ewkBackingStore, void* (*callback)(void* data, Evas_Object *ewkBackingStore), const void* data)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    priv->process.preCallback = callback;
    priv->process.preData = (void*)data;
}

void ewk_tiled_backing_store_updates_process_post_set(Evas_Object* ewkBackingStore, void* (*callback)(void* data, void* preData, Evas_Object *ewkBackingStore), const void* data)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    priv->process.postCallback = callback;
    priv->process.postData = (void*)data;
}

void ewk_tiled_backing_store_updates_process(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    _ewk_tiled_backing_store_updates_process(priv);
}

void ewk_tiled_backing_store_updates_clear(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    ewk_tile_matrix_updates_clear(priv->model.matrix);
}

void ewk_tiled_backing_store_contents_resize(Evas_Object* ewkBackingStore, Evas_Coord width, Evas_Coord height)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    if (width == priv->model.width && height == priv->model.height)
        return;

    priv->model.width = width;
    priv->model.height = height;
    priv->changed.model = true;

    DBG("w,h=%d, %d", width, height);
    _ewk_tiled_backing_store_changed(priv);
}

void ewk_tiled_backing_store_disabled_update_set(Evas_Object* ewkBackingStore, Eina_Bool value)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);

    if (value != priv->render.disabled)
        priv->render.disabled = value;
}

void ewk_tiled_backing_store_flush(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    Ewk_Tile_Unused_Cache* tileUnusedCache = 0;

    priv->view.offset.cur.x = 0;
    priv->view.offset.cur.y = 0;
    priv->view.offset.old.x = 0;
    priv->view.offset.old.y = 0;
    priv->view.offset.base.x = 0;
    priv->view.offset.base.y = 0;
    priv->model.base.column = 0;
    priv->model.base.row = 0;
    priv->model.current.columns = 1;
    priv->model.current.rows = 1;
    priv->model.old.columns = 0;
    priv->model.old.rows = 0;
    priv->changed.size = true;

#ifdef DEBUG_MEM_LEAKS
    printf("\nFLUSHED BACKING STORE, STATUS BEFORE DELETING TILE MATRIX:\n");
    _ewk_tiled_backing_store_mem_dbg(priv);
#endif

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    _ewk_tiled_backing_store_tile_dissociate_all(priv);
    tileUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_clear(tileUnusedCache);

#ifdef DEBUG_MEM_LEAKS
    printf("\nFLUSHED BACKING STORE, STATUS AFTER RECREATING TILE MATRIX:\n");
    _ewk_tiled_backing_store_mem_dbg(priv);
#endif
}

Eina_Bool ewk_tiled_backing_store_pre_render_tile_add(Evas_Object* ewkBackingStore, int column, int row, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);

    if (ewk_tile_matrix_tile_exact_exists(priv->model.matrix, column, row, zoom))
        return false;

    if (!_ewk_tiled_backing_store_pre_render_request_add(priv, column, row, zoom))
        return false;

    return true;
}

Eina_Bool ewk_tiled_backing_store_pre_render_spiral_queue(Evas_Object* ewkBackingStore, Eina_Rectangle* viewRect, Eina_Rectangle* renderRect, int maxMemory, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(viewRect, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(renderRect, false);

    const int tileWidth = priv->view.tile.width;
    const int tileHeight = priv->view.tile.height;

    Eina_Tile_Grid_Slicer viewSlicer;
    Eina_Tile_Grid_Slicer renderSlicer;

    if (!eina_tile_grid_slicer_setup(&viewSlicer,
        viewRect->x, viewRect->y, viewRect->w, viewRect->h, tileWidth, tileHeight)) {
        ERR("could not setup grid viewSlicer for %d,%d+%dx%d tile=%dx%d", viewRect->x, viewRect->y, viewRect->w, viewRect->h, tileWidth, tileHeight);
        return false;
    }

    if (!eina_tile_grid_slicer_setup(&renderSlicer,
        renderRect->x, renderRect->y, renderRect->w, renderRect->h, tileWidth, tileHeight)) {
        ERR("could not setup grid RenderSlicer for %d,%d+%dx%d tile=%dx%d", renderRect->y, renderRect->y, renderRect->w, renderRect->h, tileWidth, tileHeight);
        return false;
    }

    // set limits of the loop.
    int memoryLimits = maxMemory / (EWK_ARGB_BYTES_SIZE * tileWidth * tileHeight);
    const int maxViewSideLength = std::max(viewSlicer.col2 - viewSlicer.col1, viewSlicer.row2 - viewSlicer.row1);
    const int maxRenderSideLength = std::max(renderSlicer.col2 - renderSlicer.col1, renderSlicer.row2 - renderSlicer.row1);
    const int maxLoopCount = maxViewSideLength + maxRenderSideLength;

    // spire starts from the center of the view area.
    int centerColumn = (viewSlicer.col1 + viewSlicer.col2) / 2;
    int centerRow = (viewSlicer.row1 + viewSlicer.row2) / 2;

    int step = 1;
    const int squareSide = 4;
    for (int loop = 0; loop < maxLoopCount; loop++) {
        for (int i = 1; i < step * squareSide + 1; i++) {
            if (memoryLimits <= 0)
                goto memoryLimitsReached;
            /*
            this code means the loop runs like spiral. (i.g. left->down->right->up)
            when it moves back to original place and then walk 1 tile left and up.
            the loop keeps on doing this until it reaches max memory to draw tiles.
            e.g. )
                         333333
                         322223
                         321123
                         321123
                         322223
                         333333
            */
            if (i > 0 && i <= step)
                centerColumn++; // move left.
            else if (i > step && i <= step * 2)
                centerRow++; // move down.
            else if (i > step * 2 && i <= step * 3)
                centerColumn--; // move right.
            else if (i > step * 3 && i <= step * 4)
                centerRow--; // move up.
            else
                ERR("ERROR : out of bounds\r\n");

            // skip in view port area.
            if (static_cast<int>(viewSlicer.col1) < centerColumn
                && static_cast<int>(viewSlicer.col2) > centerColumn
                && static_cast<int>(viewSlicer.row1) < centerRow
                && static_cast<int>(viewSlicer.row2) > centerRow)
                continue;

            if (static_cast<int>(renderSlicer.col1) <= centerColumn
                && static_cast<int>(renderSlicer.col2) >= centerColumn
                && static_cast<int>(renderSlicer.row1) <= centerRow
                && static_cast<int>(renderSlicer.row2) >= centerRow) {

                if (!ewk_tiled_backing_store_pre_render_tile_add(ewkBackingStore, centerColumn, centerRow, zoom))
                    continue;
                DBG("R>[%d %d] ", centerColumn, centerRow);
                memoryLimits--;
            }
        }
        centerRow--;
        centerColumn--;
        step = step + 2;
    }

memoryLimitsReached:
    _ewk_tiled_backing_store_item_process_idler_start(priv);

    return true;
}

Eina_Bool ewk_tiled_backing_store_pre_render_region(Evas_Object* ewkBackingStore, Evas_Coord x, Evas_Coord y, Evas_Coord width, Evas_Coord height, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    Eina_Tile_Grid_Slicer slicer;
    const Eina_Tile_Grid_Info* info;
    Evas_Coord tileWidth, tileHeight;
    Ewk_Tile_Unused_Cache* tileUnusedCache;

    tileWidth = priv->view.tile.width;
    tileHeight = priv->view.tile.height;
    zoom = ROUNDED_ZOOM(priv->view.tile.width, zoom);

    if (!eina_tile_grid_slicer_setup(&slicer, x, y, width, height, tileWidth, tileHeight)) {
        ERR("could not setup grid slicer for %d,%d+%dx%d tile=%dx%d",
            x, y, width, height, tileWidth, tileHeight);
        return false;
    }

    while (eina_tile_grid_slicer_next(&slicer, &info)) {
        const unsigned long c = info->col;
        const unsigned long r = info->row;
        if (!_ewk_tiled_backing_store_pre_render_request_add(priv, c, r, zoom))
            break;
    }

    _ewk_tiled_backing_store_item_process_idler_start(priv);

    tileUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_lock_area(tileUnusedCache, x, y, width, height, zoom);
    return true;
}

Eina_Bool ewk_tiled_backing_store_pre_render_relative_radius(Evas_Object* ewkBackingStore, unsigned int n, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    unsigned long startRow, endRow, startCol, endCol, i, j, width, height;
    Ewk_Tile_Unused_Cache* tileUnusedCache;

    INF("priv->model.base.row =%ld, n=%u priv->view.rows=%lu",
        priv->model.base.row, n, priv->view.rows);
    startRow = (long)priv->model.base.row - n;
    startCol = (long)priv->model.base.column - n;
    endRow = std::min(priv->model.current.rows - 1,
                       priv->model.base.row + priv->view.rows + n - 1);
    endCol = std::min(priv->model.current.columns - 1,
                       priv->model.base.column + priv->view.cols + n - 1);

    INF("startRow=%lu, endRow=%lu, startCol=%lu, endCol=%lu",
        startRow, endRow, startCol, endCol);

    zoom = ROUNDED_ZOOM(priv->view.tile.width, zoom);

    for (i = startRow; i <= endRow; i++)
        for (j = startCol; j <= endCol; j++)
            if (!_ewk_tiled_backing_store_pre_render_request_add(priv, j, i, zoom))
                goto start_processing;

start_processing:
    _ewk_tiled_backing_store_item_process_idler_start(priv);

    tileUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    height = (endRow - startRow + 1) * TILE_SIZE_AT_ZOOM(priv->view.tile.height, zoom);
    width = (endCol - startCol + 1) * TILE_SIZE_AT_ZOOM(priv->view.tile.width, zoom);
    ewk_tile_unused_cache_lock_area(tileUnusedCache,
                                    startCol * TILE_SIZE_AT_ZOOM(priv->view.tile.width, zoom),
                                    startRow * TILE_SIZE_AT_ZOOM(priv->view.tile.height, zoom), width, height, zoom);

    return true;
}

void ewk_tiled_backing_store_pre_render_cancel(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv);
    Ewk_Tile_Unused_Cache* tileUnusedCache;

    _ewk_tiled_backing_store_pre_render_request_clear(priv);

    tileUnusedCache = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_unlock_area(tileUnusedCache);
}

Eina_Bool ewk_tiled_backing_store_disable_render(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    return _ewk_tiled_backing_store_disable_render(priv);
}

Eina_Bool ewk_tiled_backing_store_enable_render(Evas_Object* ewkBackingStore)
{
    PRIV_DATA_GET_OR_RETURN(ewkBackingStore, priv, false);
    _ewk_tiled_backing_store_changed(priv);
    return _ewk_tiled_backing_store_enable_render(priv);
}
