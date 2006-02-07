/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

#include "cairoint.h"

cairo_fixed_t
_cairo_fixed_from_int (int i)
{
    return i << 16;
}

cairo_fixed_t
_cairo_fixed_from_double (double d)
{
    return (cairo_fixed_t) floor (d * 65536 + 0.5);
}

cairo_fixed_t
_cairo_fixed_from_26_6 (uint32_t i)
{
    return i << 10;
}

double
_cairo_fixed_to_double (cairo_fixed_t f)
{
    return ((double) f) / 65536.0;
}

int
_cairo_fixed_is_integer (cairo_fixed_t f)
{
    return (f & 0xFFFF) == 0;
}

int
_cairo_fixed_integer_part (cairo_fixed_t f)
{
    return f >> 16;
}

int
_cairo_fixed_integer_floor (cairo_fixed_t f)
{
    if (f >= 0)
	return f >> 16;
    else
	return -((-f - 1) >> 16) - 1;
}

int
_cairo_fixed_integer_ceil (cairo_fixed_t f)
{
    if (f > 0)
	return ((f - 1)>>16) + 1;
    else
	return - (-f >> 16);
}
