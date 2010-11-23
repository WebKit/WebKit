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

#define _GNU_SOURCE
#include "ewk_tiled_private.h"
#include <Ecore.h>
#include <Eina.h>
#include <errno.h>
#include <math.h>
#include <stdio.h> // XXX REMOVE ME LATER
#include <stdlib.h>
#include <string.h>

#define IDX(col, row, rowspan) (col + (row * rowspan))

#if !defined(MIN)
# define MIN(a, b) ((a < b) ? a : b)
#endif

#if !defined(MAX)
# define MAX(a, b) ((a > b) ? a : b)
#endif

typedef enum _Ewk_Tiled_Backing_Store_Pre_Render_Priority Ewk_Tiled_Backing_Store_Pre_Render_Priority;
typedef struct _Ewk_Tiled_Backing_Store_Data Ewk_Tiled_Backing_Store_Data;
typedef struct _Ewk_Tiled_Backing_Store_Item Ewk_Tiled_Backing_Store_Item;
typedef struct _Ewk_Tiled_Backing_Store_Pre_Render_Request Ewk_Tiled_Backing_Store_Pre_Render_Request;

enum _Ewk_Tiled_Backing_Store_Pre_Render_Priority {
    PRE_RENDER_PRIORITY_LOW = 0, /**< Append the request to the list */
    PRE_RENDER_PRIORITY_HIGH     /**< Prepend the request to the list */
};

struct _Ewk_Tiled_Backing_Store_Item {
    EINA_INLIST;
    Ewk_Tile *tile;
    struct {
        Evas_Coord x, y, w, h;
    } geometry;
    struct {
        Eina_List *process;
        unsigned long row, col;
        float zoom;
    } update;
    Ewk_Tiled_Backing_Store_Pre_Render_Request *pre_render;
    Eina_Bool smooth_scale;
};

struct _Ewk_Tiled_Backing_Store_Pre_Render_Request {
    EINA_INLIST;
    unsigned long col, row;
    float zoom;
    struct _Ewk_Tiled_Backing_Store_Item *it;
};

struct _Ewk_Tiled_Backing_Store_Data {
    Evas_Object_Smart_Clipped_Data base;
    Evas_Object *self;
    Evas_Object *contents_clipper;
    struct {
        Eina_Inlist **items;
        Evas_Coord x, y, w, h;
        long cols, rows;
        struct {
            Evas_Coord w, h;
            float zoom;
            Eina_Bool zoom_weak_smooth_scale:1;
        } tile;
        struct {
            struct {
                Evas_Coord x, y;
            } cur, old, base, zoom_center;
        } offset;
    } view;
    Evas_Colorspace cspace;
    struct {
        Ewk_Tile_Matrix *matrix;
        struct {
            unsigned long col, row;
        } base;
        struct {
            unsigned long cols, rows;
        } cur, old;
        Evas_Coord width, height;
    } model;
    struct {
        Eina_Bool (*cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *area);
        void *data;
        Eina_List *queue;
        Eina_Bool process_entire_queue;
        Eina_Inlist *pre_render_requests;
        Ecore_Idler *idler;
        Eina_Bool disabled;
        Eina_Bool suspend:1;
    } render;
    struct {
        void *(*pre_cb)(void *data, Evas_Object *o);
        void *pre_data;
        void *(*post_cb)(void *data, void *pre_data, Evas_Object *o);
        void *post_data;
    } process;
    struct {
        Eina_Bool any:1;
        Eina_Bool pos:1;
        Eina_Bool size:1;
        Eina_Bool model:1;
        Eina_Bool offset:1;
    } changed;
#ifdef DEBUG_MEM_LEAKS
    Ecore_Event_Handler *sig_usr;
#endif
};

static Evas_Smart_Class _parent_sc = EVAS_SMART_CLASS_INIT_NULL;
int _ewk_tiled_log_dom = -1;

#define PRIV_DATA_GET_OR_RETURN(obj, ptr, ...)                       \
    Ewk_Tiled_Backing_Store_Data *ptr = evas_object_smart_data_get(obj); \
    if (!ptr) {                                                      \
        CRITICAL("no private data in obj=%p", obj);                  \
        return __VA_ARGS__;                                          \
    }

static inline void _ewk_tiled_backing_store_item_request_del(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it);
static inline void _ewk_tiled_backing_store_item_request_add(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it, int m_col, int m_row, float zoom);
static void _ewk_tiled_backing_store_fill_renderers(Ewk_Tiled_Backing_Store_Data *priv);
static inline void _ewk_tiled_backing_store_view_dbg(const Ewk_Tiled_Backing_Store_Data *priv);
static inline void _ewk_tiled_backing_store_changed(Ewk_Tiled_Backing_Store_Data *priv);

static inline void _ewk_tiled_backing_store_updates_process(Ewk_Tiled_Backing_Store_Data *priv)
{
    void *data = NULL;

    /* Do not process updates. Note that we still want to get updates requests
     * in the queue in order to not miss any updates after the render is
     * resumed.
     */
    if (priv->render.suspend || !evas_object_visible_get(priv->self))
        return;

    if (priv->process.pre_cb)
        data = priv->process.pre_cb(priv->process.pre_data, priv->self);

    ewk_tile_matrix_updates_process(priv->model.matrix);

    if (priv->process.post_cb)
        priv->process.post_cb(priv->process.post_data, data, priv->self);
}

static int _ewk_tiled_backing_store_flush(void *data)
{
    Ewk_Tiled_Backing_Store_Data *priv = data;
    Ewk_Tile_Unused_Cache *tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);

    if (tuc) {
        DBG("flush unused tile cache.");
        ewk_tile_unused_cache_auto_flush(tuc);
    } else
        ERR("no cache?!");

    return 0;
}

static Ewk_Tile *_ewk_tiled_backing_store_tile_new(Ewk_Tiled_Backing_Store_Data *priv, unsigned long col, unsigned long row, float zoom)
{
    Ewk_Tile *t;
    Evas *evas = evas_object_evas_get(priv->self);
    if (!evas) {
        CRITICAL("evas_object_evas_get failed!");
        return NULL;
    }

    t = ewk_tile_matrix_tile_new
        (priv->model.matrix, evas, col, row, zoom);

    if (!t) {
        CRITICAL("ewk_tile_matrix_tile_new failed!");
        return NULL;
    }

    return t;
}

static void _ewk_tiled_backing_store_item_move(Ewk_Tiled_Backing_Store_Item *it, Evas_Coord x, Evas_Coord y)
{
    it->geometry.x = x;
    it->geometry.y = y;

    if (it->tile)
        evas_object_move(it->tile->image, x, y);
}

static void _ewk_tiled_backing_store_item_resize(Ewk_Tiled_Backing_Store_Item *it, Evas_Coord w, Evas_Coord h)
{
    it->geometry.w = w;
    it->geometry.h = h;

    if (it->tile) {
        evas_object_resize(it->tile->image, w, h);
        evas_object_image_fill_set(it->tile->image, 0, 0, w, h);
    }
}

static void _ewk_tiled_backing_store_tile_associate(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tile *t, Ewk_Tiled_Backing_Store_Item *it)
{
    if (it->tile)
        CRITICAL("it->tile=%p, but it should be NULL!", it->tile);
    it->tile = t;
    evas_object_move(it->tile->image, it->geometry.x, it->geometry.y);
    evas_object_resize(it->tile->image, it->geometry.w, it->geometry.h);
    evas_object_image_fill_set
        (it->tile->image, 0, 0, it->geometry.w, it->geometry.h);
    evas_object_image_smooth_scale_set(it->tile->image, it->smooth_scale);

    if (!ewk_tile_visible_get(t))
        evas_object_smart_member_add(t->image, priv->self);

    ewk_tile_show(t);
}

static void _ewk_tiled_backing_store_tile_dissociate(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it, double last_used)
{
    Ewk_Tile_Unused_Cache *tuc;
    ewk_tile_hide(it->tile);
    if (!ewk_tile_visible_get(it->tile))
        evas_object_smart_member_del(it->tile->image);
    ewk_tile_matrix_tile_put(priv->model.matrix, it->tile, last_used);
    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_auto_flush(tuc);

    it->tile = NULL;
}

static void _ewk_tiled_backing_store_tile_dissociate_all(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist *it;
    Ewk_Tiled_Backing_Store_Item *item;
    int i;
    double last_used = ecore_loop_time_get();

    for (i = 0; i < priv->view.rows; i++) {
        it = priv->view.items[i];
        EINA_INLIST_FOREACH(it, item)
            if (item->tile)
                _ewk_tiled_backing_store_tile_dissociate(priv, item, last_used);
    }
}

static inline Eina_Bool _ewk_tiled_backing_store_pre_render_request_add(Ewk_Tiled_Backing_Store_Data *priv, unsigned long col, unsigned long row, float zoom, Ewk_Tiled_Backing_Store_Item *it, Ewk_Tiled_Backing_Store_Pre_Render_Priority priority)
{
    Ewk_Tiled_Backing_Store_Pre_Render_Request *r;

    MALLOC_OR_OOM_RET(r, sizeof(*r), EINA_FALSE);

    if (priority == PRE_RENDER_PRIORITY_HIGH)
        priv->render.pre_render_requests = eina_inlist_prepend
            (priv->render.pre_render_requests, EINA_INLIST_GET(r));
    else
        priv->render.pre_render_requests = eina_inlist_append
            (priv->render.pre_render_requests, EINA_INLIST_GET(r));

    r->col = col;
    r->row = row;
    r->zoom = zoom;
    r->it = it;

    if (it)
        it->pre_render = r;
    return EINA_TRUE;
}

static inline void _ewk_tiled_backing_store_pre_render_request_del(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Pre_Render_Request *r)
{
    priv->render.pre_render_requests = eina_inlist_remove
        (priv->render.pre_render_requests, EINA_INLIST_GET(r));
    free(r);
}

static inline Ewk_Tiled_Backing_Store_Pre_Render_Request *_ewk_tiled_backing_store_pre_render_request_first(const Ewk_Tiled_Backing_Store_Data *priv)
{
    return EINA_INLIST_CONTAINER_GET(
        priv->render.pre_render_requests,
        Ewk_Tiled_Backing_Store_Pre_Render_Request);
}

static void _ewk_tiled_backing_store_pre_render_request_flush(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist **pl = &priv->render.pre_render_requests;
    while (*pl) {
        Ewk_Tiled_Backing_Store_Pre_Render_Request *r;
        r = _ewk_tiled_backing_store_pre_render_request_first(priv);
        if (r->it && r->it->pre_render)
            r->it->pre_render = NULL;
        *pl = eina_inlist_remove(*pl, *pl);
        free(r);
    }
}

static void _ewk_tiled_backing_store_pre_render_request_remove_unassociated(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist **pl = &priv->render.pre_render_requests;
    Eina_Inlist *iter = *pl, *tmp;
    while (iter) {
        Ewk_Tiled_Backing_Store_Pre_Render_Request *r =
            EINA_INLIST_CONTAINER_GET(
                iter, Ewk_Tiled_Backing_Store_Pre_Render_Request);
        if (!r->it) {
            tmp = iter->next;
            *pl = eina_inlist_remove(*pl, iter);
            iter = tmp;
            free(r);
        } else
            iter = iter->next;
    }
}

static void _ewk_tiled_backing_store_pre_render_request_remove_associated(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist **pl = &priv->render.pre_render_requests;
    Eina_Inlist *iter = *pl, *tmp;
    while (iter) {
        Ewk_Tiled_Backing_Store_Pre_Render_Request *r =
            EINA_INLIST_CONTAINER_GET(
                iter, Ewk_Tiled_Backing_Store_Pre_Render_Request);
        if (r->it) {
            if (r->it->pre_render)
                r->it->pre_render = NULL;
            tmp = iter->next;
            *pl = eina_inlist_remove(*pl, iter);
            iter = tmp;
            free(r);
        } else
            iter = iter->next;
    }
}

/* assumes priv->process.pre_cb was called if required! */
static void _ewk_tiled_backing_store_pre_render_request_process_single(Ewk_Tiled_Backing_Store_Data *priv)
{
    Ewk_Tiled_Backing_Store_Pre_Render_Request *req;
    Eina_Rectangle area;
    Ewk_Tile_Matrix *tm = priv->model.matrix;
    Ewk_Tile *t;
    Ewk_Tile_Unused_Cache *tuc;
    unsigned long col, row;
    float zoom;
    double last_used = ecore_loop_time_get();

    req = _ewk_tiled_backing_store_pre_render_request_first(priv);
    if (!req)
        return;

    col = req->col;
    row = req->row;
    zoom = req->zoom;

    if (ewk_tile_matrix_tile_exact_exists(tm, col, row, zoom)) {
        DBG("no pre-render required for tile %lu,%lu @ %f.", col, row, zoom);
        goto end;
    }

    if (req->it && req->it->tile) {
        CRITICAL("it->tile = %p (%lu, %lu), but should be NULL", req->it->tile, req->it->tile->row, req->it->tile->col);
        goto end;
    }

    t = _ewk_tiled_backing_store_tile_new(priv, col, row, zoom);
    if (!t)
        goto end;

    area.x = 0;
    area.y = 0;
    area.w = priv->view.tile.w;
    area.h = priv->view.tile.h;

    priv->render.cb(priv->render.data, t, &area);
    evas_object_image_data_update_add(
        t->image,
        area.x, area.y, area.w, area.h);
    ewk_tile_matrix_tile_updates_clear(tm, t);

    if (req->it) {
        _ewk_tiled_backing_store_tile_associate(priv, t, req->it);
        if (req->it->pre_render)
            req->it->pre_render = NULL;
    } else
        ewk_tile_matrix_tile_put(tm, t, last_used);

end:
    _ewk_tiled_backing_store_pre_render_request_del(priv, req);
    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_auto_flush(tuc);
}

static Eina_Bool _ewk_tiled_backing_store_item_process_idler_cb(void *data)
{
    Ewk_Tiled_Backing_Store_Data *priv = data;
    Ewk_Tiled_Backing_Store_Item *it = NULL;

    while (priv->render.queue) {
        it = priv->render.queue->data;
        if (it->tile->zoom == priv->view.tile.zoom) {
            _ewk_tiled_backing_store_item_request_del(priv, it);
            it = NULL;
        } else {
            unsigned long row, col;
            float zoom;
            Ewk_Tile *t;
            if (it->tile) {
                double last_used = ecore_loop_time_get();
                _ewk_tiled_backing_store_tile_dissociate(priv, it, last_used);
            }

            row = it->update.row;
            col = it->update.col;
            zoom = it->update.zoom;
            t = _ewk_tiled_backing_store_tile_new(priv, col, row, zoom);
            if (!t) {
                priv->render.idler = NULL;
                return EINA_FALSE;
            }

            _ewk_tiled_backing_store_tile_associate(priv, t, it);
            it->update.process = NULL;
            priv->render.queue = eina_list_remove_list(priv->render.queue,
                                                       priv->render.queue);
            if (!priv->render.process_entire_queue)
                break;
        }
    }

    if (priv->process.pre_cb)
        data = priv->process.pre_cb(priv->process.pre_data, priv->self);

    ewk_tile_matrix_updates_process(priv->model.matrix);

    if (!it)
        _ewk_tiled_backing_store_pre_render_request_process_single(priv);

    if (priv->process.post_cb)
        priv->process.post_cb(priv->process.post_data, data, priv->self);

    if (!priv->render.queue && !priv->render.pre_render_requests) {
        priv->render.idler = NULL;
        return EINA_FALSE;
    }

    return EINA_TRUE;
}

static inline void _ewk_tiled_backing_store_item_process_idler_stop(Ewk_Tiled_Backing_Store_Data *priv)
{
    if (!priv->render.idler)
        return;

    ecore_idler_del(priv->render.idler);
    priv->render.idler = NULL;
}

static inline void _ewk_tiled_backing_store_item_process_idler_start(Ewk_Tiled_Backing_Store_Data *priv)
{
    if (priv->render.idler)
        return;
    priv->render.idler = ecore_idler_add(
        _ewk_tiled_backing_store_item_process_idler_cb, priv);
}

static inline void _ewk_tiled_backing_store_item_request_del(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it)
{
    priv->render.queue = eina_list_remove_list(priv->render.queue,
                                               it->update.process);
    it->update.process = NULL;
}

static inline void _ewk_tiled_backing_store_item_request_add(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it, int m_col, int m_row, float zoom)
{
    if (it->update.process)
        return;

    it->update.col = m_col;
    it->update.row = m_row;
    it->update.zoom = zoom;

    priv->render.queue = eina_list_append(priv->render.queue, it);
    it->update.process = eina_list_last(priv->render.queue);

    if (!priv->render.suspend)
        _ewk_tiled_backing_store_item_process_idler_start(priv);
}

static Eina_Bool _ewk_tiled_backing_store_disable_render(Ewk_Tiled_Backing_Store_Data *priv)
{
    if (priv->render.suspend)
        return EINA_TRUE;

    priv->render.suspend = EINA_TRUE;
    _ewk_tiled_backing_store_item_process_idler_stop(priv);
    return EINA_TRUE;
}

static Eina_Bool _ewk_tiled_backing_store_enable_render(Ewk_Tiled_Backing_Store_Data *priv)
{
    if (!priv->render.suspend)
        return EINA_TRUE;

    priv->render.suspend = EINA_FALSE;

    _ewk_tiled_backing_store_fill_renderers(priv);
    _ewk_tiled_backing_store_item_process_idler_start(priv);

    return EINA_TRUE;
}

/**
 * Returns a rectangle whose coordinates indicate which tiles are
 * currently inside the viewport.
 *
 * Specifically, the returned rectangle's coordinates have the
 * following meaning:
 *  - x: Column number for the top-level tile inside the viewport.
 *  - y: Row number for the top-level tile inside the viewport.
 *  - w: Number of tiles horizontally inside the viewport.
 *  - h: Number of tiles vertically inside the viewport.
 */
static Eina_Rectangle _ewk_tiled_backing_store_visible_tiles_rect(Ewk_Tiled_Backing_Store_Data *priv)
{
     Eina_Rectangle r;

     const Evas_Coord ox = -priv->view.offset.cur.x;
     const Evas_Coord oy = -priv->view.offset.cur.y;
     const Evas_Coord tw = priv->view.tile.w;
     const Evas_Coord th = priv->view.tile.h;

     r.x = MAX(0, ox / tw);
     r.y = MAX(0, oy / th);
     r.w = MIN(priv->model.cur.cols,
               ceil((float)(priv->view.w + (ox % tw)) / tw));
     r.h = MIN(priv->model.cur.rows,
               ceil((float)(priv->view.h + (oy % th)) / th));

     DBG("Returning %d,%d+%dx%d", r.x, r.y, r.w, r.h);

     return r;
}

static Eina_Bool _ewk_tiled_backing_store_tile_is_inside_viewport(Ewk_Tiled_Backing_Store_Data *priv, int col, int row)
{
    const Eina_Rectangle r = _ewk_tiled_backing_store_visible_tiles_rect(priv);

    return eina_rectangle_coords_inside(&r, col, row);
}

static Eina_Bool _ewk_tiled_backing_store_tile_is_adjacent_to_viewport(Ewk_Tiled_Backing_Store_Data *priv, int col, int row)
{
    const Eina_Rectangle r = _ewk_tiled_backing_store_visible_tiles_rect(priv);

    if (row == (r.y - 1) || row == (r.y + r.h))
        return (col >= (r.x - 1) && col <= (r.x + r.w));

    if (col == (r.x - 1) || col == (r.x + r.w))
        return (row >= (r.y - 1) && row <= (r.y + r.h));

    return EINA_FALSE;
}

static inline Eina_Bool _ewk_tiled_backing_store_item_fill(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it, long col, int row)
{
    int m_col = priv->model.base.col + col;
    int m_row = priv->model.base.row + row;
    double last_used = ecore_loop_time_get();

    if (m_col < 0 || m_row < 0
        || m_col >= priv->model.cur.cols || m_row >= priv->model.cur.rows) {

        if (it->tile) {
            _ewk_tiled_backing_store_tile_dissociate(priv, it, last_used);
            if (it->update.process)
                _ewk_tiled_backing_store_item_request_del(priv, it);
        }
    } else {
        Ewk_Tile *t;
        const float zoom = priv->view.tile.zoom;

        if (it->update.process) {
            if (it->update.row == m_row
                && it->update.col == m_col
                && it->update.zoom == zoom)
                return EINA_TRUE;

            _ewk_tiled_backing_store_item_request_del(priv, it);
        }

        if (it->pre_render) {
            _ewk_tiled_backing_store_pre_render_request_del(
                priv, it->pre_render);
            it->pre_render = NULL;
        }

        if (it->tile) {
            Ewk_Tile *old = it->tile;
            if (old->row != m_row || old->col != m_col || old->zoom != zoom) {
                _ewk_tiled_backing_store_tile_dissociate(priv, it,
                                                         last_used);
                if (it->update.process)
                    _ewk_tiled_backing_store_item_request_del(priv, it);
            } else if (old->row == m_row && old->col == m_col
                       && old->zoom == zoom)
                goto end;
        }

        t = ewk_tile_matrix_tile_exact_get
            (priv->model.matrix, m_col, m_row, zoom);
        if (!t) {
            /* NOTE: it never returns NULL if it->tile was set! */
            if (it->tile) {
                CRITICAL("it->tile=%p, but it should be NULL!", it->tile);
                _ewk_tiled_backing_store_tile_dissociate(priv, it,
                                                         last_used);
            }

            /* Do not add new requests to the render queue */
            if (!priv->render.suspend) {
                if (!_ewk_tiled_backing_store_tile_is_inside_viewport(
                        priv, m_col, m_row)) {
                    DBG("%d,%d is not inside the viewport", m_col, m_row);
                    if (_ewk_tiled_backing_store_tile_is_adjacent_to_viewport(
                            priv, m_col, m_row))
                        _ewk_tiled_backing_store_pre_render_request_add(
                            priv, m_col, m_row, zoom, it,
                            PRE_RENDER_PRIORITY_HIGH);
                    _ewk_tiled_backing_store_item_process_idler_start(priv);

                    goto end;
                }

                t = _ewk_tiled_backing_store_tile_new(priv, m_col, m_row, zoom);
                if (!t)
                    return EINA_FALSE;
                _ewk_tiled_backing_store_tile_associate(priv, t, it);
            }
        } else if (t != it->tile) {
            if (!it->update.process) {
                if (it->tile)
                    _ewk_tiled_backing_store_tile_dissociate(priv,
                                                             it, last_used);
                _ewk_tiled_backing_store_tile_associate(priv, t, it);
            }
        }

      end:

        return EINA_TRUE;
    }

    return EINA_TRUE;
}

static Ewk_Tiled_Backing_Store_Item *_ewk_tiled_backing_store_item_add(Ewk_Tiled_Backing_Store_Data *priv, long col, int row)
{
    Ewk_Tiled_Backing_Store_Item *it;
    Evas_Coord x, y, tw, th;

    DBG("o=%p", priv->self);

    MALLOC_OR_OOM_RET(it, sizeof(*it), NULL);

    tw = priv->view.tile.w;
    th = priv->view.tile.h;
    x = priv->view.offset.base.x + priv->view.x + tw  *col;
    y = priv->view.offset.base.y + priv->view.y + th  *row;

    it->tile = NULL;
    it->update.process = NULL;
    it->smooth_scale = priv->view.tile.zoom_weak_smooth_scale;
    it->pre_render = NULL;
    _ewk_tiled_backing_store_item_move(it, x, y);
    _ewk_tiled_backing_store_item_resize(it, tw, th);
    if (!_ewk_tiled_backing_store_item_fill(priv, it, col, row)) {
        free(it);
        return NULL;
    }

    return it;
}

static void _ewk_tiled_backing_store_item_del(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tiled_Backing_Store_Item *it)
{
    if (it->tile) {
        double last_used = ecore_loop_time_get();
        _ewk_tiled_backing_store_tile_dissociate(priv, it, last_used);
    }
    if (it->update.process)
        _ewk_tiled_backing_store_item_request_del(priv, it);
    if (it->pre_render) {
        _ewk_tiled_backing_store_pre_render_request_del(
            priv, it->pre_render);
        it->pre_render = NULL;
    }
    free(it);
}

static void _ewk_tiled_backing_store_item_smooth_scale_set(Ewk_Tiled_Backing_Store_Item *it, Eina_Bool smooth_scale)
{
    if (it->smooth_scale == smooth_scale)
        return;

    if (it->tile)
        evas_object_image_smooth_scale_set(it->tile->image, smooth_scale);
}

static inline void _ewk_tiled_backing_store_changed(Ewk_Tiled_Backing_Store_Data *priv)
{
    if (priv->changed.any)
        return;
    evas_object_smart_changed(priv->self);
    priv->changed.any = EINA_TRUE;
}

static void _ewk_tiled_backing_store_view_cols_end_del(Ewk_Tiled_Backing_Store_Data *priv, Eina_Inlist **p_row, unsigned int count)
{
    Eina_Inlist *n;
    unsigned int i;

    if (!count)
        return;

    n = (*p_row)->last;

    for (i = 0; i < count; i++) {
        Ewk_Tiled_Backing_Store_Item *it;
        it = EINA_INLIST_CONTAINER_GET(n, Ewk_Tiled_Backing_Store_Item);
        n = n->prev;
        *p_row = eina_inlist_remove(*p_row, EINA_INLIST_GET(it));
        _ewk_tiled_backing_store_item_del(priv, it);
    }
}

static Eina_Bool _ewk_tiled_backing_store_view_cols_end_add(Ewk_Tiled_Backing_Store_Data *priv, Eina_Inlist **p_row, unsigned int base_col, unsigned int count)
{
    unsigned int i, r = p_row - priv->view.items;

    for (i = 0; i < count; i++, base_col++) {
        Ewk_Tiled_Backing_Store_Item *it;

        it = _ewk_tiled_backing_store_item_add(priv, base_col, r);
        if (!it) {
            CRITICAL("failed to add column %u of %u in row %u.", i, count, r);
            _ewk_tiled_backing_store_view_cols_end_del(priv, p_row, i);
            return EINA_FALSE;
        }

        *p_row = eina_inlist_append(*p_row, EINA_INLIST_GET(it));
    }
    return EINA_TRUE;
}

static void _ewk_tiled_backing_store_view_row_del(Ewk_Tiled_Backing_Store_Data *priv, Eina_Inlist *row)
{
    while (row) {
        Ewk_Tiled_Backing_Store_Item *it;
        it = EINA_INLIST_CONTAINER_GET(row, Ewk_Tiled_Backing_Store_Item);
        row = row->next;
        _ewk_tiled_backing_store_item_del(priv, it);
    }
}

static void _ewk_tiled_backing_store_view_rows_range_del(Ewk_Tiled_Backing_Store_Data *priv, Eina_Inlist **start, Eina_Inlist **end)
{
    for (; start < end; start++) {
        _ewk_tiled_backing_store_view_row_del(priv, *start);
        *start = NULL;
    }
}

static void _ewk_tiled_backing_store_view_rows_all_del(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist **start;
    Eina_Inlist **end;

    start = priv->view.items;
    end = priv->view.items + priv->view.rows;
    _ewk_tiled_backing_store_view_rows_range_del(priv, start, end);

    free(priv->view.items);
    priv->view.items = NULL;
    priv->view.cols = 0;
    priv->view.rows = 0;
}

static void _ewk_tiled_backing_store_render(void *data, Ewk_Tile *t, const Eina_Rectangle *area)
{
    Ewk_Tiled_Backing_Store_Data *priv = data;

    INF("TODO %p (visible? %d) [%lu,%lu] %d,%d + %dx%d",
        t, t->visible, t->col, t->row, area->x, area->y, area->w, area->h);

    if (!t->visible)
        return;

    if (priv->view.tile.w != t->w || priv->view.tile.h != t->h)
        return; // todo: remove me later, don't even flag as dirty!

    EINA_SAFETY_ON_NULL_RETURN(priv->render.cb);
    if (!priv->render.cb(priv->render.data, t, area))
        return;

    evas_object_image_data_update_add(t->image, area->x, area->y, area->w, area->h);
}

static inline void _ewk_tiled_backing_store_model_matrix_create(Ewk_Tiled_Backing_Store_Data *priv, Ewk_Tile_Unused_Cache *tuc)
{
    if (priv->model.matrix) {
        _ewk_tiled_backing_store_view_rows_all_del(priv);

        priv->changed.offset = EINA_FALSE;
        priv->changed.size = EINA_TRUE;

        ewk_tile_matrix_free(priv->model.matrix);
    }

    priv->model.matrix = ewk_tile_matrix_new
        (tuc, priv->model.cur.cols, priv->model.cur.rows, priv->cspace,
         _ewk_tiled_backing_store_render, priv);
}

static void _ewk_tiled_backing_store_smart_member_del(Evas_Object *o, Evas_Object *member)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    if (!priv->contents_clipper)
        return;
    evas_object_clip_unset(member);
    if (!evas_object_clipees_get(priv->contents_clipper))
        evas_object_hide(priv->contents_clipper);
}

static void _ewk_tiled_backing_store_smart_member_add(Evas_Object *o, Evas_Object *member)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    if (!priv->contents_clipper)
        return;
    evas_object_clip_set(member, priv->contents_clipper);
    if (evas_object_visible_get(o))
        evas_object_show(priv->contents_clipper);
}

#ifdef DEBUG_MEM_LEAKS
static void _ewk_tiled_backing_store_mem_dbg(Ewk_Tiled_Backing_Store_Data *priv)
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

static Eina_Bool _ewk_tiled_backing_store_sig_usr(void *data, int type, void *event)
{
    Ecore_Event_Signal_User *sig = (Ecore_Event_Signal_User*)event;
    Ewk_Tiled_Backing_Store_Data *priv = (Ewk_Tiled_Backing_Store_Data*)data;

    if (sig->number == 2) {
        Ewk_Tile_Unused_Cache *tuc;
        tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
        ewk_tile_unused_cache_auto_flush(tuc);
    }

    _ewk_tiled_backing_store_view_dbg(priv);
    _ewk_tiled_backing_store_mem_dbg(priv);
    return EINA_TRUE;
}
#endif

static void _ewk_tiled_backing_store_smart_add(Evas_Object *o)
{
    Ewk_Tiled_Backing_Store_Data *priv;

    DBG("o=%p", o);

    CALLOC_OR_OOM_RET(priv, sizeof(*priv));

    priv->self = o;
    priv->view.tile.zoom = 1.0;
    priv->view.tile.w = TILE_W;
    priv->view.tile.h = TILE_H;
    priv->view.offset.cur.x = 0;
    priv->view.offset.cur.y = 0;
    priv->view.offset.old.x = 0;
    priv->view.offset.old.y = 0;
    priv->view.offset.base.x = -TILE_W;
    priv->view.offset.base.y = -TILE_H;

    priv->model.base.col = -1;
    priv->model.base.row = -1;
    priv->model.cur.cols = 1;
    priv->model.cur.rows = 1;
    priv->model.old.cols = 0;
    priv->model.old.rows = 0;
    priv->model.width = 0;
    priv->model.height = 0;
    priv->render.process_entire_queue = EINA_TRUE;
    priv->render.suspend = EINA_FALSE;
    priv->cspace = EVAS_COLORSPACE_ARGB8888; // TODO: detect it.

    evas_object_smart_data_set(o, priv);
    _parent_sc.add(o);

    priv->contents_clipper = evas_object_rectangle_add(
        evas_object_evas_get(o));
    evas_object_move(priv->contents_clipper, 0, 0);
    evas_object_resize(priv->contents_clipper,
                       priv->model.width, priv->model.height);
    evas_object_color_set(priv->contents_clipper, 255, 255, 255, 255);
    evas_object_show(priv->contents_clipper);
    evas_object_smart_member_add(priv->contents_clipper, o);

    _ewk_tiled_backing_store_model_matrix_create(priv, NULL);
    evas_object_move(priv->base.clipper, 0, 0);
    evas_object_resize(priv->base.clipper, 0, 0);
    evas_object_clip_set(priv->contents_clipper, priv->base.clipper);

#ifdef DEBUG_MEM_LEAKS
    priv->sig_usr = ecore_event_handler_add
        (ECORE_EVENT_SIGNAL_USER, _ewk_tiled_backing_store_sig_usr, priv);
#endif
}

static void _ewk_tiled_backing_store_smart_del(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    DBG("o=%p", o);
    Ewk_Tile_Unused_Cache *tuc;

    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_unlock_area(tuc);

    _ewk_tiled_backing_store_flush(priv);

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    _ewk_tiled_backing_store_item_process_idler_stop(priv);
    _ewk_tiled_backing_store_view_rows_all_del(priv);

#ifdef DEBUG_MEM_LEAKS
    _ewk_tiled_backing_store_mem_dbg(priv);
    if (priv->sig_usr)
        priv->sig_usr = ecore_event_handler_del(priv->sig_usr);
#endif

    ewk_tile_matrix_free(priv->model.matrix);
    evas_object_smart_member_del(priv->contents_clipper);
    evas_object_del(priv->contents_clipper);

    _parent_sc.del(o);

#ifdef DEBUG_MEM_LEAKS
    printf("\nIMPORTANT: TILED BACKING STORE DELETED (may be real leaks)\n");
    ewk_tile_accounting_dbg();
#endif
}

static void _ewk_tiled_backing_store_smart_move(Evas_Object *o, Evas_Coord x, Evas_Coord y)
{
    DBG("o=%p, new pos: %dx%d", o, x, y);

    PRIV_DATA_GET_OR_RETURN(o, priv);

    if (priv->changed.pos)
        return;

    if (priv->view.x == x && priv->view.y == y)
        return;

    priv->changed.pos = EINA_TRUE;
    _ewk_tiled_backing_store_changed(priv);
}

static void _ewk_tiled_backing_store_smart_resize(Evas_Object *o, Evas_Coord w, Evas_Coord h)
{
    DBG("o=%p, new size: %dx%d", o, w, h);

    PRIV_DATA_GET_OR_RETURN(o, priv);

    if (priv->changed.size)
        return;

    if (priv->view.w == w && priv->view.h == h)
        return;

    priv->changed.size = EINA_TRUE;
    _ewk_tiled_backing_store_changed(priv);
}

static void _ewk_tiled_backing_store_recalc_renderers(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord w, Evas_Coord h, Evas_Coord tw, Evas_Coord th)
{
    long cols, rows, old_rows, old_cols;
    INF("o=%p, new size: %dx%d", priv->self, w, h);

    cols = 2 + (int)ceil((float)w / (float)tw);
    rows = 2 + (int)ceil((float)h / (float)th);

    INF("o=%p new grid size cols: %ld, rows: %ld, was %ld, %ld",
        priv->self, cols, rows, priv->view.cols, priv->view.rows);

    if (priv->view.cols == cols && priv->view.rows == rows)
        return;

    _ewk_tiled_backing_store_pre_render_request_remove_associated(priv);

    old_cols = priv->view.cols;
    old_rows = priv->view.rows;

    if (rows < old_rows) {
        Eina_Inlist **start, **end;
        start = priv->view.items + rows;
        end = priv->view.items + old_rows;
        _ewk_tiled_backing_store_view_rows_range_del(priv, start, end);
    }
    REALLOC_OR_OOM_RET(priv->view.items, sizeof(Eina_Inlist*) * (int)rows);
    priv->view.rows = rows;
    priv->view.cols = cols;
    if (rows > old_rows) {
        Eina_Inlist **start, **end;
        start = priv->view.items + old_rows;
        end = priv->view.items + rows;
        for (; start < end; start++) {
            Eina_Bool r;
            *start = NULL;
            r = _ewk_tiled_backing_store_view_cols_end_add
                (priv, start, 0, cols);
            if (!r) {
                CRITICAL("failed to allocate %ld columns", cols);
                _ewk_tiled_backing_store_view_rows_range_del
                    (priv, priv->view.items + old_rows, start);
                priv->view.rows = old_rows;
                return;
            }
        }
    }

    if (cols != old_cols) {
        Eina_Inlist **start, **end;
        int todo = cols - old_cols;
        start = priv->view.items;
        end = start + MIN(old_rows, rows);
        if (todo > 0) {
            for (; start < end; start++) {
                Eina_Bool r;
                r = _ewk_tiled_backing_store_view_cols_end_add
                    (priv, start, old_cols, todo);
                if (!r) {
                    CRITICAL("failed to allocate %d columns!", todo);

                    for (start--; start >= priv->view.items; start--)
                        _ewk_tiled_backing_store_view_cols_end_del(priv, start, todo);
                    if (rows > old_rows) {
                        start = priv->view.items + old_rows;
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

static void _ewk_tiled_backing_store_smart_calculate_size(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord w, Evas_Coord h)
{
    evas_object_resize(priv->base.clipper, w, h);

    priv->view.w = w;
    priv->view.h = h;

    _ewk_tiled_backing_store_recalc_renderers(
        priv, w, h, priv->view.tile.w, priv->view.tile.h);
}

// TODO: remove me later.
static inline void _ewk_tiled_backing_store_view_dbg(const Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist **start, **end;
    printf("tiles=%2ld,%2ld  model=%2ld,%2ld [%dx%d] base=%+3ld,%+4ld offset=%+4d,%+4d old=%+4d,%+4d base=%+3d,%+3d\n",
           priv->view.cols, priv->view.rows,
           priv->model.cur.cols, priv->model.cur.rows,
           priv->model.width, priv->model.height,
           priv->model.base.col, priv->model.base.row,
           priv->view.offset.cur.x, priv->view.offset.cur.y,
           priv->view.offset.old.x, priv->view.offset.old.y,
           priv->view.offset.base.x, priv->view.offset.base.y);

    start = priv->view.items;
    end = priv->view.items + priv->view.rows;
    for (; start < end; start++) {
        const Ewk_Tiled_Backing_Store_Item *it;

        EINA_INLIST_FOREACH(*start, it) {
            printf(" %+4d,%+4d ", it->geometry.x, it->geometry.y);

            if (!it->tile)
                printf("            ;");
            else
                printf("%8p %lu,%lu;", it->tile, it->tile->col, it->tile->row);
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
static void _ewk_tiled_backing_store_view_wrap_up(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int last_row = priv->view.rows - 1;
    Evas_Coord tw = priv->view.tile.w;
    Evas_Coord th = priv->view.tile.h;
    Evas_Coord off_y = (priv->view.offset.base.y % th) - th;
    Evas_Coord oy = y + (last_row - count + 1) * th + off_y;
    Eina_Inlist **itr_start, **itr_end;

    itr_start = priv->view.items;
    itr_end = itr_start + last_row;

    for (; count > 0; count--) {
        Eina_Inlist **itr;
        Eina_Inlist *tmp = *itr_start;
        Ewk_Tiled_Backing_Store_Item *it;
        Evas_Coord ox = x + priv->view.offset.base.x;
        int c = 0;

        for (itr = itr_start; itr < itr_end; itr++)
            *itr = *(itr + 1);
        *itr = tmp;

        priv->model.base.row++;
        EINA_INLIST_FOREACH(tmp, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            ox += tw;
            _ewk_tiled_backing_store_item_fill(priv, it, c, last_row);
            c++;
        }
        oy += th;
    }
    priv->view.offset.base.y = off_y;
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
static void _ewk_tiled_backing_store_view_wrap_down(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    Evas_Coord tw = priv->view.tile.w;
    Evas_Coord th = priv->view.tile.h;
    Evas_Coord off_y = (priv->view.offset.base.y % th) - th;
    Evas_Coord oy = y + off_y + (count - 1) * th;
    Eina_Inlist **itr_start, **itr_end;

    itr_start = priv->view.items + priv->view.rows - 1;
    itr_end = priv->view.items;

    for (; count > 0; count--) {
        Eina_Inlist **itr;
        Eina_Inlist *tmp = *itr_start;
        Ewk_Tiled_Backing_Store_Item *it;
        Evas_Coord ox = x + priv->view.offset.base.x;
        int c = 0;

        for (itr = itr_start; itr > itr_end; itr--)
            *itr = *(itr - 1);
        *itr = tmp;

        priv->model.base.row--;
        EINA_INLIST_FOREACH(tmp, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            ox += tw;
            _ewk_tiled_backing_store_item_fill(priv, it, c, 0);
            c++;
        }
        oy -= th;
    }
    priv->view.offset.base.y = off_y;
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
static void _ewk_tiled_backing_store_view_wrap_left(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int r, last_col = priv->view.cols - 1;
    Evas_Coord tw = priv->view.tile.w;
    Evas_Coord th = priv->view.tile.h;
    Evas_Coord off_x = (priv->view.offset.base.x % tw) - tw;
    Evas_Coord oy = y + priv->view.offset.base.y;
    Eina_Inlist **itr;
    Eina_Inlist **itr_end;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;
    r = 0;

    priv->model.base.col += count;

    for (; itr < itr_end; itr++, r++) {
        Evas_Coord ox = x + (last_col - count + 1) * tw + off_x;
        unsigned int i, c = last_col - count + 1;

        for (i = 0; i < count; i++, c++, ox += tw) {
            Ewk_Tiled_Backing_Store_Item *it;
            it = EINA_INLIST_CONTAINER_GET(*itr, Ewk_Tiled_Backing_Store_Item);
            *itr = eina_inlist_demote(*itr, *itr);

            _ewk_tiled_backing_store_item_move(it, ox, oy);
            _ewk_tiled_backing_store_item_fill(priv, it, c, r);
        }
        oy += th;
    }

    priv->view.offset.base.x = off_x;
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
static void _ewk_tiled_backing_store_view_wrap_right(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y, unsigned int count)
{
    unsigned int r;
    Evas_Coord tw = priv->view.tile.w;
    Evas_Coord th = priv->view.tile.h;
    Evas_Coord off_x = (priv->view.offset.base.x % tw) - tw;
    Evas_Coord oy = y + priv->view.offset.base.y;
    Eina_Inlist **itr, **itr_end;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;
    r = 0;

    priv->model.base.col -= count;

    for (; itr < itr_end; itr++, r++) {
        Evas_Coord ox = x + (count - 1) * tw + off_x;
        unsigned int i, c = count - 1;

        for (i = 0; i < count; i++, c--, ox -= tw) {
            Ewk_Tiled_Backing_Store_Item *it;
            it = EINA_INLIST_CONTAINER_GET((*itr)->last, Ewk_Tiled_Backing_Store_Item);
            *itr = eina_inlist_promote(*itr, (*itr)->last);

            _ewk_tiled_backing_store_item_move(it, ox, oy);
            _ewk_tiled_backing_store_item_fill(priv, it, c, r);
        }
        oy += th;
    }

    priv->view.offset.base.x = off_x;
}

static void _ewk_tiled_backing_store_view_refill(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y, int step_x, int step_y)
{
    Eina_Inlist **itr, **itr_end;
    Evas_Coord base_ox, oy, tw, th;
    unsigned int r;

    evas_object_move(priv->base.clipper, x, y);

    tw = priv->view.tile.w;
    th = priv->view.tile.h;

    base_ox = x + priv->view.offset.base.x;
    oy = y + priv->view.offset.base.y;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;
    r = 0;

    priv->model.base.col -= step_x;
    priv->model.base.row -= step_y;

    for (; itr < itr_end; itr++, r++) {
        Ewk_Tiled_Backing_Store_Item *it;
        Evas_Coord ox = base_ox;
        unsigned int c = 0;
        EINA_INLIST_FOREACH(*itr, it) {
            _ewk_tiled_backing_store_item_fill(priv, it, c, r);
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            c++;
            ox += tw;
        }
        oy += th;
    }
}

static void _ewk_tiled_backing_store_view_pos_apply(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y)
{
    Eina_Inlist **itr, **itr_end;
    Evas_Coord base_ox, oy, tw, th;

    evas_object_move(priv->base.clipper, x, y);

    tw = priv->view.tile.w;
    th = priv->view.tile.h;

    base_ox = x + priv->view.offset.base.x;
    oy = y + priv->view.offset.base.y;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;
    for (; itr < itr_end; itr++) {
        Ewk_Tiled_Backing_Store_Item *it;
        Evas_Coord ox = base_ox;
        EINA_INLIST_FOREACH(*itr, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            ox += tw;
        }
        oy += th;
    }
}

static void _ewk_tiled_backing_store_smart_calculate_offset_force(Ewk_Tiled_Backing_Store_Data *priv)
{
    Evas_Coord dx = priv->view.offset.cur.x - priv->view.offset.old.x;
    Evas_Coord dy = priv->view.offset.cur.y - priv->view.offset.old.y;
    Evas_Coord tw, th;
    int step_y, step_x;

    INF("o=%p, offset: %+4d, %+4d (%+4d, %+4d)",
        priv->self, dx, dy, priv->view.offset.cur.x, priv->view.offset.cur.y);

    tw = priv->view.tile.w;
    th = priv->view.tile.h;

    step_x = (dx + priv->view.offset.base.x + tw) / tw;
    step_y = (dy + priv->view.offset.base.y + th) / th;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    evas_object_move(
        priv->contents_clipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);

    priv->view.offset.base.x += dx - step_x * tw;
    priv->view.offset.base.y += dy - step_y * th;

    _ewk_tiled_backing_store_view_refill
        (priv, priv->view.x, priv->view.y, step_x, step_y);
}

static void _ewk_tiled_backing_store_smart_calculate_offset(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y)
{
    Evas_Coord dx = priv->view.offset.cur.x - priv->view.offset.old.x;
    Evas_Coord dy = priv->view.offset.cur.y - priv->view.offset.old.y;
    Evas_Coord tw, th;
    int step_y, step_x;

    INF("o=%p, offset: %+4d, %+4d (%+4d, %+4d)",
        priv->self, dx, dy, priv->view.offset.cur.x, priv->view.offset.cur.y);

    if (!dx && !dy)
        return;

    tw = priv->view.tile.w;
    th = priv->view.tile.h;

    step_x = (dx + priv->view.offset.base.x + tw) / tw;
    step_y = (dy + priv->view.offset.base.y + th) / th;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    evas_object_move(
        priv->contents_clipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);

    if ((step_x < 0 && step_x <= -priv->view.cols)
        || (step_x > 0 && step_x >= priv->view.cols)
        || (step_y < 0 && step_y <= -priv->view.rows)
        || (step_y > 0 && step_y >= priv->view.rows)) {

        priv->view.offset.base.x += dx - step_x * tw;
        priv->view.offset.base.y += dy - step_y * th;

        _ewk_tiled_backing_store_view_refill
            (priv, priv->view.x, priv->view.y, step_x, step_y);
        return;
    }

    priv->view.offset.base.x += dx;
    priv->view.offset.base.y += dy;

    if (step_y < 0)
        _ewk_tiled_backing_store_view_wrap_up(priv, x, y, -step_y);
    else if (step_y > 0)
        _ewk_tiled_backing_store_view_wrap_down(priv, x, y, step_y);

    if (step_x < 0)
        _ewk_tiled_backing_store_view_wrap_left(priv, x, y, -step_x);
    else if (step_x > 0)
        _ewk_tiled_backing_store_view_wrap_right(priv, x, y, step_x);

    _ewk_tiled_backing_store_view_pos_apply(priv, x, y);
}

static void _ewk_tiled_backing_store_smart_calculate_pos(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y)
{
    _ewk_tiled_backing_store_view_pos_apply(priv, x, y);
    priv->view.x = x;
    priv->view.y = y;
    evas_object_move(
        priv->contents_clipper,
        priv->view.offset.cur.x + priv->view.x,
        priv->view.offset.cur.y + priv->view.y);
}

static void _ewk_tiled_backing_store_fill_renderers(Ewk_Tiled_Backing_Store_Data *priv)
{
    Eina_Inlist *it;
    Ewk_Tiled_Backing_Store_Item *item;
    int i, j;

    for (i = 0; i < priv->view.rows; i++) {
        it = priv->view.items[i];
        j = 0;
        EINA_INLIST_FOREACH(it, item)
            _ewk_tiled_backing_store_item_fill(priv, item, j++, i);
    }
}

static void _ewk_tiled_backing_store_smart_calculate(Evas_Object *o)
{
    Evas_Coord x, y, w, h;

    evas_object_geometry_get(o, &x, &y, &w, &h);
    DBG("o=%p at %d,%d + %dx%d", o, x, y, w, h);

    PRIV_DATA_GET_OR_RETURN(o, priv);

    priv->changed.any = EINA_FALSE;

    ewk_tile_matrix_freeze(priv->model.matrix);

    if (!priv->render.suspend && priv->changed.model) {
        long cols, rows;

        cols = priv->model.width / priv->view.tile.w + 1;
        rows = priv->model.height / priv->view.tile.h + 1;

        priv->model.old.cols = priv->model.cur.cols;
        priv->model.old.rows = priv->model.cur.rows;
        priv->model.cur.cols = cols;
        priv->model.cur.rows = rows;
        if (priv->model.old.cols > cols)
            cols = priv->model.old.cols;
        if (priv->model.old.rows > rows)
            rows = priv->model.old.rows;
        ewk_tile_matrix_resize(priv->model.matrix, cols, rows);
    }

    if (priv->changed.pos && (priv->view.x != x || priv->view.y != y)) {
        _ewk_tiled_backing_store_smart_calculate_pos(priv, x, y);
        priv->changed.pos = EINA_FALSE;
    } else if (priv->changed.offset) {
        _ewk_tiled_backing_store_smart_calculate_offset(priv, x, y);
        priv->changed.offset = EINA_FALSE;
    }

    if (priv->changed.size) {
        _ewk_tiled_backing_store_smart_calculate_size(priv, w, h);
        priv->changed.size = EINA_FALSE;
    }

    if (!priv->render.suspend && priv->changed.model) {
        Eina_Rectangle rect;
        rect.x = 0;
        rect.y = 0;
        rect.w = priv->model.width;
        rect.h = priv->model.height;
        _ewk_tiled_backing_store_fill_renderers(priv);
        ewk_tile_matrix_resize(priv->model.matrix,
                           priv->model.cur.cols,
                           priv->model.cur.rows);
        priv->changed.model = EINA_FALSE;
        evas_object_resize(priv->contents_clipper,
                           priv->model.width, priv->model.height);
        _ewk_tiled_backing_store_smart_calculate_offset_force(priv);

        /* Make sure we do not miss any important repaint by
         * repainting the whole viewport */
        const Eina_Rectangle r =
            { 0, 0, priv->model.width, priv->model.height };
        ewk_tile_matrix_update(priv->model.matrix, &r,
                               priv->view.tile.zoom);
    }

    ewk_tile_matrix_thaw(priv->model.matrix);

    _ewk_tiled_backing_store_updates_process(priv);

    if (priv->view.offset.base.x >= 0
        || priv->view.offset.base.x <= -2 * priv->view.tile.w
        || priv->view.offset.base.y >= 0
        || priv->view.offset.base.y <= -2 * priv->view.tile.h)
        ERR("incorrect base offset %+4d,%+4d, tile=%dx%d, cur=%+4d,%+4d\n",
            priv->view.offset.base.x, priv->view.offset.base.y,
            priv->view.tile.w, priv->view.tile.h,
            priv->view.offset.cur.x, priv->view.offset.cur.y);

}

Evas_Object *ewk_tiled_backing_store_add(Evas *e)
{
    static Evas_Smart *smart = NULL;

    if (_ewk_tiled_log_dom < 0)
        _ewk_tiled_log_dom = eina_log_domain_register("Ewk_Tiled_Backing_Store", NULL);

    if (!smart) {
        static Evas_Smart_Class sc =
            EVAS_SMART_CLASS_INIT_NAME_VERSION("Ewk_Tiled_Backing_Store");

        evas_object_smart_clipped_smart_set(&sc);
        _parent_sc = sc;

        sc.add = _ewk_tiled_backing_store_smart_add;
        sc.del = _ewk_tiled_backing_store_smart_del;
        sc.resize = _ewk_tiled_backing_store_smart_resize;
        sc.move = _ewk_tiled_backing_store_smart_move;
        sc.calculate = _ewk_tiled_backing_store_smart_calculate;
        sc.member_add = _ewk_tiled_backing_store_smart_member_add;
        sc.member_del = _ewk_tiled_backing_store_smart_member_del;

        smart = evas_smart_class_new(&sc);
    }

    return evas_object_smart_add(e, smart);
}

void ewk_tiled_backing_store_render_cb_set(Evas_Object *o, Eina_Bool (*cb)(void *data, Ewk_Tile *t, const Eina_Rectangle *area), const void *data)
{
    EINA_SAFETY_ON_NULL_RETURN(cb);
    PRIV_DATA_GET_OR_RETURN(o, priv);
    priv->render.cb = cb;
    priv->render.data = (void*)data;
}

Ewk_Tile_Unused_Cache *ewk_tiled_backing_store_tile_unused_cache_get(const Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, NULL);
    return ewk_tile_matrix_unused_cache_get(priv->model.matrix);
}

void ewk_tiled_backing_store_tile_unused_cache_set(Evas_Object *o, Ewk_Tile_Unused_Cache *tuc)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);

    if (ewk_tile_matrix_unused_cache_get(priv->model.matrix) == tuc)
        return;

    _ewk_tiled_backing_store_model_matrix_create(priv, tuc);
}

static Eina_Bool _ewk_tiled_backing_store_scroll_full_offset_set_internal(Ewk_Tiled_Backing_Store_Data *priv, Evas_Coord x, Evas_Coord y)
{
    /* TODO: check offset go out of bounds, clamp */
    if (priv->render.disabled)
        return EINA_FALSE;

    priv->view.offset.cur.x = x;
    priv->view.offset.cur.y = y;

    priv->changed.offset = EINA_TRUE;
    _ewk_tiled_backing_store_changed(priv);

    return EINA_TRUE;
}

Eina_Bool ewk_tiled_backing_store_scroll_full_offset_set(Evas_Object *o, Evas_Coord x, Evas_Coord y)
{
    DBG("o=%p, x=%d, y=%d", o, x, y);

    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    if (x == priv->view.offset.cur.x && y == priv->view.offset.cur.y)
        return EINA_TRUE;

    return _ewk_tiled_backing_store_scroll_full_offset_set_internal(priv, x, y);
}

Eina_Bool ewk_tiled_backing_store_scroll_full_offset_add(Evas_Object *o, Evas_Coord dx, Evas_Coord dy)
{
    DBG("o=%p, dx=%d, dy=%d", o, dx, dy);

    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    if (!dx && !dy)
        return EINA_TRUE;

    return _ewk_tiled_backing_store_scroll_full_offset_set_internal
        (priv, priv->view.offset.cur.x + dx, priv->view.offset.cur.y + dy);
}

static Eina_Bool _ewk_tiled_backing_store_zoom_set_internal(Ewk_Tiled_Backing_Store_Data *priv, float *zoom, Evas_Coord cx, Evas_Coord cy, Evas_Coord *offx, Evas_Coord *offy)
{
    *offx = priv->view.offset.cur.x;
    *offy = priv->view.offset.cur.y;

    if (fabsf(priv->view.tile.zoom - *zoom) < ZOOM_STEP_MIN) {
        DBG("ignored as zoom difference is < %f: %f",
            (double)ZOOM_STEP_MIN, fabsf(priv->view.tile.zoom - *zoom));
        return EINA_TRUE;
    }

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    Evas_Coord tw, th;
    tw = TILE_SIZE_AT_ZOOM(TILE_W, *zoom);
    tw = (tw >> 1) << 1;
    *zoom = TILE_W_ZOOM_AT_SIZE(tw);
    /* WARNING: assume reverse zoom is the same for both axis */
    th = TILE_SIZE_AT_ZOOM(TILE_H, *zoom);

    float scale = *zoom / priv->view.tile.zoom;

    priv->view.tile.zoom = *zoom;
    // todo: check cx [0, w]...
    priv->view.offset.zoom_center.x = cx;
    priv->view.offset.zoom_center.y = cy;

    priv->view.tile.w = tw;
    priv->view.tile.h = th;

    if (!priv->view.w || !priv->view.h) {
        priv->view.offset.base.x = -tw;
        priv->view.offset.base.y = -th;
        return EINA_TRUE;
    }
    Eina_Inlist **itr, **itr_end;
    Ewk_Tiled_Backing_Store_Item *it;

    Evas_Coord new_x = cx + (priv->view.offset.cur.x - cx) * scale;
    Evas_Coord new_y = cy + (priv->view.offset.cur.y - cy) * scale;
    Evas_Coord bx = cx + (priv->view.offset.base.x - cx) * scale;
    Evas_Coord by = cy + (priv->view.offset.base.y - cy) * scale;

    Evas_Coord model_width = priv->model.width * scale;
    Evas_Coord model_height = priv->model.height * scale;

    if (model_width < priv->view.w || new_x >= 0)
        new_x = 0;
    else if (-new_x + priv->view.w >= model_width)
        new_x = -model_width + priv->view.w;

    if (model_height < priv->view.h || new_y >= 0)
        new_y = 0;
    else if (-new_y + priv->view.h >= model_height)
        new_y = -model_height + priv->view.h;

    bx = new_x % tw - tw;
    priv->model.base.col = - new_x / tw - 1;
    by = new_y % th - th;
    priv->model.base.row = - new_y / th - 1;

    priv->changed.size = EINA_TRUE;
    _ewk_tiled_backing_store_changed(priv);

    priv->view.offset.cur.x = new_x;
    priv->view.offset.cur.y = new_y;
    priv->view.offset.base.x = bx;
    priv->view.offset.base.y = by;

    priv->view.offset.old.x = priv->view.offset.cur.x;
    priv->view.offset.old.y = priv->view.offset.cur.y;
    *offx = priv->view.offset.cur.x;
    *offy = priv->view.offset.cur.y;

    evas_object_move(
        priv->contents_clipper,
        new_x + priv->view.x,
        new_y + priv->view.y);

    _ewk_tiled_backing_store_fill_renderers(priv);

    Evas_Coord oy = priv->view.offset.base.y + priv->view.y;
    Evas_Coord base_ox = priv->view.x + priv->view.offset.base.x;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;

    for (; itr < itr_end; itr++) {
        Evas_Coord ox = base_ox;
        Eina_Inlist *lst = *itr;

        EINA_INLIST_FOREACH(lst, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            _ewk_tiled_backing_store_item_resize(it, tw, th);
            ox += tw;
        }
        oy += th;
    }

    return EINA_TRUE;
}

Eina_Bool ewk_tiled_backing_store_zoom_set(Evas_Object *o, float *zoom, Evas_Coord cx, Evas_Coord cy, Evas_Coord *offx, Evas_Coord *offy)
{
    DBG("o=%p, zoom=%f", o, (double)*zoom);

    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);

    return _ewk_tiled_backing_store_zoom_set_internal(priv, zoom, cx, cy, offx, offy);
}

Eina_Bool ewk_tiled_backing_store_zoom_weak_set(Evas_Object *o, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    DBG("o=%p, zoom=%f", o, (double)zoom);
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    if (!priv->view.w || !priv->view.h)
        return EINA_FALSE;
    Eina_Inlist **itr, **itr_end;
    Ewk_Tiled_Backing_Store_Item *it;
    Evas_Coord tw, th;
    Eina_Bool recalc = EINA_FALSE;

    tw = TILE_SIZE_AT_ZOOM(TILE_W, zoom);
    zoom = TILE_W_ZOOM_AT_SIZE(tw);
    /* WARNING: assume reverse zoom is the same for both axis */
    th = TILE_SIZE_AT_ZOOM(TILE_H, zoom);

    float scale = zoom / priv->view.tile.zoom;

    Evas_Coord model_width = priv->model.width * scale;
    Evas_Coord model_height = priv->model.height * scale;

    evas_object_resize(priv->contents_clipper,
                       model_width, model_height);

    int vrows = ceil((float)priv->view.h / (float)th) + 2;
    int vcols = ceil((float)priv->view.w / (float)tw) + 2;
    Evas_Coord new_x = cx + (priv->view.offset.cur.x - cx) * scale;
    Evas_Coord new_y = cy + (priv->view.offset.cur.y - cy) * scale;
    Evas_Coord bx = new_x % tw - tw;
    Evas_Coord by = new_y % th - th;
    unsigned long base_row = -new_y / th - 1;
    unsigned long base_col = -new_x / tw - 1;

    if (base_row != priv->model.base.row || base_col != priv->model.base.col) {
        priv->model.base.row = base_row;
        priv->model.base.col = base_col;
        recalc = EINA_TRUE;
    }

    if (vrows > priv->view.rows || vcols > priv->view.cols)
        recalc = EINA_TRUE;

    if (recalc) {
        Evas_Coord w, h;
        evas_object_geometry_get(o, NULL, NULL, &w, &h);
        _ewk_tiled_backing_store_recalc_renderers(priv, w, h, tw, th);
        _ewk_tiled_backing_store_fill_renderers(priv);
        _ewk_tiled_backing_store_updates_process(priv);
    }

    Evas_Coord base_ox = bx + priv->view.x;
    Evas_Coord oy = by + priv->view.y;

    evas_object_move(priv->contents_clipper,
                     new_x + priv->view.x,
                     new_y + priv->view.y);

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;

    for (; itr < itr_end; itr++) {
        Evas_Coord ox = base_ox;
        Eina_Inlist *lst = *itr;

        EINA_INLIST_FOREACH(lst, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            _ewk_tiled_backing_store_item_resize(it, tw, th);
            ox += tw;
        }
        oy += th;
    }

    return EINA_TRUE;
}

void ewk_tiled_backing_store_fix_offsets(Evas_Object *o, Evas_Coord w, Evas_Coord h)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    Eina_Inlist **itr, **itr_end;
    Ewk_Tiled_Backing_Store_Item *it;
    Evas_Coord new_x = priv->view.offset.cur.x;
    Evas_Coord new_y = priv->view.offset.cur.y;
    Evas_Coord bx = priv->view.offset.base.x;
    Evas_Coord by = priv->view.offset.base.y;
    Evas_Coord tw = priv->view.tile.w;
    Evas_Coord th = priv->view.tile.h;

    if (-new_x > w) {
        new_x = -w;
        bx = new_x % tw - tw;
        priv->model.base.col = -new_x / tw - 1;
    }

    if (-new_y > h) {
        new_y = -h;
        by = new_y % th - th;
        priv->model.base.row = -new_y / th - 1;
    }

    if (bx >= 0 || bx <= -2 * priv->view.tile.w) {
        bx = new_x % tw - tw;
        priv->model.base.col = -new_x / tw - 1;
    }

    if (by >= 0 || by <= -2 * priv->view.tile.h) {
        by = new_y % th - th;
        priv->model.base.row = -new_y / th - 1;
    }

    priv->view.offset.cur.x = new_x;
    priv->view.offset.cur.y = new_y;
    priv->view.offset.old.x = new_x;
    priv->view.offset.old.y = new_y;
    priv->view.offset.base.x = bx;
    priv->view.offset.base.y = by;
    evas_object_move(priv->contents_clipper,
                     new_x + priv->view.x,
                     new_y + priv->view.y);

    Evas_Coord oy = priv->view.offset.base.y + priv->view.y;
    Evas_Coord base_ox = priv->view.x + priv->view.offset.base.x;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;

    for (; itr < itr_end; itr++) {
        Evas_Coord ox = base_ox;
        Eina_Inlist *lst = *itr;

        EINA_INLIST_FOREACH(lst, it) {
            _ewk_tiled_backing_store_item_move(it, ox, oy);
            _ewk_tiled_backing_store_item_resize(it, tw, th);
            ox += tw;
        }
        oy += th;
    }
}

void ewk_tiled_backing_store_zoom_weak_smooth_scale_set(Evas_Object *o, Eina_Bool smooth_scale)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    Eina_Inlist **itr, **itr_end;

    itr = priv->view.items;
    itr_end = itr + priv->view.rows;
    priv->view.tile.zoom_weak_smooth_scale = smooth_scale;

    for (; itr< itr_end; itr++) {
        Ewk_Tiled_Backing_Store_Item *it;
        EINA_INLIST_FOREACH(*itr, it)
            if (it->tile)
                _ewk_tiled_backing_store_item_smooth_scale_set
                    (it, smooth_scale);
    }
}

Eina_Bool ewk_tiled_backing_store_update(Evas_Object *o, const Eina_Rectangle *update)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);

    if (priv->render.disabled)
        return EINA_FALSE;

    return ewk_tile_matrix_update(priv->model.matrix, update,
                                  priv->view.tile.zoom);
}

void ewk_tiled_backing_store_updates_process_pre_set(Evas_Object *o, void *(*cb)(void *data, Evas_Object *o), const void *data)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    priv->process.pre_cb = cb;
    priv->process.pre_data = (void*)data;
}

void ewk_tiled_backing_store_updates_process_post_set(Evas_Object *o, void *(*cb)(void *data, void *pre_data, Evas_Object *o), const void *data)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    priv->process.post_cb = cb;
    priv->process.post_data = (void*)data;
}

void ewk_tiled_backing_store_updates_process(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    _ewk_tiled_backing_store_updates_process(priv);
}

void ewk_tiled_backing_store_updates_clear(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);

    ewk_tile_matrix_updates_clear(priv->model.matrix);
}

void ewk_tiled_backing_store_contents_resize(Evas_Object *o, Evas_Coord width, Evas_Coord height)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);

    if (width == priv->model.width && height == priv->model.height)
        return;

    priv->model.width = width;
    priv->model.height = height;
    priv->changed.model = EINA_TRUE;

    DBG("width,height=%d, %d", width, height);
    _ewk_tiled_backing_store_changed(priv);
}

void ewk_tiled_backing_store_disabled_update_set(Evas_Object *o, Eina_Bool value)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);

    if (value != priv->render.disabled)
        priv->render.disabled = value;
}

void ewk_tiled_backing_store_flush(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    Ewk_Tile_Unused_Cache *tuc = NULL;

    priv->view.offset.cur.x = 0;
    priv->view.offset.cur.y = 0;
    priv->view.offset.old.x = 0;
    priv->view.offset.old.y = 0;
    priv->view.offset.base.x = -priv->view.tile.w;
    priv->view.offset.base.y = -priv->view.tile.h;
    priv->model.base.col = -1;
    priv->model.base.row = -1;
    priv->changed.size = EINA_TRUE;

#ifdef DEBUG_MEM_LEAKS
    printf("\nFLUSHED BACKING STORE, STATUS BEFORE DELETING TILE MATRIX:\n");
    _ewk_tiled_backing_store_mem_dbg(priv);
#endif

    _ewk_tiled_backing_store_pre_render_request_flush(priv);
    _ewk_tiled_backing_store_tile_dissociate_all(priv);
    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_clear(tuc);

#ifdef DEBUG_MEM_LEAKS
    printf("\nFLUSHED BACKING STORE, STATUS AFTER RECREATING TILE MATRIX:\n");
    _ewk_tiled_backing_store_mem_dbg(priv);
#endif
}

Eina_Bool ewk_tiled_backing_store_pre_render_region(Evas_Object *o, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    Eina_Tile_Grid_Slicer slicer;
    const Eina_Tile_Grid_Info *info;
    Evas_Coord tw, th;
    Ewk_Tile_Unused_Cache *tuc;

    tw = TILE_SIZE_AT_ZOOM(TILE_W, zoom);
    tw = (tw >> 1) << 1;
    zoom = TILE_W_ZOOM_AT_SIZE(tw);
    /* WARNING: assume reverse zoom is the same for both axis */
    th = TILE_SIZE_AT_ZOOM(TILE_H, zoom);

    if (!eina_tile_grid_slicer_setup(&slicer, x, y, w, h, tw, th)) {
        ERR("could not setup grid slicer for %d,%d+%dx%d tile=%dx%d",
            x, y, w, h, tw, th);
        return EINA_FALSE;
    }

    while (eina_tile_grid_slicer_next(&slicer, &info)) {
        const unsigned long c = info->col;
        const unsigned long r = info->row;
        if (!_ewk_tiled_backing_store_pre_render_request_add(priv, c, r, zoom, NULL, PRE_RENDER_PRIORITY_LOW))
            break;
    }

    _ewk_tiled_backing_store_item_process_idler_start(priv);

    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_lock_area(tuc, x, y, w, h, zoom);
    return EINA_TRUE;
}

Eina_Bool ewk_tiled_backing_store_pre_render_relative_radius(Evas_Object *o, unsigned int n, float zoom)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    unsigned long start_row, end_row, start_col, end_col, i, j, w, h;
    Ewk_Tile_Unused_Cache *tuc;

    INF("priv->model.base.row =%ld, n=%u priv->view.rows=%lu",
            priv->model.base.row, n, priv->view.rows);
    start_row = (long)priv->model.base.row - n;
    start_col = (long)priv->model.base.col - n;
    end_row = MIN(priv->model.cur.rows - 1,
                  priv->model.base.row + priv->view.rows + n - 1);
    end_col = MIN(priv->model.cur.cols - 1,
                  priv->model.base.col + priv->view.cols + n - 1);

    INF("start_row=%lu, end_row=%lu, start_col=%lu, end_col=%lu",
         start_row, end_row, start_col, end_col);

    for (i = start_row; i <= end_row; i++)
        for (j = start_col; j <= end_col; j++)
            if (!_ewk_tiled_backing_store_pre_render_request_add(priv, j, i, zoom, NULL, PRE_RENDER_PRIORITY_LOW))
                goto start_processing;

start_processing:
    _ewk_tiled_backing_store_item_process_idler_start(priv);

    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    h = (end_row - start_row + 1) * TILE_SIZE_AT_ZOOM(TILE_H, zoom);
    w = (end_col - start_col + 1) * TILE_SIZE_AT_ZOOM(TILE_W, zoom);
    ewk_tile_unused_cache_lock_area(tuc,
            start_col * TILE_SIZE_AT_ZOOM(TILE_W, zoom),
            start_row * TILE_SIZE_AT_ZOOM(TILE_H, zoom), w, h, zoom);

    return EINA_TRUE;
}

void ewk_tiled_backing_store_pre_render_cancel(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    Ewk_Tile_Unused_Cache *tuc;

    _ewk_tiled_backing_store_pre_render_request_remove_unassociated(priv);

    tuc = ewk_tile_matrix_unused_cache_get(priv->model.matrix);
    ewk_tile_unused_cache_unlock_area(tuc);
}

Eina_Bool ewk_tiled_backing_store_disable_render(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    return _ewk_tiled_backing_store_disable_render(priv);
}

Eina_Bool ewk_tiled_backing_store_enable_render(Evas_Object *o)
{
    PRIV_DATA_GET_OR_RETURN(o, priv, EINA_FALSE);
    _ewk_tiled_backing_store_changed(priv);
    return _ewk_tiled_backing_store_enable_render(priv);
}

/**
 * Set the process_entire_queue flag of the renderer idler.
 *
 *
 * @param o the tiled backing store object
 * @param value EINA_TRUE if we want to process all the request of our queue. EINA_FALSE otherwise.
 */
void ewk_tiled_backing_store_process_entire_queue_set(Evas_Object *o, Eina_Bool value)
{
    PRIV_DATA_GET_OR_RETURN(o, priv);
    if (priv->render.process_entire_queue != value)
        priv->render.process_entire_queue = value;
}
