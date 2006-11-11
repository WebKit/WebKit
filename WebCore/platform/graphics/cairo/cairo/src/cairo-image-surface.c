/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

static int
_cairo_format_bpp (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

cairo_surface_t *
_cairo_image_surface_create_for_pixman_image (pixman_image_t *pixman_image,
					      cairo_format_t  format)
{
    cairo_image_surface_t *surface;

    surface = malloc (sizeof (cairo_image_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_image_surface_backend);

    surface->pixman_image = pixman_image;

    surface->format = format;
    surface->data = (unsigned char *) pixman_image_get_data (pixman_image);
    surface->owns_data = FALSE;
    surface->has_clip = FALSE;

    surface->width = pixman_image_get_width (pixman_image);
    surface->height = pixman_image_get_height (pixman_image);
    surface->stride = pixman_image_get_stride (pixman_image);
    surface->depth = pixman_image_get_depth (pixman_image);

    return &surface->base;
}

/* Try to recover a cairo_format_t from a pixman_format
 * by looking at the bpp and masks values. */
static cairo_format_t
_cairo_format_from_pixman_format (pixman_format_t *pixman_format)
{
    int bpp, am, rm, gm, bm;

    pixman_format_get_masks (pixman_format, &bpp, &am, &rm, &gm, &bm);

    if (bpp == 32 &&
	am == 0xff000000 &&
	rm == 0x00ff0000 &&
	gm == 0x0000ff00 &&
	bm == 0x000000ff)
	return CAIRO_FORMAT_ARGB32;

    if (bpp == 32 &&
	am == 0x0 &&
	rm == 0x00ff0000 &&
	gm == 0x0000ff00 &&
	bm == 0x000000ff)
	return CAIRO_FORMAT_RGB24;

    if (bpp == 8 &&
	am == 0xff &&
	rm == 0x0 &&
	gm == 0x0 &&
	bm == 0x0)
	return CAIRO_FORMAT_A8;

    if (bpp == 1 &&
	am == 0x1 &&
	rm == 0x0 &&
	gm == 0x0 &&
	bm == 0x0)
	return CAIRO_FORMAT_A1;

    return (cairo_format_t) -1;
}

cairo_surface_t *
_cairo_image_surface_create_with_masks (unsigned char	       *data,
					cairo_format_masks_t   *format,
					int			width,
					int			height,
					int			stride)
{
    cairo_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;
    cairo_format_t cairo_format;

    pixman_format = pixman_format_create_masks (format->bpp,
						format->alpha_mask,
						format->red_mask,
						format->green_mask,
						format->blue_mask);

    if (pixman_format == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    cairo_format = _cairo_format_from_pixman_format (pixman_format);

    pixman_image = pixman_image_create_for_data ((pixman_bits_t *) data, pixman_format,
						 width, height, format->bpp, stride);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image,
							    cairo_format);

    return surface;
}

static pixman_format_t *
_create_pixman_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return pixman_format_create (PIXMAN_FORMAT_NAME_A1);
	break;
    case CAIRO_FORMAT_A8:
	return pixman_format_create (PIXMAN_FORMAT_NAME_A8);
	break;
    case CAIRO_FORMAT_RGB24:
	return pixman_format_create (PIXMAN_FORMAT_NAME_RGB24);
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	return pixman_format_create (PIXMAN_FORMAT_NAME_ARGB32);
	break;
    }
}

/**
 * cairo_image_surface_create:
 * @format: format of pixels in the surface to create 
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 * 
 * Creates an image surface of the specified format and
 * dimensions. The initial contents of the surface is undefined; you
 * must explicitly initialize the surface contents, using, for
 * example, cairo_paint().
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height)
{
    cairo_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;

    if (! CAIRO_FORMAT_VALID (format))
	return (cairo_surface_t*) &_cairo_surface_nil;

    pixman_format = _create_pixman_format (format);
    if (pixman_format == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    pixman_image = pixman_image_create (pixman_format, width, height);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image, format);

    return surface;
}

cairo_surface_t *
_cairo_image_surface_create_with_content (cairo_content_t	content,
					  int			width,
					  int			height)
{
    if (! CAIRO_CONTENT_VALID (content))
	return (cairo_surface_t*) &_cairo_surface_nil;

    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

/**
 * cairo_image_surface_create_for_data:
 * @data: a pointer to a buffer supplied by the application
 *    in which to write contents.
 * @format: the format of pixels in the buffer
 * @width: the width of the image to be stored in the buffer
 * @height: the height of the image to be stored in the buffer
 * @stride: the number of bytes between the start of rows
 *   in the buffer. Having this be specified separate from @width
 *   allows for padding at the end of rows, or for writing
 *   to a subportion of a larger image.
 * 
 * Creates an image surface for the provided pixel data. The output
 * buffer must be kept around until the #cairo_surface_t is destroyed
 * or cairo_surface_finish() is called on the surface.  The initial
 * contents of @buffer will be used as the inital image contents; you
 * must explicitely clear the buffer, using, for example,
 * cairo_rectangle() and cairo_fill() if you want it cleared.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_image_surface_create_for_data (unsigned char     *data,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     int		stride)
{
    cairo_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;

    if (! CAIRO_FORMAT_VALID (format))
	return (cairo_surface_t*) &_cairo_surface_nil;

    pixman_format = _create_pixman_format (format);
    if (pixman_format == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }
    
    pixman_image = pixman_image_create_for_data ((pixman_bits_t *) data, pixman_format,
						 width, height,
						 _cairo_format_bpp (format),
						 stride);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image, format);

    return surface;
}

cairo_surface_t *
_cairo_image_surface_create_for_data_with_content (unsigned char	*data,
						   cairo_content_t	 content,
						   int			 width,
						   int			 height,
						   int			 stride)
{
    if (! CAIRO_CONTENT_VALID (content))
	return (cairo_surface_t*) &_cairo_surface_nil;

    return cairo_image_surface_create_for_data (data,
						_cairo_format_from_content (content),
						width, height, stride);
}

/**
 * cairo_image_surface_get_width:
 * @surface: a #cairo_image_surface_t
 * 
 * Get the width of the image surface in pixels.
 * 
 * Return value: the width of the surface in pixels.
 **/
int
cairo_image_surface_get_width (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (!_cairo_surface_is_image (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return image_surface->width;
}

/**
 * cairo_image_surface_get_height:
 * @surface: a #cairo_image_surface_t
 * 
 * Get the height of the image surface in pixels.
 * 
 * Return value: the height of the surface in pixels.
 **/
int
cairo_image_surface_get_height (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (!_cairo_surface_is_image (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return image_surface->height;
}

cairo_format_t
_cairo_format_from_content (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
	return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
	return CAIRO_FORMAT_ARGB32;
    }

    ASSERT_NOT_REACHED;
    return CAIRO_FORMAT_ARGB32;
}

cairo_content_t
_cairo_content_from_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return CAIRO_CONTENT_COLOR_ALPHA;
    case CAIRO_FORMAT_RGB24:
	return CAIRO_CONTENT_COLOR;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	return CAIRO_CONTENT_ALPHA;
    }

    ASSERT_NOT_REACHED;
    return CAIRO_CONTENT_COLOR_ALPHA;
}

static cairo_surface_t *
_cairo_image_surface_create_similar (void	       *abstract_src,
				     cairo_content_t	content,
				     int		width,
				     int		height)
{
    assert (CAIRO_CONTENT_VALID (content));

    return _cairo_image_surface_create_with_content (content,
						     width, height);
}

static cairo_status_t
_cairo_image_surface_finish (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (surface->pixman_image) {
	pixman_image_destroy (surface->pixman_image);
	surface->pixman_image = NULL;
    }

    if (surface->owns_data) {
	free (surface->data);
	surface->data = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_image_surface_assume_ownership_of_data (cairo_image_surface_t *surface)
{
    surface->owns_data = 1;
}

static cairo_status_t
_cairo_image_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    *image_out = abstract_surface;
    *image_extra = NULL;
    
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_release_source_image (void                   *abstract_surface,
					   cairo_image_surface_t  *image,
					   void                   *image_extra)
{
}

static cairo_status_t
_cairo_image_surface_acquire_dest_image (void                    *abstract_surface,
					 cairo_rectangle_t       *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_t       *image_rect_out,
					 void                   **image_extra)
{
    cairo_image_surface_t *surface = abstract_surface;
    
    image_rect_out->x = 0;
    image_rect_out->y = 0;
    image_rect_out->width = surface->width;
    image_rect_out->height = surface->height;

    *image_out = surface;
    *image_extra = NULL;
    
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_release_dest_image (void                   *abstract_surface,
					 cairo_rectangle_t      *interest_rect,
					 cairo_image_surface_t  *image,
					 cairo_rectangle_t      *image_rect,
					 void                   *image_extra)
{
}

static cairo_status_t
_cairo_image_surface_clone_similar (void		*abstract_surface,
				    cairo_surface_t	*src,
				    cairo_surface_t    **clone_out)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (src->backend == surface->base.backend) {
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    }	
    
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_image_surface_set_matrix (cairo_image_surface_t	*surface,
				 const cairo_matrix_t	*matrix)
{
    pixman_transform_t pixman_transform;

    _cairo_matrix_to_pixman_matrix (matrix, &pixman_transform);

    pixman_image_set_transform (surface->pixman_image, &pixman_transform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_set_filter (cairo_image_surface_t *surface, cairo_filter_t filter)
{
    pixman_filter_t pixman_filter;

    switch (filter) {
    case CAIRO_FILTER_FAST:
	pixman_filter = PIXMAN_FILTER_FAST;
	break;
    case CAIRO_FILTER_GOOD:
	pixman_filter = PIXMAN_FILTER_GOOD;
	break;
    case CAIRO_FILTER_BEST:
	pixman_filter = PIXMAN_FILTER_BEST;
	break;
    case CAIRO_FILTER_NEAREST:
	pixman_filter = PIXMAN_FILTER_NEAREST;
	break;
    case CAIRO_FILTER_BILINEAR:
	pixman_filter = PIXMAN_FILTER_BILINEAR;
	break;
    default:
	pixman_filter = PIXMAN_FILTER_BEST;
    }

    pixman_image_set_filter (surface->pixman_image, pixman_filter);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_set_attributes (cairo_image_surface_t      *surface,
				     cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;
    
    status = _cairo_image_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
        pixman_image_set_repeat (surface->pixman_image, PIXMAN_REPEAT_NONE);
	break;
    case CAIRO_EXTEND_REPEAT:
        pixman_image_set_repeat (surface->pixman_image, PIXMAN_REPEAT_NORMAL);
	break;
    case CAIRO_EXTEND_REFLECT:
        pixman_image_set_repeat (surface->pixman_image, PIXMAN_REPEAT_REFLECT);
	break;
    case CAIRO_EXTEND_PAD:
        pixman_image_set_repeat (surface->pixman_image, PIXMAN_REPEAT_PAD);
	break;
    }
    
    status = _cairo_image_surface_set_filter (surface, attributes->filter);
    
    return status;
}

/* XXX: I think we should fix pixman to match the names/order of the
 * cairo operators, but that will likely be better done at the same
 * time the X server is ported to pixman, (which will change a lot of
 * things in pixman I think).
 */
static pixman_operator_t
_pixman_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return PIXMAN_OPERATOR_CLEAR;

    case CAIRO_OPERATOR_SOURCE:
	return PIXMAN_OPERATOR_SRC;
    case CAIRO_OPERATOR_OVER:
	return PIXMAN_OPERATOR_OVER;
    case CAIRO_OPERATOR_IN:
	return PIXMAN_OPERATOR_IN;
    case CAIRO_OPERATOR_OUT:
	return PIXMAN_OPERATOR_OUT;
    case CAIRO_OPERATOR_ATOP:
	return PIXMAN_OPERATOR_ATOP;

    case CAIRO_OPERATOR_DEST:
	return PIXMAN_OPERATOR_DST;
    case CAIRO_OPERATOR_DEST_OVER:
	return PIXMAN_OPERATOR_OVER_REVERSE;
    case CAIRO_OPERATOR_DEST_IN:
	return PIXMAN_OPERATOR_IN_REVERSE;
    case CAIRO_OPERATOR_DEST_OUT:
	return PIXMAN_OPERATOR_OUT_REVERSE;
    case CAIRO_OPERATOR_DEST_ATOP:
	return PIXMAN_OPERATOR_ATOP_REVERSE;

    case CAIRO_OPERATOR_XOR:
	return PIXMAN_OPERATOR_XOR;
    case CAIRO_OPERATOR_ADD:
	return PIXMAN_OPERATOR_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return PIXMAN_OPERATOR_SATURATE;
    default:
	return PIXMAN_OPERATOR_OVER;
    }
}

static cairo_int_status_t
_cairo_image_surface_composite (cairo_operator_t	op,
				cairo_pattern_t		*src_pattern,
				cairo_pattern_t		*mask_pattern,
				void			*abstract_dst,
				int			src_x,
				int			src_y,
				int			mask_x,
				int			mask_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height)
{
    cairo_surface_attributes_t	src_attr, mask_attr;
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_image_surface_t	*mask;
    cairo_int_status_t		status;

    status = _cairo_pattern_acquire_surfaces (src_pattern, mask_pattern,
					      &dst->base,
					      src_x, src_y,
					      mask_x, mask_y,
					      width, height,
					      (cairo_surface_t **) &src,
					      (cairo_surface_t **) &mask,
					      &src_attr, &mask_attr);
    if (status)
	return status;
    
    status = _cairo_image_surface_set_attributes (src, &src_attr);
    if (status)
      goto CLEANUP_SURFACES;

    if (mask)
    {
	status = _cairo_image_surface_set_attributes (mask, &mask_attr);
	if (status)
	    goto CLEANUP_SURFACES;
	
	pixman_composite (_pixman_operator (op),
			  src->pixman_image,
			  mask->pixman_image,
			  dst->pixman_image,
			  src_x + src_attr.x_offset,
			  src_y + src_attr.y_offset,
			  mask_x + mask_attr.x_offset,
			  mask_y + mask_attr.y_offset,
			  dst_x, dst_y,
			  width, height);
    }
    else
    {
	pixman_composite (_pixman_operator (op),
			  src->pixman_image,
			  NULL,
			  dst->pixman_image,
			  src_x + src_attr.x_offset,
			  src_y + src_attr.y_offset,
			  0, 0,
			  dst_x, dst_y,
			  width, height);
    }
    
    if (!_cairo_operator_bounded_by_source (op))
	status = _cairo_surface_composite_fixup_unbounded (&dst->base,
							   &src_attr, src->width, src->height,
							   mask ? &mask_attr : NULL,
							   mask ? mask->width : 0,
							   mask ? mask->height : 0,
							   src_x, src_y,
							   mask_x, mask_y,
							   dst_x, dst_y, width, height);

 CLEANUP_SURFACES:
    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);
    
    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);
    
    return status;
}

static cairo_int_status_t
_cairo_image_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		op,
				      const cairo_color_t	*color,
				      cairo_rectangle_t		*rects,
				      int			num_rects)
{
    cairo_image_surface_t *surface = abstract_surface;

    pixman_color_t pixman_color;

    pixman_color.red   = color->red_short;
    pixman_color.green = color->green_short;
    pixman_color.blue  = color->blue_short;
    pixman_color.alpha = color->alpha_short;

    /* XXX: The pixman_rectangle_t cast is evil... it needs to go away somehow. */
    pixman_fill_rectangles (_pixman_operator(op), surface->pixman_image,
			    &pixman_color, (pixman_rectangle_t *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_image_surface_is_alpha_only (cairo_image_surface_t *surface)
{
    int bpp, alpha, red, green, blue;
    
    if (surface->format != (cairo_format_t) -1)
	return surface->format == CAIRO_FORMAT_A1 || surface->format == CAIRO_FORMAT_A8;

    pixman_format_get_masks (pixman_image_get_format (surface->pixman_image),
			     &bpp, &alpha, &red, &green, &blue);

    return red == 0 && blue == 0 && green == 0;
}

static cairo_int_status_t
_cairo_image_surface_composite_trapezoids (cairo_operator_t	op,
					   cairo_pattern_t	*pattern,
					   void			*abstract_dst,
					   cairo_antialias_t	antialias,
					   int			src_x,
					   int			src_y,
					   int			dst_x,
					   int			dst_y,
					   unsigned int		width,
					   unsigned int		height,
					   cairo_trapezoid_t	*traps,
					   int			num_traps)
{
    cairo_surface_attributes_t	attributes;
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_int_status_t		status;
    pixman_image_t		*mask;
    pixman_format_t		*format;
    pixman_bits_t		*mask_data;
    int				mask_stride;
    int				mask_bpp;

    /* Special case adding trapezoids onto a mask surface; we want to avoid
     * creating an intermediate temporary mask unecessarily.
     *
     * We make the assumption here that the portion of the trapezoids
     * contained within the surface is bounded by [dst_x,dst_y,width,height];
     * the Cairo core code passes bounds based on the trapezoid extents.
     *
     * Currently the check surface->has_clip is needed for correct
     * functioning, since pixman_add_trapezoids() doesn't obey the
     * surface clip, which is a libpixman bug , but there's no harm in
     * falling through to the general case when the surface is clipped
     * since libpixman would have to generate an intermediate mask anyways.
     */
    if (op == CAIRO_OPERATOR_ADD &&
	_cairo_pattern_is_opaque_solid (pattern) &&
	_cairo_image_surface_is_alpha_only (dst) &&
	!dst->has_clip &&
	antialias != CAIRO_ANTIALIAS_NONE)
    {
	pixman_add_trapezoids (dst->pixman_image, 0, 0,
			       (pixman_trapezoid_t *) traps, num_traps);
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    status = _cairo_image_surface_set_attributes (src, &attributes);
    if (status)
	goto CLEANUP_SOURCE;

    switch (antialias) {
    case CAIRO_ANTIALIAS_NONE:
	format = pixman_format_create (PIXMAN_FORMAT_NAME_A1);
	mask_stride = (width + 31)/8;
	mask_bpp = 1;
 	break;
    default:
	format = pixman_format_create (PIXMAN_FORMAT_NAME_A8);
	mask_stride = (width + 3) & ~3;
	mask_bpp = 8;
 	break;
    }
    if (!format) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_SOURCE;
    }

    /* The image must be initially transparent */
    mask_data = calloc (1, mask_stride * height);
    if (!mask_data) {
	status = CAIRO_STATUS_NO_MEMORY;
	pixman_format_destroy (format);
	goto CLEANUP_SOURCE;
    }

    mask = pixman_image_create_for_data (mask_data, format, width, height,
					 mask_bpp, mask_stride);
    pixman_format_destroy (format);
    if (!mask) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_IMAGE_DATA;
    }

    /* XXX: The pixman_trapezoid_t cast is evil and needs to go away
     * somehow. */
    pixman_add_trapezoids (mask, - dst_x, - dst_y,
			   (pixman_trapezoid_t *) traps, num_traps);

    pixman_composite (_pixman_operator (op),
		      src->pixman_image,
		      mask,
		      dst->pixman_image,
		      src_x + attributes.x_offset,
		      src_y + attributes.y_offset,
		      0, 0,
		      dst_x, dst_y,
		      width, height);

    if (!_cairo_operator_bounded_by_mask (op))
	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes, src->width, src->height,
								 width, height,
								 src_x, src_y,
								 0, 0,
								 dst_x, dst_y, width, height);
    pixman_image_destroy (mask);

 CLEANUP_IMAGE_DATA:
    free (mask_data);

 CLEANUP_SOURCE:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_set_clip_region (void *abstract_surface,
				      pixman_region16_t *region)
{
    cairo_image_surface_t *surface = (cairo_image_surface_t *) abstract_surface;

    pixman_image_set_clip_region (surface->pixman_image, region);

    surface->has_clip = region != NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_get_extents (void			*abstract_surface,
				  cairo_rectangle_t	*rectangle)
{
    cairo_image_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_surface_is_image:
 * @surface: a #cairo_surface_t
 * 
 * Checks if a surface is an #cairo_image_surface_t
 * 
 * Return value: TRUE if the surface is an image surface
 **/
cairo_bool_t
_cairo_surface_is_image (const cairo_surface_t *surface)
{
    return surface->backend == &cairo_image_surface_backend;
}

const cairo_surface_backend_t cairo_image_surface_backend = {
    CAIRO_SURFACE_TYPE_IMAGE,
    _cairo_image_surface_create_similar,
    _cairo_image_surface_finish,
    _cairo_image_surface_acquire_source_image,
    _cairo_image_surface_release_source_image,
    _cairo_image_surface_acquire_dest_image,
    _cairo_image_surface_release_dest_image,
    _cairo_image_surface_clone_similar,
    _cairo_image_surface_composite,
    _cairo_image_surface_fill_rectangles,
    _cairo_image_surface_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_image_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_image_surface_get_extents,
    NULL /* old_show_glyphs */
};
