/* vim:set ts=8 sw=4 noet cin: */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Christian Biesinger <cbiesinger@web.de>
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
 * The Initial Developer of the Original Code is Christian Biesinger
 * <cbiesinger@web.de>
 *
 * Contributor(s):
 */

// This is a C++ file in order to use the C++ BeOS API

#include <new>

#include <Bitmap.h>
#include <Region.h>
#if 0
#include <DirectWindow.h>
#endif
#include <Screen.h>
#include <Window.h>
#include <Locker.h>

#include "cairoint.h"
#include "cairo-beos.h"

#define CAIRO_INT_STATUS_SUCCESS (cairo_int_status_t)(CAIRO_STATUS_SUCCESS)

struct cairo_beos_surface_t {
    cairo_surface_t base;

    BView* view;

    /*
     * A view is either attached to a bitmap, a window, or unattached.
     * If it is attached to a window, we can copy data out of it using BScreen.
     * If it is attached to a bitmap, we can read the bitmap data.
     * If it is not attached, it doesn't draw anything, we need not bother.
     *
     * Since there doesn't seem to be a way to get the bitmap from a view if it
     * is attached to one, we have to use a special surface creation function.
     */

    BBitmap* bitmap;


};

class AutoLockView {
    public:
	AutoLockView(BView* view) : mView(view) {
	    mOK = mView->LockLooper();
	}

	~AutoLockView() {
	    if (mOK)
		mView->UnlockLooper();
	}

	operator bool() {
	    return mOK;
	}

    private:
	BView* mView;
	bool   mOK;
};

static BRect
_cairo_rect_to_brect (const cairo_rectangle_t* rect)
{
    // A BRect is one pixel wider than you'd think
    return BRect(rect->x, rect->y, rect->x + rect->width - 1,
	    	 rect->y + rect->height - 1);
}

static cairo_rectangle_t
_brect_to_cairo_rect (const BRect& rect)
{
    cairo_rectangle_t retval;
    retval.x = int(rect.left + 0.5);
    retval.y = int(rect.top + 0.5);
    retval.width = rect.IntegerWidth() + 1;
    retval.height = rect.IntegerHeight() + 1;
    return retval;
}

static rgb_color
_cairo_color_to_be_color (const cairo_color_t* color)
{
    // This factor ensures a uniform distribution of numbers
    const float factor = 256 - 1e-5;
    // Using doubles to have non-premultiplied colors
    rgb_color be_color = { uint8(color->red * factor),
			   uint8(color->green * factor),
			   uint8(color->blue * factor),
			   uint8(color->alpha * factor) };

    return be_color;
}

enum ViewCopyStatus {
    OK,
    NOT_VISIBLE, // The view or the interest rect is not visible on screen
    ERROR        // The view was visible, but the rect could not be copied. Probably OOM
};

/**
 * _cairo_beos_view_to_bitmap:
 * @bitmap: [out] The resulting bitmap.
 * @rect: [out] The rectangle that was copied, in the view's coordinate system
 * @interestRect: If non-null, only this part of the view will be copied (view's coord system).
 *
 * Gets the contents of the view as a BBitmap*. Caller must delete the bitmap.
 **/
static ViewCopyStatus
_cairo_beos_view_to_bitmap (BView*       view,
			    BBitmap**    bitmap,
			    BRect*       rect = NULL,
			    const BRect* interestRect = NULL)
{
    *bitmap = NULL;

    BWindow* wnd = view->Window();
    // If we have no window, can't do anything
    if (!wnd)
	return NOT_VISIBLE;

    view->Sync();
    wnd->Sync();

#if 0
    // Is it a direct window?
    BDirectWindow* directWnd = dynamic_cast<BDirectWindow*>(wnd);
    if (directWnd) {
	// WRITEME
    }
#endif

    // Is it visible? If so, we can copy the content off the screen
    if (wnd->IsHidden())
	return NOT_VISIBLE;

    BRect rectToCopy(view->Bounds());
    if (interestRect)
	rectToCopy = rectToCopy & *interestRect;

    if (!rectToCopy.IsValid())
	return NOT_VISIBLE;

    BScreen screen(wnd);
    BRect screenRect(view->ConvertToScreen(rectToCopy));
    screenRect = screenRect & screen.Frame();

    if (!screen.IsValid())
	return NOT_VISIBLE;

    if (rect)
	*rect = view->ConvertFromScreen(screenRect);

    if (screen.GetBitmap(bitmap, false, &screenRect) == B_OK)
	return OK;

    return ERROR;
}

inline unsigned char
unpremultiply (unsigned char color,
	       unsigned char alpha)
{
    if (alpha == 0)
	return 0;
    // plus alpha/2 to round instead of truncate
    return (color * 255 + alpha / 2) / alpha;
}

inline unsigned char
premultiply (unsigned char color,
	     unsigned char alpha)
{
    // + 127 to round, instead of truncate
    return (color * alpha + 127) / 255;
}

/**
 * unpremultiply_rgba:
 *
 * Takes an input in ABGR premultiplied image data and unmultiplies it.
 * The result is stored in retdata.
 **/
static void
unpremultiply_rgba (unsigned char* data,
		    int            width,
		    int            height,
		    int            stride,
		    unsigned char* retdata)
{
    unsigned char* end = data + stride * height;
    for (unsigned char* in = data, *out = retdata;
	 in < end;
	 in += stride, out += stride)
    {
	for (int i = 0; i < width; ++i) {
	    // XXX for a big-endian platform this'd have to change
	    int idx = 4 * i;
	    unsigned char alpha = in[idx + 3];
	    out[idx + 0] = unpremultiply(in[idx + 0], alpha); // B
	    out[idx + 1] = unpremultiply(in[idx + 1], alpha); // G
	    out[idx + 2] = unpremultiply(in[idx + 2], alpha); // R
	    out[idx + 3] = in[idx + 3]; // Alpha
	}
    }
}

/**
 * premultiply_rgba:
 *
 * Takes an input in ABGR non-premultiplied image data and premultiplies it.
 * The returned data must be freed with free().
 **/
static unsigned char*
premultiply_rgba (unsigned char* data,
		  int            width,
		  int            height,
		  int            stride)
{
    unsigned char* retdata = reinterpret_cast<unsigned char*>(malloc(stride * height));
    if (!retdata)
	return NULL;

    unsigned char* end = data + stride * height;
    for (unsigned char* in = data, *out = retdata;
	 in < end;
	 in += stride, out += stride)
    {
	for (int i = 0; i < width; ++i) {
	    // XXX for a big-endian platform this'd have to change
	    int idx = 4 * i;
	    unsigned char alpha = in[idx + 3];
	    out[idx + 0] = premultiply(in[idx + 0], alpha); // B
	    out[idx + 1] = premultiply(in[idx + 1], alpha); // G
	    out[idx + 2] = premultiply(in[idx + 2], alpha); // R
	    out[idx + 3] = in[idx + 3]; // Alpha
	}
    }
    return retdata;
}

/**
 * _cairo_beos_bitmap_to_surface:
 *
 * Returns an addrefed image surface for a BBitmap. The bitmap need not outlive
 * the surface.
 **/
static cairo_image_surface_t*
_cairo_beos_bitmap_to_surface (BBitmap* bitmap)
{
    color_space format = bitmap->ColorSpace();
    if (format != B_RGB32 && format != B_RGBA32) {
	BBitmap bmp(bitmap->Bounds(), B_RGB32, true);
	BView view(bitmap->Bounds(), "Cairo bitmap drawing view",
		   B_FOLLOW_ALL_SIDES, 0);
	bmp.AddChild(&view);

	view.LockLooper();

	view.DrawBitmap(bitmap, BPoint(0.0, 0.0));
	view.Sync();

	cairo_image_surface_t* imgsurf = _cairo_beos_bitmap_to_surface(&bmp);

	view.UnlockLooper();
	bmp.RemoveChild(&view);
	return imgsurf;
    }

    cairo_format_t cformat = format == B_RGB32 ? CAIRO_FORMAT_RGB24
						: CAIRO_FORMAT_ARGB32;

    BRect bounds(bitmap->Bounds());
    unsigned char* bits = reinterpret_cast<unsigned char*>(bitmap->Bits());
    int width = bounds.IntegerWidth() + 1;
    int height = bounds.IntegerHeight() + 1;
    unsigned char* premultiplied;
    if (cformat == CAIRO_FORMAT_ARGB32) {
       premultiplied = premultiply_rgba(bits, width, height,
					bitmap->BytesPerRow());
    } else {
	premultiplied = reinterpret_cast<unsigned char*>(
					malloc(bitmap->BytesPerRow() * height));
	if (premultiplied)
	    memcpy(premultiplied, bits, bitmap->BytesPerRow() * height);
    }
    if (!premultiplied)
	return NULL;

    cairo_image_surface_t* surf = reinterpret_cast<cairo_image_surface_t*>
	(cairo_image_surface_create_for_data(premultiplied,
					     cformat,
					     width,
					     height,
					     bitmap->BytesPerRow()));
    if (surf->base.status)
	free(premultiplied);
    else
	_cairo_image_surface_assume_ownership_of_data(surf);
    return surf;
}

/**
 * _cairo_image_surface_to_bitmap:
 *
 * Converts an image surface to a BBitmap. The return value must be freed with
 * delete.
 **/
static BBitmap*
_cairo_image_surface_to_bitmap (cairo_image_surface_t* surface)
{
    BRect size(0.0, 0.0, surface->width - 1, surface->height - 1);
    switch (surface->format) {
	case CAIRO_FORMAT_ARGB32: {
	    BBitmap* data = new BBitmap(size, B_RGBA32);
	    unpremultiply_rgba(surface->data,
			       surface->width,
			       surface->height,
			       surface->stride,
			       reinterpret_cast<unsigned char*>(data->Bits()));
	    return data;
        }
	case CAIRO_FORMAT_RGB24: {
	    BBitmap* data = new BBitmap(size, B_RGB32);
	    memcpy(data->Bits(), surface->data, surface->height * surface->stride);
	    return data;
	}
	default:
	    return NULL;
    }
}

/**
 * _cairo_op_to_be_op:
 *
 * Converts a cairo drawing operator to a beos drawing_mode. Returns true if
 * the operator could be converted, false otherwise.
 **/
static bool
_cairo_op_to_be_op (cairo_operator_t cairo_op,
		    drawing_mode*    beos_op)
{
    switch (cairo_op) {

	case CAIRO_OPERATOR_SOURCE:
	    *beos_op = B_OP_COPY;
	    return true;
	case CAIRO_OPERATOR_OVER:
	    *beos_op = B_OP_ALPHA;
	    return true;

	case CAIRO_OPERATOR_ADD:
	    // Does not actually work
#if 1
	    return false;
#else
	    *beos_op = B_OP_ADD;
	    return true;
#endif

	case CAIRO_OPERATOR_CLEAR:
	    // Does not map to B_OP_ERASE - it replaces the dest with the low
	    // color, instead of transparency; could be done by setting low
	    // color appropriately.

	case CAIRO_OPERATOR_IN:
	case CAIRO_OPERATOR_OUT:
	case CAIRO_OPERATOR_ATOP:

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_DEST_OVER:
	case CAIRO_OPERATOR_DEST_IN:
	case CAIRO_OPERATOR_DEST_OUT:
	case CAIRO_OPERATOR_DEST_ATOP:

	case CAIRO_OPERATOR_XOR:
	case CAIRO_OPERATOR_SATURATE:

	default:
	    return false;
    };
}

static cairo_status_t
_cairo_beos_surface_finish (void *abstract_surface)
{
    // Nothing to do

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_beos_surface_acquire_source_image (void                   *abstract_surface,
					  cairo_image_surface_t **image_out,
					  void                  **image_extra)
{
    fprintf(stderr, "Getting source image\n");
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker) {
	_cairo_error(CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY; /// XXX not exactly right, but what can we do?
    }


    surface->view->Sync();

    if (surface->bitmap) {
	*image_out = _cairo_beos_bitmap_to_surface(surface->bitmap);
	if (!*image_out) {
	    _cairo_error(CAIRO_STATUS_NO_MEMORY);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    BBitmap* bmp;
    if (_cairo_beos_view_to_bitmap(surface->view, &bmp) != OK) {
	_cairo_error(CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY; /// XXX incorrect if the error was NOT_VISIBLE
    }

    *image_out = _cairo_beos_bitmap_to_surface(bmp);
    if (!*image_out) {
	delete bmp;
	_cairo_error(CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY;
    }
    *image_extra = bmp;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_beos_surface_release_source_image (void                  *abstract_surface,
					  cairo_image_surface_t *image,
					  void                  *image_extra)
{
    cairo_surface_destroy (&image->base);

    BBitmap* bmp = static_cast<BBitmap*>(image_extra);
    delete bmp;
}



static cairo_status_t
_cairo_beos_surface_acquire_dest_image (void                   *abstract_surface,
                                        cairo_rectangle_t      *interest_rect,
                                        cairo_image_surface_t **image_out,
                                        cairo_rectangle_t      *image_rect,
                                        void                  **image_extra)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);

    AutoLockView locker(surface->view);
    if (!locker) {
	*image_out = NULL;
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    if (surface->bitmap) {
	surface->view->Sync();
	*image_out = _cairo_beos_bitmap_to_surface(surface->bitmap);
	if (!*image_out) {
	    _cairo_error(CAIRO_STATUS_NO_MEMORY);
	    return CAIRO_STATUS_NO_MEMORY;
	}

	image_rect->x = 0;
	image_rect->y = 0;
	image_rect->width = (*image_out)->width;
	image_rect->height = (*image_out)->height;

	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    BRect b_interest_rect(_cairo_rect_to_brect(interest_rect));

    BRect rect;
    BBitmap* bitmap;
    ViewCopyStatus status = _cairo_beos_view_to_bitmap(surface->view, &bitmap,
	                                               &rect, &b_interest_rect);
    if (status == NOT_VISIBLE) {
	*image_out = NULL;
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }
    if (status == ERROR) {
	_cairo_error(CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY;
    }

    *image_rect = _brect_to_cairo_rect(rect);

#if 0
    fprintf(stderr, "Requested: (cairo rects) (%ix%i) dim (%u, %u) returning (%ix%i) dim (%u, %u)\n",
	    interest_rect->x, interest_rect->y, interest_rect->width, interest_rect->height,
	    image_rect->x, image_rect->y, image_rect->width, image_rect->height);
#endif

    *image_out = _cairo_beos_bitmap_to_surface(bitmap);
    delete bitmap;
    if (!*image_out) {
	_cairo_error(CAIRO_STATUS_NO_MEMORY);
	return CAIRO_STATUS_NO_MEMORY;
    }
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}


static void
_cairo_beos_surface_release_dest_image (void                  *abstract_surface,
                                        cairo_rectangle_t     *intersect_rect,
                                        cairo_image_surface_t *image,
                                        cairo_rectangle_t     *image_rect,
                                        void                  *image_extra)
{
    fprintf(stderr, "Fallback drawing\n");

    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker)
	return;


    BBitmap* bitmap_to_draw = _cairo_image_surface_to_bitmap(image);

    surface->view->PushState();

	surface->view->SetDrawingMode(B_OP_COPY);
	BRect rect(_cairo_rect_to_brect(image_rect));

	surface->view->DrawBitmap(bitmap_to_draw, rect);

    surface->view->PopState();

    delete bitmap_to_draw;
    cairo_surface_destroy(&image->base);
}

static cairo_int_status_t
_cairo_beos_composite (cairo_operator_t		op,
		       cairo_pattern_t	       *src,
		       cairo_pattern_t	       *mask,
		       void		       *dst,
		       int		 	src_x,
		       int			src_y,
		       int			mask_x,
		       int			mask_y,
		       int			dst_x,
		       int			dst_y,
		       unsigned int		width,
		       unsigned int		height)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							dst);
    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_SUCCESS;

    drawing_mode mode;
    if (!_cairo_op_to_be_op(op, &mode))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    // XXX Masks are not yet supported
    if (mask)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    // XXX should eventually support the others
    if (src->type != CAIRO_PATTERN_SURFACE ||
	src->extend != CAIRO_EXTEND_NONE)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    // Can we maybe support other matrices as well? (scale? if the filter is right)
    int itx, ity;
    if (!_cairo_matrix_is_integer_translation(&src->matrix, &itx, &ity))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    BRect srcRect(src_x + itx,
		  src_y + ity,
		  src_x + itx + width - 1,
		  src_y + ity + height - 1);
    BRect dstRect(dst_x, dst_y, dst_x + width - 1, dst_y + height - 1);

    cairo_surface_t* src_surface = reinterpret_cast<cairo_surface_pattern_t*>(src)->
					surface;
    if (_cairo_surface_is_image(src_surface)) {
    	fprintf(stderr, "Composite\n");

	// Draw it on screen.
	cairo_image_surface_t* img_surface =
	    reinterpret_cast<cairo_image_surface_t*>(src_surface);

	BBitmap* bmp = _cairo_image_surface_to_bitmap(img_surface);
	surface->view->PushState();

	    // If our image rect is only a subrect of the desired size, and we
	    // aren't using B_OP_ALPHA, then we need to fill the rect first.
	    if (mode == B_OP_COPY && !bmp->Bounds().Contains(srcRect)) {
		rgb_color black = { 0, 0, 0, 0 };

		surface->view->SetDrawingMode(mode);
		surface->view->SetHighColor(black);
		surface->view->FillRect(dstRect);
	    }

	    if (mode == B_OP_ALPHA && img_surface->format != CAIRO_FORMAT_ARGB32) {
		mode = B_OP_COPY;

	    }
	    surface->view->SetDrawingMode(mode);

	    if (surface->bitmap && surface->bitmap->ColorSpace() == B_RGBA32)
		surface->view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	    else
		surface->view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	    surface->view->DrawBitmap(bmp, srcRect, dstRect);

	surface->view->PopState();
	delete bmp;

	return CAIRO_INT_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static void
_cairo_beos_fill_rectangle (cairo_beos_surface_t *surface,
			    cairo_rectangle_t    *rect)
{
    BRect brect(_cairo_rect_to_brect(rect));
    surface->view->FillRect(brect);
}

static cairo_int_status_t
_cairo_beos_fill_rectangles (void                *abstract_surface,
			     cairo_operator_t     op,
			     const cairo_color_t *color,
			     cairo_rectangle_t   *rects,
			     int                  num_rects)
{
    fprintf(stderr, "Drawing %i rectangles\n", num_rects);
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);

    if (num_rects <= 0)
	return CAIRO_INT_STATUS_SUCCESS;

    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_SUCCESS;

    drawing_mode mode;
    if (!_cairo_op_to_be_op(op, &mode))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    rgb_color be_color = _cairo_color_to_be_color(color);

    if (mode == B_OP_ALPHA && be_color.alpha == 0xFF)
	mode = B_OP_COPY;

    // For CAIRO_OPERATOR_SOURCE, cairo expects us to use the premultiplied
    // color info. This is only relevant when drawing into an rgb24 buffer
    // (as for others, we can convert when asked for the image)
    if (mode == B_OP_COPY && be_color.alpha != 0xFF &&
	(!surface->bitmap || surface->bitmap->ColorSpace() != B_RGBA32))
    {
	be_color.red = premultiply(be_color.red, be_color.alpha);
	be_color.green = premultiply(be_color.green, be_color.alpha);
	be_color.blue = premultiply(be_color.blue, be_color.alpha);
    }

    surface->view->PushState();

	surface->view->SetDrawingMode(mode);
	surface->view->SetHighColor(be_color);
	if (surface->bitmap && surface->bitmap->ColorSpace() == B_RGBA32)
	    surface->view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
	else
	    surface->view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);

	for (int i = 0; i < num_rects; ++i) {
	    _cairo_beos_fill_rectangle(surface, &rects[i]);
	}

    surface->view->PopState();

    return CAIRO_INT_STATUS_SUCCESS;
}



static cairo_int_status_t
_cairo_beos_surface_set_clip_region (void              *abstract_surface,
                                     pixman_region16_t *region)
{
    fprintf(stderr, "Setting clip region\n");
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_SUCCESS;

    if (region == NULL) {
	// No clipping
	surface->view->ConstrainClippingRegion(NULL);
	return CAIRO_INT_STATUS_SUCCESS;
    }

    int count = pixman_region_num_rects(region);
    pixman_box16_t* rects = pixman_region_rects(region);
    BRegion bregion;
    for (int i = 0; i < count; ++i) {
	// Have to substract one, because for pixman, the second coordinate
	// lies outside the rectangle.
	bregion.Include(BRect(rects[i].x1, rects[i].y1, rects[i].x2 - 1, rects[i].y2  - 1));
    }
    surface->view->ConstrainClippingRegion(&bregion);
    return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_beos_surface_get_extents (void              *abstract_surface,
				 cairo_rectangle_t *rectangle)
{
    cairo_beos_surface_t *surface = reinterpret_cast<cairo_beos_surface_t*>(
							abstract_surface);
    AutoLockView locker(surface->view);
    if (!locker)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    BRect size = surface->view->Bounds();

    *rectangle = _brect_to_cairo_rect(size);

    // Make sure to have our upperleft edge as (0,0)
    rectangle->x = 0;
    rectangle->y = 0;

    return CAIRO_INT_STATUS_SUCCESS;
}

static const struct _cairo_surface_backend cairo_beos_surface_backend = {
    NULL, /* create_similar */
    _cairo_beos_surface_finish,
    _cairo_beos_surface_acquire_source_image,
    _cairo_beos_surface_release_source_image,
    _cairo_beos_surface_acquire_dest_image,
    _cairo_beos_surface_release_dest_image,
    NULL, /* clone_similar */
    _cairo_beos_composite, /* composite */
    _cairo_beos_fill_rectangles,
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_beos_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_beos_surface_get_extents,
    NULL,  /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL  /* show_glyphs */
};

/**
 * cairo_beos_surface_create:
 * @view: The view to draw on
 *
 * Creates a Cairo surface that draws onto a BeOS BView.
 * The caller must ensure that the view does not get deleted before the surface.
 * If the view is attached to a bitmap rather than an on-screen window, use
 * cairo_beos_surface_create_for_bitmap instead of this function.
 **/
cairo_surface_t *
cairo_beos_surface_create (BView* view)
{
    return cairo_beos_surface_create_for_bitmap(view, NULL);
}

/**
 * cairo_beos_surface_create_for_bitmap:
 * @view: The view to draw on
 * @bmp: The bitmap to which the view is attached
 *
 * Creates a Cairo surface that draws onto a BeOS BView which is attached to a
 * BBitmap.
 * The caller must ensure that the view and the bitmap do not get deleted
 * before the surface.
 *
 * For views that draw to a bitmap (as opposed to a screen), use this function
 * rather than cairo_beos_surface_create. Not using this function WILL lead to
 * incorrect behaviour.
 *
 * For now, only views that draw to the entire area of bmp are supported.
 * The view must already be attached to the bitmap.
 **/
cairo_surface_t *
cairo_beos_surface_create_for_bitmap (BView*   view,
				      BBitmap* bmp)
{
    // Must use malloc, because cairo code will use free() on the surface
    cairo_beos_surface_t *surface = static_cast<cairo_beos_surface_t*>(
					malloc(sizeof(cairo_beos_surface_t)));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        return const_cast<cairo_surface_t*>(&_cairo_surface_nil);
    }

    _cairo_surface_init(&surface->base, &cairo_beos_surface_backend);

    surface->view = view;
    surface->bitmap = bmp;

    return (cairo_surface_t *) surface;
}

// ---------------------------------------------------------------------------
// Cairo uses locks without explicit initialization. To support that, we
// provide a class here which manages the locks and is in global scope, so the
// compiler will instantiate it on library load and destroy it on library
// unload.

class BeLocks {
    public:
	BLocker cairo_toy_font_face_hash_table_mutex;
	BLocker cairo_scaled_font_map_mutex;
	BLocker cairo_ft_unscaled_font_map_mutex;
};

static BeLocks locks;

void* cairo_toy_font_face_hash_table_mutex = &locks.cairo_toy_font_face_hash_table_mutex;
void* cairo_scaled_font_map_mutex = &locks.cairo_scaled_font_map_mutex;
void* cairo_ft_unscaled_font_map_mutex = &locks.cairo_ft_unscaled_font_map_mutex;

void _cairo_beos_lock (void* locker) {
    BLocker* bLocker = reinterpret_cast<BLocker*>(locker);
    bLocker->Lock();
}

void _cairo_beos_unlock (void* locker) {
    BLocker* bLocker = reinterpret_cast<BLocker*>(locker);
    bLocker->Unlock();
}

