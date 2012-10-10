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

#include "ewk_private.h"
#include "ewk_util_private.h"
#include <eina_safety_checks.h>

/**
 * Converts an image from cairo_surface to the Evas_Object.
 *
 * @param canvas display canvas
 * @param surface cairo representation of an image
 * @return converted cairo_surface object to the Evas_Object
 */
Evas_Object* ewk_util_image_from_cairo_surface_add(Evas* canvas, cairo_surface_t* surface)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(surface, 0);

    cairo_status_t status = cairo_surface_status(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        ERR("cairo surface is invalid: %s", cairo_status_to_string(status));
        return 0;
    }

    cairo_surface_type_t type = cairo_surface_get_type(surface);
    if (type != CAIRO_SURFACE_TYPE_IMAGE) {
        ERR("unknown surface type %d, required %d (CAIRO_SURFACE_TYPE_IMAGE).",
            type, CAIRO_SURFACE_TYPE_IMAGE);
        return 0;
    }

    cairo_format_t format = cairo_image_surface_get_format(surface);
    if (format != CAIRO_FORMAT_ARGB32 && format != CAIRO_FORMAT_RGB24) {
        ERR("unknown surface format %d, expected %d or %d.",
            format, CAIRO_FORMAT_ARGB32, CAIRO_FORMAT_RGB24);
        return 0;
    }

    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    if (width <= 0 || height <= 0 || stride <= 0) {
        ERR("invalid image size %dx%d, stride=%d", width, height, stride);
        return 0;
    }

    void* data = cairo_image_surface_get_data(surface);
    if (!data) {
        ERR("could not get source data.");
        return 0;
    }

    Evas_Object* image = evas_object_image_filled_add(canvas);
    if (!image) {
        ERR("could not add image to canvas.");
        return 0;
    }

    evas_object_image_colorspace_set(image, EVAS_COLORSPACE_ARGB8888);
    evas_object_image_size_set(image, width, height);
    evas_object_image_alpha_set(image, format == CAIRO_FORMAT_ARGB32);

    if (evas_object_image_stride_get(image) != stride) {
        ERR("evas' stride %d diverges from cairo's %d.",
            evas_object_image_stride_get(image), stride);
        evas_object_del(image);
        return 0;
    }

    evas_object_image_data_copy_set(image, data);

    return image;
}
