/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#ifndef CAIRO_SURFACE_FALLBACK_PRIVATE_H
#define CAIRO_SURFACE_FALLBACK_PRIVATE_H

#include "cairoint.h"

cairo_private cairo_status_t
_cairo_surface_fallback_paint (cairo_surface_t	*surface,
			       cairo_operator_t	 op,
			       cairo_pattern_t	*source);
cairo_private cairo_status_t
_cairo_surface_fallback_mask (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_pattern_t		*mask);

cairo_private cairo_status_t
_cairo_surface_fallback_stroke (cairo_surface_t		*surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_path_fixed_t	*path,
				cairo_stroke_style_t	*stroke_style,
				cairo_matrix_t		*ctm,
				cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias);

cairo_private cairo_status_t
_cairo_surface_fallback_fill (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double		 	 tolerance,
			      cairo_antialias_t		 antialias);

cairo_private cairo_status_t
_cairo_surface_fallback_show_glyphs (cairo_surface_t		*surface,
				     cairo_operator_t		 op,
				     cairo_pattern_t		*source,
				     const cairo_glyph_t	*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font);

cairo_private cairo_surface_t *
_cairo_surface_fallback_snapshot (cairo_surface_t *surface);

cairo_private cairo_status_t
_cairo_surface_fallback_composite (cairo_operator_t	op,
				   cairo_pattern_t	*src,
				   cairo_pattern_t	*mask,
				   cairo_surface_t	*dst,
				   int			src_x,
				   int			src_y,
				   int			mask_x,
				   int			mask_y,
				   int			dst_x,
				   int			dst_y,
				   unsigned int		width,
				   unsigned int		height);

cairo_private cairo_status_t
_cairo_surface_fallback_fill_rectangles (cairo_surface_t	*surface,
					 cairo_operator_t	 op,
					 const cairo_color_t	*color,
					 cairo_rectangle_t	*rects,
					 int			 num_rects);

cairo_private cairo_status_t
_cairo_surface_fallback_composite_trapezoids (cairo_operator_t		op,
					      cairo_pattern_t	       *pattern,
					      cairo_surface_t	       *dst,
					      cairo_antialias_t		antialias,
					      int			src_x,
					      int			src_y,
					      int			dst_x,
					      int			dst_y,
					      unsigned int		width,
					      unsigned int		height,
					      cairo_trapezoid_t	       *traps,
					      int			num_traps);

#endif
