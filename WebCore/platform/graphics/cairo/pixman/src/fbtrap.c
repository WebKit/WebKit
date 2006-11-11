/*
 * $Id: fbtrap.c,v 1.11 2006/03/17 22:24:10 vladimir%pobox.com Exp $
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

#include "pixman-xserver-compat.h"

#ifdef RENDER

/* XXX: Haven't added addTraps to libpixman yet. */
#if 0
void
fbAddTraps (PicturePtr	pPicture,
	    INT16	x_off,
	    INT16	y_off,
	    int		ntrap,
	    xTrap	*traps)
{
    FbBits	*buf;
    int		bpp;
    int		width;
    int		stride;
    int		height;
    int		pxoff, pyoff;

    xFixed	x_off_fixed;
    xFixed	y_off_fixed;
    RenderEdge  l, r;
    xFixed	t, b;
    
    fbGetDrawable (pPicture->pDrawable, buf, stride, bpp, pxoff, pyoff);

    width = pPicture->pDrawable->width;
    height = pPicture->pDrawable->height;
    x_off += pxoff;
    y_off += pyoff;
    
    x_off_fixed = IntToxFixed(y_off);
    y_off_fixed = IntToxFixed(y_off);

    while (ntrap--)
    {
	t = traps->top.y + y_off_fixed;
	if (t < 0)
	    t = 0;
	t = RenderSampleCeilY (t, bpp);
    
	b = traps->bot.y + y_off_fixed;
	if (xFixedToInt (b) >= height)
	    b = IntToxFixed (height) - 1;
	b = RenderSampleFloorY (b, bpp);
	
	if (b >= t)
	{
	    /* initialize edge walkers */
	    RenderEdgeInit (&l, bpp, t,
			    traps->top.l + x_off_fixed,
			    traps->top.y + y_off_fixed,
			    traps->bot.l + x_off_fixed,
			    traps->bot.y + y_off_fixed);
	
	    RenderEdgeInit (&r, bpp, t,
			    traps->top.r + x_off_fixed,
			    traps->top.y + y_off_fixed,
			    traps->bot.r + x_off_fixed,
			    traps->bot.y + y_off_fixed);
	    
	    fbRasterizeEdges (buf, bpp, width, stride, &l, &r, t, b);
	}
	traps++;
    }
}
#endif

void
fbRasterizeTrapezoid (PicturePtr    pPicture,
		      const xTrapezoid  *trap,
		      int	    x_off,
		      int	    y_off)
{
    FbBits	*buf;
    int		bpp;
    int		width;
    int		stride;
    int		height;
    int		pxoff, pyoff;

    xFixed	x_off_fixed;
    xFixed	y_off_fixed;
    RenderEdge	l, r;
    xFixed	t, b;
    
    fbGetDrawable (pPicture->pDrawable, buf, stride, bpp, pxoff, pyoff);

    width = pPicture->pDrawable->width;
    height = pPicture->pDrawable->height;
    x_off += pxoff;
    y_off += pyoff;
    
    x_off_fixed = IntToxFixed(x_off);
    y_off_fixed = IntToxFixed(y_off);
    t = trap->top + y_off_fixed;
    if (t < 0)
	t = 0;
    t = RenderSampleCeilY (t, bpp);

    b = trap->bottom + y_off_fixed;
    if (xFixedToInt (b) >= height)
	b = IntToxFixed (height) - 1;
    b = RenderSampleFloorY (b, bpp);
    
    if (b >= t)
    {
	/* initialize edge walkers */
	RenderLineFixedEdgeInit (&l, bpp, t, &trap->left, x_off, y_off);
	RenderLineFixedEdgeInit (&r, bpp, t, &trap->right, x_off, y_off);
	
	fbRasterizeEdges (buf, bpp, width, stride, &l, &r, t, b);
    }
}

/* XXX: Haven't add addTriangles to libpixman yet. */
#if 0
static int
_GreaterY (xPointFixed *a, xPointFixed *b)
{
    if (a->y == b->y)
	return a->x > b->x;
    return a->y > b->y;
}

/*
 * Note that the definition of this function is a bit odd because
 * of the X coordinate space (y increasing downwards).
 */
static int
_Clockwise (xPointFixed *ref, xPointFixed *a, xPointFixed *b)
{
    xPointFixed	ad, bd;

    ad.x = a->x - ref->x;
    ad.y = a->y - ref->y;
    bd.x = b->x - ref->x;
    bd.y = b->y - ref->y;

    return ((xFixed_32_32) bd.y * ad.x - (xFixed_32_32) ad.y * bd.x) < 0;
}

/* FIXME -- this could be made more efficient */
void
fbAddTriangles (PicturePtr  pPicture,
		INT16	    x_off,
		INT16	    y_off,
		int	    ntri,
		xTriangle   *tris)
{
    xPointFixed	    *top, *left, *right, *tmp;
    xTrapezoid	    trap;

    for (; ntri; ntri--, tris++)
    {
	top = &tris->p1;
	left = &tris->p2;
	right = &tris->p3;
	if (_GreaterY (top, left)) {
	    tmp = left; left = top; top = tmp;
	}
	if (_GreaterY (top, right)) {
	    tmp = right; right = top; top = tmp;
	}
	if (_Clockwise (top, right, left)) {
	    tmp = right; right = left; left = tmp;
	}
	
	/*
	 * Two cases:
	 *
	 *          +               +
	 *         / \             / \
	 *        /   \           /   \
	 *       /     +         +     \
	 *      /    --           --    \
	 *     /   --               --   \
	 *    / ---                   --- \
	 *   +--                         --+
	 */
	
	trap.top = top->y;
	trap.left.p1 = *top;
	trap.left.p2 = *left;
	trap.right.p1 = *top;
	trap.right.p2 = *right;
	if (right->y < left->y)
	    trap.bottom = right->y;
	else
	    trap.bottom = left->y;
	fbRasterizeTrapezoid (pPicture, &trap, x_off, y_off);
	if (right->y < left->y)
	{
	    trap.top = right->y;
	    trap.bottom = left->y;
	    trap.right.p1 = *right;
	    trap.right.p2 = *left;
	}
	else
	{
	    trap.top = left->y;
	    trap.bottom = right->y;
	    trap.left.p1 = *left;
	    trap.left.p2 = *right;
	}
	fbRasterizeTrapezoid (pPicture, &trap, x_off, y_off);
    }
}
#endif

#endif /* RENDER */
