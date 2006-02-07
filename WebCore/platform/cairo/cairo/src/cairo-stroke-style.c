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
 *	Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"

void
_cairo_stroke_style_init (cairo_stroke_style_t *style)
{
    style->line_width = CAIRO_GSTATE_LINE_WIDTH_DEFAULT;
    style->line_cap = CAIRO_GSTATE_LINE_CAP_DEFAULT;
    style->line_join = CAIRO_GSTATE_LINE_JOIN_DEFAULT;
    style->miter_limit = CAIRO_GSTATE_MITER_LIMIT_DEFAULT;

    style->dash = NULL;
    style->num_dashes = 0;
    style->dash_offset = 0.0;
}

cairo_status_t
_cairo_stroke_style_init_copy (cairo_stroke_style_t *style,
			       cairo_stroke_style_t *other)
{
    style->line_width = other->line_width;
    style->line_cap = other->line_cap;
    style->line_join = other->line_join;
    style->miter_limit = other->miter_limit;

    style->num_dashes = other->num_dashes;

    if (other->dash == NULL) {
	style->dash = NULL;
    } else {
	style->dash = malloc (style->num_dashes * sizeof (double));
	if (style->dash == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	memcpy (style->dash, other->dash,
		style->num_dashes * sizeof (double));
    }

    style->dash_offset = other->dash_offset;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_stroke_style_fini (cairo_stroke_style_t *style)
{
    if (style->dash) {
	free (style->dash);
	style->dash = NULL;
    }
    style->num_dashes = 0;
}

