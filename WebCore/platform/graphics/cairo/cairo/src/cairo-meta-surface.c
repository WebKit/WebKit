/* cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 *	Carl Worth <cworth@cworth.org>
 */

/* A meta surface is a surface that records all drawing operations at
 * the highest level of the surface backend interface, (that is, the
 * level of paint, mask, stroke, fill, and show_glyphs). The meta
 * surface can then be "replayed" against any target surface with:
 *
 *	_cairo_meta_surface_replay (meta, target);
 *
 * after which the results in target will be identical to the results
 * that would have been obtained if the original operations applied to
 * the meta surface had instead been applied to the target surface.
 *
 * The recording phase of the meta surface is careful to snapshot all
 * necessary objects (paths, patterns, etc.), in order to acheive
 * accurate replay. The efficiency of the meta surface could be
 * improved by improving the implementation of snapshot for the
 * various objects. For example, it would be nice to have a
 * copy-on-write implementation for _cairo_surface_snapshot.
 */

#include "cairoint.h"
#include "cairo-meta-surface-private.h"
#include "cairo-clip-private.h"

static const cairo_surface_backend_t cairo_meta_surface_backend;

cairo_surface_t *
_cairo_meta_surface_create (cairo_content_t	content,
			    int			width_pixels,
			    int			height_pixels)
{
    cairo_meta_surface_t *meta;

    meta = malloc (sizeof (cairo_meta_surface_t));
    if (meta == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&meta->base, &cairo_meta_surface_backend);

    meta->content = content;
    meta->width_pixels = width_pixels;
    meta->height_pixels = height_pixels;

    _cairo_array_init (&meta->commands, sizeof (cairo_command_t *));
    meta->commands_owner = NULL;

    return &meta->base;
}

static cairo_surface_t *
_cairo_meta_surface_create_similar (void	       *abstract_surface,
				    cairo_content_t	content,
				    int			width,
				    int			height)
{
    return _cairo_meta_surface_create (content, width, height);
}

static cairo_status_t
_cairo_meta_surface_finish (void *abstract_surface)
{
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_t *command;
    cairo_command_t **elements;
    int i, num_elements;

    if (meta->commands_owner) {
	cairo_surface_destroy (meta->commands_owner);
	return CAIRO_STATUS_SUCCESS;
    }

    num_elements = meta->commands.num_elements;
    elements = _cairo_array_index (&meta->commands, 0);
    for (i = 0; i < num_elements; i++) {
	command = elements[i];
	switch (command->type) {

	/* 5 basic drawing operations */

	case CAIRO_COMMAND_PAINT:
	    _cairo_pattern_fini (&command->paint.source.base);
	    free (command);
	    break;

	case CAIRO_COMMAND_MASK:
	    _cairo_pattern_fini (&command->mask.source.base);
	    _cairo_pattern_fini (&command->mask.mask.base);
	    free (command);
	    break;
 
	case CAIRO_COMMAND_STROKE:
	    _cairo_pattern_fini (&command->stroke.source.base);
	    _cairo_path_fixed_fini (&command->stroke.path);
	    _cairo_stroke_style_fini (&command->stroke.style);
	    free (command);
	    break;

	case CAIRO_COMMAND_FILL:
	    _cairo_pattern_fini (&command->fill.source.base);
	    _cairo_path_fixed_fini (&command->fill.path);
	    free (command);
	    break;

	case CAIRO_COMMAND_SHOW_GLYPHS:
	    _cairo_pattern_fini (&command->show_glyphs.source.base);
	    free (command->show_glyphs.glyphs);
	    cairo_scaled_font_destroy (command->show_glyphs.scaled_font);
	    free (command);
	    break;

	/* Other junk. */
	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    if (command->intersect_clip_path.path_pointer)
		_cairo_path_fixed_fini (&command->intersect_clip_path.path);
	    free (command);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}
    }

    _cairo_array_fini (&meta->commands);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_meta_surface_acquire_source_image (void			 *abstract_surface,
					  cairo_image_surface_t	**image_out,
					  void			**image_extra)
{
    cairo_status_t status;
    cairo_meta_surface_t *surface = abstract_surface;
    cairo_surface_t *image;

    image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					surface->width_pixels,
					surface->height_pixels);

    status = _cairo_meta_surface_replay (&surface->base, image);
    if (status) {
	cairo_surface_destroy (image);
	return status;
    }

    *image_out = (cairo_image_surface_t *) image;
    *image_extra = NULL;

    return status;
}

static void
_cairo_meta_surface_release_source_image (void			*abstract_surface,
					  cairo_image_surface_t	*image,
					  void			*image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_init_pattern_with_snapshot (cairo_pattern_t       *pattern,
			     const cairo_pattern_t *other)
{
    _cairo_pattern_init_copy (pattern, other);

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) pattern;
	cairo_surface_t *surface = surface_pattern->surface;

	surface_pattern->surface = _cairo_surface_snapshot (surface);

	cairo_surface_destroy (surface);

	if (surface_pattern->surface->status)
	    return surface_pattern->surface->status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_meta_surface_paint (void			*abstract_surface,
			   cairo_operator_t	 op,
			   cairo_pattern_t	*source)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_paint_t *command;

    command = malloc (sizeof (cairo_command_paint_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_PAINT;
    command->op = op;

    status = _init_pattern_with_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;
    
    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_SOURCE;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_mask (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_pattern_t	*mask)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_mask_t *command;

    command = malloc (sizeof (cairo_command_mask_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_MASK;
    command->op = op;

    status = _init_pattern_with_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _init_pattern_with_snapshot (&command->mask.base, mask);
    if (status)
	goto CLEANUP_SOURCE;
    
    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_MASK;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_MASK:
    _cairo_pattern_fini (&command->mask.base);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_stroke (void			*abstract_surface,
			    cairo_operator_t		 op,
			    cairo_pattern_t		*source,
			    cairo_path_fixed_t		*path,
			    cairo_stroke_style_t	*style,
			    cairo_matrix_t		*ctm,
			    cairo_matrix_t		*ctm_inverse,
			    double			 tolerance,
			    cairo_antialias_t		 antialias)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_stroke_t *command;
    
    command = malloc (sizeof (cairo_command_stroke_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_STROKE;
    command->op = op;

    status = _init_pattern_with_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (status)
	goto CLEANUP_SOURCE;

    status = _cairo_stroke_style_init_copy (&command->style, style);
    if (status)
	goto CLEANUP_PATH;

    command->ctm = *ctm;
    command->ctm_inverse = *ctm_inverse;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_STYLE;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_STYLE:
    _cairo_stroke_style_fini (&command->style);
  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_fill (void			*abstract_surface,
			  cairo_operator_t	 op,
			  cairo_pattern_t	*source,
			  cairo_path_fixed_t	*path,
			  cairo_fill_rule_t	 fill_rule,
			  double		 tolerance,
			  cairo_antialias_t	 antialias)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_fill_t *command;

    command = malloc (sizeof (cairo_command_fill_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_FILL;
    command->op = op;

    status = _init_pattern_with_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    status = _cairo_path_fixed_init_copy (&command->path, path);
    if (status)
	goto CLEANUP_SOURCE;

    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_PATH;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_PATH:
    _cairo_path_fixed_fini (&command->path);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

static cairo_int_status_t
_cairo_meta_surface_show_glyphs (void			*abstract_surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 const cairo_glyph_t	*glyphs,
				 int			 num_glyphs,
				 cairo_scaled_font_t	*scaled_font)
{
    cairo_status_t status;
    cairo_meta_surface_t *meta = abstract_surface;
    cairo_command_show_glyphs_t *command;

    command = malloc (sizeof (cairo_command_show_glyphs_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_SHOW_GLYPHS;
    command->op = op;

    status = _init_pattern_with_snapshot (&command->source.base, source);
    if (status)
	goto CLEANUP_COMMAND;

    command->glyphs = malloc (sizeof (cairo_glyph_t) * num_glyphs);
    if (command->glyphs == NULL) {
	status = CAIRO_STATUS_NO_MEMORY;
	goto CLEANUP_SOURCE;
    }
    memcpy (command->glyphs, glyphs, sizeof (cairo_glyph_t) * num_glyphs);

    command->num_glyphs = num_glyphs;

    command->scaled_font = cairo_scaled_font_reference (scaled_font);

    status = _cairo_array_append (&meta->commands, &command);
    if (status)
	goto CLEANUP_SCALED_FONT;

    return CAIRO_STATUS_SUCCESS;

  CLEANUP_SCALED_FONT:
    cairo_scaled_font_destroy (command->scaled_font);
    free (command->glyphs);
  CLEANUP_SOURCE:
    _cairo_pattern_fini (&command->source.base);
  CLEANUP_COMMAND:
    free (command);
    return status;
}

/**
 * _cairo_meta_surface_snapshot
 * @surface: a #cairo_surface_t which must be a meta surface
 *
 * Make an immutable copy of @surface. It is an error to call a
 * surface-modifying function on the result of this function.
 *
 * The caller owns the return value and should call
 * cairo_surface_destroy when finished with it. This function will not
 * return NULL, but will return a nil surface instead.
 *
 * Return value: The snapshot surface.
 **/
static cairo_surface_t *
_cairo_meta_surface_snapshot (void *abstract_other)
{
    cairo_meta_surface_t *other = abstract_other;
    cairo_meta_surface_t *meta;

    meta = malloc (sizeof (cairo_meta_surface_t));
    if (meta == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t*) &_cairo_surface_nil;
    }

    _cairo_surface_init (&meta->base, &cairo_meta_surface_backend);
    meta->base.is_snapshot = TRUE;

    meta->width_pixels = other->width_pixels;
    meta->height_pixels = other->height_pixels;

    _cairo_array_init_snapshot (&meta->commands, &other->commands);
    meta->commands_owner = cairo_surface_reference (&other->base);

    return &meta->base;
}

static cairo_int_status_t
_cairo_meta_surface_intersect_clip_path (void		    *dst,
					 cairo_path_fixed_t *path,
					 cairo_fill_rule_t   fill_rule,
					 double		     tolerance,
					 cairo_antialias_t   antialias)
{
    cairo_meta_surface_t *meta = dst;
    cairo_command_intersect_clip_path_t *command;
    cairo_status_t status;

    command = malloc (sizeof (cairo_command_intersect_clip_path_t));
    if (command == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    command->type = CAIRO_COMMAND_INTERSECT_CLIP_PATH;

    if (path) {
	status = _cairo_path_fixed_init_copy (&command->path, path);
	if (status) {
	    free (command);
	    return status;
	}
	command->path_pointer = &command->path;
    } else {
	command->path_pointer = NULL;
    }
    command->fill_rule = fill_rule;
    command->tolerance = tolerance;
    command->antialias = antialias;

    status = _cairo_array_append (&meta->commands, &command);
    if (status) {
	if (path)
	    _cairo_path_fixed_fini (&command->path);
	free (command);
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* A meta-surface is logically unbounded, but when it is used as a
 * source, the drawing code can optimize based on the extents of the
 * surface.
 *
 * XXX: The optimization being attempted here would only actually work
 * if the meta-surface kept track of its extents as commands were
 * added to it.
 */
static cairo_int_status_t
_cairo_meta_surface_get_extents (void			*abstract_surface,
				 cairo_rectangle_t	*rectangle)
{
    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width = CAIRO_MAXSHORT;
    rectangle->height = CAIRO_MAXSHORT;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_surface_is_meta:
 * @surface: a #cairo_surface_t
 * 
 * Checks if a surface is a #cairo_meta_surface_t
 * 
 * Return value: TRUE if the surface is a meta surface
 **/
cairo_bool_t
_cairo_surface_is_meta (const cairo_surface_t *surface)
{
    return surface->backend == &cairo_meta_surface_backend;
}

static const cairo_surface_backend_t cairo_meta_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_META,
    _cairo_meta_surface_create_similar,
    _cairo_meta_surface_finish,
    _cairo_meta_surface_acquire_source_image,
    _cairo_meta_surface_release_source_image,
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_meta_surface_intersect_clip_path,
    _cairo_meta_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here are the 5 basic drawing operations, (which are in some
     * sense the only things that cairo_meta_surface should need to
     * implement). */
    
    _cairo_meta_surface_paint,
    _cairo_meta_surface_mask,
    _cairo_meta_surface_stroke,
    _cairo_meta_surface_fill,
    _cairo_meta_surface_show_glyphs,

    _cairo_meta_surface_snapshot
};

cairo_status_t
_cairo_meta_surface_replay (cairo_surface_t *surface,
			    cairo_surface_t *target)
{
    cairo_meta_surface_t *meta;
    cairo_command_t *command, **elements;
    int i, num_elements;
    cairo_int_status_t status;
    cairo_clip_t clip;

    meta = (cairo_meta_surface_t *) surface;
    status = CAIRO_STATUS_SUCCESS;

    _cairo_clip_init (&clip, target);    

    num_elements = meta->commands.num_elements;
    elements = _cairo_array_index (&meta->commands, 0);
    for (i = 0; i < num_elements; i++) {
	command = elements[i];
	switch (command->type) {
	case CAIRO_COMMAND_PAINT:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_paint (target,
					   command->paint.op,
					   &command->paint.source.base);
	    break;

	case CAIRO_COMMAND_MASK:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_mask (target,
					  command->mask.op,
					  &command->mask.source.base,
					  &command->mask.mask.base);
	    break;

	case CAIRO_COMMAND_STROKE:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_stroke (target,
					    command->stroke.op,
					    &command->stroke.source.base,
					    &command->stroke.path,
					    &command->stroke.style,
					    &command->stroke.ctm,
					    &command->stroke.ctm_inverse,
					    command->stroke.tolerance,
					    command->stroke.antialias);
	    break;

	case CAIRO_COMMAND_FILL:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_fill (target,
					  command->fill.op,
					  &command->fill.source.base,
					  &command->fill.path,
					  command->fill.fill_rule,
					  command->fill.tolerance,
					  command->fill.antialias);
	    break;

	case CAIRO_COMMAND_SHOW_GLYPHS:
	    status = _cairo_surface_set_clip (target, &clip);
	    if (status)
		break;

	    status = _cairo_surface_show_glyphs	(target,
						 command->show_glyphs.op,
						 &command->show_glyphs.source.base,
						 command->show_glyphs.glyphs,
						 command->show_glyphs.num_glyphs,
						 command->show_glyphs.scaled_font);
	    break;

	case CAIRO_COMMAND_INTERSECT_CLIP_PATH:
	    /* XXX Meta surface clipping is broken and requires some
	     * cairo-gstate.c rewriting.  Work around it for now. */
	    if (command->intersect_clip_path.path_pointer == NULL)
		status = _cairo_clip_reset (&clip);
	    else
		status = _cairo_clip_clip (&clip,
					   command->intersect_clip_path.path_pointer,
					   command->intersect_clip_path.fill_rule,
					   command->intersect_clip_path.tolerance,
					   command->intersect_clip_path.antialias,
					   target);
	    break;

	default:
	    ASSERT_NOT_REACHED;
	}

	if (status)
	    break;
    }

    _cairo_clip_fini (&clip);

    return status;
}
