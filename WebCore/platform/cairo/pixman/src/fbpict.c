/*
 * $Id: fbpict.c,v 1.5 2006/02/03 04:49:30 vladimir%pobox.com Exp $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "pixman-xserver-compat.h"

#ifdef RENDER

#include "fbpict.h"
#include "fbmmx.h"

static CARD32
fbOver (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    p = FbOverU(x,y,24,a,t);
    return m|n|o|p;
}

static CARD32
fbOver24 (CARD32 x, CARD32 y)
{
    CARD16  a = ~x >> 24;
    CARD16  t;
    CARD32  m,n,o;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    return m|n|o;
}

static CARD32
fbIn (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = FbInU(x,24,a,t);
    return m|n|o|p;
}

static CARD32
fbIn24 (CARD32 x, CARD8 y)
{
    CARD16  a = y;
    CARD16  t;
    CARD32  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = (y << 24);
    return m|n|o|p;
}

#define genericCombine24(a,b,c,d) (((a)*(c)+(b)*(d)))

/*
 * This macro does src IN mask OVER dst when src and dst are 0888.
 * If src has alpha, this will not work
 */
#define inOver0888(alpha, source, destval, dest) { \
	CARD32 dstrb=destval&0xFF00FF; CARD32 dstag=(destval>>8)&0xFF00FF; \
	CARD32 drb=((source&0xFF00FF)-dstrb)*alpha; CARD32 dag=(((source>>8)&0xFF00FF)-dstag)*alpha; \
	dest =((((drb>>8) + dstrb) & 0x00FF00FF) | ((((dag>>8) + dstag) << 8) & 0xFF00FF00)); \
	}

/*
 * This macro does src IN mask OVER dst when src and dst are 0565 and
 * mask is a 5-bit alpha value.  Again, if src has alpha, this will not
 * work.
 */

#define inOver0565(alpha, source, destval, dest) { \
	CARD16 dstrb = destval & 0xf81f; CARD16 dstg  = destval & 0x7e0; \
	CARD32 drb = ((source&0xf81f)-dstrb)*alpha; CARD32 dg=((source & 0x7e0)-dstg)*alpha; \
	dest = ((((drb>>5) + dstrb)&0xf81f) | (((dg>>5)  + dstg) & 0x7e0)); \
	}

#define inOver2x0565(alpha, source, destval, dest) { \
	CARD32 dstrb = destval & 0x07e0f81f; CARD32 dstg  = (destval & 0xf81f07e0)>>5; \
	CARD32 drb = ((source&0x07e0f81f)-dstrb)*alpha; CARD32 dg=(((source & 0xf81f07e0)>>5)-dstg)*alpha; \
	dest = ((((drb>>5) + dstrb)&0x07e0f81f) | ((((dg>>5)  + dstg)<<5) & 0xf81f07e0)); \
	}

#if IMAGE_BYTE_ORDER == LSBFirst
#	define setupPackedReader(count,temp,where,workingWhere,workingVal) count=(long)where; \
					temp=count&3; \
					where-=temp; \
					workingWhere=(CARD32 *)where; \
					workingVal=*workingWhere++; \
					count=4-temp; \
					workingVal>>=(8*temp)
#	define readPacked(where,x,y,z) {if(!(x)) { (x)=4; y=*z++; } where=(y)&0xff; (y)>>=8; (x)--;}
#	define readPackedSource(where) readPacked(where,ws,workingSource,wsrc)
#	define readPackedDest(where) readPacked(where,wd,workingiDest,widst)
#	define writePacked(what) workingoDest>>=8; workingoDest|=(what<<24); ww--; if(!ww) { ww=4; *wodst++=workingoDest; }
#else
#	define setupPackedReader(count,temp,where,workingWhere,workingVal) count=(long)where; \
					temp=count&3; \
					where-=temp; \
					workingWhere=(CARD32 *)where; \
					workingVal=*workingWhere++; \
					count=4-temp; \
					workingVal<<=(8*temp)
#	define readPacked(where,x,y,z) {if(!(x)) { (x)=4; y=*z++; } where=(y)>>24; (y)<<=8; (x)--;}
#	define readPackedSource(where) readPacked(where,ws,workingSource,wsrc)
#	define readPackedDest(where) readPacked(where,wd,workingiDest,widst)
#	define writePacked(what) workingoDest<<=8; workingoDest|=what; ww--; if(!ww) { ww=4; *wodst++=workingoDest; }
#endif
/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

static void
fbCompositeSolidMask_nx8x8888 (pixman_operator_t   op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;

    fbComposeGetSolid(pSrc, pDst, src);

    dstMask = FbFullMask (pDst->pDrawable->depth);
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (m)
	    {
		d = fbIn (src, m);
		*dst = fbOver (d, *dst) & dstMask;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSolidMask_nx8888x8888C (pixman_operator_t   op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD32	*dstLine, *dst, d, dstMask;
    CARD32	*maskLine, *mask, ma;
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o, p;

    fbComposeGetSolid(pSrc, pDst, src);

    dstMask = FbFullMask (pDst->pDrawable->depth);
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = fbOver (src, *dst) & dstMask;
	    }
	    else if (ma)
	    {
		d = *dst;
#define FbInOverC(src,srca,msk,dst,i,result) { \
    CARD16  __a = FbGet8(msk,i); \
    CARD32  __t, __ta; \
    CARD32  __i; \
    __t = FbIntMult (FbGet8(src,i), __a,__i); \
    __ta = (CARD8) ~FbIntMult (srca, __a,__i); \
    __t = __t + FbIntMult(FbGet8(dst,i),__ta,__i); \
    __t = (CARD32) (CARD8) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		FbInOverC (src, srca, ma, d, 24, p);
		*dst = m|n|o|p;
	    }
	    dst++;
	}
    }
}

#define srcAlphaCombine24(a,b) genericCombine24(a,b,srca,srcia)
static void
fbCompositeSolidMask_nx8x0888 (pixman_operator_t   op,
			       PicturePtr pSrc,
			       PicturePtr pMask,
			       PicturePtr pDst,
			       INT16      xSrc,
			       INT16      ySrc,
			       INT16      xMask,
			       INT16      yMask,
			       INT16      xDst,
			       INT16      yDst,
			       CARD16     width,
			       CARD16     height)
{
    CARD32	src, srca, srcia;
    CARD8	*dstLine, *dst, *edst;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w;
	CARD32 rs,gs,bs,rd,gd,bd;

    fbComposeGetSolid(pSrc, pDst, src);

    srca = src >> 24;
    srcia = 255-srca;
    if (src == 0)
	return;

	rs=src&0xff;
	gs=(src>>8)&0xff;
	bs=(src>>16)&0xff;

    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
	{
		/* fixme: cleanup unused */
		unsigned long wt,wd;
		CARD32 workingiDest;
		CARD32 *widst;

		edst=dst = dstLine;
		dstLine += dstStride;
		mask = maskLine;
		maskLine += maskStride;
		w = width;

#ifndef NO_MASKED_PACKED_READ
		setupPackedReader(wd,wt,edst,widst,workingiDest);
#endif

		while (w--)
		{
#ifndef NO_MASKED_PACKED_READ
			readPackedDest(rd);
			readPackedDest(gd);
			readPackedDest(bd);
#else
			rd= *edst++;
			gd= *edst++;
			bd= *edst++;
#endif
			m = *mask++;
			if (m == 0xff)
			{
				if (srca == 0xff)
				{
					*dst++=rs;
					*dst++=gs;
					*dst++=bs;
				}
				else
				{
					*dst++=(srcAlphaCombine24(rs, rd)>>8);
					*dst++=(srcAlphaCombine24(gs, gd)>>8);
					*dst++=(srcAlphaCombine24(bs, bd)>>8);
				}
			}
			else if (m)
			{
				int na=(srca*(int)m)>>8;
				int nia=255-na;
				*dst++=(genericCombine24(rs, rd, na, nia)>>8);
				*dst++=(genericCombine24(gs, gd, na, nia)>>8);
				*dst++=(genericCombine24(bs, bd, na, nia)>>8);
			}
			else
			{
				dst+=3;
			}
		}
	}
}

static void
fbCompositeSolidMask_nx8x0565 (pixman_operator_t      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca8, srca5;
    CARD16	*dstLine, *dst;
    CARD16	d;
    CARD32	t;
    CARD8	*maskLine, *mask, m;
    FbStride	dstStride, maskStride;
    CARD16	w,src16;

    fbComposeGetSolid(pSrc, pDst, src);

    if (src == 0)
	return;

    srca8 = (src >> 24);
    srca5 = (srca8 >> 3);

    src16 = cvt8888to0565(src);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD8, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0)
		dst++;
	    else if (srca5 == (0xff >> 3))
	    {
		if (m == 0xff)
		    *dst++ = src16;
		else
		{
		    d = *dst;
		    m >>= 3;
		    inOver0565 (m, src16, d, *dst++);
		}
	    }
	    else
	    {
		d = *dst;
		if (m == 0xff)
		{
		    t = fbOver24 (src, cvt0565to0888 (d));
		}
		else
		{
		    t = fbIn (src, m);
		    t = fbOver (t, cvt0565to0888 (d));
		}
		*dst++ = cvt8888to0565 (t);
	    }
	}
    }
}

static void
fbCompositeSolidMask_nx8888x0565 (pixman_operator_t      op,
				  PicturePtr pSrc,
				  PicturePtr pMask,
				  PicturePtr pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height)
{
    CARD32	src, srca8, srca5;
    CARD16	*dstLine, *dst;
    CARD16	d;
    CARD32	*maskLine, *mask;
    CARD32	t;
    CARD8	m;
    FbStride	dstStride, maskStride;
    CARD16	w, src16;

    fbComposeGetSolid(pSrc, pDst, src);

    if (src == 0)
	return;

    srca8 = src >> 24;
    srca5 = srca8 >> 3;
    src16 = cvt8888to0565(src);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++ >> 24;
	    if (m == 0)
		dst++;
	    else if (srca5 == (0xff >> 3))
	    {
		if (m == 0xff)
		    *dst++ = src16;
		else
		{
		    d = *dst;
		    m >>= 3;
		    inOver0565 (m, src16, d, *dst++);
		}
	    }
	    else
	    {
		if (m == 0xff)
		{
		    d = *dst;
		    t = fbOver24 (src, cvt0565to0888 (d));
		    *dst++ = cvt8888to0565 (t);
		}
		else
		{
		    d = *dst;
		    t = fbIn (src, m);
		    t = fbOver (t, cvt0565to0888 (d));
		    *dst++ = cvt8888to0565 (t);
		}
	    }
	}
    }
}

static void
fbCompositeSolidMask_nx8888x0565C (pixman_operator_t      op,
				   PicturePtr pSrc,
				   PicturePtr pMask,
				   PicturePtr pDst,
				   INT16      xSrc,
				   INT16      ySrc,
				   INT16      xMask,
				   INT16      yMask,
				   INT16      xDst,
				   INT16      yDst,
				   CARD16     width,
				   CARD16     height)
{
    CARD32	src, srca;
    CARD16	src16;
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*maskLine, *mask, ma;
    FbStride	dstStride, maskStride;
    CARD16	w;
    CARD32	m, n, o;

    fbComposeGetSolid(pSrc, pDst, src);

    srca = src >> 24;
    if (src == 0)
	return;

    src16 = cvt8888to0565(src);

    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, CARD32, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = fbOver24 (src, cvt0565to0888(d));
		    *dst = cvt8888to0565(d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = cvt0565to0888(d);
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrc_8888x8888 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD32	*dstLine, *dst, dstMask;
    CARD32	*srcLine, *src, s;
    FbStride	dstStride, srcStride;
    CARD8	a;
    CARD16	w;

    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);

    dstMask = FbFullMask (pDst->pDrawable->depth);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s & dstMask;
	    else if (a)
		*dst = fbOver (s, *dst) & dstMask;
	    dst++;
	}
    }
}

static void
fbCompositeSrc_8888x0888 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    FbStride	dstStride, srcStride;
    CARD16	w;

    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);
    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = fbOver24 (s, Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

static void
fbCompositeSrc_8888x0565 (pixman_operator_t  op,
			 PicturePtr pSrc,
			 PicturePtr pMask,
			 PicturePtr pDst,
			 INT16      xSrc,
			 INT16      ySrc,
			 INT16      xMask,
			 INT16      yMask,
			 INT16      xDst,
			 INT16      yDst,
			 CARD16     width,
			 CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD32	d;
    CARD32	*srcLine, *src, s;
    CARD8	a;
    FbStride	dstStride, srcStride;
    CARD16	w;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		{
		    d = *dst;
		    d = fbOver24 (s, cvt0565to0888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}



static void
fbCompositeSrcAdd_8000x8000 (pixman_operator_t	  op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD8	*dstLine, *dst;
    CARD8	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD8	s, d;
    CARD16	t;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = *dst;
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrcAdd_8888x8888 (pixman_operator_t   op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD32	*dstLine, *dst;
    CARD32	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    CARD32	s, d;
    CARD16	t;
    CARD32	m,n,o,p;

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD32, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD32, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = *dst;
		    if (d)
		    {
			m = FbAdd(s,d,0,t);
			n = FbAdd(s,d,8,t);
			o = FbAdd(s,d,16,t);
			p = FbAdd(s,d,24,t);
			s = m|n|o|p;
		    }
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrcAdd_1000x1000 (pixman_operator_t   op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    FbBits	*dstBits, *srcBits;
    FbStride	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;

    fbGetDrawable(pSrc->pDrawable, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    fbGetDrawable(pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    fbBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   FB_ALLONES,
	   srcBpp,

	   FALSE,
	   FALSE);
}

static void
fbCompositeSolidMask_nx1xn (pixman_operator_t   op,
			    PicturePtr pSrc,
			    PicturePtr pMask,
			    PicturePtr pDst,
			    INT16      xSrc,
			    INT16      ySrc,
			    INT16      xMask,
			    INT16      yMask,
			    INT16      xDst,
			    INT16      yDst,
			    CARD16     width,
			    CARD16     height)
{
    FbBits	*dstBits;
    FbStip	*maskBits;
    FbStride	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    FbBits	src;

    fbComposeGetSolid(pSrc, pDst, src);

    if ((src & 0xff000000) != 0xff000000)
    {
	pixman_compositeGeneral  (op, pSrc, pMask, pDst,
			     xSrc, ySrc, xMask, yMask, xDst, yDst,
			     width, height);
	return;
    }
    FbGetStipPixels (pMask->pixels, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    fbGetDrawable (pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = fbReplicatePixel (src, dstBpp);

    fbBltOne (maskBits + maskStride * (yMask + maskYoff),
	      maskStride,
	      xMask + maskXoff,

	      dstBits + dstStride * (yDst + dstYoff),
	      dstStride,
	      (xDst + dstXoff) * dstBpp,
	      dstBpp,

	      width * dstBpp,
	      height,

	      0x0,
	      src,
	      FB_ALLONES,
	      0x0);
}

/* prototype to help with merging */
static void
fbCompositeSrcSrc_nxn  (pixman_operator_t	op,
			PicturePtr pSrc,
			PicturePtr pMask,
			PicturePtr pDst,
			INT16      xSrc,
			INT16      ySrc,
			INT16      xMask,
			INT16      yMask,
			INT16      xDst,
			INT16      yDst,
			CARD16     width,
			CARD16     height);
/*
 * Apply a constant alpha value in an over computation
 */
static void
fbCompositeTrans_0565xnx0565(pixman_operator_t      op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD16	*dstLine, *dst;
    CARD16	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    FbBits	mask;
    CARD8	maskAlpha;
    CARD16	s_16, d_16;
    CARD32	s_32, d_32;

    fbComposeGetSolid (pMask, pDst, mask);
    maskAlpha = mask >> 27;

    if (!maskAlpha)
	return;
    if (maskAlpha == 0xff)
    {
	fbCompositeSrcSrc_nxn (PIXMAN_OPERATOR_SRC, pSrc, pMask, pDst,
			       xSrc, ySrc, xMask, yMask, xDst, yDst,
			       width, height);
	return;
    }

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD16, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, CARD16, dstStride, dstLine, 1);

    while (height--)
	{
		CARD32 *isrc, *idst;
		dst = dstLine;
		dstLine += dstStride;
		src = srcLine;
		srcLine += srcStride;
		w = width;

		if(((long)src&1)==1)
		{
			s_16 = *src++;
			d_16 = *dst;
			inOver0565(maskAlpha, s_16, d_16, *dst++);
			w--;
		}
		isrc=(CARD32 *)src;
		if(((long)dst&1)==0)
		{
			idst=(CARD32 *)dst;
			while (w>1)
			{
				s_32 = *isrc++;
				d_32 = *idst;
				inOver2x0565(maskAlpha,s_32,d_32,*idst++);
				w-=2;
			}
			dst=(CARD16 *)idst;
		}
		else
		{
		    while (w > 1)
		    {
			s_32 = *isrc++;
#if IMAGE_BYTE_ORDER == LSBFirst
				s_16=s_32&0xffff;
#else
				s_16=s_32>>16;
#endif
				d_16 = *dst;
				inOver0565(maskAlpha, s_16, d_16, *dst++);
#if IMAGE_BYTE_ORDER == LSBFirst
				s_16=s_32>>16;
#else
				s_16=s_32&0xffff;
#endif
			d_16 = *dst;
			inOver0565(maskAlpha, s_16, d_16, *dst++);
			w-=2;
		    }
		}
		src=(CARD16 *)isrc;
		if(w!=0)
		{
			s_16 = *src;
			d_16 = *dst;
			inOver0565(maskAlpha, s_16, d_16, *dst);
		}
	}
}



/* macros for "i can't believe it's not fast" packed pixel handling */
#define alphamaskCombine24(a,b) genericCombine24(a,b,maskAlpha,maskiAlpha)
static void
fbCompositeTrans_0888xnx0888(pixman_operator_t      op,
			     PicturePtr pSrc,
			     PicturePtr pMask,
			     PicturePtr pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height)
{
    CARD8	*dstLine, *dst,*idst;
    CARD8	*srcLine, *src;
    FbStride	dstStride, srcStride;
    CARD16	w;
    FbBits	mask;
    CARD16	maskAlpha,maskiAlpha;

    fbComposeGetSolid (pMask, pDst, mask);
    maskAlpha = mask >> 24;
	maskiAlpha= 255-maskAlpha;

    if (!maskAlpha)
	return;
    /*
    if (maskAlpha == 0xff)
    {
	fbCompositeSrc_0888x0888 (op, pSrc, pMask, pDst,
				  xSrc, ySrc, xMask, yMask, xDst, yDst,
				  width, height);
	return;
    }
    */

    fbComposeGetStart (pSrc, xSrc, ySrc, CARD8, srcStride, srcLine, 3);
    fbComposeGetStart (pDst, xDst, yDst, CARD8, dstStride, dstLine, 3);

	{
		unsigned long ws,wt;
		CARD32 workingSource;
		CARD32 *wsrc, *wdst, *widst;
		CARD32 rs, rd, nd;
		CARD8 *isrc;

		/* are xSrc and xDst at the same alignment?  if not, we need to be complicated :)*/
		/* if(0==0) */
		if( (((xSrc*3)&3)!=((xDst*3)&3)) || ((srcStride&3)!=(dstStride&3)))
		{
			while (height--)
			{
				dst = dstLine;
				dstLine += dstStride;
				isrc = src = srcLine;
				srcLine += srcStride;
				w = width*3;

				setupPackedReader(ws,wt,isrc,wsrc,workingSource);

				/* get to word aligned */
				switch(!(long)dst&3)
				{
					case 1:
						readPackedSource(rs);
						/* *dst++=alphamaskCombine24(rs, *dst)>>8; */
						rd=*dst;  /* make gcc happy.  hope it doens't cost us too much performance*/
						*dst++=alphamaskCombine24(rs, rd)>>8;
						w--; if(w==0) break;
					case 2:
						readPackedSource(rs);
						rd=*dst;
						*dst++=alphamaskCombine24(rs, rd)>>8;
						w--; if(w==0) break;
					case 3:
						readPackedSource(rs);
						rd=*dst;
						*dst++=alphamaskCombine24(rs, rd)>>8;
						w--; if(w==0) break;
				}
				wdst=(CARD32 *)dst;
				while (w>3)
				{
					/* FIXME: write a special readPackedWord macro, which knows how to
					 * halfword combine
					 */

#if IMAGE_BYTE_ORDER == LSBFirst
					rd=*wdst;
					readPackedSource(nd);
					readPackedSource(rs);
					nd|=rs<<8;
					readPackedSource(rs);
					nd|=rs<<16;
					readPackedSource(rs);
					nd|=rs<<24;
#else
					readPackedSource(nd);
					nd<<=24;
					readPackedSource(rs);
					nd|=rs<<16;
					readPackedSource(rs);
					nd|=rs<<8;
					readPackedSource(rs);
					nd|=rs;
#endif
					inOver0888(maskAlpha, nd, rd, *wdst++)
					w-=4;
				}
				dst=(CARD8 *)wdst;
				switch(w)
				{
					case 3:
						readPackedSource(rs);
						rd=*dst;
						*dst++=alphamaskCombine24(rs, rd)>>8;
					case 2:
						readPackedSource(rs);
						rd=*dst;
						*dst++=alphamaskCombine24(rs, rd)>>8;
					case 1:
						readPackedSource(rs);
						rd=*dst;
						*dst++=alphamaskCombine24(rs, rd)>>8;
				}
			}
		}
		else
		{
			while (height--)
			{
				idst=dst = dstLine;
				dstLine += dstStride;
				src = srcLine;
				srcLine += srcStride;
				w = width*3;
				/* get to word aligned */
				switch(!(long)src&3)
				{
					case 1:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
					case 2:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
					case 3:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
						w--; if(w==0) break;
				}
				wsrc=(CARD32 *)src;
				widst=(CARD32 *)dst;

				while(w>3)
				{
					rs = *wsrc++;
					rd = *widst;
					inOver0888(maskAlpha, rs, rd, *widst++);
					w-=4;
				}
				src=(CARD8 *)wsrc;
				dst=(CARD8 *)widst;
				switch(w)
				{
					case 3:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
					case 2:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
					case 1:
						rd=alphamaskCombine24(*src++, *dst)>>8;
						*dst++=rd;
				}
			}
		}
	}
}

/*
 * Simple bitblt
 */

static void
fbCompositeSrcSrc_nxn  (pixman_operator_t	op,
			PicturePtr pSrc,
			PicturePtr pMask,
			PicturePtr pDst,
			INT16      xSrc,
			INT16      ySrc,
			INT16      xMask,
			INT16      yMask,
			INT16      xDst,
			INT16      yDst,
			CARD16     width,
			CARD16     height)
{
    FbBits	*dst;
    FbBits	*src;
    FbStride	dstStride, srcStride;
    int		srcXoff, srcYoff;
    int		dstXoff, dstYoff;
    int		srcBpp;
    int		dstBpp;
    Bool	reverse = FALSE;
    Bool	upsidedown = FALSE;

    fbGetDrawable(pSrc->pDrawable,src,srcStride,srcBpp,srcXoff,srcYoff);
    fbGetDrawable(pDst->pDrawable,dst,dstStride,dstBpp,dstXoff,dstYoff);

    fbBlt (src + (ySrc + srcYoff) * srcStride,
	   srcStride,
	   (xSrc + srcXoff) * srcBpp,

	   dst + (yDst + dstYoff) * dstStride,
	   dstStride,
	   (xDst + dstXoff) * dstBpp,

	   (width) * dstBpp,
	   (height),

	   GXcopy,
	   FB_ALLONES,
	   dstBpp,

	   reverse,
	   upsidedown);
}

/*
 * Solid fill
void
fbCompositeSolidSrc_nxn  (CARD8	op,
			  PicturePtr pSrc,
			  PicturePtr pMask,
			  PicturePtr pDst,
			  INT16      xSrc,
			  INT16      ySrc,
			  INT16      xMask,
			  INT16      yMask,
			  INT16      xDst,
			  INT16      yDst,
			  CARD16     width,
			  CARD16     height)
{

}
 */

# define mod(a,b)	((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-a) % (b))

void
pixman_composite (pixman_operator_t	op,
	     PicturePtr pSrc,
	     PicturePtr pMask,
	     PicturePtr pDst,
	     int	xSrc,
	     int	ySrc,
	     int	xMask,
	     int	yMask,
	     int	xDst,
	     int	yDst,
	     int	width,
	     int	height)
{
    pixman_region16_t	    *region;
    int		    n;
    pixman_box16_t    *pbox;
    CompositeFunc   func = NULL;
    Bool	    srcRepeat = pSrc->pDrawable && pSrc->repeat == RepeatNormal;
    Bool	    maskRepeat = FALSE;
    Bool	    srcTransform = pSrc->transform != 0;
    Bool	    maskTransform = FALSE;
    Bool	    srcAlphaMap = pSrc->alphaMap != 0;
    Bool	    maskAlphaMap = FALSE;
    Bool	    dstAlphaMap = pDst->alphaMap != 0;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;

#ifdef USE_MMX
    static Bool mmx_setup = FALSE;
    if (!mmx_setup) {
        fbComposeSetupMMX();
        mmx_setup = TRUE;
    }
#endif
        
    xDst += pDst->pDrawable->x;
    yDst += pDst->pDrawable->y;
    if (pSrc->pDrawable) {
        xSrc += pSrc->pDrawable->x;
        ySrc += pSrc->pDrawable->y;
    }

    if (srcRepeat && srcTransform &&
	pSrc->pDrawable->width == 1 &&
	pSrc->pDrawable->height == 1)
	srcTransform = FALSE;

    if (pMask && pMask->pDrawable)
    {
	xMask += pMask->pDrawable->x;
	yMask += pMask->pDrawable->y;
	maskRepeat = pMask->repeat == RepeatNormal;
	maskTransform = pMask->transform != 0;
#ifdef PIXMAN_CONVOLUTION
	if (pMask->filter == PictFilterConvolution)
	    maskTransform = TRUE;
#endif

	maskAlphaMap = pMask->alphaMap != 0;

	if (maskRepeat && maskTransform &&
	    pMask->pDrawable->width == 1 &&
	    pMask->pDrawable->height == 1)
	    maskTransform = FALSE;
    }

    if (pSrc->pDrawable && (!pMask || pMask->pDrawable)
        && !srcTransform && !maskTransform
        && !maskAlphaMap && !srcAlphaMap && !dstAlphaMap
#ifdef PIXMAN_CONVOLUTION
        && (pSrc->filter != PictFilterConvolution)
        && (!pMask || pMask->filter != PictFilterConvolution)
#endif
        )
    switch (op) {
    case PIXMAN_OPERATOR_OVER:
	if (pMask)
	{
	    if (srcRepeat &&
		pSrc->pDrawable->width == 1 &&
		pSrc->pDrawable->height == 1)
	    {
		if (PICT_FORMAT_COLOR(pSrc->format_code)) {
		    switch (pMask->format_code) {
		    case PICT_a8:
			switch (pDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
#ifdef USE_MMX
			    if (fbHaveMMX())
				func = fbCompositeSolidMask_nx8x0565mmx;
			    else
#endif
				func = fbCompositeSolidMask_nx8x0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    func = fbCompositeSolidMask_nx8x0888;
			    break;
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
#ifdef USE_MMX
			    if (fbHaveMMX())
				func = fbCompositeSolidMask_nx8x8888mmx;
			    else
#endif
				func = fbCompositeSolidMask_nx8x8888;
			    break;
			}
			break;
		    case PICT_a8r8g8b8:
			if (pMask->componentAlpha) {
			    switch (pDst->format_code) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSolidMask_nx8888x8888Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_r5g6b5:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSolidMask_nx8888x0565Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			else
			{
			    switch (pDst->format_code) {
			    case PICT_r5g6b5:
				func = fbCompositeSolidMask_nx8888x0565;
				break;
			    }
			}
			break;
		    case PICT_a8b8g8r8:
			if (pMask->componentAlpha) {
			    switch (pDst->format_code) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSolidMask_nx8888x8888Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PICT_b5g6r5:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSolidMask_nx8888x0565Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			else
			{
			    switch (pDst->format_code) {
			    case PICT_b5g6r5:
				func = fbCompositeSolidMask_nx8888x0565;
				break;
			    }
			}
			break;
		    case PICT_a1:
			switch (pDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = fbCompositeSolidMask_nx1xn;
			    break;
			}
		    }
		}
		if (func != pixman_compositeGeneral)
		    srcRepeat = FALSE;
	    }
	    else /* has mask and non-repeating source */
	    {
		if (pSrc->pDrawable == pMask->pDrawable &&
		    xSrc == xMask && ySrc == yMask &&
		    !pMask->componentAlpha)
		{
		    /* source == mask: non-premultiplied data */
		    switch (pSrc->format_code) {
		    case PICT_x8b8g8r8:
			switch (pMask->format_code) {
			case PICT_a8r8g8b8:
			case PICT_a8b8g8r8:
			    switch (pDst->format_code) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSrc_8888RevNPx8888mmx;
#endif
				break;
			    case PICT_r5g6b5:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSrc_8888RevNPx0565mmx;
#endif
				break;
			    }
			    break;
			}
			break;
		    case PICT_x8r8g8b8:
			switch (pMask->format_code) {
			case PICT_a8r8g8b8:
			case PICT_a8b8g8r8:
			    switch (pDst->format_code) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSrc_8888RevNPx8888mmx;
#endif
				break;
			    case PICT_r5g6b5:
#ifdef USE_MMX
				if (fbHaveMMX())
				    func = fbCompositeSrc_8888RevNPx0565mmx;
#endif
				break;
			    }
			    break;
			}
			break;
		    }
		    break;
		}
		else
		{
		    /* non-repeating source, repeating mask => translucent window */
		    if (maskRepeat &&
			pMask->pDrawable->width == 1 &&
			pMask->pDrawable->height == 1)
		    {
			switch (pSrc->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			    if (pDst->format_code == pSrc->format_code)
				func = fbCompositeTrans_0565xnx0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    if (pDst->format_code == pSrc->format_code)
				func = fbCompositeTrans_0888xnx0888;
			    break;
#ifdef USE_MMX
			case PICT_x8r8g8b8:
			case PICT_x8b8g8r8:
			    if (pDst->format_code == pSrc->format_code &&
				pMask->format_code == PICT_a8 && fbHaveMMX())
				func = fbCompositeSrc_x888x8x8888mmx;
			    break;
#if 0 /* This case fails rendercheck for me */
			case PICT_a8r8g8b8:
			    if ((pDst->format == PICT_a8r8g8b8 ||
				 pDst->format == PICT_x8r8g8b8) &&
				pMask->format == PICT_a8 && fbHaveMMX())
				func = fbCompositeSrc_8888x8x8888mmx;
			    break;
#endif
			case PICT_a8b8g8r8:
			    if ((pDst->format_code == PICT_a8b8g8r8 ||
				 pDst->format_code == PICT_x8b8g8r8) &&
				pMask->format_code == PICT_a8 && fbHaveMMX())
				func = fbCompositeSrc_8888x8x8888mmx;
			    break;
#endif
			}

                        if (func != pixman_compositeGeneral)
			    maskRepeat = FALSE;
		    }
		}
	    }
	}
	else /* no mask */
	{
	    if (srcRepeat &&
		pSrc->pDrawable->width == 1 &&
		pSrc->pDrawable->height == 1)
	    {
		/* no mask and repeating source */
		switch (pSrc->format_code) {
		case PICT_a8r8g8b8:
		    switch (pDst->format_code) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
#ifdef USE_MMX
			if (fbHaveMMX())
			{
			    srcRepeat = FALSE;
			    func = fbCompositeSolid_nx8888mmx;
			}
#endif
			break;
		    case PICT_r5g6b5:
#ifdef USE_MMX
			if (fbHaveMMX())
			{
			    srcRepeat = FALSE;
			    func = fbCompositeSolid_nx0565mmx;
			}
#endif
			break;
		    }
		    break;
		}
	    }
	    else
	    {
		/*
		 * Formats without alpha bits are just Copy with Over
		 */
		if (pSrc->format_code == pDst->format_code && !PICT_FORMAT_A(pSrc->format_code))
		{
#ifdef USE_MMX
			if (fbHaveMMX() &&
			    (pSrc->format_code == PICT_x8r8g8b8 || pSrc->format_code == PICT_x8b8g8r8))
			    func = fbCompositeCopyAreammx;
			else
#endif
			    func = fbCompositeSrcSrc_nxn;
		}
		else switch (pSrc->format_code) {
		case PICT_a8r8g8b8:
		    switch (pDst->format_code) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
#ifdef USE_MMX
			if (fbHaveMMX())
			    func = fbCompositeSrc_8888x8888mmx;
			else
#endif
			    func = fbCompositeSrc_8888x8888;
			break;
		    case PICT_r8g8b8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PICT_r5g6b5:
			func = fbCompositeSrc_8888x0565;
			break;
		    }
		    break;
		case PICT_a8b8g8r8:
		    switch (pDst->format_code) {
		    case PICT_a8b8g8r8:
		    case PICT_x8b8g8r8:
#ifdef USE_MMX
			if (fbHaveMMX())
			    func = fbCompositeSrc_8888x8888mmx;
			else
#endif
			    func = fbCompositeSrc_8888x8888;
			break;
		    case PICT_b8g8r8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PICT_b5g6r5:
			func = fbCompositeSrc_8888x0565;
			break;
		    }
		    break;
		}
	    }
	}
	break;
    case PIXMAN_OPERATOR_ADD:
	if (pMask == 0)
	{
	    switch (pSrc->format_code) {
	    case PICT_a8r8g8b8:
		switch (pDst->format_code) {
		case PICT_a8r8g8b8:
#ifdef USE_MMX
		    if (fbHaveMMX())
			func = fbCompositeSrcAdd_8888x8888mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (pDst->format_code) {
		case PICT_a8b8g8r8:
#ifdef USE_MMX
		    if (fbHaveMMX())
			func = fbCompositeSrcAdd_8888x8888mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8:
		switch (pDst->format_code) {
		case PICT_a8:
#ifdef USE_MMX
		    if (fbHaveMMX())
			func = fbCompositeSrcAdd_8000x8000mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8000x8000;
		    break;
		}
		break;
	    case PICT_a1:
		switch (pDst->format_code) {
		case PICT_a1:
		    func = fbCompositeSrcAdd_1000x1000;
		    break;
		}
		break;
	    }
	}
	break;
    case PIXMAN_OPERATOR_SRC:
	if (pMask)
	  {
#ifdef USE_MMX
	    if (srcRepeat &&
		pSrc->pDrawable->width == 1 &&
		pSrc->pDrawable->height == 1)
	    {
		if (pMask->format_code == PICT_a8)
		{
		    switch (pDst->format_code) {
		    case PICT_a8r8g8b8:
		    case PICT_x8r8g8b8:
		    case PICT_a8b8g8r8:
		    case PICT_x8b8g8r8:
			if (fbHaveMMX())
			    func = fbCompositeSolidMaskSrc_nx8x8888mmx;
			break;
		    }
		}
	    }
#endif
	  }
	else
	{
	    if (pSrc->format_code == pDst->format_code)
	    {
#ifdef USE_MMX
		if (pSrc->pDrawable != pDst->pDrawable &&
                    (PICT_FORMAT_BPP (pSrc->format_code) == 16 ||
                     PICT_FORMAT_BPP (pSrc->format_code) == 32))
		    func = fbCompositeCopyAreammx;
		else
#endif
		    func = fbCompositeSrcSrc_nxn;
	    }
	}
	break;
    }

    if (!func) {
        /* no fast path, use the general code */
        pixman_compositeGeneral(op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask, xDst, yDst, width, height);
        return;
    }

    /* if we are transforming, we handle repeats in IcFetch[a]_transform */
    if (srcTransform)
	srcRepeat = 0;
    if (maskTransform)
	maskRepeat = 0;

    region = pixman_region_create();
    pixman_region_union_rect (region, region, xDst, yDst, width, height);
    
    if (!FbComputeCompositeRegion (region,
				   pSrc,
				   pMask,
				   pDst,
				   xSrc,
				   ySrc,
				   xMask,
				   yMask,
				   xDst,
				   yDst,
				   width,
				   height))
	return;
    
    n = pixman_region_num_rects (region);
    pbox = pixman_region_rects (region);
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - yDst + ySrc;
	y_msk = pbox->y1 - yDst + yMask;
	y_dst = pbox->y1;
	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - xDst + xSrc;
	    x_msk = pbox->x1 - xDst + xMask;
	    x_dst = pbox->x1;
	    if (maskRepeat)
	    {
		y_msk = mod (y_msk, pMask->pDrawable->height);
		if (h_this > pMask->pDrawable->height - y_msk)
		    h_this = pMask->pDrawable->height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = mod (y_src, pSrc->pDrawable->height);
		if (h_this > pSrc->pDrawable->height - y_src)
		    h_this = pSrc->pDrawable->height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = mod (x_msk, pMask->pDrawable->width);
		    if (w_this > pMask->pDrawable->width - x_msk)
			w_this = pMask->pDrawable->width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = mod (x_src, pSrc->pDrawable->width);
		    if (w_this > pSrc->pDrawable->width - x_src)
			w_this = pSrc->pDrawable->width - x_src;
		}
		(*func) (op, pSrc, pMask, pDst,
			 x_src, y_src, x_msk, y_msk, x_dst, y_dst,
			 w_this, h_this);
		w -= w_this;
		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }
	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}
	pbox++;
    }
    pixman_region_destroy (region);
}
slim_hidden_def(pixman_composite);

/* The CPU detection code needs to be in a file not compiled with
 * "-mmmx -msse", as gcc would generate CMOV instructions otherwise
 * that would lead to SIGILL instructions on old CPUs that don't have
 * it.
 */
#if defined(USE_MMX) && !defined(__amd64__) && !defined(__x86_64__)

enum CPUFeatures {
    NoFeatures = 0,
    MMX = 0x1,
    MMX_Extensions = 0x2, 
    SSE = 0x6,
    SSE2 = 0x8,
    CMOV = 0x10
};

static unsigned int detectCPUFeatures(void) {
    unsigned int result, features;
    char vendor[13];
    vendor[0] = 0;
    vendor[12] = 0;
    /* see p. 118 of amd64 instruction set manual Vol3 */
    /* We need to be careful about the handling of %ebx and
     * %esp here. We can't declare either one as clobbered
     * since they are special registers (%ebx is the "PIC
     * register" holding an offset to global data, %esp the
     * stack pointer), so we need to make sure they have their
     * original values when we access the output operands.
     */
    __asm__ ("pushf\n"
             "pop %%eax\n"
             "mov %%eax, %%ecx\n"
             "xor $0x00200000, %%eax\n"
             "push %%eax\n"
             "popf\n"
             "pushf\n"
             "pop %%eax\n"
             "mov $0x0, %%edx\n"
             "xor %%ecx, %%eax\n"
             "jz 1f\n"

             "mov $0x00000000, %%eax\n"
	     "push %%ebx\n"
             "cpuid\n"
             "mov %%ebx, %%eax\n"
	     "pop %%ebx\n"
             "mov %%eax, %1\n"
             "mov %%edx, %2\n"
             "mov %%ecx, %3\n"
             "mov $0x00000001, %%eax\n"
	     "push %%ebx\n"
             "cpuid\n"
	     "pop %%ebx\n"
             "1:\n"
             "mov %%edx, %0\n"
             : "=r" (result), 
               "=m" (vendor[0]), 
               "=m" (vendor[4]), 
               "=m" (vendor[8])
             :
             : "%eax", "%ecx", "%edx"
        );

    features = 0;
    if (result) {
        /* result now contains the standard feature bits */
        if (result & (1 << 15))
            features |= CMOV;
        if (result & (1 << 23))
            features |= MMX;
        if (result & (1 << 25))
            features |= SSE;
        if (result & (1 << 26))
            features |= SSE2;
        if ((result & MMX) && !(result & SSE) && (strcmp(vendor, "AuthenticAMD") == 0)) {
            /* check for AMD MMX extensions */

            unsigned int result;            
            __asm__("push %%ebx\n"
                    "mov $0x80000000, %%eax\n"
                    "cpuid\n"
                    "xor %%edx, %%edx\n"
                    "cmp $0x1, %%eax\n"
                    "jge 1f\n"
                    "mov $0x80000001, %%eax\n"
                    "cpuid\n"
                    "1:\n"
                    "pop %%ebx\n"
                    "mov %%edx, %0\n"
                    : "=r" (result)
                    :
                    : "%eax", "%ecx", "%edx"
                );
            if (result & (1<<22))
                features |= MMX_Extensions;
        }
    }
    return features;
}

Bool
fbHaveMMX (void)
{
    static Bool initialized = FALSE;
    static Bool mmx_present;

    if (!initialized)
    {
        unsigned int features = detectCPUFeatures();
	mmx_present = (features & (MMX|MMX_Extensions)) == (MMX|MMX_Extensions);
        initialized = TRUE;
    }
    
    return mmx_present;
}
#endif /* USE_MMX && !amd64 */
#endif /* RENDER */
