/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

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

#include "ewk_frame.h"
#include "ewk_logging.h"

#include <Evas.h>
#include <eina_safety_checks.h>
#include <string.h>

static Ewk_View_Smart_Class _parent_sc = EWK_VIEW_SMART_CLASS_INIT_NULL;

static void _ewk_view_single_on_del(void *data, Evas *e, Evas_Object *o, void *event_info)
{
    Evas_Object *clip = (Evas_Object*)data;
    evas_object_del(clip);
}

static Evas_Object *_ewk_view_single_smart_backing_store_add(Ewk_View_Smart_Data *sd)
{
    Evas_Object *bs = evas_object_image_add(sd->base.evas);
    Evas_Object *clip = evas_object_rectangle_add(sd->base.evas);
    evas_object_image_alpha_set(bs, EINA_FALSE);
    evas_object_image_smooth_scale_set(bs, sd->zoom_weak_smooth_scale);
    evas_object_clip_set(bs, clip);
    evas_object_show(clip);

    evas_object_event_callback_add
        (bs, EVAS_CALLBACK_DEL, _ewk_view_single_on_del, clip);

    return bs;
}

static void _ewk_view_single_smart_resize(Evas_Object *o, Evas_Coord w, Evas_Coord h)
{
    Ewk_View_Smart_Data *sd = (Ewk_View_Smart_Data*)evas_object_smart_data_get(o);
    _parent_sc.sc.resize(o, w, h);

    // these should be queued and processed in calculate as well!
    evas_object_image_size_set(sd->backing_store, w, h);
    if (sd->animated_zoom.zoom.current < 0.00001) {
        Evas_Object *clip = evas_object_clip_get(sd->backing_store);
        Evas_Coord x, y, cw, ch;
        evas_object_image_fill_set(sd->backing_store, 0, 0, w, h);
        evas_object_geometry_get(sd->backing_store, &x, &y, 0, 0);
        evas_object_move(clip, x, y);
        ewk_frame_contents_size_get(sd->main_frame, &cw, &ch);
        if (w > cw)
            w = cw;
        if (h > ch)
            h = ch;
        evas_object_resize(clip, w, h);
    }
}

static inline void _ewk_view_4b_move_region_up(uint32_t *image, size_t rows, size_t x, size_t y, size_t w, size_t h, size_t rowsize)
{
    uint32_t *src;
    uint32_t *dst;

    dst = image + x + y * rowsize;
    src = dst + rows * rowsize;
    h -= rows;

    for (; h > 0; h--, dst += rowsize, src += rowsize)
        memcpy(dst, src, w * 4);
}

static inline void _ewk_view_4b_move_region_down(uint32_t *image, size_t rows, size_t x, size_t y, size_t w, size_t h, size_t rowsize)
{
    uint32_t *src;
    uint32_t *dst;

    h -= rows;
    src = image + x + (y + h - 1) * rowsize;
    dst = src + rows * rowsize;

    for (; h > 0; h--, dst -= rowsize, src -= rowsize)
        memcpy(dst, src, w * 4);
}

static inline void _ewk_view_4b_move_line_left(uint32_t *dst, const uint32_t *src, size_t count)
{
    uint32_t *dst_end = dst + count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; dst < dst_end; dst++, src++)
        *dst = *src;
}

static inline void _ewk_view_4b_move_line_right(uint32_t *dst, uint32_t *src, size_t count)
{
    uint32_t *dst_end = dst - count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; dst > dst_end; dst--, src--)
        *dst = *src;
}

static inline void _ewk_view_4b_move_region_left(uint32_t *image, size_t cols, size_t x, size_t y, size_t w, size_t h, size_t rowsize)
{
    uint32_t *src;
    uint32_t *dst;

    dst = image + x + y * rowsize;
    src = dst + cols;
    w -= cols;

    for (; h > 0; h--, dst += rowsize, src += rowsize)
        _ewk_view_4b_move_line_left(dst, src, w);
}

static inline void _ewk_view_4b_move_region_right(uint32_t *image, size_t cols, size_t x, size_t y, size_t w, size_t h, size_t rowsize)
{
    uint32_t *src;
    uint32_t *dst;

    w -= cols;
    src = image + (x + w - 1) + y * rowsize;
    dst = src + cols;

    for (; h > 0; h--, dst += rowsize, src += rowsize)
        _ewk_view_4b_move_line_right(dst, src, w);
}

/* catch-all function, not as optimized as the others, but does the work. */
static inline void _ewk_view_4b_move_region(uint32_t *image, int dx, int dy, size_t x, size_t y, size_t w, size_t h, size_t rowsize)
{
    uint32_t *src;
    uint32_t *dst;

    if (dy < 0) {
        h += dy;
        dst = image + x + y * rowsize;
        src = dst - dy * rowsize;
        if (dx <= 0) {
            w += dx;
            src -= dx;
            for (; h > 0; h--, dst += rowsize, src += rowsize)
                _ewk_view_4b_move_line_left(dst, src, w);
        } else {
            w -= dx;
            src += w - 1;
            dst += w + dx -1;
            for (; h > 0; h--, dst += rowsize, src += rowsize)
                _ewk_view_4b_move_line_right(dst, src, w);
        }
    } else {
        h -= dy;
        src = image + x + (y + h - 1) * rowsize;
        dst = src + dy * rowsize;
        if (dx <= 0) {
            w += dx;
            src -= dx;
            for (; h > 0; h--, dst -= rowsize, src -= rowsize)
                _ewk_view_4b_move_line_left(dst, src, w);
        } else {
            w -= dx;
            src += w - 1;
            dst += w + dx - 1;
            for (; h > 0; h--, dst -= rowsize, src -= rowsize)
                _ewk_view_4b_move_line_right(dst, src, w);
        }
    }
}

static inline void _ewk_view_single_scroll_process_single(Ewk_View_Smart_Data *sd, void *pixels, Evas_Coord ow, Evas_Coord oh, const Ewk_Scroll_Request *sr)
{
    Evas_Coord sx, sy, sw, sh;

    DBG("%d,%d + %d,%d %+03d,%+03d, store: %p %dx%d",
        sr->x, sr->y, sr->w, sr->h, sr->dx, sr->dy, pixels, ow, oh);

    sx = sr->x;
    sy = sr->y;
    sw = sr->w;
    sh = sr->h;

    if (abs(sr->dx) >= sw || abs(sr->dy) >= sh) {
        /* doubt webkit would be so stupid... */
        DBG("full page scroll %+03d,%+03d. convert to repaint %d,%d + %dx%d",
            sr->dx, sr->dy, sx, sy, sw, sh);
        ewk_view_repaint_add(sd->_priv, sx, sy, sw, sh);
        return;
    }

    if (sx < 0) {
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh += sy;
        sy = 0;
    }

    if (sx + sw > ow)
        sw = ow - sx;
    if (sy + sh > oh)
        sh = oh - sy;

    if (sw < 0)
        sw = 0;
    if (sh < 0)
        sh = 0;

    EINA_SAFETY_ON_TRUE_RETURN(!sw || !sh);
    if (!sr->dx) {
        if (sr->dy < 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                sr->dx, sr->dy, sx, sy, sw, sh + sr->dy,
                sx, sy + sh + sr->dy, sw, -sr->dy);

            _ewk_view_4b_move_region_up
                ((uint32_t*)pixels, -sr->dy, sx, sy, sw, sh, ow);
            evas_object_image_data_update_add
                (sd->backing_store, sx, sy, sw, sh + sr->dy);

            ewk_view_repaint_add(sd->_priv, sx, sy + sh + sr->dy, sw, -sr->dy);
        } else if (sr->dy > 0) {
            DBG("scroll down: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                sr->dx, sr->dy, sx, sy + sr->dy, sw, sh - sr->dy,
                sx, sy, sw, sr->dy);

            _ewk_view_4b_move_region_down
                ((uint32_t*)pixels, sr->dy, sx, sy, sw, sh, ow);
            evas_object_image_data_update_add
                (sd->backing_store, sx, sy + sr->dy, sw, sh - sr->dy);

            ewk_view_repaint_add(sd->_priv, sx, sy, sw, sr->dy);
        }
    } else if (!sr->dy) {
        if (sr->dx < 0) {
            DBG("scroll left: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                sr->dx, sr->dy, sx, sy, sw + sr->dx, sh,
                sx + sw + sr->dx, sy, -sr->dx, sh);

            _ewk_view_4b_move_region_left
                ((uint32_t*)pixels, -sr->dx, sx, sy, sw, sh, ow);
            evas_object_image_data_update_add
                (sd->backing_store, sx, sy, sw + sr->dx, sh);

            ewk_view_repaint_add(sd->_priv, sx + sw + sr->dx, sy, -sr->dx, sh);
        } else if (sr->dx > 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                sr->dx, sr->dy, sx + sr->dx, sy, sw - sr->dx, sh,
                sx, sy, sr->dx, sh);

            _ewk_view_4b_move_region_right
                ((uint32_t*)pixels, sr->dx, sx, sy, sw, sh, ow);
            evas_object_image_data_update_add
                (sd->backing_store, sx + sr->dx, sy, sw - sr->dx, sh);

            ewk_view_repaint_add(sd->_priv, sx, sy, sr->dx, sh);
        }
    } else {
        Evas_Coord mx, my, mw, mh, ax, ay, aw, ah, bx, by, bw, bh;

        if (sr->dx < 0) {
            mx = sx;
            mw = sw + sr->dx;
            ax = mx + mw;
            aw = -sr->dx;
        } else {
            ax = sx;
            aw = sr->dx;
            mx = ax + aw;
            mw = sw - sr->dx;
        }

        if (sr->dy < 0) {
            my = sy;
            mh = sh + sr->dy;
            by = my + mh;
            bh = -sr->dy;
        } else {
            by = sy;
            bh = sr->dy;
            my = by + bh;
            mh = sh - sr->dy;
        }

        ay = my;
        ah = mh;
        bx = sx;
        bw = sw;

        DBG("scroll diagonal: %+03d,%+03d update=%d,%d+%dx%d, "
            "repaints: h=%d,%d+%dx%d v=%d,%d+%dx%d",
            sr->dx, sr->dy, mx, my, mw, mh, ax, ay, aw, ah, bx, by, bw, bh);

        _ewk_view_4b_move_region
            ((uint32_t*)pixels, sr->dx, sr->dy, sx, sy, sw, sh, ow);

        evas_object_image_data_update_add(sd->backing_store, mx, my, mw, mh);
        ewk_view_repaint_add(sd->_priv, ax, ay, aw, ah);
        ewk_view_repaint_add(sd->_priv, bx, by, bw, bh);
    }
}

static Eina_Bool _ewk_view_single_smart_scrolls_process(Ewk_View_Smart_Data *sd)
{
    const Ewk_Scroll_Request *sr;
    const Ewk_Scroll_Request *sr_end;
    Evas_Coord ow, oh;
    size_t count;
    void *pixels = evas_object_image_data_get(sd->backing_store, 1);
    evas_object_image_size_get(sd->backing_store, &ow, &oh);

    sr = ewk_view_scroll_requests_get(sd->_priv, &count);
    sr_end = sr + count;
    for (; sr < sr_end; sr++)
        _ewk_view_single_scroll_process_single(sd, pixels, ow, oh, sr);

    return EINA_TRUE;
}

static Eina_Bool _ewk_view_single_smart_repaints_process(Ewk_View_Smart_Data *sd)
{
    Ewk_View_Paint_Context *ctxt;
    Evas_Coord ow, oh;
    void *pixels;
    Eina_Rectangle r = {0, 0, 0, 0};
    const Eina_Rectangle *pr;
    const Eina_Rectangle *pr_end;
    Eina_Tiler *tiler;
    Eina_Iterator *itr;
    cairo_status_t status;
    cairo_surface_t *surface;
    cairo_format_t format;
    cairo_t *cairo;
    size_t count;
    Eina_Bool ret = EINA_TRUE;

    if (sd->animated_zoom.zoom.current < 0.00001) {
        Evas_Object *clip = evas_object_clip_get(sd->backing_store);
        Evas_Coord w, h, cw, ch;
        // reset effects of zoom_weak_set()
        evas_object_image_fill_set
            (sd->backing_store, 0, 0, sd->view.w, sd->view.h);
        evas_object_move(clip, sd->view.x, sd->view.y);

        w = sd->view.w;
        h = sd->view.h;

        ewk_frame_contents_size_get(sd->main_frame, &cw, &ch);
        if (w > cw)
            w = cw;
        if (h > ch)
            h = ch;
        evas_object_resize(clip, w, h);
    }

    pixels = evas_object_image_data_get(sd->backing_store, 1);
    evas_object_image_size_get(sd->backing_store, &ow, &oh);

    if (sd->bg_color.a < 255)
        format = CAIRO_FORMAT_ARGB32;
    else
        format = CAIRO_FORMAT_RGB24;

    surface = cairo_image_surface_create_for_data
        ((unsigned char*)pixels, format, ow, oh, ow * 4);
    status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("could not create surface from data %dx%d: %s",
            ow, oh, cairo_status_to_string(status));
        ret = EINA_FALSE;
        goto error_cairo_surface;
    }
    cairo = cairo_create(surface);
    status = cairo_status(cairo);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("could not create cairo from surface %dx%d: %s",
            ow, oh, cairo_status_to_string(status));
        ret = EINA_FALSE;
        goto error_cairo;
    }

    ctxt = ewk_view_paint_context_new(sd->_priv, cairo);
    if (!ctxt) {
        ERR("could not create paint context");
        ret = EINA_FALSE;
        goto error_paint_context;
    }

    tiler = eina_tiler_new(ow, oh);
    if (!tiler) {
        ERR("could not create tiler %dx%d", ow, oh);
        ret = EINA_FALSE;
        goto error_tiler;
    }

    pr = ewk_view_repaints_get(sd->_priv, &count);
    pr_end = pr + count;
    for (; pr < pr_end; pr++)
        eina_tiler_rect_add(tiler, pr);

    itr = eina_tiler_iterator_new(tiler);
    if (!itr) {
        ERR("could not get iterator for tiler");
        ret = EINA_FALSE;
        goto error_iterator;
    }

    int sx, sy;
    ewk_frame_scroll_pos_get(sd->main_frame, &sx, &sy);

    EINA_ITERATOR_FOREACH(itr, r) {
        Eina_Rectangle scrolled_rect = {
            r.x + sx, r.y + sy,
            r.w, r.h
        };

        ewk_view_paint_context_save(ctxt);

        if ((sx) || (sy))
            ewk_view_paint_context_translate(ctxt, -sx, -sy);

        ewk_view_paint_context_clip(ctxt, &scrolled_rect);
        ewk_view_paint_context_paint_contents(ctxt, &scrolled_rect);

        ewk_view_paint_context_restore(ctxt);
        evas_object_image_data_update_add
            (sd->backing_store, r.x, r.y, r.w, r.h);
    }
    eina_iterator_free(itr);

error_iterator:
    eina_tiler_free(tiler);
error_tiler:
    ewk_view_paint_context_free(ctxt);
error_paint_context:
    cairo_destroy(cairo);
error_cairo:
    cairo_surface_destroy(surface);
error_cairo_surface:
    evas_object_image_data_set(sd->backing_store, pixels); /* dec refcount */

    return ret;
}

static Eina_Bool _ewk_view_single_smart_zoom_weak_set(Ewk_View_Smart_Data *sd, float zoom, Evas_Coord cx, Evas_Coord cy)
{
    // TODO: review
    float scale = zoom / sd->animated_zoom.zoom.start;
    Evas_Coord w = sd->view.w * scale;
    Evas_Coord h = sd->view.h * scale;
    Evas_Coord dx, dy, cw, ch;
    Evas_Object *clip = evas_object_clip_get(sd->backing_store);

    ewk_frame_contents_size_get(sd->main_frame, &cw, &ch);
    if (sd->view.w > 0 && sd->view.h > 0) {
        dx = (w * (sd->view.w - cx)) / sd->view.w;
        dy = (h * (sd->view.h - cy)) / sd->view.h;
    } else {
        dx = 0;
        dy = 0;
    }

    evas_object_image_fill_set(sd->backing_store, cx + dx, cy + dy, w, h);

    if (sd->view.w > 0 && sd->view.h > 0) {
        dx = ((sd->view.w - w) * cx) / sd->view.w;
        dy = ((sd->view.h - h) * cy) / sd->view.h;
    } else {
        dx = 0;
        dy = 0;
    }
    evas_object_move(clip, sd->view.x + dx, sd->view.y + dy);

    if (cw < sd->view.w)
        w = cw * scale;
    if (ch < sd->view.h)
        h = ch * scale;
    evas_object_resize(clip, w, h);
    return EINA_TRUE;
}

static void _ewk_view_single_smart_zoom_weak_smooth_scale_set(Ewk_View_Smart_Data *sd, Eina_Bool smooth_scale)
{
    evas_object_image_smooth_scale_set(sd->backing_store, smooth_scale);
}

static void _ewk_view_single_smart_bg_color_set(Ewk_View_Smart_Data *sd, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    evas_object_image_alpha_set(sd->backing_store, a < 255);
}

/**
 * Sets the smart class api using single backing store, enabling view
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
Eina_Bool ewk_view_single_smart_set(Ewk_View_Smart_Class *api)
{
    if (!ewk_view_base_smart_set(api))
        return EINA_FALSE;

    if (EINA_UNLIKELY(!_parent_sc.sc.add))
        ewk_view_base_smart_set(&_parent_sc);

    api->sc.resize = _ewk_view_single_smart_resize;

    api->backing_store_add = _ewk_view_single_smart_backing_store_add;
    api->scrolls_process = _ewk_view_single_smart_scrolls_process;
    api->repaints_process = _ewk_view_single_smart_repaints_process;
    api->zoom_weak_set = _ewk_view_single_smart_zoom_weak_set;
    api->zoom_weak_smooth_scale_set = _ewk_view_single_smart_zoom_weak_smooth_scale_set;
    api->bg_color_set = _ewk_view_single_smart_bg_color_set;

    return EINA_TRUE;
}

static inline Evas_Smart *_ewk_view_single_smart_class_new(void)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Ewk_View_Single");
    static Evas_Smart *smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_single_smart_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

/**
 * Creates a new EFL WebKit View object.
 *
 * View objects are the recommended way to deal with EFL WebKit as it
 * abstracts the complex pieces of the process.
 *
 * Each view is composed by a set of frames. The set has at least one
 * frame, called 'main_frame'. See ewk_view_frame_main_get() and
 * ewk_view_frame_focused_get().
 *
 * @param e canvas where to create the view object.
 *
 * @return view object or @c NULL if errors.
 *
 * @see ewk_view_uri_set()
 */
Evas_Object *ewk_view_single_add(Evas *e)
{
    return evas_object_smart_add(e, _ewk_view_single_smart_class_new());
}
