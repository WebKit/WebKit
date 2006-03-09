/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *      Vladimir Vukicevic <vladimir@mozilla.com>
 */

#ifndef CAIRO_QUARTZGL_H
#define CAIRO_QUARTZGL_H

#include <cairo.h>

#if CAIRO_HAS_QUARTZGL_SURFACE

#include <Carbon/Carbon.h>

#include <AGL/agl.h>

CAIRO_BEGIN_DECLS

cairo_surface_t *
cairo_quartzgl_surface_create (cairo_format_t format,
                               unsigned int width,
                               unsigned int height,
                               cairo_bool_t y_grows_down);

cairo_surface_t *
cairo_quartzgl_surface_create_for_agl_context (AGLContext aglContext,
                                               unsigned int width,
                                               unsigned int height,
                                               cairo_bool_t y_grows_down);

cairo_surface_t *
cairo_quartzgl_surface_create_for_cg_context (CGContextRef cgContext,
                                              unsigned int width,
                                              unsigned int height,
                                              cairo_bool_t y_grows_down);

cairo_bool_t
cairo_surface_is_quartzgl (cairo_surface_t *surf);

CAIRO_END_DECLS

#else  /* CAIRO_HAS_QUARTZGL_SURFACE */
# error Cairo was not compiled with support for the quartzgl backend
#endif /* CAIRO_HAS_QUARTZGL_SURFACE */

#endif /* CAIRO_QUARTZGL_H */
