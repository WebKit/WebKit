/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 *	Carl D. Worth <cworth@redhat.com>
 */

#ifndef CAIRO_GSTATE_PRIVATE_H
#define CAIRO_GSTATE_PRIVATE_H

#include "cairo-clip-private.h"

struct _cairo_gstate {
    cairo_operator_t op;
    
    double tolerance;
    cairo_antialias_t antialias;

    cairo_stroke_style_t stroke_style;

    cairo_fill_rule_t fill_rule;

    cairo_font_face_t *font_face;
    cairo_scaled_font_t *scaled_font;	/* Specific to the current CTM */
    cairo_matrix_t font_matrix;
    cairo_font_options_t font_options;

    cairo_clip_t clip;

    cairo_surface_t *target;		/* The target to which all rendering is directed */
    cairo_surface_t *parent_target;	/* The previous target which was receiving rendering */
    cairo_surface_t *original_target;	/* The original target the initial gstate was created with */

    cairo_matrix_t ctm;
    cairo_matrix_t ctm_inverse;
    cairo_matrix_t source_ctm_inverse; /* At the time ->source was set */

    cairo_pattern_t *source;

    struct _cairo_gstate *next;
};

#endif /* CAIRO_GSTATE_PRIVATE_H */
