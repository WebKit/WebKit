/*
 * Copyright © 2003 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carl Worth not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Carl Worth makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * CARL WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL CARL WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ICINT_H_
#define _ICINT_H_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "pixman.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "slim_internal.h"

#ifndef __GNUC__
#define __inline
#endif

#if defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* C89 has implementation-defined behavior for % with negative operands.
   C99 has well-defined behavior which is that / with integers rounds toward zero
       and a%b is defined so that (a/b)*b + a%b == a.

   The C99 version gives negative remainders rather than the modulus
   in [0 .. b-1] that we want. This macro avoids using % with negative
   operands to avoid both problems.

   a and b are integers. b > 0.
*/
#define MOD(a, b) ((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-(a) - 1) % (b) - 1)

typedef struct _FbPoint {
	int16_t    x,y ;
} FbPoint;

typedef unsigned int	Mask;


#define GXcopy		0x3
#define GXor		0x7
#define ClipByChildren  0
#define PolyEdgeSharp   0
#define PolyModePrecise 0
#define CPClipMask      (1 << 6)
#define CPLastBit       11


/* Define any names that the server code will be expecting in
 * terms of libpixman names. */

typedef uint8_t			CARD8;
typedef uint16_t		CARD16;
typedef uint32_t		CARD32;
typedef int16_t			INT16;

typedef int			Bool;
#define FALSE 0
#define TRUE  1

typedef pixman_bits_t		FbBits;
typedef pixman_image_t*		PicturePtr;
typedef pixman_box16_t		BoxRec;
typedef pixman_box16_t*		BoxPtr;

typedef pixman_point_fixed_t	xPointFixed;
typedef pixman_line_fixed_t	xLineFixed;
typedef pixman_trapezoid_t	xTrapezoid;
typedef pixman_triangle_t	xTriangle;

/* These few definitions avoid me needing to include servermd.h and misc.h from Xserver/include */
#ifndef BITMAP_SCANLINE_PAD
#define BITMAP_SCANLINE_PAD  32
#define LOG2_BITMAP_PAD		5
#define LOG2_BYTES_PER_SCANLINE_PAD	2
#endif

#define LSBFirst 0
#define MSBFirst 1

#ifdef WORDS_BIGENDIAN
#  define IMAGE_BYTE_ORDER MSBFirst
#  define BITMAP_BIT_ORDER MSBFirst
#else
#  define IMAGE_BYTE_ORDER LSBFirst
#  define BITMAP_BIT_ORDER LSBFirst
#endif


#define MAXSHORT SHRT_MAX
#define MINSHORT SHRT_MIN

/* XXX: What do we need from here?
#include "picture.h"
*/



#include "pixman.h"

/* XXX: Most of this file is straight from fb.h and I imagine we can
   drop quite a bit of it. Once the real ic code starts to come
   together I can probably figure out what is not needed here. */

#define FB_UNIT	    (1 << FB_SHIFT)
#define FB_HALFUNIT (1 << (FB_SHIFT-1))
#define FB_MASK	    (FB_UNIT - 1)
#define FB_ALLONES  ((pixman_bits_t) -1)
    
/* whether to bother to include 24bpp support */
#ifndef ICNO24BIT
#define FB_24BIT
#endif

/*
 * Unless otherwise instructed, ic includes code to advertise 24bpp
 * windows with 32bpp image format for application compatibility
 */

#ifdef FB_24BIT
#ifndef ICNO24_32
#define FB_24_32BIT
#endif
#endif

#define FB_STIP_SHIFT	LOG2_BITMAP_PAD
#define FB_STIP_UNIT	(1 << FB_STIP_SHIFT)
#define FB_STIP_MASK	(FB_STIP_UNIT - 1)
#define FB_STIP_ALLONES	((FbStip) -1)
    
#define FB_STIP_ODDSTRIDE(s)	(((s) & (FB_MASK >> FB_STIP_SHIFT)) != 0)
#define FB_STIP_ODDPTR(p)	((((long) (p)) & (FB_MASK >> 3)) != 0)
    
#define FbStipStrideToBitsStride(s) (((s) >> (FB_SHIFT - FB_STIP_SHIFT)))
#define FbBitsStrideToStipStride(s) (((s) << (FB_SHIFT - FB_STIP_SHIFT)))
    
#define FbFullMask(n)   ((n) == FB_UNIT ? FB_ALLONES : ((((FbBits) 1) << n) - 1))


typedef uint32_t	    FbStip;
typedef int		    FbStride;


#ifdef FB_DEBUG
extern void fbValidateDrawable(DrawablePtr d);
extern void fbInitializeDrawable(DrawablePtr d);
extern void fbSetBits (FbStip *bits, int stride, FbStip data);
#define FB_HEAD_BITS   (FbStip) (0xbaadf00d)
#define FB_TAIL_BITS   (FbStip) (0xbaddf0ad)
#else
#define fbValidateDrawable(d)
#define fdInitializeDrawable(d)
#endif

#if BITMAP_BIT_ORDER == LSBFirst
#define FbScrLeft(x,n)	((x) >> (n))
#define FbScrRight(x,n)	((x) << (n))
/* #define FbLeftBits(x,n)	((x) & ((((FbBits) 1) << (n)) - 1)) */
#define FbLeftStipBits(x,n) ((x) & ((((FbStip) 1) << (n)) - 1))
#define FbStipMoveLsb(x,s,n)	(FbStipRight (x,(s)-(n)))
#define FbPatternOffsetBits	0
#else
#define FbScrLeft(x,n)	((x) << (n))
#define FbScrRight(x,n)	((x) >> (n))
/* #define FbLeftBits(x,n)	((x) >> (FB_UNIT - (n))) */
#define FbLeftStipBits(x,n) ((x) >> (FB_STIP_UNIT - (n)))
#define FbStipMoveLsb(x,s,n)	(x)
#define FbPatternOffsetBits	(sizeof (FbBits) - 1)
#endif

#define FbStipLeft(x,n)	FbScrLeft(x,n)
#define FbStipRight(x,n) FbScrRight(x,n)

#define FbRotLeft(x,n)	FbScrLeft(x,n) | (n ? FbScrRight(x,FB_UNIT-n) : 0)
#define FbRotRight(x,n)	FbScrRight(x,n) | (n ? FbScrLeft(x,FB_UNIT-n) : 0)

#define FbRotStipLeft(x,n)  FbStipLeft(x,n) | (n ? FbStipRight(x,FB_STIP_UNIT-n) : 0)
#define FbRotStipRight(x,n)  FbStipRight(x,n) | (n ? FbStipLeft(x,FB_STIP_UNIT-n) : 0)

#define FbLeftMask(x)	    ( ((x) & FB_MASK) ? \
			     FbScrRight(FB_ALLONES,(x) & FB_MASK) : 0)
#define FbRightMask(x)	    ( ((FB_UNIT - (x)) & FB_MASK) ? \
			     FbScrLeft(FB_ALLONES,(FB_UNIT - (x)) & FB_MASK) : 0)

#define FbLeftStipMask(x)   ( ((x) & FB_STIP_MASK) ? \
			     FbStipRight(FB_STIP_ALLONES,(x) & FB_STIP_MASK) : 0)
#define FbRightStipMask(x)  ( ((FB_STIP_UNIT - (x)) & FB_STIP_MASK) ? \
			     FbScrLeft(FB_STIP_ALLONES,(FB_STIP_UNIT - (x)) & FB_STIP_MASK) : 0)

#define FbBitsMask(x,w)	(FbScrRight(FB_ALLONES,(x) & FB_MASK) & \
			 FbScrLeft(FB_ALLONES,(FB_UNIT - ((x) + (w))) & FB_MASK))

#define FbStipMask(x,w)	(FbStipRight(FB_STIP_ALLONES,(x) & FB_STIP_MASK) & \
			 FbStipLeft(FB_STIP_ALLONES,(FB_STIP_UNIT - ((x)+(w))) & FB_STIP_MASK))


#define FbMaskBits(x,w,l,n,r) { \
    n = (w); \
    r = FbRightMask((x)+n); \
    l = FbLeftMask(x); \
    if (l) { \
	n -= FB_UNIT - ((x) & FB_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= FB_SHIFT; \
}

#ifdef ICNOPIXADDR
#define FbMaskBitsBytes(x,w,copy,l,lb,n,r,rb) FbMaskBits(x,w,l,n,r)
#define FbDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    *dst = FbDoMaskRRop(*dst,and,xor,l); \
}
#define FbDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    *dst = FbDoMaskRRop(*dst,and,xor,r); \
}
#else

#define FbByteMaskInvalid   0x10

#define FbPatternOffset(o,t)  ((o) ^ (FbPatternOffsetBits & ~(sizeof (t) - 1)))

#define FbPtrOffset(p,o,t)		((t *) ((CARD8 *) (p) + (o)))
#define FbSelectPatternPart(xor,o,t)	((xor) >> (FbPatternOffset (o,t) << 3))
#define FbStorePart(dst,off,t,xor)	(*FbPtrOffset(dst,off,t) = \
					 FbSelectPart(xor,off,t))
#ifndef FbSelectPart
#define FbSelectPart(x,o,t) FbSelectPatternPart(x,o,t)
#endif

#define FbMaskBitsBytes(x,w,copy,l,lb,n,r,rb) { \
    n = (w); \
    lb = 0; \
    rb = 0; \
    r = FbRightMask((x)+n); \
    if (r) { \
	/* compute right byte length */ \
	if ((copy) && (((x) + n) & 7) == 0) { \
	    rb = (((x) + n) & FB_MASK) >> 3; \
	} else { \
	    rb = FbByteMaskInvalid; \
	} \
    } \
    l = FbLeftMask(x); \
    if (l) { \
	/* compute left byte length */ \
	if ((copy) && ((x) & 7) == 0) { \
	    lb = ((x) & FB_MASK) >> 3; \
	} else { \
	    lb = FbByteMaskInvalid; \
	} \
	/* subtract out the portion painted by leftMask */ \
	n -= FB_UNIT - ((x) & FB_MASK); \
	if (n < 0) { \
	    if (lb != FbByteMaskInvalid) { \
		if (rb == FbByteMaskInvalid) { \
		    lb = FbByteMaskInvalid; \
		} else if (rb) { \
		    lb |= (rb - lb) << (FB_SHIFT - 3); \
		    rb = 0; \
		} \
	    } \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	}\
    } \
    n >>= FB_SHIFT; \
}

#if FB_SHIFT == 6
#define FbDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (FbBits) - 7) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 7) | (2 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 7) | (3 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 7) | (4 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 7) | (5 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 7) | (6 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 7): \
	FbStorePart(dst,sizeof (FbBits) - 7,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (FbBits) - 6) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 6) | (2 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 6) | (3 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 6) | (4 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 6) | (5 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 6): \
	FbStorePart(dst,sizeof (FbBits) - 6,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (FbBits) - 5) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 5,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 5) | (2 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 5,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 5) | (3 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 5,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 5) | (4 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 5,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 5): \
	FbStorePart(dst,sizeof (FbBits) - 5,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD32,xor); \
	break; \
    case (sizeof (FbBits) - 4) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 4) | (2 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	break; \
    case (sizeof (FbBits) - 4) | (3 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD16,xor); \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 4): \
	FbStorePart(dst,sizeof (FbBits) - 4,CARD32,xor); \
	break;

#define FbDoRightMaskByteRRop6Cases(dst,xor) \
    case 4: \
	FbStorePart(dst,0,CARD32,xor); \
	break; \
    case 5: \
	FbStorePart(dst,0,CARD32,xor); \
	FbStorePart(dst,4,CARD8,xor); \
	break; \
    case 6: \
	FbStorePart(dst,0,CARD32,xor); \
	FbStorePart(dst,4,CARD16,xor); \
	break; \
    case 7: \
	FbStorePart(dst,0,CARD32,xor); \
	FbStorePart(dst,4,CARD16,xor); \
	FbStorePart(dst,6,CARD8,xor); \
	break;
#else
#define FbDoLeftMaskByteRRop6Cases(dst,xor)
#define FbDoRightMaskByteRRop6Cases(dst,xor)
#endif

#define FbDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    switch (lb) { \
    FbDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (FbBits) - 3) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 3,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 3) | (2 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 3,CARD8,xor); \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case (sizeof (FbBits) - 2) | (1 << (FB_SHIFT - 3)): \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD8,xor); \
	break; \
    case sizeof (FbBits) - 3: \
	FbStorePart(dst,sizeof (FbBits) - 3,CARD8,xor); \
    case sizeof (FbBits) - 2: \
	FbStorePart(dst,sizeof (FbBits) - 2,CARD16,xor); \
	break; \
    case sizeof (FbBits) - 1: \
	FbStorePart(dst,sizeof (FbBits) - 1,CARD8,xor); \
	break; \
    default: \
	*dst = FbDoMaskRRop(*dst, and, xor, l); \
	break; \
    } \
}


#define FbDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    switch (rb) { \
    case 1: \
	FbStorePart(dst,0,CARD8,xor); \
	break; \
    case 2: \
	FbStorePart(dst,0,CARD16,xor); \
	break; \
    case 3: \
	FbStorePart(dst,0,CARD16,xor); \
	FbStorePart(dst,2,CARD8,xor); \
	break; \
    FbDoRightMaskByteRRop6Cases(dst,xor) \
    default: \
	*dst = FbDoMaskRRop (*dst, and, xor, r); \
    } \
}
#endif

#define FbMaskStip(x,w,l,n,r) { \
    n = (w); \
    r = FbRightStipMask((x)+n); \
    l = FbLeftStipMask(x); \
    if (l) { \
	n -= FB_STIP_UNIT - ((x) & FB_STIP_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= FB_STIP_SHIFT; \
}

/*
 * These macros are used to transparently stipple
 * in copy mode; the expected usage is with 'n' constant
 * so all of the conditional parts collapse into a minimal
 * sequence of partial word writes
 *
 * 'n' is the bytemask of which bytes to store, 'a' is the address
 * of the FbBits base unit, 'o' is the offset within that unit
 *
 * The term "lane" comes from the hardware term "byte-lane" which
 */

#define FbLaneCase1(n,a,o)  ((n) == 0x01 ? \
			     (*(CARD8 *) ((a)+FbPatternOffset(o,CARD8)) = \
			      fgxor) : 0)
#define FbLaneCase2(n,a,o)  ((n) == 0x03 ? \
			     (*(CARD16 *) ((a)+FbPatternOffset(o,CARD16)) = \
			      fgxor) : \
			     ((void)FbLaneCase1((n)&1,a,o), \
				    FbLaneCase1((n)>>1,a,(o)+1)))
#define FbLaneCase4(n,a,o)  ((n) == 0x0f ? \
			     (*(CARD32 *) ((a)+FbPatternOffset(o,CARD32)) = \
			      fgxor) : \
			     ((void)FbLaneCase2((n)&3,a,o), \
				    FbLaneCase2((n)>>2,a,(o)+2)))
#define FbLaneCase8(n,a,o)  ((n) == 0x0ff ? (*(FbBits *) ((a)+(o)) = fgxor) : \
			     ((void)FbLaneCase4((n)&15,a,o), \
				    FbLaneCase4((n)>>4,a,(o)+4)))

#if FB_SHIFT == 6
#define FbLaneCase(n,a)   FbLaneCase8(n,(CARD8 *) (a),0)
#endif

#if FB_SHIFT == 5
#define FbLaneCase(n,a)   FbLaneCase4(n,(CARD8 *) (a),0)
#endif

/* Rotate a filled pixel value to the specified alignement */
#define FbRot24(p,b)	    (FbScrRight(p,b) | FbScrLeft(p,24-(b)))
#define FbRot24Stip(p,b)    (FbStipRight(p,b) | FbStipLeft(p,24-(b)))

/* step a filled pixel value to the next/previous FB_UNIT alignment */
#define FbNext24Pix(p)	(FbRot24(p,(24-FB_UNIT%24)))
#define FbPrev24Pix(p)	(FbRot24(p,FB_UNIT%24))
#define FbNext24Stip(p)	(FbRot24(p,(24-FB_STIP_UNIT%24)))
#define FbPrev24Stip(p)	(FbRot24(p,FB_STIP_UNIT%24))

/* step a rotation value to the next/previous rotation value */
#if FB_UNIT == 64
#define FbNext24Rot(r)        ((r) == 16 ? 0 : (r) + 8)
#define FbPrev24Rot(r)        ((r) == 0 ? 16 : (r) - 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define FbFirst24Rot(x)		(((x) + 8) % 24)
#else
#define FbFirst24Rot(x)		((x) % 24)
#endif

#endif

#if FB_UNIT == 32
#define FbNext24Rot(r)        ((r) == 0 ? 16 : (r) - 8)
#define FbPrev24Rot(r)        ((r) == 16 ? 0 : (r) + 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define FbFirst24Rot(x)		(((x) + 16) % 24)
#else
#define FbFirst24Rot(x)		((x) % 24)
#endif
#endif

#define FbNext24RotStip(r)        ((r) == 0 ? 16 : (r) - 8)
#define FbPrev24RotStip(r)        ((r) == 16 ? 0 : (r) + 8)

/* Whether 24-bit specific code is needed for this filled pixel value */
#define FbCheck24Pix(p)	((p) == FbNext24Pix(p))

#define FbGetPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = icpixels->data; \
    (_stride_) = icpixels->stride / sizeof(pixman_bits_t); \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
    (yoff) = icpixels->y; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
}

#define FbGetStipPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = (FbStip *) icpixels->data; \
    (_stride_) = icpixels->stride / sizeof(FbStip); \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; \
    (yoff) = icpixels->y; \
}

#ifdef FB_OLD_SCREEN
#define BitsPerPixel(d) (\
    ((1 << PixmapWidthPaddingInfo[d].padBytesLog2) * 8 / \
    (PixmapWidthPaddingInfo[d].padRoundUp+1)))
#endif

#define FbPowerOfTwo(w)	    (((w) & ((w) - 1)) == 0)
/*
 * Accelerated tiles are power of 2 width <= FB_UNIT
 */
#define FbEvenTile(w)	    ((w) <= FB_UNIT && FbPowerOfTwo(w))
/*
 * Accelerated stipples are power of 2 width and <= FB_UNIT/dstBpp
 * with dstBpp a power of 2 as well
 */
#define FbEvenStip(w,bpp)   ((w) * (bpp) <= FB_UNIT && FbPowerOfTwo(w) && FbPowerOfTwo(bpp))

/*
 * icblt.c
 */
pixman_private void
fbBlt (pixman_bits_t   *src, 
       FbStride	srcStride,
       int	srcX,
       
       FbBits   *dst,
       FbStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       FbBits	pm,
       int	bpp,
       
       Bool	reverse,
       Bool	upsidedown);

pixman_private void
fbBlt24 (pixman_bits_t	    *srcLine,
	 FbStride   srcStride,
	 int	    srcX,

	 FbBits	    *dstLine,
	 FbStride   dstStride,
	 int	    dstX,

	 int	    width, 
	 int	    height,

	 int	    alu,
	 FbBits	    pm,

	 Bool	    reverse,
	 Bool	    upsidedown);
    
pixman_private void
fbBltStip (FbStip   *src,
	   FbStride srcStride,	    /* in FbStip units, not FbBits units */
	   int	    srcX,
	   
	   FbStip   *dst,
	   FbStride dstStride,	    /* in FbStip units, not FbBits units */
	   int	    dstX,

	   int	    width, 
	   int	    height,

	   int	    alu,
	   FbBits   pm,
	   int	    bpp);
    
/*
 * icbltone.c
 */
pixman_private void
fbBltOne (FbStip   *src,
	  FbStride srcStride,
	  int	   srcX,
	  FbBits   *dst,
	  FbStride dstStride,
	  int	   dstX,
	  int	   dstBpp,

	  int	   width,
	  int	   height,

	  FbBits   fgand,
	  FbBits   fbxor,
	  FbBits   bgand,
	  FbBits   bgxor);
 
#ifdef FB_24BIT
pixman_private void
fbBltOne24 (FbStip    *src,
	  FbStride  srcStride,	    /* FbStip units per scanline */
	  int	    srcX,	    /* bit position of source */
	  FbBits    *dst,
	  FbStride  dstStride,	    /* FbBits units per scanline */
	  int	    dstX,	    /* bit position of dest */
	  int	    dstBpp,	    /* bits per destination unit */

	  int	    width,	    /* width in bits of destination */
	  int	    height,	    /* height in scanlines */

	  FbBits    fgand,	    /* rrop values */
	  FbBits    fgxor,
	  FbBits    bgand,
	  FbBits    bgxor);
#endif

/*
 * icstipple.c
 */

pixman_private void
fbTransparentSpan (pixman_bits_t   *dst,
		   pixman_bits_t   stip,
		   pixman_bits_t   fgxor,
		   int	    n);

pixman_private void
fbEvenStipple (pixman_bits_t   *dst,
	       FbStride dstStride,
	       int	dstX,
	       int	dstBpp,

	       int	width,
	       int	height,

	       FbStip   *stip,
	       FbStride	stipStride,
	       int	stipHeight,

	       FbBits   fgand,
	       FbBits   fgxor,
	       FbBits   bgand,
	       FbBits   bgxor,

	       int	xRot,
	       int	yRot);

pixman_private void
fbOddStipple (pixman_bits_t	*dst,
	      FbStride	dstStride,
	      int	dstX,
	      int	dstBpp,

	      int	width,
	      int	height,

	      FbStip	*stip,
	      FbStride	stipStride,
	      int	stipWidth,
	      int	stipHeight,

	      FbBits	fgand,
	      FbBits	fgxor,
	      FbBits	bgand,
	      FbBits	bgxor,

	      int	xRot,
	      int	yRot);

pixman_private void
fbStipple (pixman_bits_t   *dst,
	   FbStride dstStride,
	   int	    dstX,
	   int	    dstBpp,

	   int	    width,
	   int	    height,

	   FbStip   *stip,
	   FbStride stipStride,
	   int	    stipWidth,
	   int	    stipHeight,
	   Bool	    even,

	   FbBits   fgand,
	   FbBits   fgxor,
	   FbBits   bgand,
	   FbBits   bgxor,

	   int	    xRot,
	   int	    yRot);

/* XXX: Is depth redundant here? */
struct pixman_format {
    int		format_code;
    int		depth;
    int		red, redMask;
    int		green, greenMask;
    int		blue, blueMask;
    int		alpha, alphaMask;
};

typedef struct _FbPixels {
    pixman_bits_t		*data;
    unsigned int	width;
    unsigned int	height;
    unsigned int	depth;
    unsigned int	bpp;
    unsigned int	stride;
    int			x;
    int			y;
    unsigned int	refcnt;
} FbPixels;

/* XXX: This is to avoid including colormap.h from the server includes */
typedef uint32_t Pixel;

/* icutil.c */
pixman_private pixman_bits_t
fbReplicatePixel (Pixel p, int bpp);

/* fbtrap.c */

pixman_private void
fbRasterizeTrapezoid (pixman_image_t		*pMask,
		      const pixman_trapezoid_t  *pTrap,
		      int		x_off,
		      int		y_off);

/* XXX: This is to avoid including gc.h from the server includes */
/* clientClipType field in GC */
#define CT_NONE			0
#define CT_PIXMAP		1
#define CT_REGION		2
#define CT_UNSORTED		6
#define CT_YSORTED		10
#define CT_YXSORTED		14
#define CT_YXBANDED		18

#include "icimage.h"

/* iccolor.c */

/* GCC 3.4 supports a "population count" builtin, which on many targets is
   implemented with a single instruction.  There is a fallback definition
   in libgcc in case a target does not have one, which should be just as
   good as the static function below.  */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
# if __INT_MIN__ == 0x7fffffff
#  define _FbOnes(mask)		__builtin_popcount(mask)
# else
#  define _FbOnes(mask)		__builtin_popcountl((mask) & 0xffffffff)
# endif
#else
# define ICINT_NEED_IC_ONES
int
_FbOnes(unsigned long mask);
#endif

/* icformat.c */

pixman_private void
pixman_format_init (pixman_format_t *format, int format_code);

/* icimage.c */

pixman_private pixman_image_t *
pixman_image_createForPixels (FbPixels	*pixels,
			pixman_format_t	*format);

pixman_private uint32_t
pixman_gradient_color (pixman_gradient_stop_t *stop1,
		       pixman_gradient_stop_t *stop2,
		       uint32_t		      x);

#define PictureGradientColor pixman_gradient_color

/* icpixels.c */

pixman_private FbPixels *
FbPixelsCreate (int width, int height, int depth);

pixman_private FbPixels *
FbPixelsCreateForData (pixman_bits_t *data, int width, int height, int depth, int bpp, int stride);

pixman_private void
FbPixelsDestroy (FbPixels *pixels);

/* ictransform.c */

pixman_private int
pixman_transform_point (pixman_transform_t	*transform,
		  pixman_vector_t	*vector);

/* Avoid unnessecary PLT entries.  */

slim_hidden_proto(pixman_image_create)
slim_hidden_proto(pixman_color_to_pixel)
slim_hidden_proto(pixman_format_init)
slim_hidden_proto(pixman_image_destroy)
slim_hidden_proto(pixman_fill_rectangles)
slim_hidden_proto(pixman_image_set_component_alpha)
slim_hidden_proto(pixman_image_set_repeat)
slim_hidden_proto(pixman_composite)


#include "icrop.h"

/* XXX: For now, I'm just wholesale pasting Xserver/render/picture.h here: */
#ifndef _PICTURE_H_
#define _PICTURE_H_

typedef struct _DirectFormat	*DirectFormatPtr;
typedef struct _PictFormat	*PictFormatPtr;

/*
 * While the protocol is generous in format support, the
 * sample implementation allows only packed RGB and GBR
 * representations for data to simplify software rendering,
 */
#define PICT_FORMAT(bpp,type,a,r,g,b)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((a) << 12) | \
					 ((r) << 8) | \
					 ((g) << 4) | \
					 ((b)))

/*
 * gray/color formats use a visual index instead of argb
 */
#define PICT_VISFORMAT(bpp,type,vi)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((vi)))

#define PICT_FORMAT_BPP(f)	(((f) >> 24)       )
#define PICT_FORMAT_TYPE(f)	(((f) >> 16) & 0xff)
#define PICT_FORMAT_A(f)	(((f) >> 12) & 0x0f)
#define PICT_FORMAT_R(f)	(((f) >>  8) & 0x0f)
#define PICT_FORMAT_G(f)	(((f) >>  4) & 0x0f)
#define PICT_FORMAT_B(f)	(((f)      ) & 0x0f)
#define PICT_FORMAT_RGB(f)	(((f)      ) & 0xfff)
#define PICT_FORMAT_VIS(f)	(((f)      ) & 0xffff)

#define PICT_TYPE_OTHER	0
#define PICT_TYPE_A	1
#define PICT_TYPE_ARGB	2
#define PICT_TYPE_ABGR	3
#define PICT_TYPE_COLOR	4
#define PICT_TYPE_GRAY	5

#define PICT_FORMAT_COLOR(f)	(PICT_FORMAT_TYPE(f) & 2)

/* 32bpp formats */
#define PICT_a8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,8,8,8,8)
#define PICT_x8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_a8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,8,8,8,8)
#define PICT_x8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,0,8,8,8)

/* 24bpp formats */
#define PICT_r8g8b8	PICT_FORMAT(24,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_b8g8r8	PICT_FORMAT(24,PICT_TYPE_ABGR,0,8,8,8)

/* 16bpp formats */
#define PICT_r5g6b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,6,5)
#define PICT_b5g6r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,6,5)

#define PICT_a1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,1,5,5,5)
#define PICT_x1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,5,5)
#define PICT_a1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,1,5,5,5)
#define PICT_x1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,5,5)
#define PICT_a4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_x4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,0,4,4,4)
#define PICT_a4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ABGR,4,4,4,4)
#define PICT_x4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ABGR,0,4,4,4)

/* 8bpp formats */
#define PICT_a8		PICT_FORMAT(8,PICT_TYPE_A,8,0,0,0)
#define PICT_r3g3b2	PICT_FORMAT(8,PICT_TYPE_ARGB,0,3,3,2)
#define PICT_b2g3r3	PICT_FORMAT(8,PICT_TYPE_ABGR,0,3,3,2)
#define PICT_a2r2g2b2	PICT_FORMAT(8,PICT_TYPE_ARGB,2,2,2,2)
#define PICT_a2b2g2r2	PICT_FORMAT(8,PICT_TYPE_ABGR,2,2,2,2)

#define PICT_c8		PICT_FORMAT(8,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g8		PICT_FORMAT(8,PICT_TYPE_GRAY,0,0,0,0)

/* 4bpp formats */
#define PICT_a4		PICT_FORMAT(4,PICT_TYPE_A,4,0,0,0)
#define PICT_r1g2b1	PICT_FORMAT(4,PICT_TYPE_ARGB,0,1,2,1)
#define PICT_b1g2r1	PICT_FORMAT(4,PICT_TYPE_ABGR,0,1,2,1)
#define PICT_a1r1g1b1	PICT_FORMAT(4,PICT_TYPE_ARGB,1,1,1,1)
#define PICT_a1b1g1r1	PICT_FORMAT(4,PICT_TYPE_ABGR,1,1,1,1)
				    
#define PICT_c4		PICT_FORMAT(4,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g4		PICT_FORMAT(4,PICT_TYPE_GRAY,0,0,0,0)

/* 1bpp formats */
#define PICT_a1		PICT_FORMAT(1,PICT_TYPE_A,1,0,0,0)

#define PICT_g1		PICT_FORMAT(1,PICT_TYPE_GRAY,0,0,0,0)

/*
 * For dynamic indexed visuals (GrayScale and PseudoColor), these control the 
 * selection of colors allocated for drawing to Pictures.  The default
 * policy depends on the size of the colormap:
 *
 * Size		Default Policy
 * ----------------------------
 *  < 64	PolicyMono
 *  < 256	PolicyGray
 *  256		PolicyColor (only on PseudoColor)
 *
 * The actual allocation code lives in miindex.c, and so is
 * austensibly server dependent, but that code does:
 *
 * PolicyMono	    Allocate no additional colors, use black and white
 * PolicyGray	    Allocate 13 gray levels (11 cells used)
 * PolicyColor	    Allocate a 4x4x4 cube and 13 gray levels (71 cells used)
 * PolicyAll	    Allocate as big a cube as possible, fill with gray (all)
 *
 * Here's a picture to help understand how many colors are
 * actually allocated (this is just the gray ramp):
 *
 *                 gray level
 * all   0000 1555 2aaa 4000 5555 6aaa 8000 9555 aaaa bfff d555 eaaa ffff
 * b/w   0000                                                        ffff
 * 4x4x4                     5555                aaaa
 * extra      1555 2aaa 4000      6aaa 8000 9555      bfff d555 eaaa
 *
 * The default colormap supplies two gray levels (black/white), the
 * 4x4x4 cube allocates another two and nine more are allocated to fill
 * in the 13 levels.  When the 4x4x4 cube is not allocated, a total of
 * 11 cells are allocated.
 */   

#define PictureCmapPolicyInvalid    -1
#define PictureCmapPolicyDefault    0
#define PictureCmapPolicyMono	    1
#define PictureCmapPolicyGray	    2
#define PictureCmapPolicyColor	    3
#define PictureCmapPolicyAll	    4

extern int PictureCmapPolicy pixman_private;

int	PictureParseCmapPolicy (const char *name);

/* Fixed point updates from Carl Worth, USC, Information Sciences Institute */

#ifdef WIN32
typedef __int64		xFixed_32_32;
#else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef long		xFixed_32_32;
# else
#  if defined(__GNUC__) && \
    ((__GNUC__ > 2) || \
     ((__GNUC__ == 2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ > 7)))
__extension__
#  endif
typedef long long int	xFixed_32_32;
# endif
#endif

typedef xFixed_32_32		xFixed_48_16;
typedef uint32_t		xFixed_1_31;
typedef uint32_t		xFixed_1_16;
typedef int32_t		xFixed_16_16;

/*
 * An unadorned "xFixed" is the same as xFixed_16_16, 
 * (since it's quite common in the code) 
 */
typedef	xFixed_16_16	xFixed;
#define XFIXED_BITS	16

#define xFixedToInt(f)	(int) ((f) >> XFIXED_BITS)
#define IntToxFixed(i)	((xFixed) (i) << XFIXED_BITS)
#define xFixedE		((xFixed) 1)
#define xFixed1		(IntToxFixed(1))
#define xFixed1MinusE	(xFixed1 - xFixedE)
#define xFixedFrac(f)	((f) & xFixed1MinusE)
#define xFixedFloor(f)	((f) & ~xFixed1MinusE)
#define xFixedCeil(f)	xFixedFloor((f) + xFixed1MinusE)

#define xFixedFraction(f)	((f) & xFixed1MinusE)
#define xFixedMod2(f)		((f) & (xFixed1 | xFixed1MinusE))

/* whether 't' is a well defined not obviously empty trapezoid */
#define xTrapezoidValid(t)  ((t)->left.p1.y != (t)->left.p2.y && \
			     (t)->right.p1.y != (t)->right.p2.y && \
			     (int) ((t)->bottom - (t)->top) > 0)

/*
 * Standard NTSC luminance conversions:
 *
 *  y = r * 0.299 + g * 0.587 + b * 0.114
 *
 * Approximate this for a bit more speed:
 *
 *  y = (r * 153 + g * 301 + b * 58) / 512
 *
 * This gives 17 bits of luminance; to get 15 bits, lop the low two
 */

#define CvtR8G8B8toY15(s)	(((((s) >> 16) & 0xff) * 153 + \
				  (((s) >>  8) & 0xff) * 301 + \
				  (((s)      ) & 0xff) * 58) >> 2)

#endif /* _PICTURE_H_ */


/* Macros needed by fbpict.c */

#define cvt8888to0565(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 5) & 0x07e0) | \
			     (((s) >> 8) & 0xf800))
#define cvt0565to0888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
			     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a) << 16) | *((CARD16 *) ((a)+1))) : \
		     ((*((CARD16 *) (a)) << 8) | *((a)+2)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (CARD8) ((v) >> 16)), \
		       (*((CARD16 *) ((a)+1)) = (CARD16) (v))) : \
		      ((*((CARD16 *) (a)) = (CARD16) ((v) >> 8)), \
		       (*((a)+2) = (CARD8) (v))))
#else
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a)) | (*((CARD16 *) ((a)+1)) << 8)) : \
		     ((*((CARD16 *) (a))) | (*((a)+2) << 16)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (CARD8) (v)), \
		       (*((CARD16 *) ((a)+1)) = (CARD16) ((v) >> 8))) : \
		      ((*((CARD16 *) (a)) = (CARD16) (v)),\
		       (*((a)+2) = (CARD8) ((v) >> 16))))
#endif

#endif /* _ICINT_H_ */
