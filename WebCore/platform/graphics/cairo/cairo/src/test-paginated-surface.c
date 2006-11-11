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

/* This isn't a "real" surface, but just something to be used by the
 * test suite to help exercise the paginated-surface paths in cairo.
 *
 * The defining feature of this backend is that it uses a paginated
 * surface to record all operations, and then replays everything to an
 * image surface.
 *
 * It's possible that this code might serve as a good starting point
 * for someone working on bringing up a new paginated-surface-based
 * backend.
 */

#include "test-paginated-surface.h"

#include "cairoint.h"

#include "cairo-paginated-surface-private.h"

cairo_surface_t *
_test_paginated_surface_create_for_data (unsigned char		*data,
					 cairo_content_t	 content,
					 int		 	 width,
					 int		 	 height,
					 int		 	 stride)
{
    cairo_surface_t *target;

    target = _cairo_image_surface_create_for_data_with_content (data, content,
								width, height,
								stride);

    return _cairo_paginated_surface_create (target, content, width, height);
}
