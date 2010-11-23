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
#include "ewk_view.h"

#include "ewk_logging.h"

#include <Evas.h>
#include <eina_safety_checks.h>
#include <ewk_tiled_backing_store.h>

static Ewk_View_Smart_Class _parent_sc = EWK_VIEW_SMART_CLASS_INIT_NULL;

static Eina_Bool _ewk_view_tiled_render_cb(void *data, Ewk_Tile *t, const Eina_Rectangle *area)
{
    Ewk_View_Private_Data *priv = (Ewk_View_Private_Data*)data;
    Eina_Rectangle r = {area->x + t->x, area->y + t->y, area->w, area->h};

    return ewk_view_paint_contents(priv, t->cairo, &r);
}

static void *_ewk_view_tiled_updates_process_pre(void *data, Evas_Object *o)
{
    Ewk_View_Private_Data *priv = (Ewk_View_Private_Data*)data;
    ewk_view_layout_if_needed_recursive(priv);
    return 0;
}

static Evas_Object *_ewk_view_tiled_smart_backing_store_add(Ewk_View_Smart_Data *sd)
{
    Evas_Object *bs = ewk_tiled_backing_store_add(sd->base.evas);
    ewk_tiled_backing_store_render_cb_set
        (bs, _ewk_view_tiled_render_cb, sd->_priv);
    ewk_tiled_backing_store_updates_process_pre_set
        (bs, _ewk_view_tiled_updates_process_pre, sd->_priv);
    return bs;
}

static void
_ewk_view_tiled_contents_size_changed_cb(void *data, Evas_Object *o, void *event_info)
{
    Evas_Coord *size = (Evas_Coord*)event_info;
    Ewk_View_Smart_Data *sd = (Ewk_View_Smart_Data*)data;

    ewk_tiled_backing_store_contents_resize
        (sd->backing_store, size[0], size[1]);
}

static void _ewk_view_tiled_smart_add(Evas_Object *o)
{
    Ewk_View_Smart_Data *sd;

    _parent_sc.sc.add(o);

    sd = (Ewk_View_Smart_Data*)evas_object_smart_data_get(o);
    evas_object_smart_callback_add(
        sd->main_frame, "contents,size,changed",
        _ewk_view_tiled_contents_size_changed_cb, sd);
    ewk_frame_paint_full_set(sd->main_frame, EINA_TRUE);
}

static Eina_Bool _ewk_view_tiled_smart_scrolls_process(Ewk_View_Smart_Data *sd)
{
    const Ewk_Scroll_Request *sr;
    const Ewk_Scroll_Request *sr_end;
    size_t count;
    Evas_Coord vw, vh;

    ewk_frame_contents_size_get(sd->main_frame, &vw, &vh);

    sr = ewk_view_scroll_requests_get(sd->_priv, &count);
    sr_end = sr + count;
    for (; sr < sr_end; sr++) {
        if (sr->main_scroll)
            ewk_tiled_backing_store_scroll_full_offset_add
                (sd->backing_store, sr->dx, sr->dy);
        else {
            Evas_Coord sx, sy, sw, sh;

            sx = sr->x;
            sy = sr->y;
            sw = sr->w;
            sh = sr->h;

            if (abs(sr->dx) >= sw || abs(sr->dy) >= sh) {
                /* doubt webkit would be so     stupid... */
                DBG("full page scroll %+03d,%+03d. convert to repaint %d,%d + %dx%d",
                    sr->dx, sr->dy, sx, sy, sw, sh);
                ewk_view_repaint_add(sd->_priv, sx, sy, sw, sh);
                continue;
            }

            if (sx + sw > vw)
                sw = vw - sx;
            if (sy + sh > vh)
                sh = vh - sy;

            if (sw < 0)
                sw = 0;
            if (sh < 0)
                sh = 0;

            if (!sw || !sh)
                continue;

            sx -= abs(sr->dx);
            sy -= abs(sr->dy);
            sw += abs(sr->dx);
            sh += abs(sr->dy);
            ewk_view_repaint_add(sd->_priv, sx, sy, sw, sh);
            INF("using repaint for inner frame scolling!");
        }
    }

    return EINA_TRUE;
}

static Eina_Bool _ewk_view_tiled_smart_repaints_process(Ewk_View_Smart_Data *sd)
{
    const Eina_Rectangle *pr, *pr_end;
    size_t count;
    int sx, sy;

    ewk_frame_scroll_pos_get(sd->main_frame, &sx, &sy);

    pr = ewk_view_repaints_get(sd->_priv, &count);
    pr_end = pr + count;
    for (; pr < pr_end; pr++) {
        Eina_Rectangle r;
        r.x = pr->x + sx;
        r.y = pr->y + sy;
        r.w = pr->w;
        r.h = pr->h;
        ewk_tiled_backing_store_update(sd->backing_store, &r);
    }
    ewk_tiled_backing_store_updates_process(sd->backing_store);

    return EINA_TRUE;
}

static Eina_Bool _ewk_view_tiled_smart_contents_resize(Ewk_View_Smart_Data *sd, int w, int h)
{
    ewk_tiled_backing_store_contents_resize(sd->backing_store, w, h);
    return EINA_TRUE;
}

static Eina_Bool _ewk_view_tiled_smart_zoom_set(Ewk_View_Smart_Data *sd, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    Evas_Coord x, y, w, h;
    Eina_Bool r;
    r = ewk_tiled_backing_store_zoom_set(sd->backing_store,
                                         &zoom, cx, cy, &x, &y);
    if (!r)
        return r;
    ewk_tiled_backing_store_disabled_update_set(sd->backing_store, EINA_TRUE);
    r = _parent_sc.zoom_set(sd, zoom, cx, cy);
    ewk_frame_scroll_set(sd->main_frame, -x, -y);
    ewk_frame_scroll_size_get(sd->main_frame, &w, &h);
    ewk_tiled_backing_store_fix_offsets(sd->backing_store, w, h);
    ewk_view_scrolls_process(sd);
    evas_object_smart_calculate(sd->backing_store);
    evas_object_smart_calculate(sd->self);
    ewk_tiled_backing_store_disabled_update_set(sd->backing_store, EINA_FALSE);
    return r;
}

static Eina_Bool _ewk_view_tiled_smart_zoom_weak_set(Ewk_View_Smart_Data *sd, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    return ewk_tiled_backing_store_zoom_weak_set(sd->backing_store, zoom, cx, cy);
}

static void _ewk_view_tiled_smart_zoom_weak_smooth_scale_set(Ewk_View_Smart_Data *sd, Eina_Bool smooth_scale)
{
    ewk_tiled_backing_store_zoom_weak_smooth_scale_set(sd->backing_store, smooth_scale);
}

static void _ewk_view_tiled_smart_flush(Ewk_View_Smart_Data *sd)
{
    ewk_tiled_backing_store_flush(sd->backing_store);
    _parent_sc.flush(sd);
}

static Eina_Bool _ewk_view_tiled_smart_pre_render_region(Ewk_View_Smart_Data *sd, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, float zoom)
{
    return ewk_tiled_backing_store_pre_render_region
        (sd->backing_store, x, y, w, h, zoom);
}

static Eina_Bool _ewk_view_tiled_smart_pre_render_relative_radius(Ewk_View_Smart_Data *sd, unsigned int n, float zoom)
{
    return ewk_tiled_backing_store_pre_render_relative_radius
        (sd->backing_store, n, zoom);
}

static void _ewk_view_tiled_smart_pre_render_cancel(Ewk_View_Smart_Data *sd)
{
    ewk_tiled_backing_store_pre_render_cancel(sd->backing_store);
}

static Eina_Bool _ewk_view_tiled_smart_disable_render(Ewk_View_Smart_Data *sd)
{
    return ewk_tiled_backing_store_disable_render(sd->backing_store);
}

static Eina_Bool _ewk_view_tiled_smart_enable_render(Ewk_View_Smart_Data *sd)
{
    return ewk_tiled_backing_store_enable_render(sd->backing_store);
}

/**
 * Sets the smart class api using tiled backing store, enabling view
 * to be inherited.
 *
 * @param api class definition to be set, all members with the
 *        exception of Evas_Smart_Class->data may be overridden. Must
 *        @b not be @c NULL.
 *
 * @note Evas_Smart_Class->data is used to implement type checking and
 *       is not supposed to be changed/overridden. If you need extra
 *       data for your smart class to work, just extend
 *       Ewk_View_Smart_Class instead.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on failure (probably
 *         version mismatch).
 *
 * @see ewk_view_base_smart_set()
 */
Eina_Bool ewk_view_tiled_smart_set(Ewk_View_Smart_Class *api)
{
    if (!ewk_view_base_smart_set(api))
        return EINA_FALSE;

    if (EINA_UNLIKELY(!_parent_sc.sc.add))
        ewk_view_base_smart_set(&_parent_sc);

    api->sc.add = _ewk_view_tiled_smart_add;

    api->backing_store_add = _ewk_view_tiled_smart_backing_store_add;
    api->scrolls_process = _ewk_view_tiled_smart_scrolls_process;
    api->repaints_process = _ewk_view_tiled_smart_repaints_process;
    api->contents_resize = _ewk_view_tiled_smart_contents_resize;
    api->zoom_set = _ewk_view_tiled_smart_zoom_set;
    api->zoom_weak_set = _ewk_view_tiled_smart_zoom_weak_set;
    api->zoom_weak_smooth_scale_set = _ewk_view_tiled_smart_zoom_weak_smooth_scale_set;
    api->flush = _ewk_view_tiled_smart_flush;
    api->pre_render_region = _ewk_view_tiled_smart_pre_render_region;
    api->pre_render_relative_radius = _ewk_view_tiled_smart_pre_render_relative_radius;
    api->pre_render_cancel = _ewk_view_tiled_smart_pre_render_cancel;
    api->disable_render = _ewk_view_tiled_smart_disable_render;
    api->enable_render = _ewk_view_tiled_smart_enable_render;
    return EINA_TRUE;
}

static inline Evas_Smart *_ewk_view_tiled_smart_class_new(void)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("EWK_View_Tiled");
    static Evas_Smart *smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_tiled_smart_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

/**
 * Creates a new EFL WebKit View object using tiled backing store.
 *
 * View objects are the recommended way to deal with EFL WebKit as it
 * abstracts the complex pieces of the process.
 *
 * This object is almost the same as the one returned by the ewk_view_add()
 * function, but it uses the tiled backing store instead of the default
 * backing store.
 *
 * @param e canvas where to create the view object.
 *
 * @return view object or @c NULL if errors.
 *
 * @see ewk_view_uri_set()
 */
Evas_Object *ewk_view_tiled_add(Evas *e)
{
    return evas_object_smart_add(e, _ewk_view_tiled_smart_class_new());
}

/**
 * Get the cache of unused tiles used by this view.
 *
 * @param o view object to get cache.
 * @return instance of "cache of unused tiles" or @c NULL on errors.
 */
Ewk_Tile_Unused_Cache *ewk_view_tiled_unused_cache_get(const Evas_Object *o)
{
    Ewk_View_Smart_Data *sd = ewk_view_smart_data_get(o);
    EINA_SAFETY_ON_NULL_RETURN_VAL(sd, 0);
    return ewk_tiled_backing_store_tile_unused_cache_get(sd->backing_store);
}

/**
 * Set the cache of unused tiles used by this view.
 *
 * @param o view object to get cache.
 * @param cache instance of "cache of unused tiles". This can be used
 *        to share a single cache amongst different views. The tiles
 *        from one view will not be used by the other! This is just to
 *        limit the group with amount of unused memory.
 *        If @c NULL is provided, then a new cache is created.
 */
void ewk_view_tiled_unused_cache_set(Evas_Object *o, Ewk_Tile_Unused_Cache *cache)
{
    Ewk_View_Smart_Data *sd = ewk_view_smart_data_get(o);
    EINA_SAFETY_ON_NULL_RETURN(sd);
    ewk_tiled_backing_store_tile_unused_cache_set(sd->backing_store, cache);
}

/**
 * Set the function with the same name of the tiled backing store.
 * @param o the tiled backing store object.
 * @param flag value of the tiled backing store flag to set.
 */
void ewk_view_tiled_process_entire_queue_set(Evas_Object *o, Eina_Bool flag)
{
    Ewk_View_Smart_Data *sd = ewk_view_smart_data_get(o);
    EINA_SAFETY_ON_NULL_RETURN(sd);
    ewk_tiled_backing_store_process_entire_queue_set(sd->backing_store, flag);
}
