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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"

#ifndef CAIRO_FONT_SUBSET_PRIVATE_H
#define CAIRO_FONT_SUBSET_PRIVATE_H

typedef struct cairo_font_subset_backend cairo_font_subset_backend_t;
typedef struct cairo_font_subset cairo_font_subset_t;
struct cairo_font_subset {
    cairo_font_subset_backend_t *backend;
    cairo_unscaled_font_t *unscaled_font;
    unsigned int font_id;
    char *base_font;
    int num_glyphs;
    int *widths;
    long x_min, y_min, x_max, y_max;
    long ascent, descent;
};


cairo_private int
_cairo_font_subset_use_glyph (cairo_font_subset_t *font, int glyph);

cairo_private cairo_status_t
_cairo_font_subset_generate (cairo_font_subset_t *font,
			    const char **data, unsigned long *length);

cairo_private void
_cairo_font_subset_destroy (cairo_font_subset_t *font);

cairo_private cairo_font_subset_t *
_cairo_font_subset_create (cairo_unscaled_font_t *unscaled_font);

#endif /* CAIRO_FONT_SUBSET_PRIVATE_H */
