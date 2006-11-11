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
 */

#ifndef CAIRO_META_SURFACE_H
#define CAIRO_META_SURFACE_H

#include "cairoint.h"
#include "cairo-path-fixed-private.h"

typedef enum {
    /* The 5 basic drawing operations. */
    CAIRO_COMMAND_PAINT,
    CAIRO_COMMAND_MASK,
    CAIRO_COMMAND_STROKE,
    CAIRO_COMMAND_FILL,
    CAIRO_COMMAND_SHOW_GLYPHS,

    /* Other junk. For most of these, we should be able to assert that
     * they never get called except as part of fallbacks for the 5
     * basic drawing operations (which we implement already so the
     * fallbacks should never get triggered). So the plan is to
     * eliminate as many of these as possible. */

    CAIRO_COMMAND_INTERSECT_CLIP_PATH

} cairo_command_type_t;

typedef struct _cairo_command_paint {
    cairo_command_type_t	 type;
    cairo_operator_t		 op;
    cairo_pattern_union_t	 source;
} cairo_command_paint_t;

typedef struct _cairo_command_mask {
    cairo_command_type_t	 type;
    cairo_operator_t		 op;
    cairo_pattern_union_t	 source;
    cairo_pattern_union_t	 mask;
} cairo_command_mask_t;

typedef struct _cairo_command_stroke {
    cairo_command_type_t	 type;
    cairo_operator_t		 op;
    cairo_pattern_union_t	 source;
    cairo_path_fixed_t		 path;
    cairo_stroke_style_t	 style;
    cairo_matrix_t		 ctm;
    cairo_matrix_t		 ctm_inverse;
    double			 tolerance;
    cairo_antialias_t		 antialias;
} cairo_command_stroke_t;

typedef struct _cairo_command_fill {
    cairo_command_type_t	 type;
    cairo_operator_t		 op;
    cairo_pattern_union_t	 source;
    cairo_path_fixed_t		 path;
    cairo_fill_rule_t		 fill_rule;
    double			 tolerance;
    cairo_antialias_t		 antialias;
} cairo_command_fill_t;

typedef struct _cairo_command_show_glyphs {
    cairo_command_type_t	 type;
    cairo_operator_t		 op;
    cairo_pattern_union_t	 source;
    cairo_glyph_t		*glyphs;
    int				 num_glyphs;
    cairo_scaled_font_t		*scaled_font;
} cairo_command_show_glyphs_t;

typedef struct _cairo_command_intersect_clip_path {
    cairo_command_type_t	type;
    cairo_path_fixed_t	       *path_pointer;
    cairo_path_fixed_t		path;
    cairo_fill_rule_t		fill_rule;
    double			tolerance;
    cairo_antialias_t		antialias;
} cairo_command_intersect_clip_path_t;

typedef union _cairo_command {
    cairo_command_type_t			type;

    /* The 5 basic drawing operations. */
    cairo_command_paint_t			paint;
    cairo_command_mask_t			mask;
    cairo_command_stroke_t			stroke;
    cairo_command_fill_t			fill;
    cairo_command_show_glyphs_t			show_glyphs;

    /* The other junk. */
    cairo_command_intersect_clip_path_t		intersect_clip_path;
} cairo_command_t;

typedef struct _cairo_meta_surface {
    cairo_surface_t base;

    cairo_content_t content;

    /* A meta-surface is logically unbounded, but when used as a
     * source we need to render it to an image, so we need a size at
     * which to create that image. */
    int width_pixels;
    int height_pixels;

    cairo_array_t commands;
    cairo_surface_t *commands_owner;
} cairo_meta_surface_t;

cairo_private cairo_surface_t *
_cairo_meta_surface_create (cairo_content_t	content,
			    int			width_pixels,
			    int			height_pixels);

cairo_private cairo_status_t
_cairo_meta_surface_replay (cairo_surface_t *surface,
			    cairo_surface_t *target);

cairo_private cairo_bool_t
_cairo_surface_is_meta (const cairo_surface_t *surface);

#endif /* CAIRO_META_SURFACE_H */
