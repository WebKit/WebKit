/*
 * $Id: cairo-wideint.h,v 1.11 2006/02/03 04:49:23 vladimir%pobox.com Exp $
 *
 * Copyright © 2004 Keith Packard
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *	Keith R. Packard <keithp@keithp.com>
 *
 */

#ifndef CAIRO_WIDEINT_H
#define CAIRO_WIDEINT_H

#if   HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
# include <sys/int_types.h>
#elif defined(_MSC_VER)
  typedef __int8 int8_t;
  typedef unsigned __int8 uint8_t;
  typedef __int16 int16_t;
  typedef unsigned __int16 uint16_t;
  typedef __int32 int32_t;
  typedef unsigned __int32 uint32_t;
  typedef __int64 int64_t;
  typedef unsigned __int64 uint64_t;
# ifndef HAVE_UINT64_T
#  define HAVE_UINT64_T 1
# endif
#else
#error Cannot find definitions for fixed-width integral types (uint8_t, uint32_t, etc.)
#endif

/*
 * 64-bit datatypes.  Two separate implementations, one using
 * built-in 64-bit signed/unsigned types another implemented
 * as a pair of 32-bit ints
 */

#define I cairo_private

#if !HAVE_UINT64_T

typedef struct _cairo_uint64 {
    uint32_t	lo, hi;
} cairo_uint64_t, cairo_int64_t;

cairo_uint64_t I	_cairo_uint32_to_uint64 (uint32_t i);
#define			_cairo_uint64_to_uint32(a)  ((a).lo)
cairo_uint64_t I	_cairo_uint64_add (cairo_uint64_t a, cairo_uint64_t b);
cairo_uint64_t I	_cairo_uint64_sub (cairo_uint64_t a, cairo_uint64_t b);
cairo_uint64_t I	_cairo_uint64_mul (cairo_uint64_t a, cairo_uint64_t b);
cairo_uint64_t I	_cairo_uint32x32_64_mul (uint32_t a, uint32_t b);
cairo_uint64_t I	_cairo_uint64_lsl (cairo_uint64_t a, int shift);
cairo_uint64_t I	_cairo_uint64_rsl (cairo_uint64_t a, int shift);
cairo_uint64_t I	_cairo_uint64_rsa (cairo_uint64_t a, int shift);
int	       I	_cairo_uint64_lt (cairo_uint64_t a, cairo_uint64_t b);
int	       I	_cairo_uint64_eq (cairo_uint64_t a, cairo_uint64_t b);
cairo_uint64_t I	_cairo_uint64_negate (cairo_uint64_t a);
#define			_cairo_uint64_negative(a)   (((int32_t) ((a).hi)) < 0)
cairo_uint64_t I	_cairo_uint64_not (cairo_uint64_t a);

#define			_cairo_uint64_to_int64(i)   (i)
#define			_cairo_int64_to_uint64(i)   (i)

cairo_int64_t  I	_cairo_int32_to_int64(int32_t i);
#define			_cairo_int64_to_int32(a)    ((int32_t) _cairo_uint64_to_uint32(a))
#define			_cairo_int64_add(a,b)	    _cairo_uint64_add (a,b)
#define			_cairo_int64_sub(a,b)	    _cairo_uint64_sub (a,b)
#define			_cairo_int64_mul(a,b)	    _cairo_uint64_mul (a,b)
cairo_int64_t  I	_cairo_int32x32_64_mul (int32_t a, int32_t b);
int	       I	_cairo_int64_lt (cairo_uint64_t a, cairo_uint64_t b);
#define			_cairo_int64_eq(a,b)	    _cairo_uint64_eq (a,b)
#define			_cairo_int64_lsl(a,b)	    _cairo_uint64_lsl (a,b)
#define			_cairo_int64_rsl(a,b)	    _cairo_uint64_rsl (a,b)
#define			_cairo_int64_rsa(a,b)	    _cairo_uint64_rsa (a,b)
#define			_cairo_int64_negate(a)	    _cairo_uint64_negate(a)
#define			_cairo_int64_negative(a)    (((int32_t) ((a).hi)) < 0)
#define			_cairo_int64_not(a)	    _cairo_uint64_not(a)

#else

typedef uint64_t    cairo_uint64_t;
typedef int64_t	    cairo_int64_t;

#define			_cairo_uint32_to_uint64(i)  ((uint64_t) (i))
#define			_cairo_uint64_to_uint32(i)  ((uint32_t) (i))
#define			_cairo_uint64_add(a,b)	    ((a) + (b))
#define			_cairo_uint64_sub(a,b)	    ((a) - (b))
#define			_cairo_uint64_mul(a,b)	    ((a) * (b))
#define			_cairo_uint32x32_64_mul(a,b)	((uint64_t) (a) * (b))
#define			_cairo_uint64_lsl(a,b)	    ((a) << (b))
#define			_cairo_uint64_rsl(a,b)	    ((uint64_t) (a) >> (b))
#define			_cairo_uint64_rsa(a,b)	    ((uint64_t) ((int64_t) (a) >> (b)))
#define			_cairo_uint64_lt(a,b)	    ((a) < (b))
#define			_cairo_uint64_eq(a,b)	    ((a) == (b))
#define			_cairo_uint64_negate(a)	    ((uint64_t) -((int64_t) (a)))
#define			_cairo_uint64_negative(a)   ((int64_t) (a) < 0)
#define			_cairo_uint64_not(a)	    (~(a))

#define			_cairo_uint64_to_int64(i)   ((int64_t) (i))
#define			_cairo_int64_to_uint64(i)   ((uint64_t) (i))

#define			_cairo_int32_to_int64(i)    ((int64_t) (i))
#define			_cairo_int64_to_int32(i)    ((int32_t) (i))
#define			_cairo_int64_add(a,b)	    ((a) + (b))
#define			_cairo_int64_sub(a,b)	    ((a) - (b))
#define			_cairo_int64_mul(a,b)	    ((a) * (b))
#define			_cairo_int32x32_64_mul(a,b) ((int64_t) (a) * (b))
#define			_cairo_int64_lt(a,b)	    ((a) < (b))
#define			_cairo_int64_eq(a,b)	    ((a) == (b))
#define			_cairo_int64_lsl(a,b)	    ((a) << (b))
#define			_cairo_int64_rsl(a,b)	    ((int64_t) ((uint64_t) (a) >> (b)))
#define			_cairo_int64_rsa(a,b)	    ((int64_t) (a) >> (b))
#define			_cairo_int64_negate(a)	    (-(a))
#define			_cairo_int64_negative(a)    ((a) < 0)
#define			_cairo_int64_not(a)	    (~(a))

#endif

/*
 * 64-bit comparisions derived from lt or eq
 */
#define			_cairo_uint64_le(a,b)	    (!_cairo_uint64_gt(a,b))
#define			_cairo_uint64_ne(a,b)	    (!_cairo_uint64_eq(a,b))
#define			_cairo_uint64_ge(a,b)	    (!_cairo_uint64_lt(a,b))
#define			_cairo_uint64_gt(a,b)	    _cairo_uint64_lt(b,a)

#define			_cairo_int64_le(a,b)	    (!_cairo_int64_gt(a,b))
#define			_cairo_int64_ne(a,b)	    (!_cairo_int64_eq(a,b))
#define			_cairo_int64_ge(a,b)	    (!_cairo_int64_lt(a,b))
#define			_cairo_int64_gt(a,b)	    _cairo_int64_lt(b,a)

/*
 * As the C implementation always computes both, create
 * a function which returns both for the 'native' type as well
 */

typedef struct _cairo_uquorem64 {
    cairo_uint64_t	quo;
    cairo_uint64_t	rem;
} cairo_uquorem64_t;

typedef struct _cairo_quorem64 {
    cairo_int64_t	quo;
    cairo_int64_t	rem;
} cairo_quorem64_t;

cairo_uquorem64_t I
_cairo_uint64_divrem (cairo_uint64_t num, cairo_uint64_t den);

cairo_quorem64_t I
_cairo_int64_divrem (cairo_int64_t num, cairo_int64_t den);

/*
 * 128-bit datatypes.  Again, provide two implementations in
 * case the machine has a native 128-bit datatype.  GCC supports int128_t
 * on ia64
 */
 
#if !HAVE_UINT128_T

typedef struct cairo_uint128 {
    cairo_uint64_t	lo, hi;
} cairo_uint128_t, cairo_int128_t;

cairo_uint128_t I	_cairo_uint32_to_uint128 (uint32_t i);
cairo_uint128_t I	_cairo_uint64_to_uint128 (cairo_uint64_t i);
#define			_cairo_uint128_to_uint64(a)	((a).lo)
#define			_cairo_uint128_to_uint32(a)	_cairo_uint64_to_uint32(_cairo_uint128_to_uint64(a))
cairo_uint128_t I	_cairo_uint128_add (cairo_uint128_t a, cairo_uint128_t b);
cairo_uint128_t I	_cairo_uint128_sub (cairo_uint128_t a, cairo_uint128_t b);
cairo_uint128_t I	_cairo_uint128_mul (cairo_uint128_t a, cairo_uint128_t b);
cairo_uint128_t I	_cairo_uint64x64_128_mul (cairo_uint64_t a, cairo_uint64_t b);
cairo_uint128_t I	_cairo_uint128_lsl (cairo_uint128_t a, int shift);
cairo_uint128_t I	_cairo_uint128_rsl (cairo_uint128_t a, int shift);
cairo_uint128_t I	_cairo_uint128_rsa (cairo_uint128_t a, int shift);
int	        I	_cairo_uint128_lt (cairo_uint128_t a, cairo_uint128_t b);
int	        I	_cairo_uint128_eq (cairo_uint128_t a, cairo_uint128_t b);
cairo_uint128_t I	_cairo_uint128_negate (cairo_uint128_t a);
#define			_cairo_uint128_negative(a)  (_cairo_uint64_negative(a.hi))
cairo_uint128_t I	_cairo_uint128_not (cairo_uint128_t a);

#define			_cairo_uint128_to_int128_(i)	(i)
#define			_cairo_int128_to_uint128(i)	(i)

cairo_int128_t  I	_cairo_int32_to_int128 (int32_t i);
cairo_int128_t  I	_cairo_int64_to_int128 (cairo_int64_t i);
#define			_cairo_int128_to_int64(a)   ((cairo_int64_t) (a).lo)
#define			_cairo_int128_to_int32(a)   _cairo_int64_to_int32(_cairo_int128_to_int64(a))
#define			_cairo_int128_add(a,b)	    _cairo_uint128_add(a,b)
#define			_cairo_int128_sub(a,b)	    _cairo_uint128_sub(a,b)
#define			_cairo_int128_mul(a,b)	    _cairo_uint128_mul(a,b)
cairo_int128_t I _cairo_int64x64_128_mul (cairo_int64_t a, cairo_int64_t b);
#define			_cairo_int128_lsl(a,b)	    _cairo_uint128_lsl(a,b)
#define			_cairo_int128_rsl(a,b)	    _cairo_uint128_rsl(a,b)
#define			_cairo_int128_rsa(a,b)	    _cairo_uint128_rsa(a,b)
int 	        I	_cairo_int128_lt (cairo_int128_t a, cairo_int128_t b);
#define			_cairo_int128_eq(a,b)	    _cairo_uint128_eq (a,b)
#define			_cairo_int128_negate(a)	    _cairo_uint128_negate(a)
#define			_cairo_int128_negative(a)   (_cairo_uint128_negative(a))
#define			_cairo_int128_not(a)	    _cairo_uint128_not(a)

#else	/* !HAVE_UINT128_T */

typedef uint128_t	cairo_uint128_t;
typedef int128_t	cairo_int128_t;

#define			_cairo_uint32_to_uint128(i) ((uint128_t) (i))
#define			_cairo_uint64_to_uint128(i) ((uint128_t) (i))
#define			_cairo_uint128_to_uint64(i) ((uint64_t) (i))
#define			_cairo_uint128_to_uint32(i) ((uint32_t) (i))
#define			_cairo_uint128_add(a,b)	    ((a) + (b))
#define			_cairo_uint128_sub(a,b)	    ((a) - (b))
#define			_cairo_uint128_mul(a,b)	    ((a) * (b))
#define			_cairo_uint64x64_128_mul(a,b)	((uint128_t) (a) * (b))
#define			_cairo_uint128_lsl(a,b)	    ((a) << (b))
#define			_cairo_uint128_rsl(a,b)	    ((uint128_t) (a) >> (b))
#define			_cairo_uint128_rsa(a,b)	    ((uint128_t) ((int128_t) (a) >> (b)))
#define			_cairo_uint128_lt(a,b)	    ((a) < (b))
#define			_cairo_uint128_eq(a,b)	    ((a) == (b))
#define			_cairo_uint128_negate(a)    ((uint128_t) -((int128_t) (a)))
#define			_cairo_uint128_negative(a)  ((int128_t) (a) < 0)
#define			_cairo_uint128_not(a)	    (~(a))

#define			_cairo_uint128_to_int128(i) ((int128_t) (i))
#define			_cairo_int128_to_uint128(i) ((uint128_t) (i))

#define			_cairo_int32_to_int128(i)   ((int128_t) (i))
#define			_cairo_int64_to_int128(i)   ((int128_t) (i))
#define			_cairo_int128_to_int64(i)   ((int64_t) (i))
#define			_cairo_int128_to_int32(i)   ((int32_t) (i))
#define			_cairo_int128_add(a,b)	    ((a) + (b))
#define			_cairo_int128_sub(a,b)	    ((a) - (b))
#define			_cairo_int128_mul(a,b)	    ((a) * (b))
#define			_cairo_int64x64_128_mul(a,b) ((int128_t) (a) * (b))
#define			_cairo_int128_lt(a,b)	    ((a) < (b))
#define			_cairo_int128_eq(a,b)	    ((a) == (b))
#define			_cairo_int128_lsl(a,b)	    ((a) << (b))
#define			_cairo_int128_rsl(a,b)	    ((int128_t) ((uint128_t) (a) >> (b)))
#define			_cairo_int128_rsa(a,b)	    ((int128_t) (a) >> (b))
#define			_cairo_int128_negate(a)	    (-(a))
#define			_cairo_int128_negative(a)   ((a) < 0)
#define			_cairo_int128_not(a)	    (~(a))

#endif	/* HAVE_UINT128_T */

typedef struct _cairo_uquorem128 {
    cairo_uint128_t	quo;
    cairo_uint128_t	rem;
} cairo_uquorem128_t;

typedef struct _cairo_quorem128 {
    cairo_int128_t	quo;
    cairo_int128_t	rem;
} cairo_quorem128_t;

cairo_uquorem128_t I
_cairo_uint128_divrem (cairo_uint128_t num, cairo_uint128_t den);

cairo_quorem128_t I
_cairo_int128_divrem (cairo_int128_t num, cairo_int128_t den);

#define			_cairo_uint128_le(a,b)	    (!_cairo_uint128_gt(a,b))
#define			_cairo_uint128_ne(a,b)	    (!_cairo_uint128_eq(a,b))
#define			_cairo_uint128_ge(a,b)	    (!_cairo_uint128_lt(a,b))
#define			_cairo_uint128_gt(a,b)	    _cairo_uint128_lt(b,a)

#define			_cairo_int128_le(a,b)	    (!_cairo_int128_gt(a,b))
#define			_cairo_int128_ne(a,b)	    (!_cairo_int128_eq(a,b))
#define			_cairo_int128_ge(a,b)	    (!_cairo_int128_lt(a,b))
#define			_cairo_int128_gt(a,b)	    _cairo_int128_lt(b,a)

#undef I

#endif /* CAIRO_WIDEINT_H */
