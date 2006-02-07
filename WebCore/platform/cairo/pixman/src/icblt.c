/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "pixman-xserver-compat.h"

#define InitializeShifts(sx,dx,ls,rs) { \
    if (sx != dx) { \
	if (sx > dx) { \
	    ls = sx - dx; \
	    rs = FB_UNIT - ls; \
	} else { \
	    rs = dx - sx; \
	    ls = FB_UNIT - rs; \
	} \
    } \
}

void
fbBlt (FbBits   *srcLine,
       FbStride	srcStride,
       int	srcX,
       
       FbBits   *dstLine,
       FbStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       FbBits	pm,
       int	bpp,
       
       Bool	reverse,
       Bool	upsidedown)
{
    FbBits  *src, *dst;
    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    FbBits  bits, bits1;
    int	    n, nmiddle;
    Bool    destInvarient;
    int	    startbyte, endbyte;
    FbDeclareMergeRop ();
 
    /* are we just copying multiples of 8 bits?  if so, run, forrest, run!
       the memcpy()'s should be pluggable ala mplayer|xine - perhaps we can get
       one of the above to give up their code for us.
     */
    if((pm==FB_ALLONES) && (alu==GXcopy) && !reverse && (srcX&7)==0 && (dstX&7)==0 && (width&7)==0)
    {
		CARD8 *isrc=(CARD8 *)srcLine;
		CARD8 *idst=(CARD8 *)dstLine;
		int sstride=srcStride*sizeof(FbBits);
		int dstride=dstStride*sizeof(FbBits);
		int j;
		width>>=3;
		isrc+=(srcX>>3);
		idst+=(dstX>>3);
		if(!upsidedown)
			for(j=0;j<height;j++)
				memcpy(idst+j*dstride, isrc+j*sstride, width);
		else
			for(j=(height-1);j>=0;j--)
				memcpy(idst+j*dstride, isrc+j*sstride, width);
	
		return;
    }
    
#ifdef FB_24BIT
    if (bpp == 24 && !FbCheck24Pix (pm))
    {
	fbBlt24 (srcLine, srcStride, srcX, dstLine, dstStride, dstX,
		 width, height, alu, pm, reverse, upsidedown);
	return;
    }
#endif
    FbInitializeMergeRop(alu, pm);
    destInvarient = FbDestInvarientMergeRop();
    if (upsidedown)
    {
	srcLine += (height - 1) * (srcStride);
	dstLine += (height - 1) * (dstStride);
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    FbMaskBitsBytes (dstX, width, destInvarient, startmask, startbyte,
		     nmiddle, endmask, endbyte);
    if (reverse)
    {
	srcLine += ((srcX + width - 1) >> FB_SHIFT) + 1;
	dstLine += ((dstX + width - 1) >> FB_SHIFT) + 1;
	srcX = (srcX + width - 1) & FB_MASK;
	dstX = (dstX + width - 1) & FB_MASK;
    }
    else
    {
	srcLine += srcX >> FB_SHIFT;
	dstLine += dstX >> FB_SHIFT;
	srcX &= FB_MASK;
	dstX &= FB_MASK;
    }
    if (srcX == dstX)
    {
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    if (reverse)
	    {
		if (endmask)
		{
		    bits = *--src;
		    --dst;
		    FbDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
			*--dst = FbDoDestInvarientMergeRop(*--src);
		}
		else
		{
		    while (n--)
		    {
			bits = *--src;
			--dst;
			*dst = FbDoMergeRop (bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = *--src;
		    --dst;
		    FbDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (startmask)
		{
		    bits = *src++;
		    FbDoLeftMaskByteMergeRop(dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
#if 0
		    /*
		     * This provides some speedup on screen->screen blts
		     * over the PCI bus, usually about 10%.  But fb
		     * isn't usually used for this operation...
		     */
		    if (_ca2 + 1 == 0 && _cx2 == 0)
		    {
			FbBits	t1, t2, t3, t4;
			while (n >= 4)
			{
			    t1 = *src++;
			    t2 = *src++;
			    t3 = *src++;
			    t4 = *src++;
			    *dst++ = t1;
			    *dst++ = t2;
			    *dst++ = t3;
			    *dst++ = t4;
			    n -= 4;
			}
		    }
#endif
		    while (n--)
			*dst++ = FbDoDestInvarientMergeRop(*src++);
		}
		else
		{
		    while (n--)
		    {
			bits = *src++;
			*dst = FbDoMergeRop (bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = *src;
		    FbDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = FB_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = FB_UNIT - rightShift;
	}
	while (height--)
	{
	    src = srcLine;
	    srcLine += srcStride;
	    dst = dstLine;
	    dstLine += dstStride;
	    
	    bits1 = 0;
	    if (reverse)
	    {
		if (srcX < dstX)
		    bits1 = *--src;
		if (endmask)
		{
		    bits = FbScrRight(bits1, rightShift); 
		    if (FbScrRight(endmask, leftShift))
		    {
			bits1 = *--src;
			bits |= FbScrLeft(bits1, leftShift);
		    }
		    --dst;
		    FbDoRightMaskByteMergeRop(dst, bits, endbyte, endmask);
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = FbScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= FbScrLeft(bits1, leftShift);
			--dst;
			*dst = FbDoDestInvarientMergeRop(bits);
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = FbScrRight(bits1, rightShift); 
			bits1 = *--src;
			bits |= FbScrLeft(bits1, leftShift);
			--dst;
			*dst = FbDoMergeRop(bits, *dst);
		    }
		}
		if (startmask)
		{
		    bits = FbScrRight(bits1, rightShift); 
		    if (FbScrRight(startmask, leftShift))
		    {
			bits1 = *--src;
			bits |= FbScrLeft(bits1, leftShift);
		    }
		    --dst;
		    FbDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		}
	    }
	    else
	    {
		if (srcX > dstX)
		    bits1 = *src++;
		if (startmask)
		{
		    bits = FbScrLeft(bits1, leftShift); 
		    bits1 = *src++;
		    bits |= FbScrRight(bits1, rightShift);
		    FbDoLeftMaskByteMergeRop (dst, bits, startbyte, startmask);
		    dst++;
		}
		n = nmiddle;
		if (destInvarient)
		{
		    while (n--)
		    {
			bits = FbScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= FbScrRight(bits1, rightShift);
			*dst = FbDoDestInvarientMergeRop(bits);
			dst++;
		    }
		}
		else
		{
		    while (n--)
		    {
			bits = FbScrLeft(bits1, leftShift); 
			bits1 = *src++;
			bits |= FbScrRight(bits1, rightShift);
			*dst = FbDoMergeRop(bits, *dst);
			dst++;
		    }
		}
		if (endmask)
		{
		    bits = FbScrLeft(bits1, leftShift); 
		    if (FbScrLeft(endmask, rightShift))
		    {
			bits1 = *src;
			bits |= FbScrRight(bits1, rightShift);
		    }
		    FbDoRightMaskByteMergeRop (dst, bits, endbyte, endmask);
		}
	    }
	}
    }
}

#ifdef FB_24BIT

#undef DEBUG_BLT24
#ifdef DEBUG_BLT24

static unsigned long
getPixel (char *src, int x)
{
    unsigned long   l;

    l = 0;
    memcpy (&l, src + x * 3, 3);
    return l;
}
#endif

static void
fbBlt24Line (FbBits	    *src,
	     int	    srcX,

	     FbBits	    *dst,
	     int	    dstX,

	     int	    width,

	     int	    alu,
	     FbBits	    pm,
	 
	     Bool	    reverse)
{
#ifdef DEBUG_BLT24
    char    *origDst = (char *) dst;
    FbBits  *origLine = dst + ((dstX >> FB_SHIFT) - 1);
    int	    origNlw = ((width + FB_MASK) >> FB_SHIFT) + 3;
    int	    origX = dstX / 24;
#endif
    
    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    int	    n;
    
    FbBits  bits, bits1;
    FbBits  mask;

    int	    rot;
    FbDeclareMergeRop ();
    
    FbInitializeMergeRop (alu, FB_ALLONES);
    FbMaskBits(dstX, width, startmask, n, endmask);
#ifdef DEBUG_BLT24
    ErrorF ("dstX %d width %d reverse %d\n", dstX, width, reverse);
#endif
    if (reverse)
    {
	src += ((srcX + width - 1) >> FB_SHIFT) + 1;
	dst += ((dstX + width - 1) >> FB_SHIFT) + 1;
	rot = FbFirst24Rot (((dstX + width - 8) & FB_MASK));
	rot = FbPrev24Rot(rot);
#ifdef DEBUG_BLT24
	ErrorF ("dstX + width - 8: %d rot: %d\n", (dstX + width - 8) & FB_MASK, rot);
#endif
	srcX = (srcX + width - 1) & FB_MASK;
	dstX = (dstX + width - 1) & FB_MASK;
    }
    else
    {
	src += srcX >> FB_SHIFT;
	dst += dstX >> FB_SHIFT;
	srcX &= FB_MASK;
	dstX &= FB_MASK;
	rot = FbFirst24Rot (dstX);
#ifdef DEBUG_BLT24
	ErrorF ("dstX: %d rot: %d\n", dstX, rot);
#endif
    }
    mask = FbRot24(pm,rot);
#ifdef DEBUG_BLT24
    ErrorF ("pm 0x%x mask 0x%x\n", pm, mask);
#endif
    if (srcX == dstX)
    {
	if (reverse)
	{
	    if (endmask)
	    {
		bits = *--src;
		--dst;
		*dst = FbDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = FbPrev24Pix (mask);
	    }
	    while (n--)
	    {
		bits = *--src;
		--dst;
		*dst = FbDoMaskMergeRop (bits, *dst, mask);
		mask = FbPrev24Pix (mask);
	    }
	    if (startmask)
	    {
		bits = *--src;
		--dst;
		*dst = FbDoMaskMergeRop(bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = FbDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = FbNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = *src++;
		*dst = FbDoMaskMergeRop (bits, *dst, mask);
		dst++;
		mask = FbNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = FbDoMaskMergeRop(bits, *dst, mask & endmask);
	    }
	}
    }
    else
    {
	if (srcX > dstX)
	{
	    leftShift = srcX - dstX;
	    rightShift = FB_UNIT - leftShift;
	}
	else
	{
	    rightShift = dstX - srcX;
	    leftShift = FB_UNIT - rightShift;
	}
	
	bits1 = 0;
	if (reverse)
	{
	    if (srcX < dstX)
		bits1 = *--src;
	    if (endmask)
	    {
		bits = FbScrRight(bits1, rightShift); 
		if (FbScrRight(endmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= FbScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = FbDoMaskMergeRop (bits, *dst, mask & endmask);
		mask = FbPrev24Pix(mask);
	    }
	    while (n--)
	    {
		bits = FbScrRight(bits1, rightShift); 
		bits1 = *--src;
		bits |= FbScrLeft(bits1, leftShift);
		--dst;
		*dst = FbDoMaskMergeRop(bits, *dst, mask);
		mask = FbPrev24Pix(mask);
	    }
	    if (startmask)
	    {
		bits = FbScrRight(bits1, rightShift); 
		if (FbScrRight(startmask, leftShift))
		{
		    bits1 = *--src;
		    bits |= FbScrLeft(bits1, leftShift);
		}
		--dst;
		*dst = FbDoMaskMergeRop (bits, *dst, mask & startmask);
	    }
	}
	else
	{
	    if (srcX > dstX)
		bits1 = *src++;
	    if (startmask)
	    {
		bits = FbScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= FbScrRight(bits1, rightShift);
		*dst = FbDoMaskMergeRop (bits, *dst, mask & startmask);
		dst++;
		mask = FbNext24Pix(mask);
	    }
	    while (n--)
	    {
		bits = FbScrLeft(bits1, leftShift); 
		bits1 = *src++;
		bits |= FbScrRight(bits1, rightShift);
		*dst = FbDoMaskMergeRop(bits, *dst, mask);
		dst++;
		mask = FbNext24Pix(mask);
	    }
	    if (endmask)
	    {
		bits = FbScrLeft(bits1, leftShift); 
		if (FbScrLeft(endmask, rightShift))
		{
		    bits1 = *src;
		    bits |= FbScrRight(bits1, rightShift);
		}
		*dst = FbDoMaskMergeRop (bits, *dst, mask & endmask);
	    }
	}
    }
#ifdef DEBUG_BLT24
    {
	int firstx, lastx, x;

	firstx = origX;
	if (firstx)
	    firstx--;
	lastx = origX + width/24 + 1;
	for (x = firstx; x <= lastx; x++)
	    ErrorF ("%06x ", getPixel (origDst, x));
	ErrorF ("\n");
	while (origNlw--)
	    ErrorF ("%08x ", *origLine++);
	ErrorF ("\n");
    }
#endif
}

void
fbBlt24 (FbBits	    *srcLine,
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
	 Bool	    upsidedown)
{
    if (upsidedown)
    {
	srcLine += (height-1) * srcStride;
	dstLine += (height-1) * dstStride;
	srcStride = -srcStride;
	dstStride = -dstStride;
    }
    while (height--)
    {
	fbBlt24Line (srcLine, srcX, dstLine, dstX, width, alu, pm, reverse);
	srcLine += srcStride;
	dstLine += dstStride;
    }
#ifdef DEBUG_BLT24
    ErrorF ("\n");
#endif
}
#endif /* FB_24BIT */

#if FB_SHIFT == FB_STIP_SHIFT + 1

/*
 * Could be generalized to FB_SHIFT > FB_STIP_SHIFT + 1 by
 * creating an ring of values stepped through for each line
 */

void
fbBltOdd (FbBits    *srcLine,
	  FbStride  srcStrideEven,
	  FbStride  srcStrideOdd,
	  int	    srcXEven,
	  int	    srcXOdd,

	  FbBits    *dstLine,
	  FbStride  dstStrideEven,
	  FbStride  dstStrideOdd,
	  int	    dstXEven,
	  int	    dstXOdd,

	  int	    width,
	  int	    height,

	  int	    alu,
	  FbBits    pm,
	  int	    bpp)
{
    FbBits  *src;
    int	    leftShiftEven, rightShiftEven;
    FbBits  startmaskEven, endmaskEven;
    int	    nmiddleEven;
    
    FbBits  *dst;
    int	    leftShiftOdd, rightShiftOdd;
    FbBits  startmaskOdd, endmaskOdd;
    int	    nmiddleOdd;

    int	    leftShift, rightShift;
    FbBits  startmask, endmask;
    int	    nmiddle;
    
    int	    srcX, dstX;
    
    FbBits  bits, bits1;
    int	    n;
    
    Bool    destInvarient;
    Bool    even;
    FbDeclareMergeRop ();

    FbInitializeMergeRop (alu, pm);
    destInvarient = FbDestInvarientMergeRop();

    srcLine += srcXEven >> FB_SHIFT;
    dstLine += dstXEven >> FB_SHIFT;
    srcXEven &= FB_MASK;
    dstXEven &= FB_MASK;
    srcXOdd &= FB_MASK;
    dstXOdd &= FB_MASK;

    FbMaskBits(dstXEven, width, startmaskEven, nmiddleEven, endmaskEven);
    FbMaskBits(dstXOdd, width, startmaskOdd, nmiddleOdd, endmaskOdd);
    
    even = TRUE;
    InitializeShifts(srcXEven, dstXEven, leftShiftEven, rightShiftEven);
    InitializeShifts(srcXOdd, dstXOdd, leftShiftOdd, rightShiftOdd);
    while (height--)
    {
	src = srcLine;
	dst = dstLine;
	if (even)
	{
	    srcX = srcXEven;
	    dstX = dstXEven;
	    startmask = startmaskEven;
	    endmask = endmaskEven;
	    nmiddle = nmiddleEven;
	    leftShift = leftShiftEven;
	    rightShift = rightShiftEven;
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = FALSE;
	}
	else
	{
	    srcX = srcXOdd;
	    dstX = dstXOdd;
	    startmask = startmaskOdd;
	    endmask = endmaskOdd;
	    nmiddle = nmiddleOdd;
	    leftShift = leftShiftOdd;
	    rightShift = rightShiftOdd;
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = TRUE;
	}
	if (srcX == dstX)
	{
	    if (startmask)
	    {
		bits = *src++;
		*dst = FbDoMaskMergeRop (bits, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = FbDoDestInvarientMergeRop(bits);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits = *src++;
		    *dst = FbDoMergeRop (bits, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits = *src;
		*dst = FbDoMaskMergeRop(bits, *dst, endmask);
	    }
	}
	else
	{
	    bits = 0;
	    if (srcX > dstX)
		bits = *src++;
	    if (startmask)
	    {
		bits1 = FbScrLeft(bits, leftShift);
		bits = *src++;
		bits1 |= FbScrRight(bits, rightShift);
		*dst = FbDoMaskMergeRop (bits1, *dst, startmask);
		dst++;
	    }
	    n = nmiddle;
	    if (destInvarient)
	    {
		while (n--)
		{
		    bits1 = FbScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= FbScrRight(bits, rightShift);
		    *dst = FbDoDestInvarientMergeRop(bits1);
		    dst++;
		}
	    }
	    else
	    {
		while (n--)
		{
		    bits1 = FbScrLeft(bits, leftShift);
		    bits = *src++;
		    bits1 |= FbScrRight(bits, rightShift);
		    *dst = FbDoMergeRop(bits1, *dst);
		    dst++;
		}
	    }
	    if (endmask)
	    {
		bits1 = FbScrLeft(bits, leftShift);
		if (FbScrLeft(endmask, rightShift))
		{
		    bits = *src;
		    bits1 |= FbScrRight(bits, rightShift);
		}
		*dst = FbDoMaskMergeRop (bits1, *dst, endmask);
	    }
	}
    }
}

#ifdef FB_24BIT
void
fbBltOdd24 (FbBits	*srcLine,
	    FbStride	srcStrideEven,
	    FbStride	srcStrideOdd,
	    int		srcXEven,
	    int		srcXOdd,

	    FbBits	*dstLine,
	    FbStride	dstStrideEven,
	    FbStride	dstStrideOdd,
	    int		dstXEven,
	    int		dstXOdd,

	    int		width,
	    int		height,

	    int		alu,
	    FbBits	pm)
{
    Bool    even = TRUE;
    
    while (height--)
    {
	if (even)
	{
	    fbBlt24Line (srcLine, srcXEven, dstLine, dstXEven,
			 width, alu, pm, FALSE);
	    srcLine += srcStrideEven;
	    dstLine += dstStrideEven;
	    even = FALSE;
	}
	else
	{
	    fbBlt24Line (srcLine, srcXOdd, dstLine, dstXOdd,
			 width, alu, pm, FALSE);
	    srcLine += srcStrideOdd;
	    dstLine += dstStrideOdd;
	    even = TRUE;
	}
    }
#if 0
    fprintf (stderr, "\n");
#endif
}
#endif

#endif

#if FB_STIP_SHIFT != FB_SHIFT
void
fbSetBltOdd (FbStip	*stip,
	     FbStride	stipStride,
	     int	srcX,
	     FbBits	**bits,
	     FbStride	*strideEven,
	     FbStride	*strideOdd,
	     int	*srcXEven,
	     int	*srcXOdd)
{
    int	    srcAdjust;
    int	    strideAdjust;

    /*
     * bytes needed to align source
     */
    srcAdjust = (((int) stip) & (FB_MASK >> 3));
    /*
     * FbStip units needed to align stride
     */
    strideAdjust = stipStride & (FB_MASK >> FB_STIP_SHIFT);

    *bits = (FbBits *) ((char *) stip - srcAdjust);
    if (srcAdjust)
    {
	*strideEven = FbStipStrideToBitsStride (stipStride + 1);
	*strideOdd = FbStipStrideToBitsStride (stipStride);

	*srcXEven = srcX + (srcAdjust << 3);
	*srcXOdd = srcX + (srcAdjust << 3) - (strideAdjust << FB_STIP_SHIFT);
    }
    else
    {
	*strideEven = FbStipStrideToBitsStride (stipStride);
	*strideOdd = FbStipStrideToBitsStride (stipStride + 1);
	
	*srcXEven = srcX;
	*srcXOdd = srcX + (strideAdjust << FB_STIP_SHIFT);
    }
}
#endif

void
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
	   int	    bpp)
{
#if FB_STIP_SHIFT != FB_SHIFT
    if (FB_STIP_ODDSTRIDE(srcStride) || FB_STIP_ODDPTR(src) ||
	FB_STIP_ODDSTRIDE(dstStride) || FB_STIP_ODDPTR(dst))
    {
	FbStride    srcStrideEven, srcStrideOdd;
	FbStride    dstStrideEven, dstStrideOdd;
	int	    srcXEven, srcXOdd;
	int	    dstXEven, dstXOdd;
	FbBits	    *s, *d;
	int	    sx, dx;
	
	src += srcX >> FB_STIP_SHIFT;
	srcX &= FB_STIP_MASK;
	dst += dstX >> FB_STIP_SHIFT;
	dstX &= FB_STIP_MASK;
	
	fbSetBltOdd (src, srcStride, srcX,
		     &s,
		     &srcStrideEven, &srcStrideOdd,
		     &srcXEven, &srcXOdd);
		     
	fbSetBltOdd (dst, dstStride, dstX,
		     &d,
		     &dstStrideEven, &dstStrideOdd,
		     &dstXEven, &dstXOdd);
		     
#ifdef FB_24BIT
	if (bpp == 24 && !FbCheck24Pix (pm))
	{
	    fbBltOdd24  (s, srcStrideEven, srcStrideOdd,
			 srcXEven, srcXOdd,

			 d, dstStrideEven, dstStrideOdd,
			 dstXEven, dstXOdd,

			 width, height, alu, pm);
	}
	else
#endif
	{
	    fbBltOdd (s, srcStrideEven, srcStrideOdd,
		      srcXEven, srcXOdd,
    
		      d, dstStrideEven, dstStrideOdd,
		      dstXEven, dstXOdd,
    
		      width, height, alu, pm, bpp);
	}
    }
    else
#endif
    {
	fbBlt ((FbBits *) src, FbStipStrideToBitsStride (srcStride), 
	       srcX, 
	       (FbBits *) dst, FbStipStrideToBitsStride (dstStride), 
	       dstX, 
	       width, height,
	       alu, pm, bpp, FALSE, FALSE);
    }
}
