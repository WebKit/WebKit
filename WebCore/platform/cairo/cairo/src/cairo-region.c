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
 *	Owen Taylor <otaylor@redhat.com>
 */

#include <cairoint.h>

/**
 * _cairo_region_create_from_rectangle:
 * @rect: a #cairo_rectangle_t
 * 
 * Creates a region with extents initialized from the given
 * rectangle.
 * 
 * Return value: a newly created #pixman_region16_t or %NULL if
 *    memory couldn't a allocated.
 **/
pixman_region16_t *
_cairo_region_create_from_rectangle (cairo_rectangle_t *rect)
{
    /* We can't use pixman_region_create_simple(), because it doesn't
     * have an error return
     */
    pixman_region16_t *region = pixman_region_create ();
    if (pixman_region_union_rect (region, region,
				  rect->x, rect->y,
				  rect->width, rect->height) != PIXMAN_REGION_STATUS_SUCCESS) {
	pixman_region_destroy (region);
	return NULL;
    }
    
    return region;
}

/**
 * _cairo_region_extents_rectangle:
 * @region: a #pixman_region16_t
 * @rect: rectangle into which to store the extents
 * 
 * Gets the bounding box of a region as a cairo_rectangle_t
 **/
void
_cairo_region_extents_rectangle (pixman_region16_t *region,
				 cairo_rectangle_t *rect)
{
    pixman_box16_t *region_extents = pixman_region_extents (region);

    rect->x = region_extents->x1;
    rect->y = region_extents->y1;
    rect->width = region_extents->x2 - region_extents->x1;
    rect->height = region_extents->y2 - region_extents->y1;
}

