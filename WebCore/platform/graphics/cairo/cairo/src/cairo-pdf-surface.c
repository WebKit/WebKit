/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-pdf.h"
#include "cairo-font-subset-private.h"
#include "cairo-ft-private.h"
#include "cairo-paginated-surface-private.h"

#include <time.h>
#include <zlib.h>

/* Issues:
 *
 * - Why doesn't pages inherit /alpha%d GS dictionaries from the Pages
 *   object?
 *
 * - We embed an image in the stream each time it's composited.  We
 *   could add generation counters to surfaces and remember the stream
 *   ID for a particular generation for a particular surface.
 *
 * - Clipping: must be able to reset clipping
 *
 * - Images of other formats than 8 bit RGBA.
 *
 * - Backend specific meta data.
 *
 * - Surface patterns.
 *
 * - Alpha channels in gradients.
 *
 * - Should/does cairo support drawing into a scratch surface and then
 *   using that as a fill pattern?  For this backend, that would involve
 *   using a tiling pattern (4.6.2).  How do you create such a scratch
 *   surface?  cairo_surface_create_similar() ?
 *
 * - What if you create a similiar surface and does show_page and then
 *   does show_surface on another surface?
 *
 * - Output TM so page scales to the right size - PDF default user
 *   space has 1 unit = 1 / 72 inch.
 *
 * - Add test case for RGBA images.
 *
 * - Add test case for RGBA gradients.
 *
 * - Coordinate space for create_similar() args?
 *
 * - Investigate /Matrix entry in content stream dicts for pages
 *   instead of outputting the cm operator in every page.
 */

typedef struct cairo_pdf_object cairo_pdf_object_t;
typedef struct cairo_pdf_resource cairo_pdf_resource_t;
typedef struct cairo_pdf_stream cairo_pdf_stream_t;
typedef struct cairo_pdf_document cairo_pdf_document_t;
typedef struct cairo_pdf_surface cairo_pdf_surface_t;

struct cairo_pdf_object {
    long offset;
};

struct cairo_pdf_resource {
    unsigned int id;
};

struct cairo_pdf_stream {
    unsigned int id;
    unsigned int length_id;
    long start_offset;
};

struct cairo_pdf_document {
    cairo_output_stream_t *output_stream;
    unsigned long ref_count;
    cairo_surface_t *owner;
    cairo_bool_t finished;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    unsigned int next_available_id;
    unsigned int pages_id;

    cairo_pdf_stream_t *current_stream;

    cairo_array_t objects;
    cairo_array_t pages;

    cairo_array_t fonts;
};

struct cairo_pdf_surface {
    cairo_surface_t base;

    double width;
    double height;

    cairo_pdf_document_t *document;
    cairo_pdf_stream_t *current_stream;

    cairo_array_t patterns;
    cairo_array_t xobjects;
    cairo_array_t streams;
    cairo_array_t alphas;
    cairo_array_t fonts;
    cairo_bool_t has_clip;
};

#define DEFAULT_DPI 300

static cairo_pdf_document_t *
_cairo_pdf_document_create (cairo_output_stream_t	*stream,
			    double			width,
			    double			height);

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_finish (cairo_pdf_document_t *document);

static cairo_pdf_document_t *
_cairo_pdf_document_reference (cairo_pdf_document_t *document);

static unsigned int
_cairo_pdf_document_new_object (cairo_pdf_document_t *document);

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t *document,
			      cairo_pdf_surface_t *surface);

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface);

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*fmt,
				 ...);
static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document);

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width,
					double			height);
static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream);
static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t	*surface);

static const cairo_surface_backend_t cairo_pdf_surface_backend;

static unsigned int
_cairo_pdf_document_new_object (cairo_pdf_document_t *document)
{
    cairo_status_t status;
    cairo_pdf_object_t object;

    object.offset = _cairo_output_stream_get_position (document->output_stream);

    status = _cairo_array_append (&document->objects, &object);
    if (status)
	return 0;

    return document->next_available_id++;
}

static void
_cairo_pdf_document_update_object (cairo_pdf_document_t *document,
				   unsigned int id)
{
    cairo_pdf_object_t *object;

    object = _cairo_array_index (&document->objects, id - 1);
    object->offset = _cairo_output_stream_get_position (document->output_stream);
}

static void
_cairo_pdf_surface_add_stream (cairo_pdf_surface_t	*surface,
			       cairo_pdf_stream_t	*stream)
{
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->streams, &stream);
    surface->current_stream = stream;
}

static void
_cairo_pdf_surface_add_pattern (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;

    resource.id = id;
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->patterns, &resource);
}

static void
_cairo_pdf_surface_add_xobject (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;
    int i, num_resources;

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    for (i = 0; i < num_resources; i++) {
	_cairo_array_copy_element (&surface->xobjects, i, &resource);
	if (resource.id == id)
	    return;
    }

    resource.id = id;
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->xobjects, &resource);
}

static unsigned int
_cairo_pdf_surface_add_alpha (cairo_pdf_surface_t *surface, double alpha)
{
    int num_alphas, i;
    double other;

    num_alphas = _cairo_array_num_elements (&surface->alphas);
    for (i = 0; i < num_alphas; i++) {
	_cairo_array_copy_element (&surface->alphas, i, &other);
	if (alpha == other)
	    return i;
    }

    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->alphas, &alpha);
    return _cairo_array_num_elements (&surface->alphas) - 1;
}

static void
_cairo_pdf_surface_add_font (cairo_pdf_surface_t *surface, unsigned int id)
{
    cairo_pdf_resource_t resource;
    int i, num_fonts;

    num_fonts = _cairo_array_num_elements (&surface->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&surface->fonts, i, &resource);
	if (resource.id == id)
	    return;
    }

    resource.id = id;
    /* XXX: Should be checking the return value here. */
    _cairo_array_append (&surface->fonts, &resource);
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			 width,
					       double			 height)
{
    cairo_pdf_document_t *document;
    cairo_surface_t *target;

    document = _cairo_pdf_document_create (stream, width, height);
    if (document == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    target = _cairo_pdf_surface_create_for_document (document, width, height);

    document->owner = target;
    _cairo_pdf_document_destroy (document);

    return _cairo_paginated_surface_create (target,
					    CAIRO_CONTENT_COLOR_ALPHA,
					    width, height);
}

/**
 * cairo_pdf_surface_create_for_stream:
 * @write: a #cairo_write_func_t to accept the output data
 * @closure: the closure argument for @write
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PDF surface of the specified size in points to be written
 * incrementally to the stream represented by @write and @closure.
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
cairo_pdf_surface_create_for_stream (cairo_write_func_t		 write,
				     void			*closure,
				     double			 width_in_points,
				     double			 height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write, closure);
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_pdf_surface_create_for_stream_internal (stream,
							  width_in_points,
							  height_in_points);
}

/**
 * cairo_pdf_surface_create:
 * @filename: a filename for the PDF output (must be writable)
 * @width_in_points: width of the surface, in points (1 point == 1/72.0 inch)
 * @height_in_points: height of the surface, in points (1 point == 1/72.0 inch)
 * 
 * Creates a PDF surface of the specified size in points to be written
 * to @filename.
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
cairo_pdf_surface_create (const char		*filename,
			  double		 width_in_points,
			  double		 height_in_points)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    if (stream == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return _cairo_pdf_surface_create_for_stream_internal (stream,
							  width_in_points,
							  height_in_points);
}

static cairo_bool_t
_cairo_surface_is_pdf (cairo_surface_t *surface)
{
    return surface->backend == &cairo_pdf_surface_backend;
}

/**
 * cairo__surface_set_dpi:
 * @surface: a postscript cairo_surface_t
 * @x_dpi: horizontal dpi
 * @y_dpi: vertical dpi
 * 
 * Set the horizontal and vertical resolution for image fallbacks.
 * When the pdf backend needs to fall back to image overlays, it will
 * use this resolution. These DPI values are not used for any other
 * purpose, (in particular, they do not have any bearing on the size
 * passed to cairo_pdf_surface_create() nor on the CTM).
 **/
void
cairo_pdf_surface_set_dpi (cairo_surface_t	*surface,
			   double		x_dpi,
			   double		y_dpi)
{
    cairo_surface_t *target;
    cairo_pdf_surface_t *pdf_surface;

    if (! _cairo_surface_is_paginated (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    target = _cairo_paginated_surface_get_target (surface);

    if (! _cairo_surface_is_pdf (surface)) {
	_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    pdf_surface = (cairo_pdf_surface_t *) target;

    pdf_surface->document->x_dpi = x_dpi;    
    pdf_surface->document->y_dpi = y_dpi;    
}

static cairo_surface_t *
_cairo_pdf_surface_create_for_document (cairo_pdf_document_t	*document,
					double			width,
					double			height)
{
    cairo_pdf_surface_t *surface;

    surface = malloc (sizeof (cairo_pdf_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &cairo_pdf_surface_backend);

    surface->width = width;
    surface->height = height;

    surface->document = _cairo_pdf_document_reference (document);
    _cairo_array_init (&surface->streams, sizeof (cairo_pdf_stream_t *));
    _cairo_array_init (&surface->patterns, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->xobjects, sizeof (cairo_pdf_resource_t));
    _cairo_array_init (&surface->alphas, sizeof (double));
    _cairo_array_init (&surface->fonts, sizeof (cairo_pdf_resource_t));
    surface->has_clip = FALSE;

    return &surface->base;
}

static void
_cairo_pdf_surface_clear (cairo_pdf_surface_t *surface)
{
    int num_streams, i;
    cairo_pdf_stream_t *stream;

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);
	free (stream);
    }

    _cairo_array_truncate (&surface->streams, 0);
    _cairo_array_truncate (&surface->patterns, 0);
    _cairo_array_truncate (&surface->xobjects, 0);
    _cairo_array_truncate (&surface->alphas, 0);
    _cairo_array_truncate (&surface->fonts, 0);
}

static cairo_surface_t *
_cairo_pdf_surface_create_similar (void		       *abstract_src,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    cairo_pdf_surface_t *template = abstract_src;

    return _cairo_pdf_surface_create_for_document (template->document,
						   width, height);
}

static cairo_pdf_stream_t *
_cairo_pdf_document_open_stream (cairo_pdf_document_t	*document,
				 const char		*fmt,
				 ...)
{
    cairo_output_stream_t *output_stream = document->output_stream;
    cairo_pdf_stream_t *stream;
    va_list ap;

    stream = malloc (sizeof (cairo_pdf_stream_t));
    if (stream == NULL) {
	return NULL;
    }

    stream->id = _cairo_pdf_document_new_object (document);
    stream->length_id = _cairo_pdf_document_new_object (document);

    _cairo_output_stream_printf (output_stream,
				 "%d 0 obj\r\n"
				 "<< /Length %d 0 R\r\n",
				 stream->id,
				 stream->length_id);

    if (fmt != NULL) {
	va_start (ap, fmt);
	_cairo_output_stream_vprintf (output_stream, fmt, ap);
	va_end (ap);
    }

    _cairo_output_stream_printf (output_stream,
				 ">>\r\n"
				 "stream\r\n");

    stream->start_offset = _cairo_output_stream_get_position (output_stream);

    document->current_stream = stream;

    return stream;
}

static void
_cairo_pdf_document_close_stream (cairo_pdf_document_t	*document)
{
    cairo_output_stream_t *output_stream = document->output_stream;
    long length;
    cairo_pdf_stream_t *stream;

    stream = document->current_stream;
    if (stream == NULL)
	return;

    length = _cairo_output_stream_get_position (output_stream) -
	stream->start_offset;
    _cairo_output_stream_printf (output_stream,
				 "endstream\r\n"
				 "endobj\r\n");

    _cairo_pdf_document_update_object (document, stream->length_id);
    _cairo_output_stream_printf (output_stream,
				 "%d 0 obj\r\n"
				 "   %ld\r\n"
				 "endobj\r\n",
				 stream->length_id,
				 length);

    document->current_stream = NULL;
}

static cairo_status_t
_cairo_pdf_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    if (surface->current_stream == document->current_stream)
	_cairo_pdf_document_close_stream (document);

    if (document->owner == &surface->base)
	status = _cairo_pdf_document_finish (document);
    else
	status = CAIRO_STATUS_SUCCESS;

    _cairo_pdf_document_destroy (document);

    _cairo_array_fini (&surface->streams);
    _cairo_array_fini (&surface->patterns);
    _cairo_array_fini (&surface->xobjects);
    _cairo_array_fini (&surface->alphas);
    _cairo_array_fini (&surface->fonts);

    return status;
}

static void
_cairo_pdf_surface_ensure_stream (cairo_pdf_surface_t *surface)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_pdf_stream_t *stream;
    cairo_output_stream_t *output = document->output_stream;

    if (document->current_stream == NULL ||
	document->current_stream != surface->current_stream) {
	_cairo_pdf_document_close_stream (document);
	stream = _cairo_pdf_document_open_stream (document,
						  "   /Type /XObject\r\n"
						  "   /Subtype /Form\r\n"
						  "   /BBox [ 0 0 %f %f ]\r\n",
						  surface->width,
						  surface->height);

	_cairo_pdf_surface_add_stream (surface, stream);

	/* If this is the first stream we open for this surface,
	 * output the cairo to PDF transformation matrix. */
	if (_cairo_array_num_elements (&surface->streams) == 1)
	    _cairo_output_stream_printf (output,
					 "1 0 0 -1 0 %f cm\r\n",
					 document->height);
    }
}

static void *
compress_dup (const void *data, unsigned long data_size,
	      unsigned long *compressed_size)
{
    void *compressed;

    /* Bound calculation taken from zlib. */
    *compressed_size = data_size + (data_size >> 12) + (data_size >> 14) + 11;
    compressed = malloc (*compressed_size);
    if (compressed == NULL)
	return NULL;

    compress (compressed, compressed_size, data, data_size);

    return compressed;
}

/* XXX: This should be rewritten to use the standard cairo_status_t
 * return and the error paths here need to be checked for memory
 * leaks. */
static unsigned int
emit_image_rgb_data (cairo_pdf_document_t *document,
		     cairo_image_surface_t *image)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_stream_t *stream;
    char *rgb, *compressed;
    int i, x, y;
    unsigned long rgb_size, compressed_size;
    pixman_bits_t *pixel;
    cairo_surface_t *opaque;
    cairo_image_surface_t *opaque_image;
    cairo_pattern_union_t pattern;

    rgb_size = image->height * image->width * 3;
    rgb = malloc (rgb_size);
    if (rgb == NULL)
	return 0;

    /* XXX: We could actually output the alpha channels through PDF
     * 1.4's SMask. But for now, all we support is opaque image data,
     * so we must flatten any ARGB image by blending over white
     * first. */
    if (image->format != CAIRO_FORMAT_RGB24) {
	opaque = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
					     image->width,
					     image->height);
	if (opaque->status) {
	    free (rgb);
	    return 0;
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

    i = 0;
    for (y = 0; y < image->height; y++) {
	pixel = (pixman_bits_t *) (opaque_image->data + y * opaque_image->stride);

	for (x = 0; x < opaque_image->width; x++, pixel++) {
	    rgb[i++] = (*pixel & 0x00ff0000) >> 16;
	    rgb[i++] = (*pixel & 0x0000ff00) >>  8;
	    rgb[i++] = (*pixel & 0x000000ff) >>  0;
	}
    }

    compressed = compress_dup (rgb, rgb_size, &compressed_size);
    if (compressed == NULL) {
	free (rgb);
	return 0;
    }

    _cairo_pdf_document_close_stream (document);

    stream = _cairo_pdf_document_open_stream (document, 
					      "   /Type /XObject\r\n"
					      "   /Subtype /Image\r\n"
					      "   /Width %d\r\n"
					      "   /Height %d\r\n"
					      "   /ColorSpace /DeviceRGB\r\n"
					      "   /BitsPerComponent 8\r\n"
					      "   /Filter /FlateDecode\r\n",
					      image->width, image->height);

    _cairo_output_stream_write (output, compressed, compressed_size);
    _cairo_output_stream_printf (output,
				 "\r\n");
    _cairo_pdf_document_close_stream (document);

    free (rgb);
    free (compressed);

    if (opaque_image != image)
	cairo_surface_destroy (opaque);

    return stream->id;
}

static cairo_int_status_t
_cairo_pdf_surface_composite_image (cairo_pdf_surface_t	*dst,
				    cairo_surface_pattern_t *pattern)
{
    cairo_pdf_document_t *document = dst->document;
    cairo_output_stream_t *output = document->output_stream;
    unsigned id;
    cairo_matrix_t i2u;
    cairo_status_t status;
    cairo_image_surface_t *image;
    cairo_surface_t *src;
    void *image_extra;

    src = pattern->surface;
    status = _cairo_surface_acquire_source_image (src, &image, &image_extra);
    if (status)
	return status;

    id = emit_image_rgb_data (dst->document, image);
    if (id == 0) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto bail;
    }

    _cairo_pdf_surface_add_xobject (dst, id);

    _cairo_pdf_surface_ensure_stream (dst);

    i2u = pattern->base.matrix;
    cairo_matrix_invert (&i2u);
    cairo_matrix_translate (&i2u, 0, image->height);
    cairo_matrix_scale (&i2u, image->width, -image->height);

    _cairo_output_stream_printf (output,
				 "q %f %f %f %f %f %f cm /res%d Do Q\r\n",
				 i2u.xx, i2u.yx,
				 i2u.xy, i2u.yy,
				 i2u.x0, i2u.y0,
				 id);

 bail:
    _cairo_surface_release_source_image (src, image, image_extra);

    return status;
}

/* The contents of the surface is already transformed into PDF units,
 * but when we composite the surface we may want to use a different
 * space.  The problem I see now is that the show_surface snippet
 * creates a surface 1x1, which in the snippet environment is the
 * entire surface.  When compositing the surface, cairo gives us the
 * 1x1 to 256x256 matrix.  This would be fine if cairo didn't actually
 * also transform the drawing to the surface.  Should the CTM be part
 * of the current target surface?
 */

static cairo_int_status_t
_cairo_pdf_surface_composite_pdf (cairo_pdf_surface_t *dst,
				  cairo_surface_pattern_t *pattern)
{
    cairo_pdf_document_t *document = dst->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_matrix_t i2u;
    cairo_pdf_stream_t *stream;
    int num_streams, i;
    cairo_pdf_surface_t *src;

    _cairo_pdf_surface_ensure_stream (dst);

    src = (cairo_pdf_surface_t *) pattern->surface;

    i2u = pattern->base.matrix;
    cairo_matrix_invert (&i2u);
    cairo_matrix_scale (&i2u, 1.0 / src->width, 1.0 / src->height);

    _cairo_output_stream_printf (output,
				 "q %f %f %f %f %f %f cm",
				 i2u.xx, i2u.yx,
				 i2u.xy, i2u.yy,
				 i2u.x0, i2u.y0);

    num_streams = _cairo_array_num_elements (&src->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&src->streams, i, &stream);
	_cairo_output_stream_printf (output,
				     " /res%d Do",
				     stream->id);

	_cairo_pdf_surface_add_xobject (dst, stream->id);

    }
	
    _cairo_output_stream_printf (output, " Q\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_composite (cairo_operator_t	op,
			      cairo_pattern_t	*src_pattern,
			      cairo_pattern_t	*mask_pattern,
			      void		*abstract_dst,
			      int		src_x,
			      int		src_y,
			      int		mask_x,
			      int		mask_y,
			      int		dst_x,
			      int		dst_y,
			      unsigned int	width,
			      unsigned int	height)
{
    cairo_pdf_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) src_pattern;

    if (mask_pattern)
 	return CAIRO_STATUS_SUCCESS;
    
    if (src_pattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_STATUS_SUCCESS;

    if (src->surface->backend == &cairo_pdf_surface_backend)
	return _cairo_pdf_surface_composite_pdf (dst, src);
    else
	return _cairo_pdf_surface_composite_image (dst, src);
}

static cairo_int_status_t
_cairo_pdf_surface_fill_rectangles (void		*abstract_surface,
				    cairo_operator_t	op,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			num_rects)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    int i;

    _cairo_pdf_surface_ensure_stream (surface);

    _cairo_output_stream_printf (output,
				 "%f %f %f rg\r\n",
				 color->red, color->green, color->blue);

    for (i = 0; i < num_rects; i++) {
	_cairo_output_stream_printf (output,
				     "%d %d %d %d re f\r\n",
				     rects[i].x, rects[i].y,
				     rects[i].width, rects[i].height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
emit_solid_pattern (cairo_pdf_surface_t *surface,
		    cairo_solid_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    unsigned int alpha;
    
    alpha = _cairo_pdf_surface_add_alpha (surface, pattern->color.alpha);
    _cairo_pdf_surface_ensure_stream (surface);
    _cairo_output_stream_printf (output,
				 "%f %f %f rg /a%d gs\r\n",
				 pattern->color.red,
				 pattern->color.green,
				 pattern->color.blue,
				 alpha);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
emit_surface_pattern (cairo_pdf_surface_t	*dst,
		      cairo_surface_pattern_t	*pattern)
{
    cairo_pdf_document_t *document = dst->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_stream_t *stream;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;
    unsigned int id, alpha;
    cairo_matrix_t pm;

    /* XXX: This is broken. We need new code here to actually emit the
     * PDF surface. */
    if (pattern->surface->backend == &cairo_pdf_surface_backend)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_acquire_source_image (pattern->surface, &image, &image_extra);
    if (status)
	return status;

    _cairo_pdf_document_close_stream (document);

    id = emit_image_rgb_data (dst->document, image);

    /* BBox must be smaller than XStep by YStep or acroread wont
     * display the pattern. */

    cairo_matrix_init_identity (&pm);
    cairo_matrix_scale (&pm, image->width, image->height);
    pm = pattern->base.matrix;
    cairo_matrix_invert (&pm);

    stream = _cairo_pdf_document_open_stream (document,
					      "   /BBox [ 0 0 256 256 ]\r\n"
					      "   /XStep 256\r\n"
					      "   /YStep 256\r\n"
					      "   /PatternType 1\r\n"
					      "   /TilingType 1\r\n"
					      "   /PaintType 1\r\n"
					      "   /Resources << /XObject << /res%d %d 0 R >> >>\r\n",
					      id, id);


    _cairo_output_stream_printf (output,
				 " /res%d Do\r\n",
				 id);

    _cairo_pdf_surface_add_pattern (dst, stream->id);

    _cairo_pdf_surface_ensure_stream (dst);
    alpha = _cairo_pdf_surface_add_alpha (dst, 1.0);
    _cairo_output_stream_printf (output,
				 "/Pattern cs /res%d scn /a%d gs\r\n",
				 stream->id, alpha);

    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return CAIRO_STATUS_SUCCESS;
}


typedef struct _cairo_pdf_color_stop {
    double	  offset;
    unsigned int  gradient_id;
    unsigned char color_char[4];
} cairo_pdf_color_stop_t;


static unsigned int
emit_linear_colorgradient (cairo_pdf_document_t   *document,
			   cairo_pdf_color_stop_t *stop1, 
			   cairo_pdf_color_stop_t *stop2)
{
    cairo_output_stream_t *output = document->output_stream;
    unsigned int function_id = _cairo_pdf_document_new_object (document);
    
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 0\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /Size [ 2 ]\r\n"
				 "   /BitsPerSample 8\r\n"
				 "   /Range [ 0 1 0 1 0 1 ]\r\n"
				 "   /Length 6\r\n"
				 ">>\r\n"
				 "stream\r\n",
				 function_id);

    _cairo_output_stream_write (output, stop1->color_char, 3);
    _cairo_output_stream_write (output, stop2->color_char, 3);
    _cairo_output_stream_printf (output,
				 "\r\n"
				 "endstream\r\n"
				 "endobj\r\n");
    
    return function_id;
}


static unsigned int
emit_stiched_colorgradient (cairo_pdf_document_t   *document,
			    unsigned int 	   n_stops,
			    cairo_pdf_color_stop_t stops[])
{
    cairo_output_stream_t *output = document->output_stream;
    unsigned int function_id;
    unsigned int i;
    
    /* emit linear gradients between pairs of subsequent stops... */
    for (i = 0; i < n_stops-1; i++) {
	stops[i].gradient_id = emit_linear_colorgradient (document, 
		    					  &stops[i], 
							  &stops[i+1]);
    }
    
    /* ... and stich them together */
    function_id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /FunctionType 3\r\n"
				 "   /Domain [ 0 1 ]\r\n"
				 "   /Functions [ ",
				 function_id);
    for (i = 0; i < n_stops-1; i++)
        _cairo_output_stream_printf (output, "%d 0 R ", stops[i].gradient_id);
    _cairo_output_stream_printf (output,
		    		 "]\r\n"
				 "   /Bounds [ ");
    for (i = 1; i < n_stops-1; i++)
        _cairo_output_stream_printf (output, "%f ", stops[i].offset);
    _cairo_output_stream_printf (output,
		    		 "]\r\n"
				 "   /Encode [ ");
    for (i = 1; i < n_stops; i++)
        _cairo_output_stream_printf (output, "0 1 ");
    _cairo_output_stream_printf (output, 
	    			 "]\r\n"
	    			 ">>\r\n"
				 "endobj\r\n");

    return function_id;
}

#define COLOR_STOP_EPSILLON 1e-6

static unsigned int
emit_pattern_stops (cairo_pdf_surface_t *surface, cairo_gradient_pattern_t *pattern)
{
    cairo_pdf_document_t   *document = surface->document;
    unsigned int	   function_id;
    cairo_pdf_color_stop_t *allstops, *stops;
    unsigned int 	   n_stops;
    unsigned int 	   i;

    function_id = _cairo_pdf_document_new_object (document);

    allstops = malloc ((pattern->n_stops + 2) * sizeof (cairo_pdf_color_stop_t));
    if (allstops == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return 0;
    }
    stops = &allstops[1];
    n_stops = pattern->n_stops;
    
    for (i = 0; i < pattern->n_stops; i++) {
	stops[i].color_char[0] = pattern->stops[i].color.red   >> 8;
	stops[i].color_char[1] = pattern->stops[i].color.green >> 8;
	stops[i].color_char[2] = pattern->stops[i].color.blue  >> 8;
	stops[i].color_char[3] = pattern->stops[i].color.alpha >> 8;
	stops[i].offset = _cairo_fixed_to_double (pattern->stops[i].x);
    }

    /* make sure first offset is 0.0 and last offset is 1.0. (Otherwise Acrobat
     * Reader chokes.) */
    if (stops[0].offset > COLOR_STOP_EPSILLON) {
	    memcpy (allstops, stops, sizeof (cairo_pdf_color_stop_t));
	    stops = allstops;
	    stops[0].offset = 0.0;
	    n_stops++;
    }
    if (stops[n_stops-1].offset < 1.0 - COLOR_STOP_EPSILLON) {
	    memcpy (&stops[n_stops], 
		    &stops[n_stops - 1], 
		    sizeof (cairo_pdf_color_stop_t));
	    stops[n_stops].offset = 1.0;
	    n_stops++;
    }
    
    if (n_stops == 2) {
	/* no need for stiched function */
	function_id = emit_linear_colorgradient (document, &stops[0], &stops[1]);
    } else {
	/* multiple stops: stich. XXX possible optimization: regulary spaced
	 * stops do not require stiching. XXX */
	function_id = emit_stiched_colorgradient (document, 
						  n_stops, 
						  stops);
    }

    free (allstops);

    return function_id;
}

static cairo_status_t
emit_linear_pattern (cairo_pdf_surface_t *surface, cairo_linear_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, &pattern->base);
    if (function_id == 0)
	return CAIRO_STATUS_NO_MEMORY;

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    x0 = _cairo_fixed_to_double (pattern->gradient.p1.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.p1.y);
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = _cairo_fixed_to_double (pattern->gradient.p2.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.p2.y);
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    pattern_id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Pattern\r\n"
				 "   /PatternType 2\r\n"
				 "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
				 "   /Shading\r\n"
				 "      << /ShadingType 2\r\n"
				 "         /ColorSpace /DeviceRGB\r\n"
				 "         /Coords [ %f %f %f %f ]\r\n"
				 "         /Function %d 0 R\r\n"
				 "         /Extend [ true true ]\r\n"
				 "      >>\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 pattern_id,
				 document->height,
				 x0, y0, x1, y1,
				 function_id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    _cairo_output_stream_printf (output,
				 "/Pattern cs /res%d scn /a%d gs\r\n",
				 pattern_id, alpha);

    return CAIRO_STATUS_SUCCESS;
}
	
static cairo_status_t
emit_radial_pattern (cairo_pdf_surface_t *surface, cairo_radial_pattern_t *pattern)
{
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    unsigned int function_id, pattern_id, alpha;
    double x0, y0, x1, y1, r0, r1;
    cairo_matrix_t p2u;

    _cairo_pdf_document_close_stream (document);

    function_id = emit_pattern_stops (surface, &pattern->base);
    if (function_id == 0)
	return CAIRO_STATUS_NO_MEMORY;

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    x0 = _cairo_fixed_to_double (pattern->gradient.inner.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.inner.y);
    r0 = _cairo_fixed_to_double (pattern->gradient.inner.radius);
    cairo_matrix_transform_point (&p2u, &x0, &y0);
    x1 = _cairo_fixed_to_double (pattern->gradient.outer.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.outer.y);
    r1 = _cairo_fixed_to_double (pattern->gradient.outer.radius);
    cairo_matrix_transform_point (&p2u, &x1, &y1);

    /* FIXME: This is surely crack, but how should you scale a radius
     * in a non-orthogonal coordinate system? */
    cairo_matrix_transform_distance (&p2u, &r0, &r1);

    /* FIXME: There is a difference between the cairo gradient extend
     * semantics and PDF extend semantics. PDFs extend=false means
     * that nothing is painted outside the gradient boundaries,
     * whereas cairo takes this to mean that the end color is padded
     * to infinity. Setting extend=true in PDF gives the cairo default
     * behavoir, not yet sure how to implement the cairo mirror and
     * repeat behaviour. */
    pattern_id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Pattern\r\n"
				 "   /PatternType 2\r\n"
				 "   /Matrix [ 1 0 0 -1 0 %f ]\r\n"
				 "   /Shading\r\n"
				 "      << /ShadingType 3\r\n"
				 "         /ColorSpace /DeviceRGB\r\n"
				 "         /Coords [ %f %f %f %f %f %f ]\r\n"
				 "         /Function %d 0 R\r\n"
				 "         /Extend [ true true ]\r\n"
				 "      >>\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 pattern_id,
				 document->height,
				 x0, y0, r0, x1, y1, r1,
				 function_id);
    
    _cairo_pdf_surface_add_pattern (surface, pattern_id);

    _cairo_pdf_surface_ensure_stream (surface);
    alpha = _cairo_pdf_surface_add_alpha (surface, 1.0);

    /* Use pattern */
    _cairo_output_stream_printf (output,
				 "/Pattern cs /res%d scn /a%d gs\r\n",
				 pattern_id, alpha);

    return CAIRO_STATUS_SUCCESS;
}
	
static cairo_status_t
emit_pattern (cairo_pdf_surface_t *surface, cairo_pattern_t *pattern)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern);
    }

    ASSERT_NOT_REACHED;
    return CAIRO_STATUS_PATTERN_TYPE_MISMATCH;
}

static double
intersect (cairo_line_t *line, cairo_fixed_t y)
{
    return _cairo_fixed_to_double (line->p1.x) +
	_cairo_fixed_to_double (line->p2.x - line->p1.x) *
	_cairo_fixed_to_double (y - line->p1.y) /
	_cairo_fixed_to_double (line->p2.y - line->p1.y);
}

typedef struct
{
    cairo_output_stream_t *output_stream;
    cairo_bool_t has_current_point;
} pdf_path_info_t;

static cairo_status_t
_cairo_pdf_path_move_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f m ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y));
    info->has_current_point = TRUE;
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_line_to (void *closure, cairo_point_t *point)
{
    pdf_path_info_t *info = closure;
    const char *pdf_operator;

    if (info->has_current_point)
	pdf_operator = "l";
    else
	pdf_operator = "m";
    
    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %s ",
				 _cairo_fixed_to_double (point->x),
				 _cairo_fixed_to_double (point->y),
				 pdf_operator);
    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    pdf_path_info_t *info = closure;

    _cairo_output_stream_printf (info->output_stream,
				 "%f %f %f %f %f %f c ",
				 _cairo_fixed_to_double (b->x),
				 _cairo_fixed_to_double (b->y),
				 _cairo_fixed_to_double (c->x),
				 _cairo_fixed_to_double (c->y),
				 _cairo_fixed_to_double (d->x),
				 _cairo_fixed_to_double (d->y));
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_pdf_path_close_path (void *closure)
{
    pdf_path_info_t *info = closure;
    
    _cairo_output_stream_printf (info->output_stream,
				 "h\r\n");
    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*pattern,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    const char *pdf_operator;
    cairo_status_t status;
    pdf_path_info_t info;

    status = emit_pattern (surface, pattern);
    if (status)
	return status;

    /* After the above switch the current stream should belong to this
     * surface, so no need to _cairo_pdf_surface_ensure_stream() */
    assert (document->current_stream != NULL &&
	    document->current_stream == surface->current_stream);

    info.output_stream = document->output_stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "f";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "f*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (document->output_stream,
				 "%s\r\n",
				 pdf_operator);

    return status;
}
  
static cairo_int_status_t
_cairo_pdf_surface_composite_trapezoids (cairo_operator_t	op,
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
    cairo_pdf_surface_t *surface = abstract_dst;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_int_status_t status;
    int i;

    status = emit_pattern (surface, pattern);
    if (status)
	return status;

    /* After the above switch the current stream should belong to this
     * surface, so no need to _cairo_pdf_surface_ensure_stream() */
    assert (document->current_stream != NULL &&
	    document->current_stream == surface->current_stream);

    for (i = 0; i < num_traps; i++) {
	double left_x1, left_x2, right_x1, right_x2;

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);

	_cairo_output_stream_printf (output,
				     "%f %f m %f %f l %f %f l %f %f l h\r\n",
				     left_x1, _cairo_fixed_to_double (traps[i].top),
				     left_x2, _cairo_fixed_to_double (traps[i].bottom),
				     right_x2, _cairo_fixed_to_double (traps[i].bottom),
				     right_x1, _cairo_fixed_to_double (traps[i].top));
    }

    _cairo_output_stream_printf (output,
				 "f\r\n");

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_copy_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;

    return _cairo_pdf_document_add_page (document, surface);
}

static cairo_int_status_t
_cairo_pdf_surface_show_page (void *abstract_surface)
{
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_int_status_t status;

    status = _cairo_pdf_document_add_page (document, surface);
    if (status)
	return status;

    _cairo_pdf_surface_clear (surface);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_get_extents (void		  *abstract_surface,
				cairo_rectangle_t *rectangle)
{
    cairo_pdf_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    /* XXX: The conversion to integers here is pretty bogus, (not to
     * mention the aribitray limitation of width to a short(!). We
     * may need to come up with a better interface for get_size.
     */
    rectangle->width  = (int) ceil (surface->width);
    rectangle->height = (int) ceil (surface->height);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_font_subset_t *
_cairo_pdf_document_get_font (cairo_pdf_document_t	*document,
			      cairo_scaled_font_t	*scaled_font)
{
    cairo_status_t status;
    cairo_unscaled_font_t *unscaled_font;
    cairo_font_subset_t *pdf_font;
    unsigned int num_fonts, i;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return NULL;

    /* XXX Why is this an ft specific function? */
    unscaled_font = _cairo_ft_scaled_font_get_unscaled_font (scaled_font);

    num_fonts = _cairo_array_num_elements (&document->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&document->fonts, i, &pdf_font);
	if (pdf_font->unscaled_font == unscaled_font)
	    return pdf_font;
    }

    /* FIXME: Figure out here which font backend is in use and call
     * the appropriate constructor. */
    pdf_font = _cairo_font_subset_create (unscaled_font);
    if (pdf_font == NULL)
	return NULL;

    pdf_font->font_id = _cairo_pdf_document_new_object (document);

    status = _cairo_array_append (&document->fonts, &pdf_font);
    if (status) {
	_cairo_font_subset_destroy (pdf_font);
	return NULL;
    }

    return pdf_font;
}

static cairo_int_status_t
_cairo_pdf_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
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
    cairo_pdf_surface_t *surface = abstract_surface;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_font_subset_t *pdf_font;
    cairo_int_status_t status;
    int i, index;
    double det;

    /* XXX: Need to fix this to work with a general cairo_scaled_font_t. */
    if (! _cairo_scaled_font_is_ft (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pdf_font = _cairo_pdf_document_get_font (document, scaled_font);
    if (pdf_font == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    /* Some PDF viewers (at least older versions of xpdf) have trouble with
     * size 0 fonts. If the font size is less than 1/1000pt, ignore the
     * font */
    _cairo_matrix_compute_determinant (&scaled_font->scale, &det);
    if (fabs (det) < 0.000001)
	return CAIRO_STATUS_SUCCESS;
    
    status = emit_pattern (surface, pattern);
    if (status)
	return status;

    _cairo_output_stream_printf (output,
				 "BT /res%u 1 Tf", pdf_font->font_id);
    for (i = 0; i < num_glyphs; i++) {

	index = _cairo_font_subset_use_glyph (pdf_font, glyphs[i].index);

	_cairo_output_stream_printf (output,
				     " %f %f %f %f %f %f Tm (\\%o) Tj",
				     scaled_font->scale.xx,
				     scaled_font->scale.yx,
				     -scaled_font->scale.xy,
				     -scaled_font->scale.yy,
				     glyphs[i].x,
				     glyphs[i].y,
				     index);
    }
    _cairo_output_stream_printf (output,
				 " ET\r\n");

    _cairo_pdf_surface_add_font (surface, pdf_font->font_id);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_pdf_surface_intersect_clip_path (void			*dst,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	fill_rule,
					double			tolerance,
					cairo_antialias_t	antialias)
{
    cairo_pdf_surface_t *surface = dst;
    cairo_pdf_document_t *document = surface->document;
    cairo_output_stream_t *output = document->output_stream;
    cairo_status_t status;
    pdf_path_info_t info;
    const char *pdf_operator;

    _cairo_pdf_surface_ensure_stream (surface);

    if (path == NULL) {
	if (surface->has_clip)
	    _cairo_output_stream_printf (output, "Q\r\n");
	surface->has_clip = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    if (!surface->has_clip) {
	_cairo_output_stream_printf (output, "q ");
	surface->has_clip = TRUE;
    }

    info.output_stream = document->output_stream;
    info.has_current_point = FALSE;

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_pdf_path_move_to,
					  _cairo_pdf_path_line_to,
					  _cairo_pdf_path_curve_to,
					  _cairo_pdf_path_close_path,
					  &info);

    switch (fill_rule) {
    case CAIRO_FILL_RULE_WINDING:
	pdf_operator = "W";
	break;
    case CAIRO_FILL_RULE_EVEN_ODD:
	pdf_operator = "W*";
	break;
    default:
	ASSERT_NOT_REACHED;
    }

    _cairo_output_stream_printf (document->output_stream,
				 "%s n\r\n",
				 pdf_operator);

    return status;
}

static void
_cairo_pdf_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
  _cairo_font_options_init_default (options);

  cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
}

static const cairo_surface_backend_t cairo_pdf_surface_backend = {
    CAIRO_SURFACE_TYPE_PDF,
    _cairo_pdf_surface_create_similar,
    _cairo_pdf_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    _cairo_pdf_surface_composite,
    _cairo_pdf_surface_fill_rectangles,
    _cairo_pdf_surface_composite_trapezoids,
    _cairo_pdf_surface_copy_page,
    _cairo_pdf_surface_show_page,
    NULL, /* set_clip_region */
    _cairo_pdf_surface_intersect_clip_path,
    _cairo_pdf_surface_get_extents,
    _cairo_pdf_surface_old_show_glyphs,
    _cairo_pdf_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the drawing functions */
    
    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    _cairo_pdf_surface_fill,
    NULL  /* show_glyphs */
};

static cairo_pdf_document_t *
_cairo_pdf_document_create (cairo_output_stream_t	*output_stream,
			    double			width,
			    double			height)
{
    cairo_pdf_document_t *document;

    document = malloc (sizeof (cairo_pdf_document_t));
    if (document == NULL)
	return NULL;

    document->output_stream = output_stream;
    document->ref_count = 1;
    document->owner = NULL;
    document->finished = FALSE;
    document->width = width;
    document->height = height;
    document->x_dpi = DEFAULT_DPI;
    document->y_dpi = DEFAULT_DPI;

    _cairo_array_init (&document->objects, sizeof (cairo_pdf_object_t));
    _cairo_array_init (&document->pages, sizeof (unsigned int));
    document->next_available_id = 1;

    document->current_stream = NULL;

    document->pages_id = _cairo_pdf_document_new_object (document);

    _cairo_array_init (&document->fonts, sizeof (cairo_font_subset_t *));

    /* Document header */
    _cairo_output_stream_printf (output_stream,
				 "%%PDF-1.4\r\n");

    return document;
}

static unsigned int
_cairo_pdf_document_write_info (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Creator (cairographics.org)\r\n"
				 "   /Producer (cairographics.org)\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 id);

    return id;
}

static void
_cairo_pdf_document_write_pages (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *stream = document->output_stream;
    unsigned int page_id;
    int num_pages, i;

    _cairo_pdf_document_update_object (document, document->pages_id);
    _cairo_output_stream_printf (stream,
				 "%d 0 obj\r\n"
				 "<< /Type /Pages\r\n"
				 "   /Kids [ ",
				 document->pages_id);
    
    num_pages = _cairo_array_num_elements (&document->pages);
    for (i = 0; i < num_pages; i++) {
	_cairo_array_copy_element (&document->pages, i, &page_id);
	_cairo_output_stream_printf (stream, "%d 0 R ", page_id);
    }

    _cairo_output_stream_printf (stream, "]\r\n"); 
    _cairo_output_stream_printf (stream, "   /Count %d\r\n", num_pages);

    /* TODO: Figure out wich other defaults to be inherited by /Page
     * objects. */
    _cairo_output_stream_printf (stream,
				 "   /MediaBox [ 0 0 %f %f ]\r\n"
				 ">>\r\n"
				 "endobj\r\n",
				 document->width,
				 document->height);
}

static cairo_status_t
_cairo_pdf_document_write_fonts (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_font_subset_t *font;
    int num_fonts, i, j;
    const char *data;
    char *compressed;
    unsigned long data_size, compressed_size;
    unsigned int stream_id, descriptor_id;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    num_fonts = _cairo_array_num_elements (&document->fonts);
    for (i = 0; i < num_fonts; i++) {
	_cairo_array_copy_element (&document->fonts, i, &font);

	status = _cairo_font_subset_generate (font, &data, &data_size);
	if (status)
	    goto fail;

	compressed = compress_dup (data, data_size, &compressed_size);
	if (compressed == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto fail;
	}

	stream_id = _cairo_pdf_document_new_object (document);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Filter /FlateDecode\r\n"
				     "   /Length %lu\r\n"
				     "   /Length1 %lu\r\n"
				     ">>\r\n"
				     "stream\r\n",
				     stream_id,
				     compressed_size,
				     data_size);
	_cairo_output_stream_write (output, compressed, compressed_size);
	_cairo_output_stream_printf (output,
				     "\r\n"
				     "endstream\r\n"
				     "endobj\r\n");
	free (compressed);

	descriptor_id = _cairo_pdf_document_new_object (document);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Type /FontDescriptor\r\n"
				     "   /FontName /7%s\r\n"
				     "   /Flags 4\r\n"
				     "   /FontBBox [ %ld %ld %ld %ld ]\r\n"
				     "   /ItalicAngle 0\r\n"
				     "   /Ascent %ld\r\n"
				     "   /Descent %ld\r\n"
				     "   /CapHeight 500\r\n"
				     "   /StemV 80\r\n"
				     "   /StemH 80\r\n"
				     "   /FontFile2 %u 0 R\r\n"
				     ">>\r\n"
				     "endobj\r\n",
				     descriptor_id,
				     font->base_font,
				     font->x_min,
				     font->y_min,
				     font->x_max,
				     font->y_max,
				     font->ascent,
				     font->descent,
				     stream_id);

	_cairo_pdf_document_update_object (document, font->font_id);
	_cairo_output_stream_printf (output,
				     "%d 0 obj\r\n"
				     "<< /Type /Font\r\n"
				     "   /Subtype /TrueType\r\n"
				     "   /BaseFont /%s\r\n"
				     "   /FirstChar 0\r\n"
				     "   /LastChar %d\r\n"
				     "   /FontDescriptor %d 0 R\r\n"
				     "   /Widths ",
				     font->font_id,
				     font->base_font,
				     font->num_glyphs,
				     descriptor_id);

	_cairo_output_stream_printf (output,
				     "[");

	for (j = 0; j < font->num_glyphs; j++)
	    _cairo_output_stream_printf (output,
					 " %d",
					 font->widths[j]);

	_cairo_output_stream_printf (output,
				     " ]\r\n"
				     ">>\r\n"
				     "endobj\r\n");

    fail:
	_cairo_font_subset_destroy (font);
    }

    return status;
}

static unsigned int
_cairo_pdf_document_write_catalog (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    unsigned int id;

    id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Catalog\r\n"
				 "   /Pages %d 0 R\r\n" 
				 ">>\r\n"
				 "endobj\r\n",
				 id, document->pages_id);

    return id;
}

static long
_cairo_pdf_document_write_xref (cairo_pdf_document_t *document)
{
    cairo_output_stream_t *output = document->output_stream;
    cairo_pdf_object_t *object;
    int num_objects, i;
    long offset;
    char buffer[11];

    num_objects = _cairo_array_num_elements (&document->objects);

    offset = _cairo_output_stream_get_position (output);
    _cairo_output_stream_printf (output,
				 "xref\r\n"
				 "%d %d\r\n",
				 0, num_objects + 1);

    _cairo_output_stream_printf (output,
				 "0000000000 65535 f\r\n");
    for (i = 0; i < num_objects; i++) {
	object = _cairo_array_index (&document->objects, i);
	snprintf (buffer, sizeof buffer, "%010ld", object->offset);
	_cairo_output_stream_printf (output,
				     "%s 00000 n\r\n", buffer);
    }

    return offset;
}

static cairo_pdf_document_t *
_cairo_pdf_document_reference (cairo_pdf_document_t *document)
{
    document->ref_count++;

    return document;
}

static void
_cairo_pdf_document_destroy (cairo_pdf_document_t *document)
{
    document->ref_count--;
    if (document->ref_count > 0)
      return;

    _cairo_pdf_document_finish (document);

    free (document);
}
    
static cairo_status_t
_cairo_pdf_document_finish (cairo_pdf_document_t *document)
{
    cairo_status_t status;
    cairo_output_stream_t *output = document->output_stream;
    long offset;
    unsigned int info_id, catalog_id;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pdf_document_close_stream (document);
    _cairo_pdf_document_write_pages (document);
    _cairo_pdf_document_write_fonts (document);
    info_id = _cairo_pdf_document_write_info (document);
    catalog_id = _cairo_pdf_document_write_catalog (document);
    offset = _cairo_pdf_document_write_xref (document);
    
    _cairo_output_stream_printf (output,
				 "trailer\r\n"
				 "<< /Size %d\r\n"
				 "   /Root %d 0 R\r\n"
				 "   /Info %d 0 R\r\n"
				 ">>\r\n",
				 document->next_available_id,
				 catalog_id,
				 info_id);

    _cairo_output_stream_printf (output,
				 "startxref\r\n"
				 "%ld\r\n"
				 "%%%%EOF\r\n",
				 offset);

    status = _cairo_output_stream_get_status (output);
    _cairo_output_stream_destroy (output);

    _cairo_array_fini (&document->objects);
    _cairo_array_fini (&document->pages);
    _cairo_array_fini (&document->fonts);

    document->finished = TRUE;

    return status;
}

static cairo_status_t
_cairo_pdf_document_add_page (cairo_pdf_document_t	*document,
			      cairo_pdf_surface_t	*surface)
{
    cairo_status_t status;
    cairo_pdf_stream_t *stream;
    cairo_pdf_resource_t *res;
    cairo_output_stream_t *output = document->output_stream;
    unsigned int page_id;
    double alpha;
    int num_streams, num_alphas, num_resources, i;

    assert (!document->finished);

    _cairo_pdf_surface_ensure_stream (surface);

    if (surface->has_clip)
	_cairo_output_stream_printf (output, "Q\r\n");

    _cairo_pdf_document_close_stream (document);

    page_id = _cairo_pdf_document_new_object (document);
    _cairo_output_stream_printf (output,
				 "%d 0 obj\r\n"
				 "<< /Type /Page\r\n"
				 "   /Parent %d 0 R\r\n"
				 "   /Contents [",
				 page_id,
				 document->pages_id);

    num_streams = _cairo_array_num_elements (&surface->streams);
    for (i = 0; i < num_streams; i++) {
	_cairo_array_copy_element (&surface->streams, i, &stream);	
	_cairo_output_stream_printf (output,
				     " %d 0 R",
				     stream->id);
    }

    _cairo_output_stream_printf (output,
				 " ]\r\n"
				 "   /Resources <<\r\n");

    num_resources =  _cairo_array_num_elements (&surface->fonts);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /Font <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->fonts, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }
    
    num_alphas =  _cairo_array_num_elements (&surface->alphas);
    if (num_alphas > 0) {
	_cairo_output_stream_printf (output,
				     "      /ExtGState <<\r\n");

	for (i = 0; i < num_alphas; i++) {
	    _cairo_array_copy_element (&surface->alphas, i, &alpha);
	    _cairo_output_stream_printf (output,
					 "         /a%d << /ca %f >>\r\n",
					 i, alpha);
	}

	_cairo_output_stream_printf (output,
				     "      >>\r\n");
    }
    
    num_resources = _cairo_array_num_elements (&surface->patterns);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /Pattern <<");
	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->patterns, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }

    num_resources = _cairo_array_num_elements (&surface->xobjects);
    if (num_resources > 0) {
	_cairo_output_stream_printf (output,
				     "      /XObject <<");

	for (i = 0; i < num_resources; i++) {
	    res = _cairo_array_index (&surface->xobjects, i);
	    _cairo_output_stream_printf (output,
					 " /res%d %d 0 R",
					 res->id, res->id);
	}

	_cairo_output_stream_printf (output,
				     " >>\r\n");
    }

    _cairo_output_stream_printf (output,
				 "   >>\r\n"
				 ">>\r\n"
				 "endobj\r\n");

    status = _cairo_array_append (&document->pages, &page_id);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}
