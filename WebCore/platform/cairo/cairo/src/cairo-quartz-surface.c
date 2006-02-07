/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *    Calum Robinson <calumr@mac.com>
 */

#include "cairoint.h"
#include "cairo-private.h"
#include "cairo-quartz-private.h"

static void
ImageDataReleaseFunc(void *info, const void *data, size_t size)
{
    if (data != NULL) {
        free((void *) data);
    }
}

static cairo_status_t
_cairo_quartz_surface_finish(void *abstract_surface)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->image)
        cairo_surface_destroy(&surface->image->base);

    if (surface->cgImage)
        CGImageRelease(surface->cgImage);

	if (surface->clip_region)
		pixman_region_destroy (surface->clip_region);
		
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_source_image(void *abstract_surface,
					   cairo_image_surface_t **image_out,
					   void **image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    CGColorSpaceRef colorSpace;
    void *imageData;
    UInt32 imageDataSize, rowBytes;
    CGDataProviderRef dataProvider;

    // We keep a cached (cairo_image_surface_t *) in the cairo_quartz_surface_t
    // struct. If the window is ever drawn to without going through Cairo, then
    // we would need to refetch the pixel data from the window into the cached
    // image surface. 
    if (surface->image) {
        cairo_surface_reference(&surface->image->base);

	*image_out = surface->image;
	return CAIRO_STATUS_SUCCESS;
    }

    colorSpace = CGColorSpaceCreateDeviceRGB();


    rowBytes = surface->width * 4;
    imageDataSize = rowBytes * surface->height;
    imageData = malloc(imageDataSize);

    dataProvider =
        CGDataProviderCreateWithData(NULL, imageData, imageDataSize,
                                     ImageDataReleaseFunc);

    surface->cgImage = CGImageCreate(surface->width,
                                     surface->height,
                                     8,
                                     32,
                                     rowBytes,
                                     colorSpace,
                                     kCGImageAlphaPremultipliedFirst,
                                     dataProvider,
                                     NULL,
                                     false, kCGRenderingIntentDefault);

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(dataProvider);

    surface->image = (cairo_image_surface_t *)
        cairo_image_surface_create_for_data(imageData,
                                            CAIRO_FORMAT_ARGB32,
                                            surface->width,
                                            surface->height, rowBytes);
    if (surface->image->base.status) {
	if (surface->cgImage)
	    CGImageRelease(surface->cgImage);
	return CAIRO_STATUS_NO_MEMORY;
    }

    *image_out = surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartz_surface_acquire_dest_image(void *abstract_surface,
                                         cairo_rectangle_t * interest_rect,
                                         cairo_image_surface_t **
                                         image_out,
                                         cairo_rectangle_t * image_rect,
                                         void **image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    image_rect->x = 0;
    image_rect->y = 0;
    image_rect->width = surface->image->width;
    image_rect->height = surface->image->height;

    *image_out = surface->image;
    if (image_extra)
	*image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_quartz_surface_release_dest_image(void *abstract_surface,
                                         cairo_rectangle_t *
                                         intersect_rect,
                                         cairo_image_surface_t * image,
                                         cairo_rectangle_t * image_rect,
                                         void *image_extra)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    if (surface->image == image) {
        CGRect rect;

        rect = CGRectMake(0, 0, surface->width, surface->height);

	if (surface->flipped) {
	    CGContextSaveGState (surface->context);
	    CGContextTranslateCTM (surface->context, 0, surface->height);
	    CGContextScaleCTM (surface->context, 1, -1);
	}

        CGContextDrawImage(surface->context, rect, surface->cgImage);

	if (surface->flipped)
	    CGContextRestoreGState (surface->context);

	memset(surface->image->data, 0, surface->width * surface->height * 4);
    }
}

static cairo_int_status_t
_cairo_quartz_surface_set_clip_region(void *abstract_surface,
                                      pixman_region16_t * region)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    unsigned int serial;

    serial = _cairo_surface_allocate_clip_serial (&surface->image->base);

	if (surface->clip_region)
		pixman_region_destroy (surface->clip_region);
		
	if (region) {
		surface->clip_region = pixman_region_create ();
		pixman_region_copy (surface->clip_region, region);
	} else
		surface->clip_region = NULL;
		
    return _cairo_surface_set_clip_region(&surface->image->base,
					  region, serial);
}

static cairo_int_status_t
_cairo_quartz_surface_get_extents (void *abstract_surface,
				   cairo_rectangle_t * rectangle)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    NULL, /* create_similar */
    _cairo_quartz_surface_finish,
    _cairo_quartz_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_quartz_surface_acquire_dest_image,
    _cairo_quartz_surface_release_dest_image,
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_quartz_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_quartz_surface_get_extents,
    NULL  /* old_show_glyphs */
};


cairo_surface_t *cairo_quartz_surface_create(CGContextRef context,
					     cairo_bool_t flipped,
                                             int width, int height)
{
    cairo_quartz_surface_t *surface;

    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend);

    surface->context = context;
    surface->width = width;
    surface->height = height;
    surface->image = NULL;
    surface->cgImage = NULL;
	surface->clip_region = NULL;
    surface->flipped = flipped;

    // Set up the image surface which Cairo draws into and we blit to & from.
    void *foo;
    _cairo_quartz_surface_acquire_source_image(surface, &surface->image, &foo);

    return (cairo_surface_t *) surface;
}

int
_cairo_surface_is_quartz (cairo_surface_t *surface)
{
    return surface->backend == &cairo_quartz_surface_backend;
}
