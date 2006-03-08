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
 *	Owen Taylor <otaylor@redhat.com>
 */

#ifndef CAIRO_WIN32_PRIVATE_H
#define CAIRO_WIN32_PRIVATE_H

#include <cairo-win32.h>
#include <cairoint.h>

typedef struct _cairo_win32_surface {
    cairo_surface_t base;

    cairo_format_t format;
    
    HDC dc;

    /* We create off-screen surfaces as DIBs */
    HBITMAP bitmap;

    /* Used to save the initial 1x1 monochrome bitmap for the DC to
     * select back into the DC before deleting the DC and our
     * bitmap. For Windows XP, this doesn't seem to be necessary
     * ... we can just delete the DC and that automatically unselects
     * out bitmap. But it's standard practice so apparently is needed
     * on some versions of Windows.
     */
    HBITMAP saved_dc_bitmap;
    
    cairo_surface_t *image;
    
    cairo_rectangle_t clip_rect;

    HRGN saved_clip;

    cairo_rectangle_t extents;
} cairo_win32_surface_t;

cairo_status_t
_cairo_win32_print_gdi_error (const char *context);

cairo_bool_t
_cairo_surface_is_win32 (cairo_surface_t *surface);

#endif /* CAIRO_WIN32_PRIVATE_H */
