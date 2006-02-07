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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#ifndef CAIRO_CLIP_PRIVATE_H
#define CAIRO_CLIP_PRIVATE_H

#include "cairo-path-fixed-private.h"

struct _cairo_clip_path {
    unsigned int	ref_count;
    cairo_path_fixed_t	path;
    cairo_fill_rule_t	fill_rule;
    double		tolerance;
    cairo_antialias_t	antialias;
    cairo_clip_path_t	*prev;
};

struct _cairo_clip {
    cairo_clip_mode_t mode;

    /*
     * Mask-based clipping for cases where the backend 
     * clipping isn't sufficiently able.
     *
     * The rectangle here represents the
     * portion of the destination surface that this
     * clip surface maps to, it does not
     * represent the extents of the clip region or
     * clip paths
     */
    cairo_surface_t *surface;
    cairo_rectangle_t surface_rect;
    /*
     * Surface clip serial number to store
     * in the surface when this clip is set
     */
    unsigned int serial;
    /*
     * A clip region that can be placed in the surface
     */
    pixman_region16_t *region;
    /*
     * If the surface supports path clipping, we store the list of
     * clipping paths that has been set here as a linked list.
     */
    cairo_clip_path_t *path;
};

cairo_private void
_cairo_clip_init (cairo_clip_t *clip, cairo_surface_t *target);

cairo_private void
_cairo_clip_fini (cairo_clip_t *clip);

cairo_private void
_cairo_clip_init_copy (cairo_clip_t *clip, cairo_clip_t *other);

cairo_private cairo_status_t
_cairo_clip_reset (cairo_clip_t *clip);

cairo_private cairo_status_t
_cairo_clip_clip (cairo_clip_t       *clip,
		  cairo_path_fixed_t *path,
		  cairo_fill_rule_t   fill_rule,
		  double              tolerance,
		  cairo_antialias_t   antialias,
		  cairo_surface_t    *target);

cairo_private cairo_status_t
_cairo_clip_intersect_to_rectangle (cairo_clip_t      *clip,
				    cairo_rectangle_t *rectangle);

cairo_private cairo_status_t
_cairo_clip_intersect_to_region (cairo_clip_t      *clip,
				 pixman_region16_t *region);

cairo_private cairo_status_t
_cairo_clip_combine_to_surface (cairo_clip_t            *clip,
				cairo_operator_t         op,
				cairo_surface_t         *dst,
				int                      dst_x,
				int                      dst_y,
				const cairo_rectangle_t *extents);

#endif /* CAIRO_CLIP_PRIVATE_H */
