/*
 * $Id: fbedgeimp.h,v 1.8 2006/02/03 04:49:30 vladimir%pobox.com Exp $
 *
 * Copyright © 2004 Keith Packard
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

#ifndef rasterizeSpan
#endif

static void
rasterizeEdges (FbBits		*buf,
		int		width,
		int		stride,
		RenderEdge	*l,
		RenderEdge	*r,
		xFixed		t,
		xFixed		b)
{
    xFixed  y = t;
    FbBits  *line;
    
    line = buf + xFixedToInt (y) * stride;
    
    for (;;)
    {
	xFixed	lx, rx;
	int	lxi, rxi;
	
	/* clip X */
	lx = l->x;
	if (lx < 0)
	    lx = 0;
	rx = r->x;
	if (xFixedToInt (rx) >= width)
	    rx = IntToxFixed (width);
	
	/* Skip empty (or backwards) sections */
	if (rx > lx)
	{

	    /* Find pixel bounds for span. */
	    lxi = xFixedToInt (lx);
	    rxi = xFixedToInt (rx);

#if N_BITS == 1
	    {
		FbBits  *a = line;
		FbBits  startmask, endmask;
		int	    nmiddle;
		int	    width = rxi - lxi;
		int	    x = lxi;

		a += x >> FB_SHIFT;
		x &= FB_MASK;

		FbMaskBits (x, width, startmask, nmiddle, endmask);
		if (startmask)
		    *a++ |= startmask;
		while (nmiddle--)
		    *a++ = FB_ALLONES;
		if (endmask)
		    *a |= endmask;
	    }
#else
	    {
		DefineAlpha(line,lxi);
		int	    lxs, rxs;

		/* Sample coverage for edge pixels */
		lxs = RenderSamplesX (lx, N_BITS);
		rxs = RenderSamplesX (rx, N_BITS);

		/* Add coverage across row */
		if (lxi == rxi)
		{
		    AddAlpha (rxs - lxs);
		}
		else
		{
		    int	xi;

		    AddAlpha (N_X_FRAC(N_BITS) - lxs);
		    StepAlpha;
		    for (xi = lxi + 1; xi < rxi; xi++)
		    {
			AddAlpha (N_X_FRAC(N_BITS));
			StepAlpha;
		    }
		    /* Do not add in a 0 alpha here. This check is
		     * necessary to avoid a buffer overrun, (when rx
		     * is exactly on a pixel boundary). */
		    if (rxs)
			AddAlpha (rxs);
		}
	    }
#endif
	}

	if (y == b)
	    break;

#if N_BITS > 1
	if (xFixedFrac (y) != Y_FRAC_LAST(N_BITS))
	{
	    RenderEdgeStepSmall (l);
	    RenderEdgeStepSmall (r);
	    y += STEP_Y_SMALL(N_BITS);
	}
	else
#endif
	{
	    RenderEdgeStepBig (l);
	    RenderEdgeStepBig (r);
	    y += STEP_Y_BIG(N_BITS);
	    line += stride;
	}
    }
}

#undef rasterizeSpan
