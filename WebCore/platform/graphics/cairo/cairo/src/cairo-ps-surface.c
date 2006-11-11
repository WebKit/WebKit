/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2005 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 *	Keith Packard <keithp@keithp.com>
 */

#include "cairoint.h"
#include "cairo-ps.h"
#include "cairo-font-subset-private.h"
#include "cairo-paginated-surface-private.h"
#include "cairo-ft-private.h"

#include <time.h>
#include <zlib.h>

/* TODO:
 *
 * - Add document structure convention comments where appropriate.
 *
 * - Create a set of procs to use... specifically a trapezoid proc.
 */

static const cairo_surface_backend_t cairo_ps_surface_backend;

typedef struct cairo_ps_surface {
    cairo_surface_t base;

    /* PS-specific fields */
    cairo_output_stream_t *stream;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    cairo_bool_t need_start_page; 
    int num_pages;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    cairo_array_t fonts;
#endif
} cairo_ps_surface_t;

#define PS_SURFACE_DPI_DEFAULT 300.0

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface);
#endif

static void
_cairo_ps_surface_emit_header (cairo_ps_surface_t *surface)
{
    time_t now;

    now = time (NULL);

    _cairo_output_stream_printf (surface->stream,
				 "%%!PS-Adobe-3.0\n"
				 "%%%%Creator: cairo (http://cairographics.org)\n"
				 "%%%%CreationDate: %s"
				 "%%%%Pages: (atend)\n"
				 "%%%%BoundingBox: %f %f %f %f\n",
				 ctime (&now),
				 0.0, 0.0, 
				 surface->width,
				 surface->height);

    _cairo_output_stream_printf (surface->stream,
				 "%%%%DocumentData: Clean7Bit\n"
				 "%%%%LanguageLevel: 2\n"
				 "%%%%Orientation: Portrait\n"
				 "%%%%EndComments\n");
}

static void
_cairo_ps_surface_emit_footer (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Trailer\n"
				 "%%%%Pages: %d\n"
				 "%%%%EOF\n",
				 surface->num_pages);
}

static cairo_surface_t *
_cairo_ps_surface_create_for_stream_internal (cairo_output_stream_t *stream,
					      double		     width,
					      double		     height)
{
    cairo_ps_surface_t *surface;

    surface = malloc (sizeof (cairo_ps_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_ps_surface_backend);

    surface->stream = stream;

    surface->width  = width;
    surface->height = height;
    surface->x_dpi = PS_SURFACE_DPI_DEFAULT;
    surface->y_dpi = PS_SURFACE_DPI_DEFAULT;

    surface->need_start_page = TRUE;
    surface->num_pages = 0;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_array_init (&surface->fonts, sizeof (cairo_font_subset_t *));
#endif

    _cairo_ps_surface_emit_header (surface);

    return _cairo_paginated_surface_create (&surface->base,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height);
}

/**
 * cairo_ps_surface_create:
 * @filename: a filename for the PS output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PostScript surface of the specified size in points to be
 * written to @filename.
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
cairo_ps_surface_create (const char		*filename,
			 double			 width_in_points,
			 double			 height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

/**
 * cairo_ps_surface_create_for_stream:
 * @write: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PostScript surface of the specified size in points to be
 * written incrementally to the stream represented by @write and
 * @closure.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 */
cairo_surface_t *
cairo_ps_surface_create_for_stream (cairo_write_func_t	write_func,
				    void	       *closure,
				    double		width_in_points,
				    double		height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write_func, closure);
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_ps_surface_create_for_stream_internal (stream,
							 width_in_points,
							 height_in_points);
}

static cairo_bool_t
_cairo_surface_is_ps (cairo_surface_t *surface)
{
    return surface->backend == &cairo_ps_surface_backend;
}

/**
 * cairo_ps_surface_set_dpi:
 * @surface: a postscript cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set the horizontal and vertical resolution for image fallbacks.
 * When the ps backend needs to fall back to image overlays, it will
 * use this resolution. These DPI values are not used for any other
 * purpose, (in particular, they do not have any bearing on the size
 * passed to cairo_ps_surface_create() nor on the CTM).
 **/
void
cairo_ps_surface_set_dpi (cairo_surface_t *surface,
			  double	   x_dpi,
			  double	   y_dpi)
{
    cairo_surface_t *target;
    cairo_ps_surface_t *ps_surface;

    if (! _cairo_surface_is_paginated (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_ps (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    ps_surface = (cairo_ps_surface_t *) target;

    ps_surface->x_dpi = x_dpi;    
    ps_surface->y_dpi = y_dpi;
}

/* XXX */
static cairo_status_t
_cairo_ps_surface_finish (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_ps_surface_write_font_subsets (surface);
#endif

    _cairo_ps_surface_emit_footer (surface);

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_font_subset_destroy (subset);
    }	
    _cairo_array_fini (&surface->fonts);
#endif

    _cairo_output_stream_destroy (surface->stream);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ps_surface_start_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "%%%%Page: %d\n",
				 ++surface->num_pages);


    _cairo_output_stream_printf (surface->stream,
				 "gsave %f %f translate %f %f scale \n",
				 0.0, surface->height,
				 1.0, -1.0);

    surface->need_start_page = FALSE;
}

static void
_cairo_ps_surface_end_page (cairo_ps_surface_t *surface)
{
    _cairo_output_stream_printf (surface->stream,
				 "grestore\n");

    surface->need_start_page = TRUE;
}

static cairo_int_status_t
_cairo_ps_surface_copy_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "copypage\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_show_page (void *abstract_surface)
{
    cairo_ps_surface_t *surface = abstract_surface;

    _cairo_ps_surface_end_page (surface);

    _cairo_output_stream_printf (surface->stream, "showpage\n");

    surface->need_start_page = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_font_subset_t *
_cairo_ps_surface_get_font (cairo_ps_surface_t  *surface,
			    cairo_scaled_font_t *scaled_font)
{
    cairo_status_t status;
    cairo_unscaled_font_t *unscaled_font;
    cairo_font_subset_t *subset;
    unsigned int num_fonts, i;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return NULL;

    /* XXX Why is this an ft specific function? */
    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font);

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	if (subset->unscaled_font == unscaled_font)
	    return subset;
    }

    subset = _cairo_font_subset_create (unscaled_font);
    if (subset == NULL)
	return NULL;

    subset->font_id = surface->fonts.num_elements;

    status = _cairo_array_append (&surface->fonts, &subset);
    if (status) {
	_cairo_font_subset_destroy (subset);
	return NULL;
    }

    return subset;
}

static cairo_int_status_t
_cairo_ps_surface_write_type42_dict (cairo_ps_surface_t  *surface,
				     cairo_font_subset_t *subset)
{
    const char *data;
    unsigned long data_size;
    cairo_status_t status;
    int i;

    status = CAIRO_STATUS_SUCCESS;

    /* FIXME: Figure out document structure convention for fonts */

    _cairo_output_stream_printf (surface->stream,
				 "11 dict begin\n"
				 "/FontType 42 def\n"
				 "/FontName /f%d def\n"
				 "/PaintType 0 def\n"
				 "/FontMatrix [ 1 0 0 1 0 0 ] def\n"
				 "/FontBBox [ 0 0 0 0 ] def\n"
				 "/Encoding 256 array def\n"
				 "0 1 255 { Encoding exch /.notdef put } for\n",
				 subset->font_id);

    /* FIXME: Figure out how subset->x_max etc maps to the /FontBBox */

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "Encoding %d /g%d put\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "/CharStrings %d dict dup begin\n"
				 "/.notdef 0 def\n",
				 subset->num_glyphs);

    for (i = 1; i < subset->num_glyphs; i++)
	_cairo_output_stream_printf (surface->stream,
				     "/g%d %d def\n", i, i);

    _cairo_output_stream_printf (surface->stream,
				 "end readonly def\n");

    status = _cairo_font_subset_generate (subset, &data, &data_size);

    /* FIXME: We need to break up fonts bigger than 64k so we don't
     * exceed string size limitation.  At glyph boundaries.  Stupid
     * postscript. */
    _cairo_output_stream_printf (surface->stream,
				 "/sfnts [<");

    _cairo_output_stream_write_hex_string (surface->stream, data, data_size);

    _cairo_output_stream_printf (surface->stream,
				 ">] def\n"
				 "FontName currentdict end definefont pop\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_write_font_subsets (cairo_ps_surface_t *surface)
{
    cairo_font_subset_t *subset;
    int i;

    for (i = 0; i < surface->fonts.num_elements; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &subset);
	_cairo_ps_surface_write_type42_dict (surface, subset);
    }

    return CAIRO_STATUS_SUCCESS;
}
#endif

/* XXX: This function wil go away in favor of the new "analysis mode"
 * of cairo_paginated_surface_t */
static cairo_int_status_t
_cairo_ps_surface_add_fallback_area (cairo_ps_surface_t *surface,
				     int x, int y,
				     unsigned int width,
				     unsigned int height)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
color_is_gray (cairo_color_t *color)
{
    const double epsilon = 0.00001;

    return (fabs (color->red - color->green) < epsilon &&
	    fabs (color->red - color->blue) < epsilon);
}

static cairo_bool_t
color_is_translucent (const cairo_color_t *color)
{
    return color->alpha < 0.999;
}

static cairo_bool_t
format_is_translucent (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return TRUE;
    case CAIRO_FORMAT_RGB24:
	return FALSE;
    case CAIRO_FORMAT_A8:
	return TRUE;
    case CAIRO_FORMAT_A1:
	return TRUE;
    }
    return TRUE;
}

static cairo_bool_t
surface_is_translucent (const cairo_surface_t *surface)
{ 
    if (_cairo_surface_is_image (surface)) {
	const cairo_image_surface_t	*image_surface = (cairo_image_surface_t *) surface;

	return format_is_translucent (image_surface->format);
    }
    return TRUE;
}

static cairo_bool_t
gradient_is_translucent (const cairo_gradient_pattern_t *gradient)
{
    return TRUE;    /* XXX no gradient support */
#if 0
    int i;
    
    for (i = 0; i < gradient->n_stops; i++)
	if (color_is_translucent (&gradient->stops[i].color))
	    return TRUE;
    return FALSE;
#endif
}

static cairo_bool_t
pattern_is_translucent (const cairo_pattern_t *abstract_pattern)
{
    const cairo_pattern_union_t *pattern;

    pattern = (cairo_pattern_union_t *) abstract_pattern;
    switch (pattern->base.type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return color_is_translucent (&pattern->solid.color);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return surface_is_translucent (pattern->surface.surface);
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	return gradient_is_translucent (&pattern->gradient.base);
    }	

    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
operator_always_opaque (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:

    case CAIRO_OPERATOR_SOURCE:
	return TRUE;
	
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return TRUE;
	
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return FALSE;
    }
    return FALSE;
}

static cairo_bool_t
operator_always_translucent (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:

    case CAIRO_OPERATOR_SOURCE:
	return FALSE;
	
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return FALSE;
	
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    }
    return TRUE;
}

static cairo_bool_t
color_operation_needs_fallback (cairo_operator_t op,
				const cairo_color_t *color)
{
    if (operator_always_opaque (op))
	return FALSE;
    if (operator_always_translucent (op))
	return TRUE;
    return color_is_translucent (color);
}

static cairo_bool_t
pattern_type_supported (const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;
    return FALSE;
}

static cairo_bool_t
pattern_operation_needs_fallback (cairo_operator_t op,
				  const cairo_pattern_t *pattern)
{
    if (! pattern_type_supported (pattern))
	return TRUE;
    if (operator_always_opaque (op))
	return FALSE;
    if (operator_always_translucent (op))
	return TRUE;
    return pattern_is_translucent (pattern);
}

/* PS Output - this section handles output of the parts of the meta
 * surface we can render natively in PS. */

static cairo_status_t
emit_image (cairo_ps_surface_t    *surface,
	    cairo_image_surface_t *image,
	    cairo_matrix_t	  *matrix)
{
    cairo_status_t status;
    unsigned char *rgb, *compressed;
    unsigned long rgb_size, compressed_size;
    cairo_surface_t *opaque;
    cairo_image_surface_t *opaque_image;
    cairo_pattern_union_t pattern;
    cairo_matrix_t d2i;
    int x, y, i;

    /* PostScript can not represent the alpha channel, so we blend the
       current image over a white RGB surface to eliminate it. */

    if (image->base.status)
	return image->base.status;

    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					     image->width,
					     image->height);
	if (opaque->status) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto bail0;
	}
    
	_cairo_pattern_init_for_surface (&pattern.surface, &image->base);
    
	_cairo_surface_fill_rectangle (opaque,
				       CAIRO_OPERATOR_SOURCE,
				       CAIRO_COLOR_WHITE,
				       0, 0, image->width, image->height);

	_cairo_surface_composite (CAIRO_OPERATOR_OVER,
				  &pattern.base,
				  NULL,
				  opaque,
				  0, 0,
				  0, 0,
				  0, 0,
				  image->width,
				  image->height);
    
	_cairo_pattern_fini (&pattern.base);
	opaque_image = (cairo_image_surface_t *) opaque;
    } else {
	opaque = &image->base;
	opaque_image = image;
    }

    rgb_size = 3 * opaque_image->width * opaque_image->height;
    rgb = malloc (rgb_size);
    if (rgb == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail1;
    }

    i = 0;
    for (y = 0; y < opaque_image->height; y++) {
	pixman_bits_t *pixel = (pixman_bits_t *) (opaque_image->data + y * opaque_image->stride);
	for (x = 0; x < opaque_image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compressed = _cairo_compress_lzw (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail2;
    }

    /* matrix transforms from user space to image space.  We need to
     * transform from device space to image space to compensate for
     * postscripts coordinate system. */
    cairo_matrix_init (&d2i, 1, 0, 0, 1, 0, 0);
    cairo_matrix_multiply (&d2i, &d2i, matrix);

    _cairo_output_stream_printf (surface->stream,
				 "/DeviceRGB setcolorspace\n"
				 "<<\n"
				 "	/ImageType 1\n"
				 "	/Width %d\n"
				 "	/Height %d\n"
				 "	/BitsPerComponent 8\n"
				 "	/Decode [ 0 1 0 1 0 1 ]\n"
				 "	/DataSource currentfile /ASCII85Decode filter /LZWDecode filter \n"
				 "	/ImageMatrix [ %f %f %f %f %f %f ]\n"
				 ">>\n"
				 "image\n",
				 opaque_image->width,
				 opaque_image->height,
				 d2i.xx, d2i.yx,
				 d2i.xy, d2i.yy,
				 d2i.x0, d2i.y0);

    /* Compressed image data (Base85 encoded) */
    _cairo_output_stream_write_base85_string (surface->stream, (char *)compressed, compressed_size);
    status = CAIRO_STATUS_SUCCESS;

    /* Mark end of base85 data */
    _cairo_output_stream_printf (surface->stream,
				 "~>\n");
    free (compressed);
 bail2:
    free (rgb);
 bail1:
    if (opaque_image != image)
	cairo_surface_destroy (opaque);
 bail0:
    return status;
}

static void
emit_solid_pattern (cairo_ps_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    if (color_is_gray (&pattern->color))
	_cairo_output_stream_printf (surface->stream,
				     "%f setgray\n",
				     pattern->color.red);
    else
	_cairo_output_stream_printf (surface->stream,
				     "%f %f %f setrgbcolor\n",
				     pattern->color.red,
				     pattern->color.green,
				     pattern->color.blue);
}

static void
emit_surface_pattern (cairo_ps_surface_t *surface,
		      cairo_surface_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_linear_pattern (cairo_ps_surface_t *surface,
		     cairo_linear_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_radial_pattern (cairo_ps_surface_t *surface,
		     cairo_radial_pattern_t *pattern)
{
    /* XXX: NYI */
}

static void
emit_pattern (cairo_ps_surface_t *surface, cairo_pattern_t *pattern)
{
    /* FIXME: We should keep track of what pattern is currently set in
     * the postscript file and only emit code if we're setting a
     * different pattern. */

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
	break;	    
    }
}


static cairo_int_status_t
_cairo_ps_surface_composite (cairo_operator_t	op,
		      cairo_pattern_t  *src_pattern,
		      cairo_pattern_t  *mask_pattern,
		      void	       *abstract_dst,
		      int		src_x,
		      int		src_y,
		      int		mask_x,
		      int		mask_y,
		      int		dst_x,
		      int		dst_y,
		      unsigned int	width,
		      unsigned int	height)
{
    cairo_ps_surface_t *surface = abstract_dst;
    cairo_output_stream_t *stream = surface->stream;
    cairo_surface_pattern_t *surface_pattern;
    cairo_status_t status;
    cairo_image_surface_t *image;
    void *image_extra;

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);

    if (mask_pattern) {
	/* FIXME: Investigate how this can be done... we'll probably
	 * need pixmap fallbacks for this, though. */
	_cairo_output_stream_printf (stream,
				     "%% _cairo_ps_surface_composite: with mask\n");
	goto bail;
    }

    status = CAIRO_STATUS_SUCCESS;
    switch (src_pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	_cairo_output_stream_printf (stream,
				     "%% _cairo_ps_surface_composite: solid\n");
	goto bail;

    case CAIRO_PATTERN_TYPE_SURFACE:
	surface_pattern = (cairo_surface_pattern_t *) src_pattern;

	if (src_pattern->extend != CAIRO_EXTEND_NONE) {
	    _cairo_output_stream_printf (stream,
					 "%% _cairo_ps_surface_composite: repeating image\n");
	    goto bail;
	}
	    

	status = _cairo_surface_acquire_source_image (surface_pattern->surface,
						      &image,
						      &image_extra);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_output_stream_printf (stream,
					 "%% _cairo_ps_surface_composite: src_pattern not available as image\n");
	    goto bail;
	} else if (status) {
	    break;
	}
	status = emit_image (surface, image, &src_pattern->matrix);
	_cairo_surface_release_source_image (surface_pattern->surface,
					     image, image_extra);
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	_cairo_output_stream_printf (stream,
				     "%% _cairo_ps_surface_composite: gradient\n");
	goto bail;
    }

    return status;
bail:
    return _cairo_ps_surface_add_fallback_area (surface, dst_x, dst_y, width, height);
}

static cairo_int_status_t
_cairo_ps_surface_fill_rectangles (void		*abstract_surface,
			    cairo_operator_t	 op,
			    const cairo_color_t	*color,
			    cairo_rectangle_t	*rects,
			    int			 num_rects)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_solid_pattern_t solid;
    int i;

    if (!num_rects)
	return CAIRO_STATUS_SUCCESS;

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);
    
    if (color_operation_needs_fallback (op, color)) {
	int min_x = rects[0].x;
	int min_y = rects[0].y;
	int max_x = rects[0].x + rects[0].width;
	int max_y = rects[0].y + rects[0].height;

	for (i = 1; i < num_rects; i++) {
	    if (rects[i].x < min_x) min_x = rects[i].x;
	    if (rects[i].y < min_y) min_y = rects[i].y;
	    if (rects[i].x + rects[i].width > max_x) max_x = rects[i].x + rects[i].width;
	    if (rects[i].y + rects[i].height > max_y) max_y = rects[i].y + rects[i].height;
	}
	return _cairo_ps_surface_add_fallback_area (surface, min_x, min_y, max_x - min_x, max_y - min_y);
    }
	
    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_fill_rectangles\n");

    _cairo_pattern_init_solid (&solid, color);
    emit_pattern (surface, &solid.base);
    _cairo_pattern_fini (&solid.base);

    _cairo_output_stream_printf (stream, "[");
    for (i = 0; i < num_rects; i++) {
      _cairo_output_stream_printf (stream,
				   " %d %d %d %d",
				   rects[i].x, rects[i].y,
				   rects[i].width, rects[i].height);
    }

    _cairo_output_stream_printf (stream, " ] rectfill\n");

    return CAIRO_STATUS_SUCCESS;
}

static double
intersect (cairo_line_t *line, cairo_fixed_t y)
{
    return _cairo_fixed_to_double (line->p1.x) +
	_cairo_fixed_to_double (line->p2.x - line->p1.x) *
	_cairo_fixed_to_double (y - line->p1.y) /
	_cairo_fixed_to_double (line->p2.y - line->p1.y);
}

static cairo_int_status_t
_cairo_ps_surface_composite_trapezoids (cairo_operator_t	op,
				 cairo_pattern_t	*pattern,
				 void			*abstract_dst,
				 cairo_antialias_t	antialias,
				 int			x_src,
				 int			y_src,
				 int			x_dst,
				 int			y_dst,
				 unsigned int		width,
				 unsigned int		height,
				 cairo_trapezoid_t	*traps,
				 int			num_traps)
{
    cairo_ps_surface_t *surface = abstract_dst;
    cairo_output_stream_t *stream = surface->stream;
    int i;

    if (pattern_operation_needs_fallback (op, pattern))
	return _cairo_ps_surface_add_fallback_area (surface, x_dst, y_dst, width, height);

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_composite_trapezoids\n");

    emit_pattern (surface, pattern);

    for (i = 0; i < num_traps; i++) {
	double left_x1, left_x2, right_x1, right_x2, top, bottom;

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);
	top      = _cairo_fixed_to_double (traps[i].top);
	bottom   = _cairo_fixed_to_double (traps[i].bottom);

	_cairo_output_stream_printf
	    (stream,
	     "%f %f moveto %f %f lineto %f %f lineto %f %f lineto "
	     "closepath\n",
	     left_x1, top,
	     left_x2, bottom,
	     right_x2, bottom,
	     right_x1, top);
    }

    _cairo_output_stream_printf (stream,
				 "fill\n");

    return CAIRO_STATUS_SUCCESS;
}

typedef struct
{
    cairo_output_stream_t *output_stream;
    cairo_bool_t has_current_point;
} cairo_ps_surface_path_info_t;

static cairo_status_t
_cairo_ps_surface_path_move_to (void *closure, cairo_point_t *point)
{
    cairo_ps_surface_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f moveto ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));
    info->has_current_point = TRUE;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_line_to (void *closure, cairo_point_t *point)
{
    cairo_ps_surface_path_info_t *info = closure;
    const char *ps_operator;

    if (info->has_current_point)
	ps_operator = "lineto";
    else
	ps_operator = "moveto";
    
    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %s ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y),
				 ps_operator);
    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    cairo_ps_surface_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %f %f %f %f curveto ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ps_surface_path_close_path (void *closure)
{
    cairo_ps_surface_path_info_t *info = closure;
    
    _cairo_output_stream_printf (info->output_stream,
				 "closepath\n");
    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ps_surface_intersect_clip_path (void		   *abstract_surface,
				cairo_path_fixed_t *path,
				cairo_fill_rule_t   fill_rule,
				double		    tolerance,
				cairo_antialias_t   antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_status_t status;
    cairo_ps_surface_path_info_t info;
    const char *ps_operator;

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_intersect_clip_path\n");

    if (path == NULL) {
	_cairo_output_stream_printf (stream, "initclip\n");
	return CAIRO_STATUS_SUCCESS;
    }

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "clip";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eoclip";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s newpath\n",
				 ps_operator);

    return status;
}

static cairo_int_status_t
_cairo_ps_surface_get_extents (void		  *abstract_surface,
			       cairo_rectangle_t *rectangle)
{
    cairo_ps_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_extents.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
}

#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
static cairo_int_status_t
_cairo_ps_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
			    cairo_operator_t	 op,
			    cairo_pattern_t	*pattern,
			    void		*abstract_surface,
			    int			 source_x,
			    int			 source_y,
			    int			 dest_x,
			    int			 dest_y,
			    unsigned int	 width,
			    unsigned int	 height,
			    const cairo_glyph_t	*glyphs,
			    int			 num_glyphs)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_font_subset_t *subset;
    int i, subset_index;

    if (surface->fallback)
	return CAIRO_STATUS_SUCCESS;

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->fallback)
	return CAIRO_STATUS_SUCCESS;

    if (pattern_operation_needs_fallback (op, pattern))
	return _cairo_ps_surface_add_fallback_area (surface, dest_x, dest_y, width, height);

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_old_show_glyphs\n");

    emit_pattern (surface, pattern);

    /* FIXME: Need to optimize this so we only do this sequence if the
     * font isn't already set. */

    subset = _cairo_ps_surface_get_font (surface, scaled_font);
    _cairo_output_stream_printf (stream,
				 "/f%d findfont\n"
				 "[ %f %f %f %f 0 0 ] makefont\n"
				 "setfont\n",
				 subset->font_id,
				 scaled_font->scale.xx,
				 scaled_font->scale.yx,
				 scaled_font->scale.xy,
				 -scaled_font->scale.yy);

    /* FIXME: Need to optimize per glyph code.  Should detect when
     * glyphs share the same baseline and when the spacing corresponds
     * to the glyph widths. */

    for (i = 0; i < num_glyphs; i++) {
	subset_index = _cairo_font_subset_use_glyph (subset, glyphs[i].index);
	_cairo_output_stream_printf (stream,
				     "%f %f moveto (\\%o) show\n",
				     glyphs[i].x,
				     glyphs[i].y,
				     subset_index);
	
    }

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_int_status_t
_cairo_ps_surface_fill (void			*abstract_surface,
		 cairo_operator_t	 op,
		 cairo_pattern_t	*source,
		 cairo_path_fixed_t	*path,
		 cairo_fill_rule_t	 fill_rule,
		 double			 tolerance,
		 cairo_antialias_t	 antialias)
{
    cairo_ps_surface_t *surface = abstract_surface;
    cairo_output_stream_t *stream = surface->stream;
    cairo_int_status_t status;
    cairo_ps_surface_path_info_t info;
    const char *ps_operator;

    if (pattern_operation_needs_fallback (op, source))
	return _cairo_ps_surface_add_fallback_area (surface,
					     0, 0,
					     surface->width,
					     surface->height);

    if (surface->need_start_page)
	_cairo_ps_surface_start_page (surface);

    _cairo_output_stream_printf (stream,
				 "%% _cairo_ps_surface_fill\n");

    emit_pattern (surface, source);

    info.output_stream = stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_ps_surface_path_move_to,
					  _cairo_ps_surface_path_line_to,
					  _cairo_ps_surface_path_curve_to,
					  _cairo_ps_surface_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	ps_operator = "fill";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	ps_operator = "eofill";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (stream,
				 "%s\n", ps_operator);

    return status;
}

static const cairo_surface_backend_t cairo_ps_surface_backend = {
    CAIRO_SURFACE_TYPE_PS,
    NULL, /* create_similar */
    _cairo_ps_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    _cairo_ps_surface_composite,
    _cairo_ps_surface_fill_rectangles,
    _cairo_ps_surface_composite_trapezoids,
    _cairo_ps_surface_copy_page,
    _cairo_ps_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_ps_surface_intersect_clip_path,
    _cairo_ps_surface_get_extents,
#if DONE_ADDING_FONTS_SUPPORT_BACK_AFTER_SWITCHING_TO_PAGINATED
    _cairo_ps_surface_old_show_glyphs,
#else
    NULL, /* old_show_glyphs */
#endif
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */
    
    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    _cairo_ps_surface_fill,
    NULL /* show_glyphs */
};
