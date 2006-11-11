/***********************************************************

Copyright 1987, 1988, 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 

Copyright 1987, 1988, 1989 by 
Digital Equipment Corporation, Maynard, Massachusetts. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "pixregionint.h"
#include "slim_internal.h"

#if defined (__GNUC__) && !defined (NO_INLINES)
#define INLINE	__inline
#else
#define INLINE
#endif

#undef assert
#ifdef DEBUG_PIXREGION
#define assert(expr) {if (!(expr)) \
		FatalError("Assertion failed file %s, line %d: expr\n", \
			__FILE__, __LINE__); }
#else
#define assert(expr)
#endif

#define good(reg) assert(pixman_region16_valid(reg))

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static pixman_box16_t pixman_region_emptyBox = {0, 0, 0, 0};
static pixman_region16_data_t pixman_region_emptyData = {0, 0};

static pixman_region16_data_t  pixman_brokendata = {0, 0};
static pixman_region16_t   pixman_brokenregion = { { 0, 0, 0, 0 }, &pixman_brokendata };

static pixman_region_status_t
pixman_break (pixman_region16_t *pReg);

static void
pixman_init (pixman_region16_t *region, pixman_box16_t *rect);

static void
pixman_uninit (pixman_region16_t *region);

slim_hidden_proto(pixman_region_create_simple)
slim_hidden_proto(pixman_region_copy)
slim_hidden_proto(pixman_region_union)

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in 
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other. 
 *
 *  -----------				    -----------
 *  |         |				    |         |		    band 0
 *  |         |  --------		    -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is	    |         |  |      |
 *  -----------  |      |		    -----------  --------
 *               |      |				 |      |   band 2
 *               --------				 --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines, and
 * added pixman_region_validate in order to support several speed improvements to
 * pixman_region_validateTree.  Bob Scheifler changed the representation to be more
 * compact when empty or a single rectangle, and did a bunch of gratuitous
 * reformatting. Carl Worth did further gratuitous reformatting while re-merging
 * the server and client region code into libpixregion. 
 */

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1,r2) \
      (!( ((r1)->x2 <= (r2)->x1)  || \
          ((r1)->x1 >= (r2)->x2)  || \
          ((r1)->y2 <= (r2)->y1)  || \
          ((r1)->y1 >= (r2)->y2) ) )

/* true iff (x,y) is in Box */
#define INBOX(r,x,y) \
      ( ((r)->x2 >  x) && \
        ((r)->x1 <= x) && \
        ((r)->y2 >  y) && \
        ((r)->y1 <= y) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1,r2) \
      ( ((r1)->x1 <= (r2)->x1) && \
        ((r1)->x2 >= (r2)->x2) && \
        ((r1)->y1 <= (r2)->y1) && \
        ((r1)->y2 >= (r2)->y2) )

#define allocData(n) malloc(PIXREGION_SZOF(n))
#define freeData(reg) if ((reg)->data && (reg)->data->size) free((reg)->data)

#define RECTALLOC_BAIL(pReg,n,bail) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!pixman_rect_alloc(pReg, n)) { goto bail; }

#define RECTALLOC(pReg,n) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!pixman_rect_alloc(pReg, n)) { return PIXMAN_REGION_STATUS_FAILURE; }

#define ADDRECT(pNextRect,nx1,ny1,nx2,ny2)	\
{						\
    pNextRect->x1 = nx1;			\
    pNextRect->y1 = ny1;			\
    pNextRect->x2 = nx2;			\
    pNextRect->y2 = ny2;			\
    pNextRect++;				\
}

#define NEWRECT(pReg,pNextRect,nx1,ny1,nx2,ny2)			\
{									\
    if (!(pReg)->data || ((pReg)->data->numRects == (pReg)->data->size))\
    {									\
	if (!pixman_rect_alloc(pReg, 1))					\
	    return PIXMAN_REGION_STATUS_FAILURE;						\
	pNextRect = PIXREGION_TOP(pReg);					\
    }									\
    ADDRECT(pNextRect,nx1,ny1,nx2,ny2);					\
    pReg->data->numRects++;						\
    assert(pReg->data->numRects<=pReg->data->size);			\
}


#define DOWNSIZE(reg,numRects)						 \
if (((numRects) < ((reg)->data->size >> 1)) && ((reg)->data->size > 50)) \
{									 \
    pixman_region16_data_t * NewData;							 \
    NewData = (pixman_region16_data_t *)realloc((reg)->data, PIXREGION_SZOF(numRects));	 \
    if (NewData)							 \
    {									 \
	NewData->size = (numRects);					 \
	(reg)->data = NewData;						 \
    }									 \
}


#ifdef DEBUG_PIXREGION
int
pixman_region16_print(rgn)
    pixman_region16_t * rgn;
{
    int num, size;
    int i;
    pixman_box16_t * rects;

    num = PIXREGION_NUM_RECTS(rgn);
    size = PIXREGION_SIZE(rgn);
    rects = PIXREGION_RECTS(rgn);
    ErrorF("num: %d size: %d\n", num, size);
    ErrorF("extents: %d %d %d %d\n",
	   rgn->extents.x1, rgn->extents.y1, rgn->extents.x2, rgn->extents.y2);
    for (i = 0; i < num; i++)
      ErrorF("%d %d %d %d \n",
	     rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
    ErrorF("\n");
    return(num);
}


pixman_region_status_t
pixman_region16_tsEqual(reg1, reg2)
    pixman_region16_t * reg1;
    pixman_region16_t * reg2;
{
    int i;
    pixman_box16_t * rects1, rects2;

    if (reg1->extents.x1 != reg2->extents.x1) return PIXMAN_REGION_STATUS_FAILURE;
    if (reg1->extents.x2 != reg2->extents.x2) return PIXMAN_REGION_STATUS_FAILURE;
    if (reg1->extents.y1 != reg2->extents.y1) return PIXMAN_REGION_STATUS_FAILURE;
    if (reg1->extents.y2 != reg2->extents.y2) return PIXMAN_REGION_STATUS_FAILURE;
    if (PIXREGION_NUM_RECTS(reg1) != PIXREGION_NUM_RECTS(reg2)) return PIXMAN_REGION_STATUS_FAILURE;
    
    rects1 = PIXREGION_RECTS(reg1);
    rects2 = PIXREGION_RECTS(reg2);
    for (i = 0; i != PIXREGION_NUM_RECTS(reg1); i++) {
	if (rects1[i].x1 != rects2[i].x1) return PIXMAN_REGION_STATUS_FAILURE;
	if (rects1[i].x2 != rects2[i].x2) return PIXMAN_REGION_STATUS_FAILURE;
	if (rects1[i].y1 != rects2[i].y1) return PIXMAN_REGION_STATUS_FAILURE;
	if (rects1[i].y2 != rects2[i].y2) return PIXMAN_REGION_STATUS_FAILURE;
    }
    return PIXMAN_REGION_STATUS_SUCCESS;
}

pixman_region_status_t
pixman_region16_valid(reg)
    pixman_region16_t * reg;
{
    int i, numRects;

    if ((reg->extents.x1 > reg->extents.x2) ||
	(reg->extents.y1 > reg->extents.y2))
	return PIXMAN_REGION_STATUS_FAILURE;
    numRects = PIXREGION_NUM_RECTS(reg);
    if (!numRects)
	return ((reg->extents.x1 == reg->extents.x2) &&
		(reg->extents.y1 == reg->extents.y2) &&
		(reg->data->size || (reg->data == &pixman_region_emptyData)));
    else if (numRects == 1)
	return (!reg->data);
    else
    {
	pixman_box16_t * pboxP, pboxN;
	pixman_box16_t box;

	pboxP = PIXREGION_RECTS(reg);
	box = *pboxP;
	box.y2 = pboxP[numRects-1].y2;
	pboxN = pboxP + 1;
	for (i = numRects; --i > 0; pboxP++, pboxN++)
	{
	    if ((pboxN->x1 >= pboxN->x2) ||
		(pboxN->y1 >= pboxN->y2))
		return PIXMAN_REGION_STATUS_FAILURE;
	    if (pboxN->x1 < box.x1)
	        box.x1 = pboxN->x1;
	    if (pboxN->x2 > box.x2)
		box.x2 = pboxN->x2;
	    if ((pboxN->y1 < pboxP->y1) ||
		((pboxN->y1 == pboxP->y1) &&
		 ((pboxN->x1 < pboxP->x2) || (pboxN->y2 != pboxP->y2))))
		return PIXMAN_REGION_STATUS_FAILURE;
	}
	return ((box.x1 == reg->extents.x1) &&
		(box.x2 == reg->extents.x2) &&
		(box.y1 == reg->extents.y1) &&
		(box.y2 == reg->extents.y2));
    }
}

#endif /* DEBUG_PIXREGION */


/*	Create a new empty region	*/
pixman_region16_t *
pixman_region_create (void)
{
    return pixman_region_create_simple (NULL);
}

/*****************************************************************
 *   pixman_region_create_simple (extents)
 *     This routine creates a pixman_region16_t for a simple
 *     rectangular region.
 *****************************************************************/
pixman_region16_t *
pixman_region_create_simple (pixman_box16_t *extents)
{
    pixman_region16_t *region;
   
    region = malloc (sizeof (pixman_region16_t));
    if (region == NULL)
	return &pixman_brokenregion;

    pixman_init (region, extents);

    return region;
}
slim_hidden_def(pixman_region_create_simple);

/*****************************************************************
 *   RegionInit(pReg, rect, size)
 *     Outer region rect is statically allocated.
 *****************************************************************/

static void
pixman_init(pixman_region16_t *region, pixman_box16_t *extents)
{
    if (extents)
    {
	region->extents = *extents;
	region->data = NULL;
    }
    else
    {
	region->extents = pixman_region_emptyBox;
	region->data = &pixman_region_emptyData;
    }
}

static void
pixman_uninit (pixman_region16_t *region)
{
    good (region);
    freeData (region);
}

void
pixman_region_destroy (pixman_region16_t *region)
{
    pixman_uninit (region);

    if (region != &pixman_brokenregion)
	free (region);
}

int
pixman_region_num_rects (pixman_region16_t *region)
{
    return PIXREGION_NUM_RECTS (region);
}

pixman_box16_t *
pixman_region_rects (pixman_region16_t *region)
{
    return PIXREGION_RECTS (region);
}

static pixman_region_status_t
pixman_break (pixman_region16_t *region)
{
    freeData (region);
    region->extents = pixman_region_emptyBox;
    region->data = &pixman_brokendata;
    return PIXMAN_REGION_STATUS_FAILURE;
}

static pixman_region_status_t
pixman_rect_alloc(pixman_region16_t * region, int n)
{
    pixman_region16_data_t *data;
    
    if (!region->data)
    {
	n++;
	region->data = allocData(n);
	if (!region->data)
	    return pixman_break (region);
	region->data->numRects = 1;
	*PIXREGION_BOXPTR(region) = region->extents;
    }
    else if (!region->data->size)
    {
	region->data = allocData(n);
	if (!region->data)
	    return pixman_break (region);
	region->data->numRects = 0;
    }
    else
    {
	if (n == 1)
	{
	    n = region->data->numRects;
	    if (n > 500) /* XXX pick numbers out of a hat */
		n = 250;
	}
	n += region->data->numRects;
	data = (pixman_region16_data_t *)realloc(region->data, PIXREGION_SZOF(n));
	if (!data)
	    return pixman_break (region);
	region->data = data;
    }
    region->data->size = n;
    return PIXMAN_REGION_STATUS_SUCCESS;
}

pixman_region_status_t
pixman_region_copy(pixman_region16_t *dst, pixman_region16_t *src)
{
    good(dst);
    good(src);
    if (dst == src)
	return PIXMAN_REGION_STATUS_SUCCESS;
    dst->extents = src->extents;
    if (!src->data || !src->data->size)
    {
	freeData(dst);
	dst->data = src->data;
	return PIXMAN_REGION_STATUS_SUCCESS;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	freeData(dst);
	dst->data = allocData(src->data->numRects);
	if (!dst->data)
	    return pixman_break (dst);
	dst->data->size = src->data->numRects;
    }
    dst->data->numRects = src->data->numRects;
    memmove((char *)PIXREGION_BOXPTR(dst),(char *)PIXREGION_BOXPTR(src), 
	  dst->data->numRects * sizeof(pixman_box16_t));
    return PIXMAN_REGION_STATUS_SUCCESS;
}
slim_hidden_def(pixman_region_copy);


/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_coalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by pixman_op.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - region->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
INLINE static int
pixman_coalesce (
    pixman_region16_t *	region,	    	/* Region to coalesce		     */
    int	    	  	prevStart,  	/* Index of start of previous band   */
    int	    	  	curStart)   	/* Index of start of current band    */
{
    pixman_box16_t *	pPrevBox;   	/* Current box in previous band	     */
    pixman_box16_t *	pCurBox;    	/* Current box in current band       */
    int  	numRects;	/* Number rectangles in both bands   */
    int	y2;		/* Bottom of current band	     */
    /*
     * Figure out how many rectangles are in the band.
     */
    numRects = curStart - prevStart;
    assert(numRects == region->data->numRects - curStart);

    if (!numRects) return curStart;

    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    pPrevBox = PIXREGION_BOX(region, prevStart);
    pCurBox = PIXREGION_BOX(region, curStart);
    if (pPrevBox->y2 != pCurBox->y1) return curStart;

    /*
     * Make sure the bands have boxes in the same places. This
     * assumes that boxes have been added in such a way that they
     * cover the most area possible. I.e. two boxes in a band must
     * have some horizontal space between them.
     */
    y2 = pCurBox->y2;

    do {
	if ((pPrevBox->x1 != pCurBox->x1) || (pPrevBox->x2 != pCurBox->x2)) {
	    return (curStart);
	}
	pPrevBox++;
	pCurBox++;
	numRects--;
    } while (numRects);

    /*
     * The bands may be merged, so set the bottom y of each box
     * in the previous band to the bottom y of the current band.
     */
    numRects = curStart - prevStart;
    region->data->numRects -= numRects;
    do {
	pPrevBox--;
	pPrevBox->y2 = y2;
	numRects--;
    } while (numRects);
    return prevStart;
}


/* Quicky macro to avoid trivial reject procedure calls to pixman_coalesce */

#define Coalesce(newReg, prevBand, curBand)				\
    if (curBand - prevBand == newReg->data->numRects - curBand) {	\
	prevBand = pixman_coalesce(newReg, prevBand, curBand);		\
    } else {								\
	prevBand = curBand;						\
    }

/*-
 *-----------------------------------------------------------------------
 * pixman_region_appendNonO --
 *	Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	region->data->numRects is incremented and the rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */

INLINE static pixman_region_status_t
pixman_region_appendNonO (
    pixman_region16_t *	region,
    pixman_box16_t *	r,
    pixman_box16_t *  	  	rEnd,
    int  	y1,
    int  	y2)
{
    pixman_box16_t *	pNextRect;
    int	newRects;

    newRects = rEnd - r;

    assert(y1 < y2);
    assert(newRects != 0);

    /* Make sure we have enough space for all rectangles to be added */
    RECTALLOC(region, newRects);
    pNextRect = PIXREGION_TOP(region);
    region->data->numRects += newRects;
    do {
	assert(r->x1 < r->x2);
	ADDRECT(pNextRect, r->x1, y1, r->x2, y2);
	r++;
    } while (r != rEnd);

    return PIXMAN_REGION_STATUS_SUCCESS;
}

#define FindBand(r, rBandEnd, rEnd, ry1)		    \
{							    \
    ry1 = r->y1;					    \
    rBandEnd = r+1;					    \
    while ((rBandEnd != rEnd) && (rBandEnd->y1 == ry1)) {   \
	rBandEnd++;					    \
    }							    \
}

#define	AppendRegions(newReg, r, rEnd)					\
{									\
    int newRects;							\
    if ((newRects = rEnd - r)) {					\
	RECTALLOC(newReg, newRects);					\
	memmove((char *)PIXREGION_TOP(newReg),(char *)r, 			\
              newRects * sizeof(pixman_box16_t));				\
	newReg->data->numRects += newRects;				\
    }									\
}

/*-
 *-----------------------------------------------------------------------
 * pixman_op --
 *	Apply an operation to two regions. Called by pixman_region_union, pixman_region_inverse,
 *	pixman_region_subtract, pixman_region_intersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *	The new region is overwritten.
 *	pOverlap set to PIXMAN_REGION_STATUS_SUCCESS if overlapFunc ever returns PIXMAN_REGION_STATUS_SUCCESS.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, the nonOverlapFunc is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */

typedef pixman_region_status_t (*OverlapProcPtr)(
    pixman_region16_t	 *region,
    pixman_box16_t *r1,
    pixman_box16_t *r1End,
    pixman_box16_t *r2,
    pixman_box16_t *r2End,
    short    	 y1,
    short    	 y2,
    int		 *pOverlap);

static pixman_region_status_t
pixman_op(
    pixman_region16_t *       newReg,		    /* Place to store result	     */
    pixman_region16_t *       reg1,		    /* First region in operation     */
    pixman_region16_t *       reg2,		    /* 2d region in operation        */
    OverlapProcPtr  overlapFunc,            /* Function to call for over-
					     * lapping bands		     */
    int	    appendNon1,		    /* Append non-overlapping bands  */
					    /* in region 1 ? */
    int	    appendNon2,		    /* Append non-overlapping bands  */
					    /* in region 2 ? */
    int	    *pOverlap)
{
    pixman_box16_t * r1;			    /* Pointer into first region     */
    pixman_box16_t * r2;			    /* Pointer into 2d region	     */
    pixman_box16_t *	    r1End;		    /* End of 1st region	     */
    pixman_box16_t *	    r2End;		    /* End of 2d region		     */
    short	    ybot;		    /* Bottom of intersection	     */
    short	    ytop;		    /* Top of intersection	     */
    pixman_region16_data_t *	    oldData;		    /* Old data for newReg	     */
    int		    prevBand;		    /* Index of start of
					     * previous band in newReg       */
    int		    curBand;		    /* Index of start of current
					     * band in newReg		     */
    pixman_box16_t * r1BandEnd;		    /* End of current band in r1     */
    pixman_box16_t * r2BandEnd;		    /* End of current band in r2     */
    short	    top;		    /* Top of non-overlapping band   */
    short	    bot;		    /* Bottom of non-overlapping band*/
    int    r1y1;		    /* Temps for r1->y1 and r2->y1   */
    int    r2y1;
    int		    newSize;
    int		    numRects;

    /*
     * Break any region computed from a broken region
     */
    if (PIXREGION_NAR (reg1) || PIXREGION_NAR(reg2))
	return pixman_break (newReg);
    
    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, save the rectangles
     * of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */

    r1 = PIXREGION_RECTS(reg1);
    newSize = PIXREGION_NUM_RECTS(reg1);
    r1End = r1 + newSize;
    numRects = PIXREGION_NUM_RECTS(reg2);
    r2 = PIXREGION_RECTS(reg2);
    r2End = r2 + numRects;
    assert(r1 != r1End);
    assert(r2 != r2End);

    oldData = (pixman_region16_data_t *)NULL;
    if (((newReg == reg1) && (newSize > 1)) ||
	((newReg == reg2) && (numRects > 1)))
    {
	oldData = newReg->data;
	newReg->data = &pixman_region_emptyData;
    }
    /* guess at new size */
    if (numRects > newSize)
	newSize = numRects;
    newSize <<= 1;
    if (!newReg->data)
	newReg->data = &pixman_region_emptyData;
    else if (newReg->data->size)
	newReg->data->numRects = 0;
    if (newSize > newReg->data->size)
	if (!pixman_rect_alloc(newReg, newSize))
	    return PIXMAN_REGION_STATUS_FAILURE;

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = MIN(r1->y1, r2->y1);
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. pixman_coalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do {
	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	assert(r1 != r1End);
	assert(r2 != r2End);
    
	FindBand(r1, r1BandEnd, r1End, r1y1);
	FindBand(r2, r2BandEnd, r2End, r2y1);

	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1y1 < r2y1) {
	    if (appendNon1) {
		top = MAX(r1y1, ybot);
		bot = MIN(r1->y2, r2y1);
		if (top != bot)	{
		    curBand = newReg->data->numRects;
		    pixman_region_appendNonO(newReg, r1, r1BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r2y1;
	} else if (r2y1 < r1y1) {
	    if (appendNon2) {
		top = MAX(r2y1, ybot);
		bot = MIN(r2->y2, r1y1);
		if (top != bot) {
		    curBand = newReg->data->numRects;
		    pixman_region_appendNonO(newReg, r2, r2BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r1y1;
	} else {
	    ytop = r1y1;
	}

	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = MIN(r1->y2, r2->y2);
	if (ybot > ytop) {
	    curBand = newReg->data->numRects;
	    (* overlapFunc)(newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot,
			    pOverlap);
	    Coalesce(newReg, prevBand, curBand);
	}

	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot) r1 = r1BandEnd;
	if (r2->y2 == ybot) r2 = r2BandEnd;

    } while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1End) && appendNon1) {
	/* Do first nonOverlap1Func call, which may be able to coalesce */
	FindBand(r1, r1BandEnd, r1End, r1y1);
	curBand = newReg->data->numRects;
	pixman_region_appendNonO(newReg, r1, r1BandEnd, MAX(r1y1, ybot), r1->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Just append the rest of the boxes  */
	AppendRegions(newReg, r1BandEnd, r1End);

    } else if ((r2 != r2End) && appendNon2) {
	/* Do first nonOverlap2Func call, which may be able to coalesce */
	FindBand(r2, r2BandEnd, r2End, r2y1);
	curBand = newReg->data->numRects;
	pixman_region_appendNonO(newReg, r2, r2BandEnd, MAX(r2y1, ybot), r2->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Append rest of boxes */
	AppendRegions(newReg, r2BandEnd, r2End);
    }

    if (oldData)
	free(oldData);

    if (!(numRects = newReg->data->numRects))
    {
	freeData(newReg);
	newReg->data = &pixman_region_emptyData;
    }
    else if (numRects == 1)
    {
	newReg->extents = *PIXREGION_BOXPTR(newReg);
	freeData(newReg);
	newReg->data = (pixman_region16_data_t *)NULL;
    }
    else
    {
	DOWNSIZE(newReg, numRects);
    }

    return PIXMAN_REGION_STATUS_SUCCESS;
}

/*-
 *-----------------------------------------------------------------------
 * pixman_set_extents --
 *	Reset the extents of a region to what they should be. Called by
 *	pixman_region_subtract and pixman_region_intersect as they can't figure it out along the
 *	way or do so easily, as pixman_region_union can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void
pixman_set_extents (pixman_region16_t *region)
{
    pixman_box16_t *box, *boxEnd;

    if (!region->data)
	return;
    if (!region->data->size)
    {
	region->extents.x2 = region->extents.x1;
	region->extents.y2 = region->extents.y1;
	return;
    }

    box = PIXREGION_BOXPTR(region);
    boxEnd = PIXREGION_END(region);

    /*
     * Since box is the first rectangle in the region, it must have the
     * smallest y1 and since boxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  box and boxEnd, resp., as good things to initialize them
     * to...
     */
    region->extents.x1 = box->x1;
    region->extents.y1 = box->y1;
    region->extents.x2 = boxEnd->x2;
    region->extents.y2 = boxEnd->y2;

    assert(region->extents.y1 < region->extents.y2);
    while (box <= boxEnd) {
	if (box->x1 < region->extents.x1)
	    region->extents.x1 = box->x1;
	if (box->x2 > region->extents.x2)
	    region->extents.x2 = box->x2;
	box++;
    };

    assert(region->extents.x1 < region->extents.x2);
}

/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * pixman_region_intersectO --
 *	Handle an overlapping band for pixman_region_intersect.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_region_status_t
pixman_region_intersectO (
    pixman_region16_t *	region,
    pixman_box16_t *	r1,
    pixman_box16_t *  	r1End,
    pixman_box16_t *	r2,
    pixman_box16_t *  	r2End,
    short    	  	y1,
    short    	  	y2,
    int		*pOverlap)
{
    int  	x1;
    int  	x2;
    pixman_box16_t *	pNextRect;

    pNextRect = PIXREGION_TOP(region);

    assert(y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    do {
	x1 = MAX(r1->x1, r2->x1);
	x2 = MIN(r1->x2, r2->x2);

	/*
	 * If there's any overlap between the two rectangles, add that
	 * overlap to the new region.
	 */
	if (x1 < x2)
	    NEWRECT(region, pNextRect, x1, y1, x2, y2);

	/*
	 * Advance the pointer(s) with the leftmost right side, since the next
	 * rectangle on that list may still overlap the other region's
	 * current rectangle.
	 */
	if (r1->x2 == x2) {
	    r1++;
	}
	if (r2->x2 == x2) {
	    r2++;
	}
    } while ((r1 != r1End) && (r2 != r2End));

    return PIXMAN_REGION_STATUS_SUCCESS;
}


pixman_region_status_t
pixman_region_intersect(newReg, reg1, reg2)
    pixman_region16_t * 	newReg;     /* destination Region */
    pixman_region16_t * 	reg1;
    pixman_region16_t *	reg2;       /* source regions     */
{
    good(reg1);
    good(reg2);
    good(newReg);
   /* check for trivial reject */
    if (PIXREGION_NIL(reg1)  || PIXREGION_NIL(reg2) ||
	!EXTENTCHECK(&reg1->extents, &reg2->extents))
    {
	/* Covers about 20% of all cases */
	freeData(newReg);
	newReg->extents.x2 = newReg->extents.x1;
	newReg->extents.y2 = newReg->extents.y1;
	if (PIXREGION_NAR(reg1) || PIXREGION_NAR(reg2))
	{
	    newReg->data = &pixman_brokendata;
	    return PIXMAN_REGION_STATUS_FAILURE;
	}
	else
	    newReg->data = &pixman_region_emptyData;
    }
    else if (!reg1->data && !reg2->data)
    {
	/* Covers about 80% of cases that aren't trivially rejected */
	newReg->extents.x1 = MAX(reg1->extents.x1, reg2->extents.x1);
	newReg->extents.y1 = MAX(reg1->extents.y1, reg2->extents.y1);
	newReg->extents.x2 = MIN(reg1->extents.x2, reg2->extents.x2);
	newReg->extents.y2 = MIN(reg1->extents.y2, reg2->extents.y2);
	freeData(newReg);
	newReg->data = (pixman_region16_data_t *)NULL;
    }
    else if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
	return pixman_region_copy(newReg, reg1);
    }
    else if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
	return pixman_region_copy(newReg, reg2);
    }
    else if (reg1 == reg2)
    {
	return pixman_region_copy(newReg, reg1);
    }
    else
    {
	/* General purpose intersection */
	int overlap; /* result ignored */
	if (!pixman_op(newReg, reg1, reg2, pixman_region_intersectO, PIXMAN_REGION_STATUS_FAILURE, PIXMAN_REGION_STATUS_FAILURE,
			&overlap))
	    return PIXMAN_REGION_STATUS_FAILURE;
	pixman_set_extents(newReg);
    }

    good(newReg);
    return(PIXMAN_REGION_STATUS_SUCCESS);
}

#define MERGERECT(r)						\
{								\
    if (r->x1 <= x2) {						\
	/* Merge with current rectangle */			\
	if (r->x1 < x2) *pOverlap = PIXMAN_REGION_STATUS_SUCCESS;				\
	if (x2 < r->x2) x2 = r->x2;				\
    } else {							\
	/* Add current rectangle, start new one */		\
	NEWRECT(region, pNextRect, x1, y1, x2, y2);		\
	x1 = r->x1;						\
	x2 = r->x2;						\
    }								\
    r++;							\
}

/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_unionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *	region is overwritten.
 *	pOverlap is set to PIXMAN_REGION_STATUS_SUCCESS if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static pixman_region_status_t
pixman_region_unionO (
    pixman_region16_t	 *region,
    pixman_box16_t *r1,
    pixman_box16_t *r1End,
    pixman_box16_t *r2,
    pixman_box16_t *r2End,
    short	  y1,
    short	  y2,
    int		  *pOverlap)
{
    pixman_box16_t *     pNextRect;
    int        x1;     /* left and right side of current union */
    int        x2;

    assert (y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = PIXREGION_TOP(region);

    /* Start off current rectangle */
    if (r1->x1 < r2->x1)
    {
	x1 = r1->x1;
	x2 = r1->x2;
	r1++;
    }
    else
    {
	x1 = r2->x1;
	x2 = r2->x2;
	r2++;
    }
    while (r1 != r1End && r2 != r2End)
    {
	if (r1->x1 < r2->x1) MERGERECT(r1) else MERGERECT(r2);
    }

    /* Finish off whoever (if any) is left */
    if (r1 != r1End)
    {
	do
	{
	    MERGERECT(r1);
	} while (r1 != r1End);
    }
    else if (r2 != r2End)
    {
	do
	{
	    MERGERECT(r2);
	} while (r2 != r2End);
    }
    
    /* Add current rectangle */
    NEWRECT(region, pNextRect, x1, y1, x2, y2);

    return PIXMAN_REGION_STATUS_SUCCESS;
}

/* Convenience function for performing union of region with a single rectangle */
pixman_region_status_t
pixman_region_union_rect(pixman_region16_t *dest, pixman_region16_t *source,
		   int x, int y, unsigned int width, unsigned int height)
{
    pixman_region16_t region;

    if (!width || !height)
	return pixman_region_copy (dest, source);
    region.data = NULL;
    region.extents.x1 = x;
    region.extents.y1 = y;
    region.extents.x2 = x + width;
    region.extents.y2 = y + height;

    return pixman_region_union (dest, source, &region);
}

pixman_region_status_t
pixman_region_union(pixman_region16_t *newReg, pixman_region16_t *reg1, pixman_region16_t *reg2)
{
    int overlap; /* result ignored */

    /* Return PIXMAN_REGION_STATUS_SUCCESS if some overlap between reg1, reg2 */
    good(reg1);
    good(reg2);
    good(newReg);
    /*  checks all the simple cases */

    /*
     * Region 1 and 2 are the same
     */
    if (reg1 == reg2)
    {
	return pixman_region_copy(newReg, reg1);
    }

    /*
     * Region 1 is empty
     */
    if (PIXREGION_NIL(reg1))
    {
	if (PIXREGION_NAR(reg1))
	    return pixman_break (newReg);
        if (newReg != reg2)
	    return pixman_region_copy(newReg, reg2);
        return PIXMAN_REGION_STATUS_SUCCESS;
    }

    /*
     * Region 2 is empty
     */
    if (PIXREGION_NIL(reg2))
    {
	if (PIXREGION_NAR(reg2))
	    return pixman_break (newReg);
        if (newReg != reg1)
	    return pixman_region_copy(newReg, reg1);
        return PIXMAN_REGION_STATUS_SUCCESS;
    }

    /*
     * Region 1 completely subsumes region 2
     */
    if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        if (newReg != reg1)
	    return pixman_region_copy(newReg, reg1);
        return PIXMAN_REGION_STATUS_SUCCESS;
    }

    /*
     * Region 2 completely subsumes region 1
     */
    if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        if (newReg != reg2)
	    return pixman_region_copy(newReg, reg2);
        return PIXMAN_REGION_STATUS_SUCCESS;
    }

    if (!pixman_op(newReg, reg1, reg2, pixman_region_unionO, PIXMAN_REGION_STATUS_SUCCESS, PIXMAN_REGION_STATUS_SUCCESS, &overlap))
	return PIXMAN_REGION_STATUS_FAILURE;

    newReg->extents.x1 = MIN(reg1->extents.x1, reg2->extents.x1);
    newReg->extents.y1 = MIN(reg1->extents.y1, reg2->extents.y1);
    newReg->extents.x2 = MAX(reg1->extents.x2, reg2->extents.x2);
    newReg->extents.y2 = MAX(reg1->extents.y2, reg2->extents.y2);
    good(newReg);
    return PIXMAN_REGION_STATUS_SUCCESS;
}
slim_hidden_def(pixman_region_union);


/*======================================================================
 *	    Batch Rectangle Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_append --
 * 
 *      "Append" the rgn rectangles onto the end of dstrgn, maintaining
 *      knowledge of YX-banding when it's easy.  Otherwise, dstrgn just
 *      becomes a non-y-x-banded random collection of rectangles, and not
 *      yet a true region.  After a sequence of appends, the caller must
 *      call pixman_region_validate to ensure that a valid region is constructed.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *      dstrgn is modified if rgn has rectangles.
 *
 */
pixman_region_status_t
pixman_region_append(dstrgn, rgn)
    pixman_region16_t * dstrgn;
    pixman_region16_t * rgn;
{
    int numRects, dnumRects, size;
    pixman_box16_t *new, *old;
    int prepend;

    if (PIXREGION_NAR(rgn))
	return pixman_break (dstrgn);
    
    if (!rgn->data && (dstrgn->data == &pixman_region_emptyData))
    {
	dstrgn->extents = rgn->extents;
	dstrgn->data = (pixman_region16_data_t *)NULL;
	return PIXMAN_REGION_STATUS_SUCCESS;
    }

    numRects = PIXREGION_NUM_RECTS(rgn);
    if (!numRects)
	return PIXMAN_REGION_STATUS_SUCCESS;
    prepend = PIXMAN_REGION_STATUS_FAILURE;
    size = numRects;
    dnumRects = PIXREGION_NUM_RECTS(dstrgn);
    if (!dnumRects && (size < 200))
	size = 200; /* XXX pick numbers out of a hat */
    RECTALLOC(dstrgn, size);
    old = PIXREGION_RECTS(rgn);
    if (!dnumRects)
	dstrgn->extents = rgn->extents;
    else if (dstrgn->extents.x2 > dstrgn->extents.x1)
    {
	pixman_box16_t *first, *last;

	first = old;
	last = PIXREGION_BOXPTR(dstrgn) + (dnumRects - 1);
	if ((first->y1 > last->y2) ||
	    ((first->y1 == last->y1) && (first->y2 == last->y2) &&
	     (first->x1 > last->x2)))
	{
	    if (rgn->extents.x1 < dstrgn->extents.x1)
		dstrgn->extents.x1 = rgn->extents.x1;
	    if (rgn->extents.x2 > dstrgn->extents.x2)
		dstrgn->extents.x2 = rgn->extents.x2;
	    dstrgn->extents.y2 = rgn->extents.y2;
	}
	else
	{
	    first = PIXREGION_BOXPTR(dstrgn);
	    last = old + (numRects - 1);
	    if ((first->y1 > last->y2) ||
		((first->y1 == last->y1) && (first->y2 == last->y2) &&
		 (first->x1 > last->x2)))
	    {
		prepend = PIXMAN_REGION_STATUS_SUCCESS;
		if (rgn->extents.x1 < dstrgn->extents.x1)
		    dstrgn->extents.x1 = rgn->extents.x1;
		if (rgn->extents.x2 > dstrgn->extents.x2)
		    dstrgn->extents.x2 = rgn->extents.x2;
		dstrgn->extents.y1 = rgn->extents.y1;
	    }
	    else
		dstrgn->extents.x2 = dstrgn->extents.x1;
	}
    }
    if (prepend)
    {
	new = PIXREGION_BOX(dstrgn, numRects);
	if (dnumRects == 1)
	    *new = *PIXREGION_BOXPTR(dstrgn);
	else
	    memmove((char *)new,(char *)PIXREGION_BOXPTR(dstrgn), 
		  dnumRects * sizeof(pixman_box16_t));
	new = PIXREGION_BOXPTR(dstrgn);
    }
    else
	new = PIXREGION_BOXPTR(dstrgn) + dnumRects;
    if (numRects == 1)
	*new = *old;
    else
	memmove((char *)new, (char *)old, numRects * sizeof(pixman_box16_t));
    dstrgn->data->numRects += numRects;
    return PIXMAN_REGION_STATUS_SUCCESS;
}

   
#define ExchangeRects(a, b) \
{			    \
    pixman_box16_t     t;	    \
    t = rects[a];	    \
    rects[a] = rects[b];    \
    rects[b] = t;	    \
}

static void
QuickSortRects(
    pixman_box16_t     rects[],
    int        numRects)
{
    int	y1;
    int	x1;
    int        i, j;
    pixman_box16_t *r;

    /* Always called with numRects > 1 */

    do
    {
	if (numRects == 2)
	{
	    if (rects[0].y1 > rects[1].y1 ||
		    (rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
		ExchangeRects(0, 1);
	    return;
	}

	/* Choose partition element, stick in location 0 */
        ExchangeRects(0, numRects >> 1);
	y1 = rects[0].y1;
	x1 = rects[0].x1;

        /* Partition array */
        i = 0;
        j = numRects;
        do
	{
	    r = &(rects[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numRects &&
		     (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));
	    r = &(rects[j]);
	    do
	    {
		r--;
		j--;
            } while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));
            if (i < j)
		ExchangeRects(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeRects(0, j);

	/* Recurse */
        if (numRects-j-1 > 1)
	    QuickSortRects(&rects[j+1], numRects-j-1);
        numRects = j;
    } while (numRects > 1);
}

/*-
 *-----------------------------------------------------------------------
 * pixman_region_validate --
 * 
 *      Take a ``region'' which is a non-y-x-banded random collection of
 *      rectangles, and compute a nice region which is the union of all the
 *      rectangles.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *      The passed-in ``region'' may be modified.
 *	pOverlap set to PIXMAN_REGION_STATUS_SUCCESS if any retangles overlapped, else PIXMAN_REGION_STATUS_FAILURE;
 *
 * Strategy:
 *      Step 1. Sort the rectangles into ascending order with primary key y1
 *		and secondary key x1.
 *
 *      Step 2. Split the rectangles into the minimum number of proper y-x
 *		banded regions.  This may require horizontally merging
 *		rectangles, and vertically coalescing bands.  With any luck,
 *		this step in an identity tranformation (ala the Box widget),
 *		or a coalescing into 1 box (ala Menus).
 *
 *	Step 3. Merge the separate regions down to a single region by calling
 *		pixman_region_union.  Maximize the work each pixman_region_union call does by using
 *		a binary merge.
 *
 *-----------------------------------------------------------------------
 */

pixman_region_status_t
pixman_region_validate(badreg, pOverlap)
    pixman_region16_t * badreg;
    int *pOverlap;
{
    /* Descriptor for regions under construction  in Step 2. */
    typedef struct {
	pixman_region16_t   reg;
	int	    prevBand;
	int	    curBand;
    } RegionInfo;

	     int	numRects;   /* Original numRects for badreg	    */
	     RegionInfo *ri;	    /* Array of current regions		    */
    	     int	numRI;      /* Number of entries used in ri	    */
	     int	sizeRI;	    /* Number of entries available in ri    */
	     int	i;	    /* Index into rects			    */
    int	j;	    /* Index into ri			    */
    RegionInfo *rit;       /* &ri[j]				    */
    pixman_region16_t *  reg;        /* ri[j].reg			    */
    pixman_box16_t *	box;	    /* Current box in rects		    */
    pixman_box16_t *	riBox;      /* Last box in ri[j].reg		    */
    pixman_region16_t *  hreg;       /* ri[j_half].reg			    */
    int		ret = PIXMAN_REGION_STATUS_SUCCESS;

    *pOverlap = PIXMAN_REGION_STATUS_FAILURE;
    if (!badreg->data)
    {
	good(badreg);
	return PIXMAN_REGION_STATUS_SUCCESS;
    }
    numRects = badreg->data->numRects;
    if (!numRects)
    {
	if (PIXREGION_NAR(badreg))
	    return PIXMAN_REGION_STATUS_FAILURE;
	good(badreg);
	return PIXMAN_REGION_STATUS_SUCCESS;
    }
    if (badreg->extents.x1 < badreg->extents.x2)
    {
	if ((numRects) == 1)
	{
	    freeData(badreg);
	    badreg->data = (pixman_region16_data_t *) NULL;
	}
	else
	{
	    DOWNSIZE(badreg, numRects);
	}
	good(badreg);
	return PIXMAN_REGION_STATUS_SUCCESS;
    }

    /* Step 1: Sort the rects array into ascending (y1, x1) order */
    QuickSortRects(PIXREGION_BOXPTR(badreg), numRects);

    /* Step 2: Scatter the sorted array into the minimum number of regions */

    /* Set up the first region to be the first rectangle in badreg */
    /* Note that step 2 code will never overflow the ri[0].reg rects array */
    ri = (RegionInfo *) malloc(4 * sizeof(RegionInfo));
    if (!ri)
	return pixman_break (badreg);
    sizeRI = 4;
    numRI = 1;
    ri[0].prevBand = 0;
    ri[0].curBand = 0;
    ri[0].reg = *badreg;
    box = PIXREGION_BOXPTR(&ri[0].reg);
    ri[0].reg.extents = *box;
    ri[0].reg.data->numRects = 1;

    /* Now scatter rectangles into the minimum set of valid regions.  If the
       next rectangle to be added to a region would force an existing rectangle
       in the region to be split up in order to maintain y-x banding, just
       forget it.  Try the next region.  If it doesn't fit cleanly into any
       region, make a new one. */

    for (i = numRects; --i > 0;)
    {
	box++;
	/* Look for a region to append box to */
	for (j = numRI, rit = ri; --j >= 0; rit++)
	{
	    reg = &rit->reg;
	    riBox = PIXREGION_END(reg);

	    if (box->y1 == riBox->y1 && box->y2 == riBox->y2)
	    {
		/* box is in same band as riBox.  Merge or append it */
		if (box->x1 <= riBox->x2)
		{
		    /* Merge it with riBox */
		    if (box->x1 < riBox->x2) *pOverlap = PIXMAN_REGION_STATUS_SUCCESS;
		    if (box->x2 > riBox->x2) riBox->x2 = box->x2;
		}
		else
		{
		    RECTALLOC_BAIL(reg, 1, bail);
		    *PIXREGION_TOP(reg) = *box;
		    reg->data->numRects++;
		}
		goto NextRect;   /* So sue me */
	    }
	    else if (box->y1 >= riBox->y2)
	    {
		/* Put box into new band */
		if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
		if (reg->extents.x1 > box->x1)   reg->extents.x1 = box->x1;
		Coalesce(reg, rit->prevBand, rit->curBand);
		rit->curBand = reg->data->numRects;
		RECTALLOC_BAIL(reg, 1, bail);
		*PIXREGION_TOP(reg) = *box;
		reg->data->numRects++;
		goto NextRect;
	    }
	    /* Well, this region was inappropriate.  Try the next one. */
	} /* for j */

	/* Uh-oh.  No regions were appropriate.  Create a new one. */
	if (sizeRI == numRI)
	{
	    /* Oops, allocate space for new region information */
	    sizeRI <<= 1;
	    rit = (RegionInfo *) realloc(ri, sizeRI * sizeof(RegionInfo));
	    if (!rit)
		goto bail;
	    ri = rit;
	    rit = &ri[numRI];
	}
	numRI++;
	rit->prevBand = 0;
	rit->curBand = 0;
	rit->reg.extents = *box;
	rit->reg.data = (pixman_region16_data_t *)NULL;
	if (!pixman_rect_alloc(&rit->reg, (i+numRI) / numRI)) /* MUST force allocation */
	    goto bail;
NextRect: ;
    } /* for i */

    /* Make a final pass over each region in order to Coalesce and set
       extents.x2 and extents.y2 */

    for (j = numRI, rit = ri; --j >= 0; rit++)
    {
	reg = &rit->reg;
	riBox = PIXREGION_END(reg);
	reg->extents.y2 = riBox->y2;
	if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
	Coalesce(reg, rit->prevBand, rit->curBand);
	if (reg->data->numRects == 1) /* keep unions happy below */
	{
	    freeData(reg);
	    reg->data = (pixman_region16_data_t *)NULL;
	}
    }

    /* Step 3: Union all regions into a single region */
    while (numRI > 1)
    {
	int half = numRI/2;
	for (j = numRI & 1; j < (half + (numRI & 1)); j++)
	{
	    reg = &ri[j].reg;
	    hreg = &ri[j+half].reg;
	    if (!pixman_op(reg, reg, hreg, pixman_region_unionO, PIXMAN_REGION_STATUS_SUCCESS, PIXMAN_REGION_STATUS_SUCCESS, pOverlap))
		ret = PIXMAN_REGION_STATUS_FAILURE;
	    if (hreg->extents.x1 < reg->extents.x1)
		reg->extents.x1 = hreg->extents.x1;
	    if (hreg->extents.y1 < reg->extents.y1)
		reg->extents.y1 = hreg->extents.y1;
	    if (hreg->extents.x2 > reg->extents.x2)
		reg->extents.x2 = hreg->extents.x2;
	    if (hreg->extents.y2 > reg->extents.y2)
		reg->extents.y2 = hreg->extents.y2;
	    freeData(hreg);
	}
	numRI -= half;
    }
    *badreg = ri[0].reg;
    free(ri);
    good(badreg);
    return ret;
bail:
    for (i = 0; i < numRI; i++)
	freeData(&ri[i].reg);
    free (ri);
    return pixman_break (badreg);
}

/* XXX: Need to fix this to not use any X data structure
pixman_region16_t *
pixman_region_rectsToRegion(nrects, prect, ctype)
    int			nrects;
    xRectangle	*prect;
    int			ctype;
{
    pixman_region16_t *	region;
    pixman_region16_data_t *	pData;
    pixman_box16_t *	box;
    int        i;
    int			x1, y1, x2, y2;

    region = pixman_region_create(NullBox, 0);
    if (PIXREGION_NAR (region))
	return region;
    if (!nrects)
	return region;
    if (nrects == 1)
    {
	x1 = prect->x;
	y1 = prect->y;
	if ((x2 = x1 + (int) prect->width) > SHRT_MAX)
	    x2 = SHRT_MAX;
	if ((y2 = y1 + (int) prect->height) > SHRT_MAX)
	    y2 = SHRT_MAX;
	if (x1 != x2 && y1 != y2)
	{
	    region->extents.x1 = x1;
	    region->extents.y1 = y1;
	    region->extents.x2 = x2;
	    region->extents.y2 = y2;
	    region->data = (pixman_region16_data_t *)NULL;
	}
	return region;
    }
    pData = allocData(nrects);
    if (!pData)
    {
	pixman_break (region);
	return region;
    }
    box = (pixman_box16_t *) (pData + 1);
    for (i = nrects; --i >= 0; prect++)
    {
	x1 = prect->x;
	y1 = prect->y;
	if ((x2 = x1 + (int) prect->width) > SHRT_MAX)
	    x2 = SHRT_MAX;
	if ((y2 = y1 + (int) prect->height) > SHRT_MAX)
	    y2 = SHRT_MAX;
	if (x1 != x2 && y1 != y2)
	{
	    box->x1 = x1;
	    box->y1 = y1;
	    box->x2 = x2;
	    box->y2 = y2;
	    box++;
	}
    }
    if (box != (pixman_box16_t *) (pData + 1))
    {
	pData->size = nrects;
	pData->numRects = box - (pixman_box16_t *) (pData + 1);
    	region->data = pData;
    	if (ctype != CT_YXBANDED)
    	{
	    int overlap;
	    region->extents.x1 = region->extents.x2 = 0;
	    pixman_region_validate(region, &overlap);
    	}
    	else
	    pixman_set_extents(region);
    	good(region);
    }
    else
    {
	free (pData);
    }
    return region;
}
*/

/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/


/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *	region may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_region_status_t
pixman_region_subtractO (
    pixman_region16_t *	region,
    pixman_box16_t *	r1,
    pixman_box16_t *  	  	r1End,
    pixman_box16_t *	r2,
    pixman_box16_t *  	  	r2End,
    short  	y1,
             short  	y2,
    int		*pOverlap)
{
    pixman_box16_t *	pNextRect;
    int  	x1;

    x1 = r1->x1;
    
    assert(y1<y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = PIXREGION_TOP(region);

    do
    {
	if (r2->x2 <= x1)
	{
	    /*
	     * Subtrahend entirely to left of minuend: go to next subtrahend.
	     */
	    r2++;
	}
	else if (r2->x1 <= x1)
	{
	    /*
	     * Subtrahend preceeds minuend: nuke left edge of minuend.
	     */
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend completely covered: advance to next minuend and
		 * reset left fence to edge of new minuend.
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend now used up since it doesn't extend beyond
		 * minuend
		 */
		r2++;
	    }
	}
	else if (r2->x1 < r1->x2)
	{
	    /*
	     * Left part of subtrahend covers part of minuend: add uncovered
	     * part of minuend to region and skip to next subtrahend.
	     */
	    assert(x1<r2->x1);
	    NEWRECT(region, pNextRect, x1, y1, r2->x1, y2);

	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend used up: advance to new...
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend used up
		 */
		r2++;
	    }
	}
	else
	{
	    /*
	     * Minuend used up: add any remaining piece before advancing.
	     */
	    if (r1->x2 > x1)
		NEWRECT(region, pNextRect, x1, y1, r1->x2, y2);
	    r1++;
	    if (r1 != r1End)
		x1 = r1->x1;
	}
    } while ((r1 != r1End) && (r2 != r2End));


    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End)
    {
	assert(x1<r1->x2);
	NEWRECT(region, pNextRect, x1, y1, r1->x2, y2);
	r1++;
	if (r1 != r1End)
	    x1 = r1->x1;
    }
    return PIXMAN_REGION_STATUS_SUCCESS;
}
	
/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtract --
 *	Subtract regS from regM and leave the result in regD.
 *	S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS if successful.
 *
 * Side Effects:
 *	regD is overwritten.
 *
 *-----------------------------------------------------------------------
 */
pixman_region_status_t
pixman_region_subtract(regD, regM, regS)
    pixman_region16_t *	regD;               
    pixman_region16_t * 	regM;
    pixman_region16_t *	regS;          
{
    int overlap; /* result ignored */

    good(regM);
    good(regS);
    good(regD);
   /* check for trivial rejects */
    if (PIXREGION_NIL(regM) || PIXREGION_NIL(regS) ||
	!EXTENTCHECK(&regM->extents, &regS->extents))
    {
	if (PIXREGION_NAR (regS))
	    return pixman_break (regD);
	return pixman_region_copy(regD, regM);
    }
    else if (regM == regS)
    {
	freeData(regD);
	regD->extents.x2 = regD->extents.x1;
	regD->extents.y2 = regD->extents.y1;
	regD->data = &pixman_region_emptyData;
	return PIXMAN_REGION_STATUS_SUCCESS;
    }
 
    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!pixman_op(regD, regM, regS, pixman_region_subtractO, PIXMAN_REGION_STATUS_SUCCESS, PIXMAN_REGION_STATUS_FAILURE, &overlap))
	return PIXMAN_REGION_STATUS_FAILURE;

    /*
     * Can't alter RegD's extents before we call pixman_op because
     * it might be one of the source regions and pixman_op depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    pixman_set_extents(regD);
    good(regD);
    return PIXMAN_REGION_STATUS_SUCCESS;
}

/*======================================================================
 *	    Region Inversion
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_inverse --
 *	Take a region and a box and return a region that is everything
 *	in the box but not in the region. The careful reader will note
 *	that this is the same as subtracting the region from the box...
 *
 * Results:
 *	PIXMAN_REGION_STATUS_SUCCESS.
 *
 * Side Effects:
 *	newReg is overwritten.
 *
 *-----------------------------------------------------------------------
 */
pixman_region_status_t
pixman_region_inverse(newReg, reg1, invRect)
    pixman_region16_t * 	  newReg;       /* Destination region */
    pixman_region16_t * 	  reg1;         /* Region to invert */
    pixman_box16_t *     	  invRect; 	/* Bounding box for inversion */
{
    pixman_region16_t	  invReg;   	/* Quick and dirty region made from the
				 * bounding box */
    int	  overlap;	/* result ignored */

    good(reg1);
    good(newReg);
   /* check for trivial rejects */
    if (PIXREGION_NIL(reg1) || !EXTENTCHECK(invRect, &reg1->extents))
    {
	if (PIXREGION_NAR(reg1))
	    return pixman_break (newReg);
	newReg->extents = *invRect;
	freeData(newReg);
	newReg->data = (pixman_region16_data_t *)NULL;
        return PIXMAN_REGION_STATUS_SUCCESS;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    invReg.extents = *invRect;
    invReg.data = (pixman_region16_data_t *)NULL;
    if (!pixman_op(newReg, &invReg, reg1, pixman_region_subtractO, PIXMAN_REGION_STATUS_SUCCESS, PIXMAN_REGION_STATUS_FAILURE, &overlap))
	return PIXMAN_REGION_STATUS_FAILURE;

    /*
     * Can't alter newReg's extents before we call pixman_op because
     * it might be one of the source regions and pixman_op depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    pixman_set_extents(newReg);
    good(newReg);
    return PIXMAN_REGION_STATUS_SUCCESS;
}

/*
 *   RectIn(region, rect)
 *   This routine takes a pointer to a region and a pointer to a box
 *   and determines if the box is outside/inside/partly inside the region.
 *
 *   The idea is to travel through the list of rectangles trying to cover the
 *   passed box with them. Anytime a piece of the rectangle isn't covered
 *   by a band of rectangles, partOut is set PIXMAN_REGION_STATUS_SUCCESS. Any time a rectangle in
 *   the region covers part of the box, partIn is set PIXMAN_REGION_STATUS_SUCCESS. The process ends
 *   when either the box has been completely covered (we reached a band that
 *   doesn't overlap the box, partIn is PIXMAN_REGION_STATUS_SUCCESS and partOut is false), the
 *   box has been partially covered (partIn == partOut == PIXMAN_REGION_STATUS_SUCCESS -- because of
 *   the banding, the first time this is true we know the box is only
 *   partially in the region) or is outside the region (we reached a band
 *   that doesn't overlap the box at all and partIn is false)
 */

int
pixman_region_contains_rectangle(region, prect)
    pixman_region16_t *  region;
    pixman_box16_t *     prect;
{
    int	x;
    int	y;
    pixman_box16_t *     pbox;
    pixman_box16_t *     pboxEnd;
    int			partIn, partOut;
    int			numRects;

    good(region);
    numRects = PIXREGION_NUM_RECTS(region);
    /* useful optimization */
    if (!numRects || !EXTENTCHECK(&region->extents, prect))
        return(rgnOUT);

    if (numRects == 1)
    {
	/* We know that it must be rgnIN or rgnPART */
	if (SUBSUMES(&region->extents, prect))
	    return(rgnIN);
	else
	    return(rgnPART);
    }

    partOut = PIXMAN_REGION_STATUS_FAILURE;
    partIn = PIXMAN_REGION_STATUS_FAILURE;

    /* (x,y) starts at upper left of rect, moving to the right and down */
    x = prect->x1;
    y = prect->y1;

    /* can stop when both partOut and partIn are PIXMAN_REGION_STATUS_SUCCESS, or we reach prect->y2 */
    for (pbox = PIXREGION_BOXPTR(region), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {

        if (pbox->y2 <= y)
           continue;    /* getting up to speed or skipping remainder of band */

        if (pbox->y1 > y)
        {
           partOut = PIXMAN_REGION_STATUS_SUCCESS;      /* missed part of rectangle above */
           if (partIn || (pbox->y1 >= prect->y2))
              break;
           y = pbox->y1;        /* x guaranteed to be == prect->x1 */
        }

        if (pbox->x2 <= x)
           continue;            /* not far enough over yet */

        if (pbox->x1 > x)
        {
           partOut = PIXMAN_REGION_STATUS_SUCCESS;      /* missed part of rectangle to left */
           if (partIn)
              break;
        }

        if (pbox->x1 < prect->x2)
        {
            partIn = PIXMAN_REGION_STATUS_SUCCESS;      /* definitely overlap */
            if (partOut)
               break;
        }

        if (pbox->x2 >= prect->x2)
        {
           y = pbox->y2;        /* finished with this band */
           if (y >= prect->y2)
              break;
           x = prect->x1;       /* reset x out to left again */
        }
	else
	{
	    /*
	     * Because boxes in a band are maximal width, if the first box
	     * to overlap the rectangle doesn't completely cover it in that
	     * band, the rectangle must be partially out, since some of it
	     * will be uncovered in that band. partIn will have been set true
	     * by now...
	     */
	    partOut = PIXMAN_REGION_STATUS_SUCCESS;
	    break;
	}
    }

    return(partIn ? ((y < prect->y2) ? rgnPART : rgnIN) : rgnOUT);
}

/* pixman_region_translate (region, x, y)
   translates in place
*/

void
pixman_region_translate (pixman_region16_t * region, int x, int y)
{
    int x1, x2, y1, y2;
    int nbox;
    pixman_box16_t * pbox;

    good(region);
    region->extents.x1 = x1 = region->extents.x1 + x;
    region->extents.y1 = y1 = region->extents.y1 + y;
    region->extents.x2 = x2 = region->extents.x2 + x;
    region->extents.y2 = y2 = region->extents.y2 + y;
    if (((x1 - SHRT_MIN)|(y1 - SHRT_MIN)|(SHRT_MAX - x2)|(SHRT_MAX - y2)) >= 0)
    {
	if (region->data && (nbox = region->data->numRects))
	{
	    for (pbox = PIXREGION_BOXPTR(region); nbox--; pbox++)
	    {
		pbox->x1 += x;
		pbox->y1 += y;
		pbox->x2 += x;
		pbox->y2 += y;
	    }
	}
	return;
    }
    if (((x2 - SHRT_MIN)|(y2 - SHRT_MIN)|(SHRT_MAX - x1)|(SHRT_MAX - y1)) <= 0)
    {
	region->extents.x2 = region->extents.x1;
	region->extents.y2 = region->extents.y1;
	freeData(region);
	region->data = &pixman_region_emptyData;
	return;
    }
    if (x1 < SHRT_MIN)
	region->extents.x1 = SHRT_MIN;
    else if (x2 > SHRT_MAX)
	region->extents.x2 = SHRT_MAX;
    if (y1 < SHRT_MIN)
	region->extents.y1 = SHRT_MIN;
    else if (y2 > SHRT_MAX)
	region->extents.y2 = SHRT_MAX;
    if (region->data && (nbox = region->data->numRects))
    {
	pixman_box16_t * pboxout;

	for (pboxout = pbox = PIXREGION_BOXPTR(region); nbox--; pbox++)
	{
	    pboxout->x1 = x1 = pbox->x1 + x;
	    pboxout->y1 = y1 = pbox->y1 + y;
	    pboxout->x2 = x2 = pbox->x2 + x;
	    pboxout->y2 = y2 = pbox->y2 + y;
	    if (((x2 - SHRT_MIN)|(y2 - SHRT_MIN)|
		 (SHRT_MAX - x1)|(SHRT_MAX - y1)) <= 0)
	    {
		region->data->numRects--;
		continue;
	    }
	    if (x1 < SHRT_MIN)
		pboxout->x1 = SHRT_MIN;
	    else if (x2 > SHRT_MAX)
		pboxout->x2 = SHRT_MAX;
	    if (y1 < SHRT_MIN)
		pboxout->y1 = SHRT_MIN;
	    else if (y2 > SHRT_MAX)
		pboxout->y2 = SHRT_MAX;
	    pboxout++;
	}
	if (pboxout != pbox)
	{
	    if (region->data->numRects == 1)
	    {
		region->extents = *PIXREGION_BOXPTR(region);
		freeData(region);
		region->data = (pixman_region16_data_t *)NULL;
	    }
	    else
		pixman_set_extents(region);
	}
    }
}

/* XXX: Do we need this?
static pixman_region_status_t
pixman_region16_data_copy(pixman_region16_t * dst, pixman_region16_t * src)
{
    good(dst);
    good(src);
    if (dst->data) 
	return PIXMAN_REGION_STATUS_SUCCESS;
    if (dst == src)
	return PIXMAN_REGION_STATUS_SUCCESS;
    if (!src->data || !src->data->size)
    {
	freeData(dst);
	dst->data = (pixman_region16_data_t *)NULL;
	return PIXMAN_REGION_STATUS_SUCCESS;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	freeData(dst);
	dst->data = allocData(src->data->numRects);
	if (!dst->data)
	    return pixman_break (dst);
    }
    dst->data->size = src->data->size;
    dst->data->numRects = src->data->numRects;
    return PIXMAN_REGION_STATUS_SUCCESS;
}
*/

void
pixman_region_reset(pixman_region16_t *region, pixman_box16_t *box)
{
    good(region);
    assert(box->x1<=box->x2);
    assert(box->y1<=box->y2);
    region->extents = *box;
    freeData(region);
    region->data = (pixman_region16_data_t *)NULL;
}

int
pixman_region_contains_point(region, x, y, box)
    pixman_region16_t * region;
    int x, y;
    pixman_box16_t * box;     /* "return" value */
{
    pixman_box16_t *pbox, *pboxEnd;
    int numRects;

    good(region);
    numRects = PIXREGION_NUM_RECTS(region);
    if (!numRects || !INBOX(&region->extents, x, y))
        return(PIXMAN_REGION_STATUS_FAILURE);
    if (numRects == 1)
    {
	*box = region->extents;
	return(PIXMAN_REGION_STATUS_SUCCESS);
    }
    for (pbox = PIXREGION_BOXPTR(region), pboxEnd = pbox + numRects;
	 pbox != pboxEnd;
	 pbox++)
    {
        if (y >= pbox->y2)
	   continue;		/* not there yet */
	if ((y < pbox->y1) || (x < pbox->x1))
	   break;		/* missed it */
	if (x >= pbox->x2)
	   continue;		/* not there yet */
	*box = *pbox;
	return(PIXMAN_REGION_STATUS_SUCCESS);
    }
    return(PIXMAN_REGION_STATUS_FAILURE);
}

int
pixman_region_not_empty(region)
    pixman_region16_t * region;
{
    good(region);
    return(!PIXREGION_NIL(region));
}

/* XXX: Do we need this?
static int
pixman_region16_broken(pixman_region16_t * region)
{
    good(region);
    return (PIXREGION_NAR(region));
}
*/

void
pixman_region_empty(region)
    pixman_region16_t * region;
{
    good(region);
    freeData(region);
    region->extents.x2 = region->extents.x1;
    region->extents.y2 = region->extents.y1;
    region->data = &pixman_region_emptyData;
}

pixman_box16_t *
pixman_region_extents(region)
    pixman_region16_t * region;
{
    good(region);
    return(&region->extents);
}

#define ExchangeSpans(a, b)				    \
{							    \
    pixman_region16_point_t tpt;					    \
    int    tw;						    \
							    \
    tpt = spans[a]; spans[a] = spans[b]; spans[b] = tpt;    \
    tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;  \
}

/* ||| I should apply the merge sort code to rectangle sorting above, and see
   if mapping time can be improved.  But right now I've been at work 12 hours,
   so forget it.
*/

static void QuickSortSpans(
    pixman_region16_point_t spans[],
    int	    widths[],
    int	    numSpans)
{
    int	    y;
    int	    i, j, m;
    pixman_region16_point_t *r;

    /* Always called with numSpans > 1 */
    /* Sorts only by y, doesn't bother to sort by x */

    do
    {
	if (numSpans < 9)
	{
	    /* Do insertion sort */
	    int yprev;

	    yprev = spans[0].y;
	    i = 1;
	    do
	    { /* while i != numSpans */
		y = spans[i].y;
		if (yprev > y)
		{
		    /* spans[i] is out of order.  Move into proper location. */
		    pixman_region16_point_t tpt;
		    int	    tw, k;

		    for (j = 0; y >= spans[j].y; j++) {}
		    tpt = spans[i];
		    tw  = widths[i];
		    for (k = i; k != j; k--)
		    {
			spans[k] = spans[k-1];
			widths[k] = widths[k-1];
		    }
		    spans[j] = tpt;
		    widths[j] = tw;
		    y = spans[i].y;
		} /* if out of order */
		yprev = y;
		i++;
	    } while (i != numSpans);
	    return;
	}

	/* Choose partition element, stick in location 0 */
	m = numSpans / 2;
	if (spans[m].y > spans[0].y)		ExchangeSpans(m, 0);
	if (spans[m].y > spans[numSpans-1].y)   ExchangeSpans(m, numSpans-1);
	if (spans[m].y > spans[0].y)		ExchangeSpans(m, 0);
	y = spans[0].y;

        /* Partition array */
        i = 0;
        j = numSpans;
        do
	{
	    r = &(spans[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numSpans && r->y < y);
	    r = &(spans[j]);
	    do
	    {
		r--;
		j--;
            } while (y < r->y);
            if (i < j)
		ExchangeSpans(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeSpans(0, j);

	/* Recurse */
        if (numSpans-j-1 > 1)
	    QuickSortSpans(&spans[j+1], &widths[j+1], numSpans-j-1);
        numSpans = j;
    } while (numSpans > 1);
}

#define NextBand()						    \
{								    \
    clipy1 = pboxBandStart->y1;					    \
    clipy2 = pboxBandStart->y2;					    \
    pboxBandEnd = pboxBandStart + 1;				    \
    while (pboxBandEnd != pboxLast && pboxBandEnd->y1 == clipy1) {  \
	pboxBandEnd++;						    \
    }								    \
    for (; ppt != pptLast && ppt->y < clipy1; ppt++, pwidth++) {} \
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

#ifdef XXX_DO_WE_NEED_THIS
static int
pixman_region16_clip_spans(
    pixman_region16_t 		*prgnDst,
    pixman_region16_point_t 	*ppt,
    int	    		*pwidth,
    int			nspans,
    pixman_region16_point_t 	*pptNew,
    int			*pwidthNew,
    int			fSorted)
{
    pixman_region16_point_t 	*pptLast;
    int			*pwidthNewStart;	/* the vengeance of Xerox! */
    int	y, x1, x2;
    int	numRects;

    good(prgnDst);
    pptLast = ppt + nspans;
    pwidthNewStart = pwidthNew;

    if (!prgnDst->data)
    {
	/* Do special fast code with clip boundaries in registers(?) */
	/* It doesn't pay much to make use of fSorted in this case, 
	   so we lump everything together. */

	   int clipx1, clipx2, clipy1, clipy2;

	clipx1 = prgnDst->extents.x1;
	clipy1 = prgnDst->extents.y1;
	clipx2 = prgnDst->extents.x2;
	clipy2 = prgnDst->extents.y2;
	    
	for (; ppt != pptLast; ppt++, pwidth++)
	{
	    y = ppt->y;
	    x1 = ppt->x;
	    if (clipy1 <= y && y < clipy2)
	    {
		x2 = x1 + *pwidth;
		if (x1 < clipx1)    x1 = clipx1;
		if (x2 > clipx2)    x2 = clipx2;
		if (x1 < x2)
		{
		    /* part of span in clip rectangle */
		    pptNew->x = x1;
		    pptNew->y = y;
		    *pwidthNew = x2 - x1;
		    pptNew++;
		    pwidthNew++;
		}
	    }
	} /* end for */

    }
    else if ((numRects = prgnDst->data->numRects))
    {
	/* Have to clip against many boxes */
	pixman_box16_t *pboxBandStart, *pboxBandEnd;
	pixman_box16_t *pbox;
	pixman_box16_t *pboxLast;
	int	clipy1, clipy2;

	/* In this case, taking advantage of sorted spans gains more than
	   the sorting costs. */
	if ((! fSorted) && (nspans > 1))
	    QuickSortSpans(ppt, pwidth, nspans);

	pboxBandStart = PIXREGION_BOXPTR(prgnDst);
	pboxLast = pboxBandStart + numRects;
    
	NextBand();

	for (; ppt != pptLast; )
	{
	    y = ppt->y;
	    if (y < clipy2)
	    {
		/* span is in the current band */
		pbox = pboxBandStart;
		x1 = ppt->x;
		x2 = x1 + *pwidth;
		do
		{ /* For each box in band */
		    int    newx1, newx2;

		    newx1 = x1;
		    newx2 = x2;
		    if (newx1 < pbox->x1)   newx1 = pbox->x1;
		    if (newx2 > pbox->x2)   newx2 = pbox->x2;
		    if (newx1 < newx2)
		    {
			/* Part of span in clip rectangle */
			pptNew->x = newx1;
			pptNew->y = y;
			*pwidthNew = newx2 - newx1;
			pptNew++;
			pwidthNew++;
		    }
		    pbox++;
		} while (pbox != pboxBandEnd);
		ppt++;
		pwidth++;
	    }
	    else
	    {
		/* Move to next band, adjust ppt as needed */
		pboxBandStart = pboxBandEnd;
		if (pboxBandStart == pboxLast)
		    break; /* We're completely done */
		NextBand();
	    }
	}
    }
    return (pwidthNew - pwidthNewStart);
}

/* find the band in a region with the most rectangles */
static int
pixman_region16_find_max_band(pixman_region16_t * prgn)
{
    int nbox;
    pixman_box16_t * pbox;
    int nThisBand;
    int nMaxBand = 0;
    short yThisBand;

    good(prgn);
    nbox = PIXREGION_NUM_RECTS(prgn);
    pbox = PIXREGION_RECTS(prgn);

    while(nbox > 0)
    {
	yThisBand = pbox->y1;
	nThisBand = 0;
	while((nbox > 0) && (pbox->y1 == yThisBand))
	{
	    nbox--;
	    pbox++;
	    nThisBand++;
	}
	if (nThisBand > nMaxBand)
	    nMaxBand = nThisBand;
    }
    return (nMaxBand);
}
#endif /* XXX_DO_WE_NEED_THIS */
