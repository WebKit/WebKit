/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Red Hat, Inc
 * Copyright © 2005 Emmanuel Pacaud <emmanuel.pacaud@free.fr>
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
 * 	Emmanuel Pacaud <emmanuel.pacaud@univ-poitiers.fr>
 */

#include "cairoint.h"
#include "cairo-svg.h"
#include "cairo-path-fixed-private.h"
#include "cairo-ft-private.h"

#include <libxml/tree.h>

#define CC2XML(s) ((const xmlChar *)(s))
#define C2XML(s) ((xmlChar *)(s))

#define CAIRO_SVG_DTOSTR_BUFFER_LEN 30

#define CAIRO_SVG_DEFAULT_DPI 300

typedef struct cairo_svg_document cairo_svg_document_t;
typedef struct cairo_svg_surface cairo_svg_surface_t;

struct cairo_svg_document {
    cairo_output_stream_t *output_stream;
    unsigned long refcount;
    cairo_surface_t *owner;
    cairo_bool_t finished;

    double width;
    double height;
    double x_dpi;
    double y_dpi;

    xmlDocPtr	xml_doc;
    xmlNodePtr	xml_node_defs;
    xmlNodePtr  xml_node_main;

    unsigned int surface_id;
    unsigned int linear_pattern_id;
    unsigned int radial_pattern_id;
    unsigned int pattern_id;
    unsigned int filter_id;
    unsigned int clip_id;
};

struct cairo_svg_surface {
    cairo_surface_t base;

    unsigned int id;

    double width;
    double height;

    cairo_svg_document_t *document;

    xmlNodePtr  xml_node;

    unsigned int clip_level;
};

static cairo_svg_document_t *
_cairo_svg_document_create (cairo_output_stream_t	*stream,
			    double			width,
			    double			height);

static void
_cairo_svg_document_destroy (cairo_svg_document_t *document);

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document);

static void
_cairo_svg_document_reference (cairo_svg_document_t *document);

static cairo_surface_t *
_cairo_svg_surface_create_for_document (cairo_svg_document_t	*document,
					double			width,
					double			height);

static const cairo_surface_backend_t cairo_svg_surface_backend;

static cairo_surface_t *
_cairo_svg_surface_create_for_stream_internal (cairo_output_stream_t	*stream,
					       double			width,
					       double			height)
{
    cairo_svg_document_t *document;
    cairo_surface_t *surface;

    document = _cairo_svg_document_create (stream, width, height);
    if (document == NULL)
      return NULL;

    surface = _cairo_svg_surface_create_for_document (document, width, height);

    document->owner = surface;
    _cairo_svg_document_destroy (document);

    return surface;
}

cairo_surface_t *
cairo_svg_surface_create_for_stream (cairo_write_func_t		write,
				     void			*closure,
				     double			width,
				     double			height)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create (write, closure);
    if (stream == NULL)
	return NULL;

    return _cairo_svg_surface_create_for_stream_internal (stream, width, height);
}

cairo_surface_t *
cairo_svg_surface_create (const char	*filename,
			  double	width,
			  double	height)
{
    cairo_output_stream_t *stream;

    stream = _cairo_output_stream_create_for_file (filename);
    if (stream == NULL)
	return NULL;

    return _cairo_svg_surface_create_for_stream_internal (stream, width, height);
}

void
cairo_svg_surface_set_dpi (cairo_surface_t	*surface,
			   double		x_dpi,
			   double		y_dpi)
{
    cairo_svg_surface_t *svg_surface = (cairo_svg_surface_t *) surface;

    svg_surface->document->x_dpi = x_dpi;    
    svg_surface->document->y_dpi = y_dpi;    
}

static cairo_surface_t *
_cairo_svg_surface_create_for_document (cairo_svg_document_t	*document,
					double			width,
					double			height)
{
    cairo_svg_surface_t *surface;
    xmlNodePtr clip, clip_rect;
    int clip_id;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    surface = malloc (sizeof (cairo_svg_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_svg_surface_backend);

    surface->width = width;
    surface->height = height;

    _cairo_svg_document_reference (document);
    surface->document = document;

    surface->clip_level = 0;

    surface->id = document->surface_id++;
    clip_id = document->clip_id++;
    
    clip = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("clipPath"), NULL);
    snprintf (buffer, sizeof buffer, "clip%d", clip_id);
    xmlSetProp (clip, CC2XML ("id"), C2XML (buffer));
    clip_rect = xmlNewChild (clip, NULL, CC2XML ("rect"), NULL);
    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlSetProp (clip_rect, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlSetProp (clip_rect, CC2XML ("height"), C2XML (buffer));
    
    surface->xml_node = xmlNewChild (surface->id == 0 ? 
				     document->xml_node_main : 
				     document->xml_node_defs, 
				     NULL, CC2XML ("g"), NULL);
	
    snprintf (buffer, sizeof buffer, "surface%d", surface->id);
    xmlSetProp (surface->xml_node, CC2XML ("id"), C2XML (buffer));
    snprintf (buffer, sizeof buffer, "url(#clip%d)", clip_id);
    xmlSetProp (surface->xml_node, CC2XML ("clip-path"), C2XML (buffer));
    
    return &surface->base;
}

static cairo_surface_t *
_cairo_svg_surface_create_similar (void			*abstract_src,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    cairo_svg_surface_t *template = abstract_src;

    return _cairo_svg_surface_create_for_document (template->document,
						   width, height);
}

static cairo_status_t
_cairo_svg_surface_finish (void *abstract_surface)
{
    cairo_status_t status;
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;

    if (document->owner == &surface->base)
	status = _cairo_svg_document_finish (document);
    else
	status = CAIRO_STATUS_SUCCESS;

    _cairo_svg_document_destroy (document);

    return status;
}

static void
emit_transform (xmlNodePtr node, 
		char const *attribute_str, 
		cairo_matrix_t *matrix)
{
    xmlBufferPtr matrix_buffer;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    matrix_buffer = xmlBufferCreate ();
    xmlBufferCat (matrix_buffer, CC2XML ("matrix("));
    _cairo_dtostr (buffer, sizeof buffer, matrix->xx);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ",");
    _cairo_dtostr (buffer, sizeof buffer, matrix->yx);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ",");
    _cairo_dtostr (buffer, sizeof buffer, matrix->xy);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ",");
    _cairo_dtostr (buffer, sizeof buffer, matrix->yy);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ",");
    _cairo_dtostr (buffer, sizeof buffer, matrix->x0);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ",");
    _cairo_dtostr (buffer, sizeof buffer, matrix->y0);
    xmlBufferCat (matrix_buffer, C2XML (buffer));
    xmlBufferCat (matrix_buffer, ")");
    xmlSetProp (node, CC2XML (attribute_str), C2XML (xmlBufferContent (matrix_buffer)));
    xmlBufferFree (matrix_buffer);
}

typedef struct {
    xmlBufferPtr buffer;
    unsigned int in_mem;
    unsigned char src[3];
    unsigned char dst[5];
    unsigned int count;
    unsigned int trailing;
} base64_write_closure_t;

static unsigned char const *base64_table =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static cairo_status_t
base64_write_func (void *closure, 
		   const unsigned char *data,
		   unsigned int length)
{
    base64_write_closure_t *info = (base64_write_closure_t *) closure;
    unsigned int i;
    unsigned char *src, *dst;

    dst = info->dst;
    src = info->src;

    if (info->in_mem + length < 3) {
	for (i = 0; i < length; i++) { 
	    src[i + info->in_mem] = *data;
	    data++;
	}
	info->in_mem += length;
	return CAIRO_STATUS_SUCCESS;
    }

    while (info->in_mem + length >= 3) {
	for (i = 0; i < 3 - info->in_mem; i++) {
	    src[i + info->in_mem] = *data;
	    data++;
	    length--;
	}
	info->count++;
	if (info->count >= 18) {
	    info->count = 0;
	    xmlBufferCat (info->buffer, "\r\n");
	}
	dst[0] = base64_table[src[0] >> 2];
	dst[1] = base64_table[(src[0] & 0x03) << 4 | src[1] >> 4];
	dst[2] = base64_table[(src[1] & 0x0f) << 2 | src[2] >> 6];
	dst[3] = base64_table[src[2] & 0xfc >> 2];
	/* Special case for the last missing bits */
	switch (info->trailing) {
	    case 2:
		dst[2] = '=';
	    case 1:
		dst[3] = '=';
	    default:
		break;
	}
	xmlBufferCat (info->buffer, dst);
	info->in_mem = 0;
    }

    for (i = 0; i < length; i++) { 
	src[i] = *data;
	data++;
    }
    info->in_mem = length;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_surface_base64_encode (cairo_surface_t *surface,
			      xmlBufferPtr *buffer)
{
    cairo_status_t status;
    base64_write_closure_t info;
    unsigned int i;

    if (buffer == NULL)
	return CAIRO_STATUS_NULL_POINTER;
    
    info.buffer = xmlBufferCreate();
    info.in_mem = 0;
    info.count = 0;
    info.trailing = 0;
    memset (info.dst, '\x0', 5);
    *buffer = info.buffer;

    xmlBufferCat (info.buffer, CC2XML ("data:image/png;base64,"));

    status = cairo_surface_write_to_png_stream (surface, base64_write_func, 
						(void *) &info);

    if (status) {
	xmlBufferFree (*buffer);
	*buffer = NULL;
	return status;
    }
    
    if (info.in_mem > 0) {
	for (i = info.in_mem; i < 3; i++)
	    info.src[i] = '\x0';
	info.trailing = 3 - info.in_mem;
	info.in_mem = 3;
	base64_write_func (&info, NULL, 0);
    }
    
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
emit_composite_image_pattern (xmlNodePtr node,
			      cairo_surface_pattern_t *pattern,
			      double *width,
			      double *height,
			      cairo_bool_t is_pattern)
{
    cairo_surface_t *surface = pattern->surface;
    cairo_image_surface_t *image;
    cairo_status_t status;
    cairo_matrix_t p2u;
    xmlNodePtr child;
    xmlBufferPtr image_buffer;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
    if (status)
	return status;

    status = _cairo_surface_base64_encode (surface, &image_buffer);
    if (status)
	return status;

    child = xmlNewChild (node, NULL, CC2XML ("image"), NULL);
    _cairo_dtostr (buffer, sizeof buffer, image->width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, image->height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));
    xmlSetProp (child, CC2XML ("xlink:href"), C2XML (xmlBufferContent (image_buffer)));
		
    xmlBufferFree (image_buffer);

    if (!is_pattern) {
	p2u = pattern->base.matrix;
	cairo_matrix_invert (&p2u);
	emit_transform (child, "transform", &p2u);
    }

    if (width != NULL)
	    *width = image->width;
    if (height != NULL)
	    *height = image->height;

    _cairo_surface_release_source_image (pattern->surface, image, image_extra);

    return status;
}

static cairo_int_status_t
emit_composite_svg_pattern (xmlNodePtr node, 
			    cairo_surface_pattern_t *pattern,
			    double *width, 
			    double *height,
			    cairo_bool_t is_pattern)
{
    cairo_svg_surface_t *surface = (cairo_svg_surface_t *) pattern->surface;
    cairo_matrix_t p2u;
    xmlNodePtr child;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    /* FIXME: self copy is not supported yet */
    if (surface->id == 0)
	return CAIRO_STATUS_SUCCESS;
    
    child = xmlNewChild (node, NULL, CC2XML("use"), NULL);
    snprintf (buffer, sizeof buffer, "#surface%d", surface->id);
    xmlSetProp (child, CC2XML ("xlink:href"), C2XML (buffer));

    if (!is_pattern) {
	p2u = pattern->base.matrix;
	cairo_matrix_invert (&p2u);
	emit_transform (child, "transform", &p2u);
    }

    if (width != NULL)
	    *width = surface->width;
    if (height != NULL)
	    *height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
emit_composite_pattern (xmlNodePtr node, 
			cairo_surface_pattern_t *pattern,
			double *width,
			double *height,
			int is_pattern)
{
    if (pattern->surface->backend == &cairo_svg_surface_backend)
	return emit_composite_svg_pattern (node, pattern, width, height, is_pattern);
    else
	return emit_composite_image_pattern (node, pattern, width, height, is_pattern);
}

static cairo_int_status_t
_cairo_svg_surface_composite (cairo_operator_t	op,
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
    cairo_svg_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) src_pattern;

    if (mask_pattern)
 	return CAIRO_STATUS_SUCCESS;
    
    if (src_pattern->type != CAIRO_PATTERN_SURFACE)
	return CAIRO_STATUS_SUCCESS;

    return emit_composite_pattern (dst->xml_node, src, NULL, NULL, FALSE);
}

static void
emit_operator (cairo_operator_t op, cairo_svg_surface_t *surface,
	       xmlBufferPtr style)
{
    char const *op_str[] = {
	NULL,
	NULL, NULL, "in", "out", "atop",
	NULL, NULL, NULL, NULL, NULL, 
	"xor", NULL, NULL };
    
    cairo_svg_document_t *document = surface->document;
    xmlNodePtr child, primitive;
    xmlBufferPtr id;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (op_str[op] == NULL)
	return;

    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("filter"), NULL);
    primitive = xmlNewChild (child, NULL, CC2XML ("feComposite"), NULL);
    xmlSetProp (primitive, CC2XML ("operator"), CC2XML (op_str[op]));
    xmlSetProp (primitive, CC2XML ("in"), CC2XML ("SourceGraphic"));
    xmlSetProp (primitive, CC2XML ("in2"), CC2XML ("BackgroundImage"));

    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("filter"));
    snprintf (buffer, sizeof buffer, "%d", document->filter_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));
    
    xmlBufferCat (style, CC2XML ("filter: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));

    xmlBufferFree (id);
    
    document->filter_id++;
}

static void
emit_color (cairo_color_t const *color, xmlBufferPtr style,
	    char const *color_str, char const *opacity_str)
{
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    xmlBufferCat (style, CC2XML (color_str));
    xmlBufferCat (style, CC2XML (": rgb("));
    _cairo_dtostr (buffer, sizeof buffer, color->red * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%,"));
    _cairo_dtostr (buffer, sizeof buffer, color->green * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%,"));
    _cairo_dtostr (buffer, sizeof buffer, color->blue * 100.0);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML ("%); "));
    xmlBufferCat (style, CC2XML (opacity_str));
    xmlBufferCat (style, CC2XML (": "));
    _cairo_dtostr (buffer, sizeof buffer, color->alpha);
    xmlBufferCat (style, CC2XML (buffer));
    xmlBufferCat (style, CC2XML (";"));
}

static void
emit_solid_pattern (cairo_svg_surface_t *surface,
		    cairo_solid_pattern_t *pattern,
		    xmlBufferPtr style, int is_stroke)
{
    emit_color (&pattern->color, 
		style, is_stroke ? "stroke" : "fill", 
		"opacity");
}

static void
emit_surface_pattern (cairo_svg_surface_t *surface,
		      cairo_surface_pattern_t *pattern,
		      xmlBufferPtr style, int is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    xmlNodePtr child;
    xmlBufferPtr id;
    cairo_matrix_t p2u;
    double width, height;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("pattern"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("pattern"));
    snprintf (buffer, sizeof buffer, "%d", document->pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));

    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));
    xmlBufferFree (id);

    document->pattern_id++;

    emit_composite_pattern (child, pattern, &width, &height, TRUE);

    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));

    p2u = pattern->base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "patternTransform", &p2u);

    xmlSetProp (child, CC2XML ("patternUnits"), CC2XML ("userSpaceOnUse"));
}

static void 
emit_pattern_stops (xmlNodePtr parent,
		    cairo_gradient_pattern_t const *pattern, 
		    double start_offset)
{
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    cairo_color_t color;
    int i;

    for (i = 0; i < pattern->n_stops; i++) {
	child = xmlNewChild (parent, NULL, CC2XML ("stop"), NULL);
	_cairo_dtostr (buffer, sizeof buffer, 
		       start_offset + (1 - start_offset ) *
		       _cairo_fixed_to_double (pattern->stops[i].x));
	xmlSetProp (child, CC2XML ("offset"), C2XML (buffer));
	style = xmlBufferCreate ();
	_cairo_color_init_rgba (&color,
				pattern->stops[i].color.red   / 65535.0,
				pattern->stops[i].color.green / 65535.0,
				pattern->stops[i].color.blue  / 65535.0,
				pattern->stops[i].color.alpha / 65535.0);
	emit_color (&color, style, "stop-color", "stop-opacity");
	xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
	xmlBufferFree (style);
    }
}

static void
emit_pattern_extend (xmlNodePtr node, cairo_extend_t extend)
{
    char const *value = NULL;

    switch (extend) {
	case CAIRO_EXTEND_REPEAT:
	    value = "repeat"; 
	    break;
	case CAIRO_EXTEND_REFLECT:
	    value = "reflect"; 
	    break;
	case CAIRO_EXTEND_NONE:
	    break;
	case CAIRO_EXTEND_PAD:
	    /* FIXME not implemented */
	    break;
    }
    
    if (value != NULL)
	xmlSetProp (node, CC2XML ("spreadMethod"), CC2XML (value));
}

static void
emit_linear_pattern (cairo_svg_surface_t *surface, 
		     cairo_linear_pattern_t *pattern,
		     xmlBufferPtr style, int is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    xmlNodePtr child;
    xmlBufferPtr id;
    double x0, y0, x1, y1;
    cairo_matrix_t p2u;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("linearGradient"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("linear"));
    snprintf (buffer, sizeof buffer, "%d", document->linear_pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));
    xmlSetProp (child, CC2XML ("gradientUnits"), CC2XML ("userSpaceOnUse"));
    emit_pattern_extend (child, pattern->base.base.extend);

    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));

    xmlBufferFree (id);

    document->linear_pattern_id++;

    emit_pattern_stops (child ,&pattern->base, 0.0);

    x0 = _cairo_fixed_to_double (pattern->gradient.p1.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.p1.y);
    x1 = _cairo_fixed_to_double (pattern->gradient.p2.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.p2.y);
    
    _cairo_dtostr (buffer, sizeof buffer, x0);
    xmlSetProp (child, CC2XML ("x1"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y0);
    xmlSetProp (child, CC2XML ("y1"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, x1);
    xmlSetProp (child, CC2XML ("x2"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y1);
    xmlSetProp (child, CC2XML ("y2"), C2XML (buffer));
    
    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "gradientTransform", &p2u);
}
	
static void
emit_radial_pattern (cairo_svg_surface_t *surface, 
		     cairo_radial_pattern_t *pattern,
		     xmlBufferPtr style, int is_stroke)
{
    cairo_svg_document_t *document = surface->document;
    cairo_matrix_t p2u;
    xmlNodePtr child;
    xmlBufferPtr id;
    double x0, y0, x1, y1, r0, r1;
    double fx, fy;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    child = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("radialGradient"), NULL);
    
    id = xmlBufferCreate ();
    xmlBufferCat (id, CC2XML ("radial"));
    snprintf (buffer, sizeof buffer, "%d", document->radial_pattern_id);
    xmlBufferCat (id, C2XML (buffer));
    xmlSetProp (child, CC2XML ("id"), C2XML (xmlBufferContent (id)));
    xmlSetProp (child, CC2XML ("gradientUnits"), CC2XML ("userSpaceOnUse"));
    emit_pattern_extend (child, pattern->base.base.extend);
    
    xmlBufferCat (style, CC2XML (is_stroke ? "color: url(#" : "fill: url(#"));
    xmlBufferCat (style, xmlBufferContent (id));
    xmlBufferCat (style, CC2XML (");"));

    xmlBufferFree (id);

    document->radial_pattern_id++;

    x0 = _cairo_fixed_to_double (pattern->gradient.inner.x);
    y0 = _cairo_fixed_to_double (pattern->gradient.inner.y);
    r0 = _cairo_fixed_to_double (pattern->gradient.inner.radius);
    x1 = _cairo_fixed_to_double (pattern->gradient.outer.x);
    y1 = _cairo_fixed_to_double (pattern->gradient.outer.y);
    r1 = _cairo_fixed_to_double (pattern->gradient.outer.radius);

    /* SVG doesn't have a start radius, so computing now SVG focal coordinates
     * and emulating start radius by translating color stops.
     * FIXME: We also need to emulate cairo behaviour inside start circle when
     * extend != CAIRO_EXTEND_NONE.
     * FIXME: Handle radius1 <= radius0 */
    fx = (r1 * x0 - r0 * x1) / (r1 - r0);
    fy = (r1 * y0 - r0 * y1) / (r1 - r0);

    emit_pattern_stops (child, &pattern->base, r0 / r1);

    _cairo_dtostr (buffer, sizeof buffer, x1);
    xmlSetProp (child, CC2XML ("cx"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, y1);
    xmlSetProp (child, CC2XML ("cy"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, fx);
    xmlSetProp (child, CC2XML ("fx"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, fy);
    xmlSetProp (child, CC2XML ("fy"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, r1);
    xmlSetProp (child, CC2XML ("r"), C2XML (buffer));

    p2u = pattern->base.base.matrix;
    cairo_matrix_invert (&p2u);

    emit_transform (child, "gradientTransform", &p2u);
}
	
static void
emit_pattern (cairo_svg_surface_t *surface, cairo_pattern_t *pattern, 
	      xmlBufferPtr style, int is_stroke)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_SOLID:	
	emit_solid_pattern (surface, (cairo_solid_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_SURFACE:
	emit_surface_pattern (surface, (cairo_surface_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_LINEAR:
	emit_linear_pattern (surface, (cairo_linear_pattern_t *) pattern, style, is_stroke);
	break;

    case CAIRO_PATTERN_RADIAL:
	emit_radial_pattern (surface, (cairo_radial_pattern_t *) pattern, style, is_stroke);
	break;	    
    }
}

typedef struct
{
    cairo_svg_document_t *document;
    cairo_bool_t has_current_point;
    xmlBufferPtr path;
} svg_path_info_t;

    static cairo_status_t
_cairo_svg_path_move_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    xmlBufferCat (path, CC2XML ("M "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (point->x));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (point->y));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

    static cairo_status_t
_cairo_svg_path_line_to (void *closure, cairo_point_t *point)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (info->has_current_point)
	xmlBufferCat (path, CC2XML ("L "));
    else
	xmlBufferCat (path, CC2XML ("M "));

    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (point->x));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (point->y));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));

    info->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_curve_to (void          *closure,
			  cairo_point_t *b,
			  cairo_point_t *c,
			  cairo_point_t *d)
{
    svg_path_info_t *info = closure;
    xmlBufferPtr path = info->path;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    xmlBufferCat (path, CC2XML ("C "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (b->x));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (b->y));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (c->x));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (c->y));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (d->x));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));
    _cairo_dtostr (buffer, sizeof buffer, _cairo_fixed_to_double (d->y));
    xmlBufferCat (path, CC2XML (buffer));
    xmlBufferCat (path, CC2XML (" "));

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_svg_path_close_path (void *closure)
{
    svg_path_info_t *info = closure;

    if (info->has_current_point)
	    xmlBufferCat (info->path, CC2XML ("Z "));

    info->has_current_point = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_fill_rectangles (void		*abstract_surface,
				    cairo_operator_t	 op,
				    const cairo_color_t	*color,
				    cairo_rectangle_t	*rects,
				    int			 num_rects)
{
    cairo_svg_surface_t *surface = abstract_surface;
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    int i;

    style = xmlBufferCreate ();
    emit_color (color, style, "fill", "opacity");
    xmlBufferCat (style, " stroke: none;");
    emit_operator (op, surface, style);
    
    for (i = 0; i < num_rects; i++) {
	child = xmlNewChild (surface->xml_node, NULL, CC2XML ("rect"), NULL);
	_cairo_dtostr (buffer, sizeof buffer, rects[i].x);
	xmlSetProp (child, CC2XML ("x"), C2XML (buffer));
	_cairo_dtostr (buffer, sizeof buffer, rects[i].y);
	xmlSetProp (child, CC2XML ("y"), C2XML (buffer));
	_cairo_dtostr (buffer, sizeof buffer, rects[i].width);
	xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
	_cairo_dtostr (buffer, sizeof buffer, rects[i].height);
	xmlSetProp (child, CC2XML ("height"), C2XML (buffer));
	xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    }	

    xmlBufferFree (style);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = abstract_surface;
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    svg_path_info_t info;
    xmlNodePtr child;
    xmlBufferPtr style;

    info.document = document;
    info.has_current_point = FALSE;
    info.path = xmlBufferCreate ();
    
    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 0);
    xmlBufferCat (style, " stroke: none;");
    xmlBufferCat (style, " fill-rule: ");
    xmlBufferCat (style, fill_rule == CAIRO_FILL_RULE_EVEN_ODD ? "evenodd;" : "nonzero;");
    emit_operator (op, surface, style);

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);

    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("path"), NULL);
    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (info.path));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));

    xmlBufferFree (info.path);
    xmlBufferFree (style);

    return status;
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
_cairo_svg_surface_composite_trapezoids (cairo_operator_t	 op,
					 cairo_pattern_t	*pattern,
					 void			*abstract_dst,
					 cairo_antialias_t	 antialias,
					 int			 x_src,
					 int			 y_src,
					 int			 x_dst,
					 int			 y_dst,
					 unsigned int		 width,
					 unsigned int		 height,
					 cairo_trapezoid_t	*traps,
					 int			 num_traps)
{
    cairo_svg_surface_t *surface = abstract_dst;
    xmlBufferPtr style, path;
    xmlNodePtr child;
    double left_x1, left_x2, right_x1, right_x2;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    int i;

    style = xmlBufferCreate ();
    emit_pattern (surface, pattern, style, 0);
    xmlBufferCat (style, "stroke: none;");

    path = xmlBufferCreate ();

    for (i = 0; i < num_traps; i++) {

	left_x1  = intersect (&traps[i].left, traps[i].top);
	left_x2  = intersect (&traps[i].left, traps[i].bottom);
	right_x1 = intersect (&traps[i].right, traps[i].top);
	right_x2 = intersect (&traps[i].right, traps[i].bottom);

	xmlBufferCat (path, CC2XML ("M "));
	_cairo_dtostr (buffer, sizeof buffer, left_x1);
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" "));
	_cairo_dtostr (buffer, sizeof buffer,
		       _cairo_fixed_to_double (traps[i].top));
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" L "));
	_cairo_dtostr (buffer, sizeof buffer, left_x2);
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" "));
	_cairo_dtostr (buffer, sizeof buffer, 
		       _cairo_fixed_to_double (traps[i].bottom));
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" L "));
	_cairo_dtostr (buffer, sizeof buffer, right_x2);
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" "));
	_cairo_dtostr (buffer, sizeof buffer, 
		       _cairo_fixed_to_double (traps[i].bottom));
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" L "));
	_cairo_dtostr (buffer, sizeof buffer, right_x1);
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" "));
	_cairo_dtostr (buffer, sizeof buffer,
		      _cairo_fixed_to_double (traps[i].top));
	xmlBufferCat (path, CC2XML (buffer));
	xmlBufferCat (path, CC2XML (" Z"));
    }

    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("path"), NULL);

    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (path));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    
    xmlBufferFree (path);
    xmlBufferFree (style);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_svg_surface_get_extents (void		  *abstract_surface,
				cairo_rectangle_t *rectangle)
{
    cairo_svg_surface_t *surface = abstract_surface;

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

    /* First attempt of paint implementation, but obviously I didn't
     * understand how it's supposed to work */

#if 0
static cairo_int_status_t
_cairo_svg_surface_paint (void		    *abstract_surface,
			  cairo_operator_t   op,
			  cairo_pattern_t   *source)
{
    cairo_svg_surface_t *surface = abstract_surface;
    xmlNodePtr child;
    xmlBufferPtr style;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 0);
    xmlBufferCat (style, " stroke: none;");
    emit_operator (op, surface, style);

    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("rect"), NULL);
    xmlSetProp (child, CC2XML ("x"), CC2XML ("0"));
    xmlSetProp (child, CC2XML ("y"), CC2XML ("0"));
    _cairo_dtostr (buffer, sizeof buffer, surface->width);
    xmlSetProp (child, CC2XML ("width"), C2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, surface->height);
    xmlSetProp (child, CC2XML ("height"), C2XML (buffer));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));

    xmlBufferFree (style);

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_int_status_t
_cairo_svg_surface_stroke (void			*abstract_dst,
			   cairo_operator_t      op,
			   cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_stroke_style_t *stroke_style,
			   cairo_matrix_t	*ctm,
			   cairo_matrix_t	*ctm_inverse,
			   double		 tolerance,
			   cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = abstract_dst;
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    xmlBufferPtr style;
    xmlNodePtr child;
    svg_path_info_t info;
    double rx, ry;
    unsigned int i;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];
    
    info.document = document;
    info.has_current_point = FALSE;
    info.path = xmlBufferCreate ();

    rx = ry = stroke_style->line_width;
    cairo_matrix_transform_distance (ctm, &rx, &ry);

    style = xmlBufferCreate ();
    emit_pattern (surface, source, style, 1);
    xmlBufferCat (style, CC2XML ("fill: none; stroke-width: "));
    _cairo_dtostr (buffer, sizeof buffer, sqrt ((rx * rx + ry * ry) / 2.0));
    xmlBufferCat (style, C2XML (buffer)); 
    xmlBufferCat (style, CC2XML (";"));
    
    switch (stroke_style->line_cap) {
	    case CAIRO_LINE_CAP_BUTT:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: butt;"));
		    break;
	    case CAIRO_LINE_CAP_ROUND:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: round;"));
		    break;
	    case CAIRO_LINE_CAP_SQUARE:
		    xmlBufferCat (style, CC2XML ("stroke-linecap: square;"));
		    break;
    }

    switch (stroke_style->line_join) {
	    case CAIRO_LINE_JOIN_MITER:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: miter;"));
		    break; 
	    case CAIRO_LINE_JOIN_ROUND:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: round;"));
		    break; 
	    case CAIRO_LINE_JOIN_BEVEL:
		    xmlBufferCat (style, CC2XML ("stroke-linejoin: bevel;"));
		    break; 
    }

    if (stroke_style->num_dashes > 0) {
	xmlBufferCat (style, CC2XML (" stroke-dasharray: "));
	for (i = 0; i < stroke_style->num_dashes; i++) {
	    if (i != 0)
		xmlBufferCat (style, ",");
	    /* FIXME: Is is really what we want ? */
	    rx = ry = stroke_style->dash[i];
	    cairo_matrix_transform_distance (ctm, &rx, &ry);
	    _cairo_dtostr (buffer, sizeof buffer, sqrt ((rx * rx + ry * ry) / 2.0));
	    xmlBufferCat (style, C2XML (buffer));
	}
	xmlBufferCat (style, ";");
	if (stroke_style->dash_offset != 0.0) {
	    xmlBufferCat (style, CC2XML (" stroke-dashoffset: "));
	    rx = ry = stroke_style->dash_offset;
	    cairo_matrix_transform_distance (ctm, &rx, &ry);
	    _cairo_dtostr (buffer, sizeof buffer, sqrt ((rx * rx + ry * ry) / 2.0));
	    xmlBufferCat (style, C2XML (buffer));
	    xmlBufferCat (style, ";");
	}
    }

    xmlBufferCat (style, CC2XML (" stroke-miterlimit: "));
    _cairo_dtostr (buffer, sizeof buffer, stroke_style->miter_limit);
    xmlBufferCat (style, C2XML (buffer));
    xmlBufferCat (style, CC2XML (";"));

    status = _cairo_path_fixed_interpret (path,
					  CAIRO_DIRECTION_FORWARD,
					  _cairo_svg_path_move_to,
					  _cairo_svg_path_line_to,
					  _cairo_svg_path_curve_to,
					  _cairo_svg_path_close_path,
					  &info);
    
    child = xmlNewChild (surface->xml_node, NULL, CC2XML ("path"), NULL);

    xmlSetProp (child, CC2XML ("d"), xmlBufferContent (info.path));
    xmlSetProp (child, CC2XML ("style"), xmlBufferContent (style));
    
    xmlBufferFree (info.path);
    xmlBufferFree (style);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
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
    cairo_path_fixed_t path;
    cairo_status_t status;

    /* FIXME: We don't really want to keep this as is. There's to possibilities:
     *   - Use SVG fonts. But support for them seems very rare in SVG renderers.
     *   - Or store glyph outlines in <symbol> or <g> elements.
     * 
     * It would be also useful to add an early fallback in cairo_show_text and just
     * use <text> element.
     **/
    
    _cairo_path_fixed_init (&path);

    status = _cairo_scaled_font_glyph_path (scaled_font,(cairo_glyph_t *) glyphs, num_glyphs, &path);

    if (status)
	    return status;

    status = _cairo_svg_surface_fill (abstract_surface, op, pattern,
				      &path, CAIRO_FILL_RULE_WINDING, 0.0, CAIRO_ANTIALIAS_SUBPIXEL); 

    _cairo_path_fixed_fini (&path);

    return status;
}

static cairo_int_status_t
_cairo_svg_surface_intersect_clip_path (void			*dst,
					cairo_path_fixed_t	*path,
					cairo_fill_rule_t	 fill_rule,
					double			 tolerance,
					cairo_antialias_t	 antialias)
{
    cairo_svg_surface_t *surface = dst;
    cairo_svg_document_t *document = surface->document;
    cairo_status_t status;
    xmlNodePtr group, clip, clip_path;
    svg_path_info_t info;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    if (path == NULL) {
	while (surface->clip_level > 0) {
	    surface->xml_node = surface->xml_node->parent;
	    surface->clip_level--;
	}
	return CAIRO_STATUS_SUCCESS;
    }

    if (path != NULL) {
	info.document = document;
	info.has_current_point = FALSE;
	info.path = xmlBufferCreate ();

	group = xmlNewChild (surface->xml_node, NULL, CC2XML ("g"), NULL);
	clip = xmlNewChild (document->xml_node_defs, NULL, CC2XML ("clipPath"), NULL);
	snprintf (buffer, sizeof buffer, "clip%d", document->clip_id);
	xmlSetProp (clip, CC2XML ("id"), C2XML (buffer));

	clip_path = xmlNewChild (clip, NULL, CC2XML ("path"), NULL);
	status = _cairo_path_fixed_interpret (path,
					      CAIRO_DIRECTION_FORWARD,
					      _cairo_svg_path_move_to,
					      _cairo_svg_path_line_to,
					      _cairo_svg_path_curve_to,
					      _cairo_svg_path_close_path,
					      &info);
	xmlSetProp (clip_path, CC2XML ("d"), xmlBufferContent (info.path));
	xmlBufferFree (info.path);

	snprintf (buffer, sizeof buffer, "url(#clip%d)", document->clip_id);
	xmlSetProp (group, CC2XML ("clip-path"), C2XML (buffer));
	xmlSetProp (group, CC2XML ("clip-rule"), 
		    fill_rule == CAIRO_FILL_RULE_EVEN_ODD ? 
		    CC2XML ("evenodd") : CC2XML ("nonzero"));

	document->clip_id++;
	surface->xml_node = group;
	surface->clip_level++;
    } 

    return status;
}

static void
_cairo_svg_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
  _cairo_font_options_init_default (options);

  cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
}


static const cairo_surface_backend_t cairo_svg_surface_backend = {
	_cairo_svg_surface_create_similar,
	_cairo_svg_surface_finish,
	NULL, /* acquire_source_image */
	NULL, /* release_source_image */
	NULL, /* acquire_dest_image */
	NULL, /* release_dest_image */
	NULL, /* clone_similar */
	_cairo_svg_surface_composite,
	_cairo_svg_surface_fill_rectangles, 
	_cairo_svg_surface_composite_trapezoids,
	NULL, /* copy_page */
	NULL, /* show_page */
	NULL, /* set_clip_region */
	_cairo_svg_surface_intersect_clip_path,
	_cairo_svg_surface_get_extents,
	_cairo_svg_surface_old_show_glyphs,
	_cairo_svg_surface_get_font_options,
	NULL, /* flush */
	NULL, /* mark dirty rectangle */
	NULL, /* scaled font fini */
	NULL, /* scaled glyph fini */
	NULL, /*_cairo_svg_surface_paint,*/
	NULL, /* mask */
	_cairo_svg_surface_stroke,
	_cairo_svg_surface_fill,
	NULL  /* show_glyphs */
};

static cairo_svg_document_t *
_cairo_svg_document_create (cairo_output_stream_t	*output_stream,
			    double			width,
			    double			height)
{
    cairo_svg_document_t *document;
    xmlDocPtr doc;
    xmlNodePtr node;
    char buffer[CAIRO_SVG_DTOSTR_BUFFER_LEN];

    document = malloc (sizeof (cairo_svg_document_t));
    if (document == NULL)
	return NULL;

    document->output_stream = output_stream;
    document->refcount = 1;
    document->owner = NULL;
    document->finished = FALSE;
    document->width = width;
    document->height = height;
    document->x_dpi = CAIRO_SVG_DEFAULT_DPI;
    document->y_dpi = CAIRO_SVG_DEFAULT_DPI;

    document->surface_id = 0;
    document->linear_pattern_id = 0;
    document->radial_pattern_id = 0;
    document->pattern_id = 0;
    document->filter_id = 0;
    document->clip_id = 0;

    doc = xmlNewDoc (CC2XML ("1.0")); 
    node = xmlNewNode (NULL, CC2XML ("svg"));

    xmlDocSetRootElement (doc, node);

    document->xml_doc = doc;
    document->xml_node_main = node;
    document->xml_node_defs = xmlNewChild (node, NULL, CC2XML ("defs"), NULL); 

    _cairo_dtostr (buffer, sizeof buffer, width);
    xmlSetProp (node, CC2XML ("width"), CC2XML (buffer));
    _cairo_dtostr (buffer, sizeof buffer, height);
    xmlSetProp (node, CC2XML ("height"), CC2XML (buffer));
    xmlSetProp (node, CC2XML ("xmlns"), CC2XML ("http://www.w3.org/2000/svg"));
    xmlSetProp (node, CC2XML ("xmlns:xlink"), CC2XML ("http://www.w3.org/1999/xlink"));
    xmlSetProp (node, CC2XML ("version"), CC2XML ("1.1"));

    return document;
}

static void
_cairo_svg_document_reference (cairo_svg_document_t *document)
{
    document->refcount++;
}

static void
_cairo_svg_document_destroy (cairo_svg_document_t *document)
{
    document->refcount--;
    if (document->refcount > 0)
      return;

    _cairo_svg_document_finish (document);

    free (document);
}

static cairo_status_t
_cairo_svg_document_finish (cairo_svg_document_t *document)
{
    cairo_status_t status;
    cairo_output_stream_t *output = document->output_stream;
    xmlChar *xml_buffer;
    int xml_buffer_size;

    if (document->finished)
	return CAIRO_STATUS_SUCCESS;

    /* FIXME: Dumping xml tree in memory is silly. */
    xmlDocDumpFormatMemoryEnc (document->xml_doc, &xml_buffer, &xml_buffer_size, "UTF-8", 1);
    _cairo_output_stream_write (document->output_stream, xml_buffer, xml_buffer_size);
    xmlFree(xml_buffer);
    xmlFreeDoc (document->xml_doc);

    status = _cairo_output_stream_get_status (output);
    _cairo_output_stream_destroy (output);

    document->finished = TRUE;

    return status;
}
