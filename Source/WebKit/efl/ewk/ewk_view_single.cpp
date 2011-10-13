/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERchANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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
#include "ewk_private.h"

#include <Evas.h>
#include <eina_safety_checks.h>
#include <string.h>

static Ewk_View_Smart_Class _parent_sc = EWK_VIEW_SMART_CLASS_INIT_NULL;

static void _ewk_view_single_on_del(void* data, Evas* eventType, Evas_Object* callback, void* eventInfo)
{
    Evas_Object* clip = (Evas_Object*)data;
    evas_object_del(clip);
}

static void _ewk_view_single_smart_add(Evas_Object* ewkSingle)
{
    Ewk_View_Smart_Data* sd;

    _parent_sc.sc.add(ewkSingle);

    sd = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkSingle));
    if (!sd)
        return;

    Evas_Object* clip = evas_object_rectangle_add(sd->base.evas);
    evas_object_show(clip);

    evas_object_event_callback_add
        (sd->backing_store, EVAS_CALLBACK_DEL, _ewk_view_single_on_del, clip);
}

static Evas_Object* _ewk_view_single_smart_backing_store_add(Ewk_View_Smart_Data* smartData)
{
    Evas_Object* bs = evas_object_image_add(smartData->base.evas);
    evas_object_image_alpha_set(bs, EINA_FALSE);
    evas_object_image_smooth_scale_set(bs, smartData->zoom_weak_smooth_scale);

    return bs;
}

static void _ewk_view_single_smart_resize(Evas_Object* ewkSingle, Evas_Coord width, Evas_Coord height)
{
    Ewk_View_Smart_Data* sd = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkSingle));
    _parent_sc.sc.resize(ewkSingle, width, height);

    if (!sd)
        return;

    // these should be queued and processed in calculate as well!
    evas_object_image_size_set(sd->backing_store, width, height);
    if (sd->animated_zoom.zoom.current < 0.00001) {
        Evas_Object* clip = evas_object_clip_get(sd->backing_store);
        Evas_Coord x, y, cw, ch;
        evas_object_image_fill_set(sd->backing_store, 0, 0, width, height);
        evas_object_geometry_get(sd->backing_store, &x, &y, 0, 0);
        evas_object_move(clip, x, y);
        ewk_frame_contents_size_get(sd->main_frame, &cw, &ch);
        if (width > cw)
            width = cw;
        if (height > ch)
            height = ch;
        evas_object_resize(clip, width, height);
    }
}

static inline void _ewk_view_4b_move_region_up(uint32_t* image, size_t rows, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* src;
    uint32_t* dst;

    dst = image + x + y * rowSize;
    src = dst + rows * rowSize;
    height -= rows;

    for (; height > 0; height--, dst += rowSize, src += rowSize)
        memcpy(dst, src, width * 4);
}

static inline void _ewk_view_4b_move_region_down(uint32_t* image, size_t rows, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* src;
    uint32_t* dst;

    height -= rows;
    src = image + x + (y + height - 1) * rowSize;
    dst = src + rows * rowSize;

    for (; height > 0; height--, dst -= rowSize, src -= rowSize)
        memcpy(dst, src, width * 4);
}

static inline void _ewk_view_4b_move_line_left(uint32_t* dst, const uint32_t* src, size_t count)
{
    uint32_t* dst_end = dst + count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; dst < dst_end; dst++, src++)
        *dst = *src;
}

static inline void _ewk_view_4b_move_line_right(uint32_t* dst, uint32_t* src, size_t count)
{
    uint32_t* dst_end = dst - count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; dst > dst_end; dst--, src--)
        *dst = *src;
}

static inline void _ewk_view_4b_move_region_left(uint32_t* image, size_t cols, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* src;
    uint32_t* dst;

    dst = image + x + y * rowSize;
    src = dst + cols;
    width -= cols;

    for (; height > 0; height--, dst += rowSize, src += rowSize)
        _ewk_view_4b_move_line_left(dst, src, width);
}

static inline void _ewk_view_4b_move_region_right(uint32_t* image, size_t columns, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* src;
    uint32_t* dst;

    width -= columns;
    src = image + (x + width - 1) + y * rowSize;
    dst = src + columns;

    for (; height > 0; height--, dst += rowSize, src += rowSize)
        _ewk_view_4b_move_line_right(dst, src, width);
}

/* catch-all function, not as optimized as the others, but does the work. */
static inline void _ewk_view_4b_move_region(uint32_t* image, int deltaX, int deltaY, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* src;
    uint32_t* dst;

    if (deltaY < 0) {
        height += deltaY;
        dst = image + x + y * rowSize;
        src = dst - deltaY * rowSize;
        if (deltaX <= 0) {
            width += deltaX;
            src -= deltaX;
            for (; height > 0; height--, dst += rowSize, src += rowSize)
                _ewk_view_4b_move_line_left(dst, src, width);
        } else {
            width -= deltaX;
            src += width - 1;
            dst += width + deltaX -1;
            for (; height > 0; height--, dst += rowSize, src += rowSize)
                _ewk_view_4b_move_line_right(dst, src, width);
        }
    } else {
        height -= deltaY;
        src = image + x + (y + height - 1) * rowSize;
        dst = src + deltaY * rowSize;
        if (deltaX <= 0) {
            width += deltaX;
            src -= deltaX;
            for (; height > 0; height--, dst -= rowSize, src -= rowSize)
                _ewk_view_4b_move_line_left(dst, src, width);
        } else {
            width -= deltaX;
            src += width - 1;
            dst += width + deltaX - 1;
            for (; height > 0; height--, dst -= rowSize, src -= rowSize)
                _ewk_view_4b_move_line_right(dst, src, width);
        }
    }
}

static inline void _ewk_view_single_scroll_process_single(Ewk_View_Smart_Data* smartData, void* pixels, Evas_Coord width, Evas_Coord height, const Ewk_Scroll_Request* scrollRequest)
{
    Evas_Coord sx, sy, sw, sh;

    DBG("%d,%d + %d,%d %+03d,%+03d, store: %p %dx%d",
        scrollRequest->x, scrollRequest->y, scrollRequest->w, scrollRequest->h, scrollRequest->dx, scrollRequest->dy, pixels, width, height);

    sx = scrollRequest->x;
    sy = scrollRequest->y;
    sw = scrollRequest->w;
    sh = scrollRequest->h;

    if (abs(scrollRequest->dx) >= sw || abs(scrollRequest->dy) >= sh) {
        /* doubt webkit would be so stupid... */
        DBG("full page scroll %+03d,%+03d. convert to repaint %d,%d + %dx%d",
            scrollRequest->dx, scrollRequest->dy, sx, sy, sw, sh);
        ewk_view_repaint_add(smartData->_priv, sx, sy, sw, sh);
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

    if (sx + sw > width)
        sw = width - sx;
    if (sy + sh > height)
        sh = height - sy;

    if (sw < 0)
        sw = 0;
    if (sh < 0)
        sh = 0;

    EINA_SAFETY_ON_TRUE_RETURN(!sw || !sh);
    if (!scrollRequest->dx) {
        if (scrollRequest->dy < 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, sx, sy, sw, sh + scrollRequest->dy,
                sx, sy + sh + scrollRequest->dy, sw, -scrollRequest->dy);

            _ewk_view_4b_move_region_up
                (static_cast<uint32_t*>(pixels), -scrollRequest->dy, sx, sy, sw, sh, width);
            evas_object_image_data_update_add
                (smartData->backing_store, sx, sy, sw, sh + scrollRequest->dy);

            ewk_view_repaint_add(smartData->_priv, sx, sy + sh + scrollRequest->dy, sw, -scrollRequest->dy);
        } else if (scrollRequest->dy > 0) {
            DBG("scroll down: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, sx, sy + scrollRequest->dy, sw, sh - scrollRequest->dy,
                sx, sy, sw, scrollRequest->dy);

            _ewk_view_4b_move_region_down
                (static_cast<uint32_t*>(pixels), scrollRequest->dy, sx, sy, sw, sh, width);
            evas_object_image_data_update_add
                (smartData->backing_store, sx, sy + scrollRequest->dy, sw, sh - scrollRequest->dy);

            ewk_view_repaint_add(smartData->_priv, sx, sy, sw, scrollRequest->dy);
        }
    } else if (!scrollRequest->dy) {
        if (scrollRequest->dx < 0) {
            DBG("scroll left: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, sx, sy, sw + scrollRequest->dx, sh,
                sx + sw + scrollRequest->dx, sy, -scrollRequest->dx, sh);

            _ewk_view_4b_move_region_left
                (static_cast<uint32_t*>(pixels), -scrollRequest->dx, sx, sy, sw, sh, width);
            evas_object_image_data_update_add
                (smartData->backing_store, sx, sy, sw + scrollRequest->dx, sh);

            ewk_view_repaint_add(smartData->_priv, sx + sw + scrollRequest->dx, sy, -scrollRequest->dx, sh);
        } else if (scrollRequest->dx > 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, sx + scrollRequest->dx, sy, sw - scrollRequest->dx, sh,
                sx, sy, scrollRequest->dx, sh);

            _ewk_view_4b_move_region_right
                (static_cast<uint32_t*>(pixels), scrollRequest->dx, sx, sy, sw, sh, width);
            evas_object_image_data_update_add
                (smartData->backing_store, sx + scrollRequest->dx, sy, sw - scrollRequest->dx, sh);

            ewk_view_repaint_add(smartData->_priv, sx, sy, scrollRequest->dx, sh);
        }
    } else {
        Evas_Coord mx, my, mw, mh, ax, ay, aw, ah, bx, by, bw, bh;

        if (scrollRequest->dx < 0) {
            mx = sx;
            mw = sw + scrollRequest->dx;
            ax = mx + mw;
            aw = -scrollRequest->dx;
        } else {
            ax = sx;
            aw = scrollRequest->dx;
            mx = ax + aw;
            mw = sw - scrollRequest->dx;
        }

        if (scrollRequest->dy < 0) {
            my = sy;
            mh = sh + scrollRequest->dy;
            by = my + mh;
            bh = -scrollRequest->dy;
        } else {
            by = sy;
            bh = scrollRequest->dy;
            my = by + bh;
            mh = sh - scrollRequest->dy;
        }

        ay = my;
        ah = mh;
        bx = sx;
        bw = sw;

        DBG("scroll diagonal: %+03d,%+03d update=%d,%d+%dx%d, "
            "repaints: h=%d,%d+%dx%d v=%d,%d+%dx%d",
            scrollRequest->dx, scrollRequest->dy, mx, my, mw, mh, ax, ay, aw, ah, bx, by, bw, bh);

        _ewk_view_4b_move_region
            (static_cast<uint32_t*>(pixels), scrollRequest->dx, scrollRequest->dy, sx, sy, sw, sh, width);

        evas_object_image_data_update_add(smartData->backing_store, mx, my, mw, mh);
        ewk_view_repaint_add(smartData->_priv, ax, ay, aw, ah);
        ewk_view_repaint_add(smartData->_priv, bx, by, bw, bh);
    }
}

static Eina_Bool _ewk_view_single_smart_scrolls_process(Ewk_View_Smart_Data* smartData)
{
    const Ewk_Scroll_Request* sr;
    const Ewk_Scroll_Request* sr_end;
    Evas_Coord ow, oh;
    size_t count;
    void* pixels = evas_object_image_data_get(smartData->backing_store, 1);
    evas_object_image_size_get(smartData->backing_store, &ow, &oh);

    sr = ewk_view_scroll_requests_get(smartData->_priv, &count);
    sr_end = sr + count;
    for (; sr < sr_end; sr++)
        _ewk_view_single_scroll_process_single(smartData, pixels, ow, oh, sr);

    evas_object_image_data_set(smartData->backing_store, pixels);

    return EINA_TRUE;
}

static Eina_Bool _ewk_view_single_smart_repaints_process(Ewk_View_Smart_Data* smartData)
{
    Ewk_View_Paint_Context* ctxt;
    Evas_Coord ow, oh;
    void* pixels;
    Eina_Rectangle* r;
    const Eina_Rectangle* pr;
    const Eina_Rectangle* pr_end;
    Eina_Tiler* tiler;
    Eina_Iterator* itr;
    cairo_status_t status;
    cairo_surface_t* surface;
    cairo_format_t format;
    cairo_t* cairo;
    size_t count;
    Eina_Bool ret = EINA_TRUE;

    if (smartData->animated_zoom.zoom.current < 0.00001) {
        Evas_Object* clip = evas_object_clip_get(smartData->backing_store);
        Evas_Coord w, h, cw, ch;
        // reset effects of zoom_weak_set()
        evas_object_image_fill_set
            (smartData->backing_store, 0, 0, smartData->view.w, smartData->view.h);
        evas_object_move(clip, smartData->view.x, smartData->view.y);

        w = smartData->view.w;
        h = smartData->view.h;

        ewk_frame_contents_size_get(smartData->main_frame, &cw, &ch);
        if (w > cw)
            w = cw;
        if (h > ch)
            h = ch;
        evas_object_resize(clip, w, h);
    }

    pixels = evas_object_image_data_get(smartData->backing_store, 1);
    evas_object_image_size_get(smartData->backing_store, &ow, &oh);
    format = CAIRO_FORMAT_ARGB32;

    surface = cairo_image_surface_create_for_data
                  (static_cast<unsigned char*>(pixels), format, ow, oh, ow * 4);
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

    ctxt = ewk_view_paint_context_new(smartData->_priv, cairo);
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

    pr = ewk_view_repaints_get(smartData->_priv, &count);
    pr_end = pr + count;
    for (; pr < pr_end; pr++)
        eina_tiler_rect_add(tiler, pr);

    itr = eina_tiler_iterator_new(tiler);
    if (!itr) {
        ERR("could not get iterator for tiler");
        ret = EINA_FALSE;
        goto error_iterator;
    }

    ewk_view_layout_if_needed_recursive(smartData->_priv);

    int sx, sy;
    ewk_frame_scroll_pos_get(smartData->main_frame, &sx, &sy);

    EINA_ITERATOR_FOREACH(itr, r) {
        Eina_Rectangle scrolled_rect = {
            r->x + sx, r->y + sy,
            r->w, r->h
        };

        ewk_view_paint_context_save(ctxt);

        if ((sx) || (sy))
            ewk_view_paint_context_translate(ctxt, -sx, -sy);

        ewk_view_paint_context_clip(ctxt, &scrolled_rect);
        ewk_view_paint_context_paint_contents(ctxt, &scrolled_rect);

        ewk_view_paint_context_restore(ctxt);
        evas_object_image_data_update_add
            (smartData->backing_store, r->x, r->y, r->w, r->h);
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
    evas_object_image_data_set(smartData->backing_store, pixels); /* dec refcount */

    return ret;
}

static Eina_Bool _ewk_view_single_smart_zoom_weak_set(Ewk_View_Smart_Data* smartData, float zoom, Evas_Coord centerX, Evas_Coord centerY)
{
    // TODO: review
    float scale = zoom / smartData->animated_zoom.zoom.start;
    Evas_Coord w = smartData->view.w * scale;
    Evas_Coord h = smartData->view.h * scale;
    Evas_Coord dx, dy, cw, ch;
    Evas_Object* clip = evas_object_clip_get(smartData->backing_store);

    ewk_frame_contents_size_get(smartData->main_frame, &cw, &ch);
    if (smartData->view.w > 0 && smartData->view.h > 0) {
        dx = (w * (smartData->view.w - centerX)) / smartData->view.w;
        dy = (h * (smartData->view.h - centerY)) / smartData->view.h;
    } else {
        dx = 0;
        dy = 0;
    }

    evas_object_image_fill_set(smartData->backing_store, centerX + dx, centerY + dy, w, h);

    if (smartData->view.w > 0 && smartData->view.h > 0) {
        dx = ((smartData->view.w - w) * centerX) / smartData->view.w;
        dy = ((smartData->view.h - h) * centerY) / smartData->view.h;
    } else {
        dx = 0;
        dy = 0;
    }
    evas_object_move(clip, smartData->view.x + dx, smartData->view.y + dy);

    if (cw < smartData->view.w)
        w = cw * scale;
    if (ch < smartData->view.h)
        h = ch * scale;
    evas_object_resize(clip, w, h);
    return EINA_TRUE;
}

static void _ewk_view_single_smart_zoom_weak_smooth_scale_set(Ewk_View_Smart_Data* smartData, Eina_Bool smooth_scale)
{
    evas_object_image_smooth_scale_set(smartData->backing_store, smooth_scale);
}

static void _ewk_view_single_smart_bg_color_set(Ewk_View_Smart_Data* smartData, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
    evas_object_image_alpha_set(smartData->backing_store, alpha < 255);
}

Eina_Bool ewk_view_single_smart_set(Ewk_View_Smart_Class* api)
{
    if (!ewk_view_base_smart_set(api))
        return EINA_FALSE;

    if (EINA_UNLIKELY(!_parent_sc.sc.add))
        ewk_view_base_smart_set(&_parent_sc);

    api->sc.add = _ewk_view_single_smart_add;
    api->sc.resize = _ewk_view_single_smart_resize;

    api->backing_store_add = _ewk_view_single_smart_backing_store_add;
    api->scrolls_process = _ewk_view_single_smart_scrolls_process;
    api->repaints_process = _ewk_view_single_smart_repaints_process;
    api->zoom_weak_set = _ewk_view_single_smart_zoom_weak_set;
    api->zoom_weak_smooth_scale_set = _ewk_view_single_smart_zoom_weak_smooth_scale_set;
    api->bg_color_set = _ewk_view_single_smart_bg_color_set;

    return EINA_TRUE;
}

static inline Evas_Smart* _ewk_view_single_smart_class_new(void)
{
    static Ewk_View_Smart_Class api = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Ewk_View_Single");
    static Evas_Smart* smart = 0;

    if (EINA_UNLIKELY(!smart)) {
        ewk_view_single_smart_set(&api);
        smart = evas_smart_class_new(&api.sc);
    }

    return smart;
}

Evas_Object* ewk_view_single_add(Evas* canvas)
{
    return evas_object_smart_add(canvas, _ewk_view_single_smart_class_new());
}
