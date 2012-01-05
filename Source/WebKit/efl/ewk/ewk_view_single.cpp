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

static void _ewk_view_single_on_del(void* data, Evas*, Evas_Object*, void*)
{
    Evas_Object* clip = (Evas_Object*)data;
    evas_object_del(clip);
}

static void _ewk_view_single_smart_add(Evas_Object* ewkView)
{
    Ewk_View_Smart_Data* smartData;

    _parent_sc.sc.add(ewkView);

    smartData = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkView));
    if (!smartData)
        return;

    Evas_Object* clip = evas_object_rectangle_add(smartData->base.evas);
    evas_object_show(clip);

    evas_object_event_callback_add
        (smartData->backing_store, EVAS_CALLBACK_DEL, _ewk_view_single_on_del, clip);
}

static Evas_Object* _ewk_view_single_smart_backing_store_add(Ewk_View_Smart_Data* smartData)
{
    Evas_Object* bs = evas_object_image_add(smartData->base.evas);
    evas_object_image_alpha_set(bs, false);
    evas_object_image_smooth_scale_set(bs, smartData->zoom_weak_smooth_scale);

    return bs;
}

static void _ewk_view_single_smart_resize(Evas_Object* ewkView, Evas_Coord width, Evas_Coord height)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(evas_object_smart_data_get(ewkView));
    _parent_sc.sc.resize(ewkView, width, height);

    if (!smartData)
        return;

    // these should be queued and processed in calculate as well!
    evas_object_image_size_set(smartData->backing_store, width, height);
    if (smartData->animated_zoom.zoom.current < 0.00001) {
        Evas_Object* clip = evas_object_clip_get(smartData->backing_store);
        Evas_Coord x, y, cw, ch;
        evas_object_image_fill_set(smartData->backing_store, 0, 0, width, height);
        evas_object_geometry_get(smartData->backing_store, &x, &y, 0, 0);
        evas_object_move(clip, x, y);
        ewk_frame_contents_size_get(smartData->main_frame, &cw, &ch);
        if (width > cw)
            width = cw;
        if (height > ch)
            height = ch;
        evas_object_resize(clip, width, height);
    }
}

static inline void _ewk_view_4b_move_region_up(uint32_t* image, size_t rows, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* source;
    uint32_t* destination;

    destination = image + x + y * rowSize;
    source = destination + rows * rowSize;
    height -= rows;

    for (; height > 0; height--, destination += rowSize, source += rowSize)
        memcpy(destination, source, width * 4);
}

static inline void _ewk_view_4b_move_region_down(uint32_t* image, size_t rows, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* source;
    uint32_t* destination;

    height -= rows;
    source = image + x + (y + height - 1) * rowSize;
    destination = source + rows * rowSize;

    for (; height > 0; height--, destination -= rowSize, source -= rowSize)
        memcpy(destination, source, width * 4);
}

static inline void _ewk_view_4b_move_line_left(uint32_t* destination, const uint32_t* source, size_t count)
{
    uint32_t* endOfDestination = destination + count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; destination < endOfDestination; destination++, source++)
        *destination = *source;
}

static inline void _ewk_view_4b_move_line_right(uint32_t* destination, uint32_t* source, size_t count)
{
    uint32_t* endOfDestination = destination - count;
    /* no memcpy() as it does not allow overlapping regions */
    /* no memmove() as it will copy to a temporary buffer */
    /* TODO: loop unrolling, copying up to quad-words would help */
    for (; destination > endOfDestination; destination--, source--)
        *destination = *source;
}

static inline void _ewk_view_4b_move_region_left(uint32_t* image, size_t columns, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* source;
    uint32_t* destination;

    destination = image + x + y * rowSize;
    source = destination + columns;
    width -= columns;

    for (; height > 0; height--, destination += rowSize, source += rowSize)
        _ewk_view_4b_move_line_left(destination, source, width);
}

static inline void _ewk_view_4b_move_region_right(uint32_t* image, size_t columns, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* source;
    uint32_t* destination;

    width -= columns;
    source = image + (x + width - 1) + y * rowSize;
    destination = source + columns;

    for (; height > 0; height--, destination += rowSize, source += rowSize)
        _ewk_view_4b_move_line_right(destination, source, width);
}

/* catch-all function, not as optimized as the others, but does the work. */
static inline void _ewk_view_4b_move_region(uint32_t* image, int deltaX, int deltaY, size_t x, size_t y, size_t width, size_t height, size_t rowSize)
{
    uint32_t* source;
    uint32_t* destination;

    if (deltaY < 0) {
        height += deltaY;
        destination = image + x + y * rowSize;
        source = destination - deltaY * rowSize;
        if (deltaX <= 0) {
            width += deltaX;
            source -= deltaX;
            for (; height > 0; height--, destination += rowSize, source += rowSize)
                _ewk_view_4b_move_line_left(destination, source, width);
        } else {
            width -= deltaX;
            source += width - 1;
            destination += width + deltaX -1;
            for (; height > 0; height--, destination += rowSize, source += rowSize)
                _ewk_view_4b_move_line_right(destination, source, width);
        }
    } else {
        height -= deltaY;
        source = image + x + (y + height - 1) * rowSize;
        destination = source + deltaY * rowSize;
        if (deltaX <= 0) {
            width += deltaX;
            source -= deltaX;
            for (; height > 0; height--, destination -= rowSize, source -= rowSize)
                _ewk_view_4b_move_line_left(destination, source, width);
        } else {
            width -= deltaX;
            source += width - 1;
            destination += width + deltaX - 1;
            for (; height > 0; height--, destination -= rowSize, source -= rowSize)
                _ewk_view_4b_move_line_right(destination, source, width);
        }
    }
}

static inline void _ewk_view_single_scroll_process_single(Ewk_View_Smart_Data* smartData, void* pixels, Evas_Coord width, Evas_Coord height, const Ewk_Scroll_Request* scrollRequest)
{
    Evas_Coord scrollX, scrollY, scrollWidth, scrollHeight;

    DBG("%d,%d + %d,%d %+03d,%+03d, store: %p %dx%d",
        scrollRequest->x, scrollRequest->y, scrollRequest->w, scrollRequest->h, scrollRequest->dx, scrollRequest->dy, pixels, width, height);

    scrollX = scrollRequest->x;
    scrollY = scrollRequest->y;
    scrollWidth = scrollRequest->w;
    scrollHeight = scrollRequest->h;

    if (abs(scrollRequest->dx) >= scrollWidth || abs(scrollRequest->dy) >= scrollHeight) {
        /* doubt webkit would be so stupid... */
        DBG("full page scroll %+03d,%+03d. convert to repaint %d,%d + %dx%d",
            scrollRequest->dx, scrollRequest->dy, scrollX, scrollY, scrollWidth, scrollHeight);
        ewk_view_repaint_add(smartData->_priv, scrollX, scrollY, scrollWidth, scrollHeight);
        return;
    }

    if (scrollX < 0) {
        scrollWidth += scrollX;
        scrollX = 0;
    }
    if (scrollY < 0) {
        scrollHeight += scrollY;
        scrollY = 0;
    }

    if (scrollX + scrollWidth > width)
        scrollWidth = width - scrollX;
    if (scrollY + scrollHeight > height)
        scrollHeight = height - scrollY;

    if (scrollWidth < 0)
        scrollWidth = 0;
    if (scrollHeight < 0)
        scrollHeight = 0;

    EINA_SAFETY_ON_TRUE_RETURN(!scrollWidth || !scrollHeight);
    if (!scrollRequest->dx) {
        if (scrollRequest->dy < 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, scrollX, scrollY, scrollWidth, scrollHeight + scrollRequest->dy,
                scrollX, scrollY + scrollHeight + scrollRequest->dy, scrollWidth, -scrollRequest->dy);

            _ewk_view_4b_move_region_up
                (static_cast<uint32_t*>(pixels), -scrollRequest->dy, scrollX, scrollY, scrollWidth, scrollHeight, width);
            evas_object_image_data_update_add
                (smartData->backing_store, scrollX, scrollY, scrollWidth, scrollHeight + scrollRequest->dy);

            ewk_view_repaint_add(smartData->_priv, scrollX, scrollY + scrollHeight + scrollRequest->dy, scrollWidth, -scrollRequest->dy);
        } else if (scrollRequest->dy > 0) {
            DBG("scroll down: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, scrollX, scrollY + scrollRequest->dy, scrollWidth, scrollHeight - scrollRequest->dy,
                scrollX, scrollY, scrollWidth, scrollRequest->dy);

            _ewk_view_4b_move_region_down
                (static_cast<uint32_t*>(pixels), scrollRequest->dy, scrollX, scrollY, scrollWidth, scrollHeight, width);
            evas_object_image_data_update_add
                (smartData->backing_store, scrollX, scrollY + scrollRequest->dy, scrollWidth, scrollHeight - scrollRequest->dy);

            ewk_view_repaint_add(smartData->_priv, scrollX, scrollY, scrollWidth, scrollRequest->dy);
        }
    } else if (!scrollRequest->dy) {
        if (scrollRequest->dx < 0) {
            DBG("scroll left: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, scrollX, scrollY, scrollWidth + scrollRequest->dx, scrollHeight,
                scrollX + scrollWidth + scrollRequest->dx, scrollY, -scrollRequest->dx, scrollHeight);

            _ewk_view_4b_move_region_left
                (static_cast<uint32_t*>(pixels), -scrollRequest->dx, scrollX, scrollY, scrollWidth, scrollHeight, width);
            evas_object_image_data_update_add
                (smartData->backing_store, scrollX, scrollY, scrollWidth + scrollRequest->dx, scrollHeight);

            ewk_view_repaint_add(smartData->_priv, scrollX + scrollWidth + scrollRequest->dx, scrollY, -scrollRequest->dx, scrollHeight);
        } else if (scrollRequest->dx > 0) {
            DBG("scroll up: %+03d,%+03d update=%d,%d+%dx%d, "
                "repaint=%d,%d+%dx%d",
                scrollRequest->dx, scrollRequest->dy, scrollX + scrollRequest->dx, scrollY, scrollWidth - scrollRequest->dx, scrollHeight,
                scrollX, scrollY, scrollRequest->dx, scrollHeight);

            _ewk_view_4b_move_region_right
                (static_cast<uint32_t*>(pixels), scrollRequest->dx, scrollX, scrollY, scrollWidth, scrollHeight, width);
            evas_object_image_data_update_add
                (smartData->backing_store, scrollX + scrollRequest->dx, scrollY, scrollWidth - scrollRequest->dx, scrollHeight);

            ewk_view_repaint_add(smartData->_priv, scrollX, scrollY, scrollRequest->dx, scrollHeight);
        }
    } else {
        Evas_Coord moveX, moveY, moveWidth, moveHeight;
        Evas_Coord verticalX, verticalY, verticalWidth, verticalHeight;
        Evas_Coord horizontalX, horizontalY, horizontalWidth, horizontalHeight;

        if (scrollRequest->dx < 0) {
            moveX = scrollX;
            moveWidth = scrollWidth + scrollRequest->dx;
            verticalX = moveX + moveWidth;
            verticalWidth = -scrollRequest->dx;
        } else {
            verticalX = scrollX;
            verticalWidth = scrollRequest->dx;
            moveX = verticalX + verticalWidth;
            moveWidth = scrollWidth - scrollRequest->dx;
        }

        if (scrollRequest->dy < 0) {
            moveY = scrollY;
            moveHeight = scrollHeight + scrollRequest->dy;
            horizontalY = moveY + moveHeight;
            horizontalHeight = -scrollRequest->dy;
        } else {
            horizontalY = scrollY;
            horizontalHeight = scrollRequest->dy;
            moveY = horizontalY + horizontalHeight;
            moveHeight = scrollHeight - scrollRequest->dy;
        }

        verticalY = moveY;
        verticalHeight = moveHeight;
        horizontalX = scrollX;
        horizontalWidth = scrollWidth;

        DBG("scroll diagonal: %+03d,%+03d update=%d,%d+%dx%d, "
            "repaints: h=%d,%d+%dx%d v=%d,%d+%dx%d",
            scrollRequest->dx, scrollRequest->dy, moveX, moveY, moveWidth, moveHeight,
            verticalX, verticalY, verticalWidth, verticalHeight,
            horizontalX, horizontalY, horizontalWidth, horizontalHeight);

        _ewk_view_4b_move_region
            (static_cast<uint32_t*>(pixels), scrollRequest->dx, scrollRequest->dy, scrollX, scrollY, scrollWidth, scrollHeight, width);

        evas_object_image_data_update_add(smartData->backing_store, moveX, moveY, moveWidth, moveHeight);
        ewk_view_repaint_add(smartData->_priv, verticalX, verticalY, verticalWidth, verticalHeight);
        ewk_view_repaint_add(smartData->_priv, horizontalX, horizontalY, horizontalWidth, horizontalHeight);
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

    return true;
}

static Eina_Bool _ewk_view_single_smart_repaints_process(Ewk_View_Smart_Data* smartData)
{
    Ewk_View_Paint_Context* context;
    Evas_Coord ow, oh;
    void* pixels;
    Eina_Rectangle* rect;
    const Eina_Rectangle* pr;
    const Eina_Rectangle* pr_end;
    Eina_Tiler* tiler;
    Eina_Iterator* iterator;
    cairo_status_t status;
    cairo_surface_t* surface;
    cairo_format_t format;
    cairo_t* cairo;
    size_t count;
    Eina_Bool result = true;

    if (smartData->animated_zoom.zoom.current < 0.00001) {
        Evas_Object* clip = evas_object_clip_get(smartData->backing_store);
        Evas_Coord width, height, centerWidth, centerHeight;
        // reset effects of zoom_weak_set()
        evas_object_image_fill_set
            (smartData->backing_store, 0, 0, smartData->view.w, smartData->view.h);
        evas_object_move(clip, smartData->view.x, smartData->view.y);

        width = smartData->view.w;
        height = smartData->view.h;

        ewk_frame_contents_size_get(smartData->main_frame, &centerWidth, &centerHeight);
        if (width > centerWidth)
            width = centerWidth;
        if (height > centerHeight)
            height = centerHeight;
        evas_object_resize(clip, width, height);
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
        result = false;
        goto error_cairo_surface;
    }
    cairo = cairo_create(surface);
    status = cairo_status(cairo);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("could not create cairo from surface %dx%d: %s",
            ow, oh, cairo_status_to_string(status));
        result = false;
        goto error_cairo;
    }

    context = ewk_view_paint_context_new(smartData->_priv, cairo);
    if (!context) {
        ERR("could not create paint context");
        result = false;
        goto error_paint_context;
    }

    tiler = eina_tiler_new(ow, oh);
    if (!tiler) {
        ERR("could not create tiler %dx%d", ow, oh);
        result = false;
        goto error_tiler;
    }

    ewk_view_layout_if_needed_recursive(smartData->_priv);

    pr = ewk_view_repaints_pop(smartData->_priv, &count);
    pr_end = pr + count;
    for (; pr < pr_end; pr++)
        eina_tiler_rect_add(tiler, pr);

    iterator = eina_tiler_iterator_new(tiler);
    if (!iterator) {
        ERR("could not get iterator for tiler");
        result = false;
        goto error_iterator;
    }

    int scrollX, scrollY;
    ewk_frame_scroll_pos_get(smartData->main_frame, &scrollX, &scrollY);

    EINA_ITERATOR_FOREACH(iterator, rect) {
        Eina_Rectangle scrolled_rect = {
            rect->x + scrollX, rect->y + scrollY,
            rect->w, rect->h
        };

        ewk_view_paint_context_save(context);

        if ((scrollX) || (scrollY))
            ewk_view_paint_context_translate(context, -scrollX, -scrollY);

        ewk_view_paint_context_clip(context, &scrolled_rect);
        ewk_view_paint_context_paint_contents(context, &scrolled_rect);

        ewk_view_paint_context_restore(context);
        evas_object_image_data_update_add
            (smartData->backing_store, rect->x, rect->y, rect->w, rect->h);
    }
    eina_iterator_free(iterator);

error_iterator:
    eina_tiler_free(tiler);
error_tiler:
    ewk_view_paint_context_free(context);
error_paint_context:
    cairo_destroy(cairo);
error_cairo:
    cairo_surface_destroy(surface);
error_cairo_surface:
    evas_object_image_data_set(smartData->backing_store, pixels); /* dec refcount */

    return result;
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
    return true;
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
        return false;

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

    return true;
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
