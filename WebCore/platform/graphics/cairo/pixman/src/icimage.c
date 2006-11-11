/*
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

#include "pixman-xserver-compat.h"

pixman_image_t *
pixman_image_create (pixman_format_t	*format,
	       int	width,
	       int	height)
{
    pixman_image_t	*image;
    FbPixels	*pixels;

    pixels = FbPixelsCreate (width, height, format->depth);
    if (pixels == NULL)
	return NULL;
    
    image = pixman_image_createForPixels (pixels, format);
    if (image == NULL) {
	FbPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}
slim_hidden_def(pixman_image_create);

pixman_image_t *
pixman_image_create_for_data (FbBits *data, pixman_format_t *format, int width, int height, int bpp, int stride)
{
    pixman_image_t	*image;
    FbPixels	*pixels;

    pixels = FbPixelsCreateForData (data, width, height, format->depth, bpp, stride);
    if (pixels == NULL)
	return NULL;

    image = pixman_image_createForPixels (pixels, format);
    if (image == NULL) {
	FbPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}

pixman_image_t *
pixman_image_createForPixels (FbPixels	*pixels,
			pixman_format_t	*format)
{
    pixman_image_t		*image;

    image = malloc (sizeof (pixman_image_t));
    if (!image)
    {
	return NULL;
    }

    image->pixels = pixels;
    image->image_format = *format;
    image->format_code = format->format_code;
/* XXX: What's all this about?
    if (pDrawable->type == DRAWABLE_PIXMAP)
    {
	++((PixmapPtr)pDrawable)->refcnt;
	image->pNext = 0;
    }
    else
    {
	image->pNext = GetPictureWindow(((WindowPtr) pDrawable));
	SetPictureWindow(((WindowPtr) pDrawable), image);
    }
*/

    pixman_image_init (image);

    return image;
}

static CARD32 xRenderColorToCard32(pixman_color_t c)
{
    return
        (c.alpha >> 8 << 24) |
        (c.red >> 8 << 16) |
        (c.green & 0xff00) |
        (c.blue >> 8);
}

static uint32_t premultiply(uint32_t x)
{
    uint32_t a = x >> 24;
    uint32_t t = (x & 0xff00ff) * a + 0x800080;
    t = (t + ((t >> 8) & 0xff00ff)) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff) * a + 0x80;
    x = (x + ((x >> 8) & 0xff));
    x &= 0xff00;
    x |= t | (a << 24);
    return x;
}

static uint32_t INTERPOLATE_PIXEL_256(uint32_t x, uint32_t a, uint32_t y, uint32_t b)
{
    CARD32 t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t >>= 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x &= 0xff00ff00;
    x |= t;
    return x;
}

uint32_t
pixman_gradient_color (pixman_gradient_stop_t *stop1,
		       pixman_gradient_stop_t *stop2,
		       uint32_t		      x)
{
    uint32_t current_color, next_color;
    int	     dist, idist;

    current_color = xRenderColorToCard32 (stop1->color);
    next_color    = xRenderColorToCard32 (stop2->color);

    dist  = (int) (256 * (x - stop1->x) / (stop2->x - stop1->x));
    idist = 256 - dist;

    return premultiply (INTERPOLATE_PIXEL_256 (current_color, idist,
					       next_color, dist));
}

static int
pixman_init_gradient_color_table (pixman_gradient_image_t *gradient,
				  int			  tableSize)
{
    int begin_pos, end_pos;
    xFixed incr, dpos;
    int pos, current_stop;
    pixman_gradient_stop_t *stops = gradient->stops;
    int nstops = gradient->nstops;

    if (gradient->colorTableSize < tableSize)
    {
	uint32_t *newColorTable;

	newColorTable = realloc (gradient->colorTable,
				 tableSize * sizeof (uint32_t));
	if (!newColorTable)
	    return 1;

	gradient->colorTable = newColorTable;
	gradient->colorTableSize = tableSize;
    }

    gradient->stopRange = tableSize;

    /* The position where the gradient begins and ends */
    begin_pos = (stops[0].x * gradient->colorTableSize) >> 16;
    end_pos = (stops[nstops - 1].x * gradient->colorTableSize) >> 16;

    pos = 0; /* The position in the color table. */

    /* Up to first point */
    while (pos <= begin_pos) {
	gradient->colorTable[pos] = xRenderColorToCard32(stops[0].color);
        ++pos;
    }

    incr =  (1<<16)/ gradient->colorTableSize; /* the double increment. */
    dpos = incr * pos; /* The position in terms of 0-1. */

    current_stop = 0; /* We always interpolate between current and current + 1. */

    /* Gradient area */
    while (pos < end_pos) {
	gradient->colorTable[pos] =
	    pixman_gradient_color (&stops[current_stop],
				   &stops[current_stop + 1],
				   dpos);

        ++pos;
        dpos += incr;

        if (dpos > stops[current_stop + 1].x)
            ++current_stop;
    }

    /* After last point */
    while (pos < gradient->colorTableSize) {
	gradient->colorTable[pos] =
	    xRenderColorToCard32 (stops[nstops - 1].color);
        ++pos;
    }

    return 0;
}

static int
_pixman_init_gradient (pixman_gradient_image_t	    *gradient,
		       const pixman_gradient_stop_t *stops,
		       int			    n_stops)
{
    pixman_fixed16_16_t dpos;
    int			i;

    if (n_stops <= 0)
	return 1;

    dpos = -1;
    for (i = 0; i < n_stops; i++)
    {
	if (stops[i].x < dpos || stops[i].x > (1 << 16))
	    return 1;

	dpos = stops[i].x;
    }

    gradient->class	     = SourcePictClassUnknown;
    gradient->stopRange	     = 0xffff;
    gradient->colorTable     = NULL;
    gradient->colorTableSize = 0;

    return 0;
}

static pixman_image_t *
_pixman_create_source_image (void)
{
    pixman_image_t *image;

    image = (pixman_image_t *) malloc (sizeof (pixman_image_t));
    image->pDrawable   = 0;
    image->pixels      = 0;
    image->format_code = PICT_a8r8g8b8;

    pixman_image_init (image);

    return image;
}

pixman_image_t *
pixman_image_create_linear_gradient (const pixman_linear_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops)
{
    pixman_linear_gradient_image_t *linear;
    pixman_image_t		   *image;

    if (n_stops < 2)
	return 0;

    image = _pixman_create_source_image ();
    if (!image)
	return 0;

    linear = malloc (sizeof (pixman_linear_gradient_image_t) +
		     sizeof (pixman_gradient_stop_t) * n_stops);
    if (!linear)
    {
	free (image);
	return 0;
    }

    linear->stops  = (pixman_gradient_stop_t *) (linear + 1);
    linear->nstops = n_stops;

    memcpy (linear->stops, stops, sizeof (pixman_gradient_stop_t) * n_stops);

    linear->type = SourcePictTypeLinear;
    linear->p1   = gradient->p1;
    linear->p2   = gradient->p2;

    image->pSourcePict = (pixman_source_image_t *) linear;

    if (_pixman_init_gradient (&image->pSourcePict->gradient, stops, n_stops))
    {
	free (image);
	return 0;
    }

    return image;
}

pixman_image_t *
pixman_image_create_radial_gradient (const pixman_radial_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops)
{
    pixman_radial_gradient_image_t *radial;
    pixman_image_t		   *image;
    double			   x;

    if (n_stops < 2)
	return 0;

    image = _pixman_create_source_image ();
    if (!image)
	return 0;

    radial = malloc (sizeof (pixman_radial_gradient_image_t) +
		     sizeof (pixman_gradient_stop_t) * n_stops);
    if (!radial)
    {
	free (image);
	return 0;
    }

    radial->stops  = (pixman_gradient_stop_t *) (radial + 1);
    radial->nstops = n_stops;

    memcpy (radial->stops, stops, sizeof (pixman_gradient_stop_t) * n_stops);

    radial->type = SourcePictTypeRadial;
    x = (double) gradient->inner.radius / (double) gradient->outer.radius;
    radial->dx = (gradient->outer.x - gradient->inner.x);
    radial->dy = (gradient->outer.y - gradient->inner.y);
    radial->fx = (gradient->inner.x) - x * radial->dx;
    radial->fy = (gradient->inner.y) - x * radial->dy;
    radial->m = 1. / (1 + x);
    radial->b = -x * radial->m;
    radial->dx /= 65536.;
    radial->dy /= 65536.;
    radial->fx /= 65536.;
    radial->fy /= 65536.;
    x = gradient->outer.radius / 65536.;
    radial->a = x * x - radial->dx * radial->dx - radial->dy * radial->dy;

    image->pSourcePict = (pixman_source_image_t *) radial;

    if (_pixman_init_gradient (&image->pSourcePict->gradient, stops, n_stops))
    {
	free (image);
	return 0;
    }

    return image;
}

void
pixman_image_init (pixman_image_t *image)
{
    image->refcnt = 1;
    image->repeat = PIXMAN_REPEAT_NONE;
    image->graphicsExposures = 0;
    image->subWindowMode = ClipByChildren;
    image->polyEdge = PolyEdgeSharp;
    image->polyMode = PolyModePrecise;
    /* 
     * In the server this was 0 because the composite clip list
     * can be referenced from a window (and often is)
     */
    image->freeCompClip = 0;
    image->freeSourceClip = 0;
    image->clientClipType = CT_NONE;
    image->componentAlpha = 0;
    image->compositeClipSource = 0;

    image->alphaMap = NULL;
    image->alphaOrigin.x = 0;
    image->alphaOrigin.y = 0;

    image->clipOrigin.x = 0;
    image->clipOrigin.y = 0;
    image->clientClip = NULL;

    image->dither = 0L;

    image->stateChanges = (1 << (CPLastBit+1)) - 1;
/* XXX: What to lodge here?
    image->serialNumber = GC_CHANGE_SERIAL_BIT;
*/

    if (image->pixels)
    {
	image->pCompositeClip = pixman_region_create();
	pixman_region_union_rect (image->pCompositeClip, image->pCompositeClip,
				  0, 0, image->pixels->width,
				  image->pixels->height);
	image->freeCompClip = 1;

	image->pSourceClip = pixman_region_create ();
	pixman_region_union_rect (image->pSourceClip, image->pSourceClip,
				  0, 0, image->pixels->width,
				  image->pixels->height);
	image->freeSourceClip = 1;
    }
    else
    {
	image->pCompositeClip = NULL;
	image->pSourceClip    = NULL;
    }

    image->transform = NULL;

    image->filter = PIXMAN_FILTER_NEAREST;
    image->filter_params = NULL;
    image->filter_nparams = 0;


    image->owns_pixels = 0;

    image->pSourcePict = NULL;
}

void
pixman_image_set_component_alpha (pixman_image_t	*image,
				  int		component_alpha)
{
    if (image)
	image->componentAlpha = component_alpha;
}
slim_hidden_def(pixman_image_set_component_alpha);

int
pixman_image_set_transform (pixman_image_t		*image,
		     pixman_transform_t	*transform)
{
    static const pixman_transform_t	identity = { {
	{ xFixed1, 0x00000, 0x00000 },
	{ 0x00000, xFixed1, 0x00000 },
	{ 0x00000, 0x00000, xFixed1 },
    } };

    if (transform && memcmp (transform, &identity, sizeof (pixman_transform_t)) == 0)
	transform = NULL;
    
    if (transform)
    {
	if (!image->transform)
	{
	    image->transform = malloc (sizeof (pixman_transform_t));
	    if (!image->transform)
		return 1;
	}
	*image->transform = *transform;
    }
    else
    {
	if (image->transform)
	{
	    free (image->transform);
	    image->transform = NULL;
	}
    }
    return 0;
}

void
pixman_image_set_repeat (pixman_image_t		*image,
			 pixman_repeat_t	repeat)
{
    if (image)
	image->repeat = repeat;
}
slim_hidden_def(pixman_image_set_repeat);

void
pixman_image_set_filter (pixman_image_t	*image,
		  pixman_filter_t	filter)
{
    if (image)
	image->filter = filter;
}

int
pixman_image_get_width (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->width;

    return 0;
}

int
pixman_image_get_height (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->height;

    return 0;
}

int
pixman_image_get_depth (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->depth;

    return 0;
}

int
pixman_image_get_stride (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->stride;

    return 0;
}

pixman_format_t *
pixman_image_get_format (pixman_image_t	*image)
{
    return &image->image_format;
}

FbBits *
pixman_image_get_data (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->data;

    return 0;
}

void
pixman_image_destroy (pixman_image_t *image)
{
    pixman_image_destroyClip (image);

    if (image->freeCompClip) {
	pixman_region_destroy (image->pCompositeClip);
	image->pCompositeClip = NULL;
    }
    
    if (image->freeSourceClip) {
	pixman_region_destroy (image->pSourceClip);
	image->pSourceClip = NULL;
    }

    if (image->owns_pixels) {
	FbPixelsDestroy (image->pixels);
	image->pixels = NULL;
    }

    if (image->transform) {
	free (image->transform);
	image->transform = NULL;
    }

    if (image->pSourcePict) {
	free (image->pSourcePict);
	image->pSourcePict = NULL;
    }

    free (image);
}
slim_hidden_def(pixman_image_destroy);

void
pixman_image_destroyClip (pixman_image_t *image)
{
    switch (image->clientClipType) {
    case CT_NONE:
	return;
    case CT_PIXMAP:
	pixman_image_destroy (image->clientClip);
	break;
    default:
	pixman_region_destroy (image->clientClip);
	break;
    }
    image->clientClip = NULL;
    image->clientClipType = CT_NONE;
}    

int
pixman_image_set_clip_region (pixman_image_t	*image,
			      pixman_region16_t	*region)
{
    pixman_image_destroyClip (image);
    if (region) {
	image->clientClip = pixman_region_create ();
	pixman_region_copy (image->clientClip, region);
	image->clientClipType = CT_REGION;
    }

    image->stateChanges |= CPClipMask;
    if (image->pSourcePict)
	return 0;
    
    if (image->freeCompClip)
	pixman_region_destroy (image->pCompositeClip);
    image->pCompositeClip = pixman_region_create();
    pixman_region_union_rect (image->pCompositeClip, image->pCompositeClip,
			      0, 0, image->pixels->width, image->pixels->height);
    image->freeCompClip = 1;
    if (region) {
	pixman_region_translate (image->pCompositeClip,
				 - image->clipOrigin.x,
				 - image->clipOrigin.y);
	pixman_region_intersect (image->pCompositeClip,
				 image->pCompositeClip,
				 region);
	pixman_region_translate (image->pCompositeClip,
				 image->clipOrigin.x,
				 image->clipOrigin.y);
    }
    
    return 0;
}

#define BOUND(v)	(int16_t) ((v) < MINSHORT ? MINSHORT : (v) > MAXSHORT ? MAXSHORT : (v))

static __inline int
FbClipImageReg (pixman_region16_t	*region,
		pixman_region16_t	*clip,
		int		dx,
		int		dy)
{
    if (pixman_region_num_rects (region) == 1 &&
	pixman_region_num_rects (clip) == 1)
    {
	pixman_box16_t *pRbox = pixman_region_rects (region);
	pixman_box16_t *pCbox = pixman_region_rects (clip);
	int	v;

	if (pRbox->x1 < (v = pCbox->x1 + dx))
	    pRbox->x1 = BOUND(v);
	if (pRbox->x2 > (v = pCbox->x2 + dx))
	    pRbox->x2 = BOUND(v);
	if (pRbox->y1 < (v = pCbox->y1 + dy))
	    pRbox->y1 = BOUND(v);
	if (pRbox->y2 > (v = pCbox->y2 + dy))
	    pRbox->y2 = BOUND(v);
	if (pRbox->x1 >= pRbox->x2 ||
	    pRbox->y1 >= pRbox->y2)
	{
	    pixman_region_empty (region);
	}
    }
    else
    {
	pixman_region_translate (region, dx, dy);
	pixman_region_intersect (region, clip, region);
	pixman_region_translate (region, -dx, -dy);
    }
    return 1;
}
		  
static __inline int
FbClipImageSrc (pixman_region16_t	*region,
		pixman_image_t		*image,
		int		dx,
		int		dy)
{
    /* XXX what to do with clipping from transformed pictures? */
    if (image->transform)
	return 1;
    /* XXX davidr hates this, wants to never use source-based clipping */
    if (image->repeat != PIXMAN_REPEAT_NONE || image->pSourcePict)
    {
	/* XXX no source clipping */
	if (image->compositeClipSource &&
	    image->clientClipType != CT_NONE)
	{
	    pixman_region_translate (region, 
			   dx - image->clipOrigin.x,
			   dy - image->clipOrigin.y);
	    pixman_region_intersect (region, image->clientClip, region);
	    pixman_region_translate (region, 
			   - (dx - image->clipOrigin.x),
			   - (dy - image->clipOrigin.y));
	}
	return 1;
    }
    else
    {
	pixman_region16_t   *clip;

	if (image->compositeClipSource)
	    clip = image->pCompositeClip;
	else
	    clip = image->pSourceClip;
	return FbClipImageReg (region,
			       clip,
			       dx,
			       dy);
    }
    return 1;
}

/* XXX: Need to decide what to do with this
#define NEXT_VAL(_type) (vlist ? (_type) *vlist++ : (_type) ulist++->val)

#define NEXT_PTR(_type) ((_type) ulist++->ptr)

int
pixman_image_change (pixman_image_t		*image,
	       Mask		vmask,
	       unsigned int	*vlist,
	       DevUnion		*ulist,
	       int		*error_value)
{
    BITS32		index2;
    int			error = 0;
    BITS32		maskQ;
    
    maskQ = vmask;
    while (vmask && !error)
    {
	index2 = (BITS32) lowbit (vmask);
	vmask &= ~index2;
	image->stateChanges |= index2;
	switch (index2)
	{
	case CPRepeat:
	    {
		unsigned int	newr;
		newr = NEXT_VAL(unsigned int);
		if (newr <= xTrue)
		    image->repeat = newr;
		else
		{
		    *error_value = newr;
		    error = BadValue;
		}
	    }
	    break;
	case CPAlphaMap:
	    {
		pixman_image_t *iAlpha;
		
		iAlpha = NEXT_PTR(pixman_image_t *);
		if (iAlpha)
		    iAlpha->refcnt++;
		if (image->alphaMap)
		    pixman_image_destroy ((void *) image->alphaMap);
		image->alphaMap = iAlpha;
	    }
	    break;
	case CPAlphaXOrigin:
	    image->alphaOrigin.x = NEXT_VAL(int16_t);
	    break;
	case CPAlphaYOrigin:
	    image->alphaOrigin.y = NEXT_VAL(int16_t);
	    break;
	case CPClipXOrigin:
	    image->clipOrigin.x = NEXT_VAL(int16_t);
	    break;
	case CPClipYOrigin:
	    image->clipOrigin.y = NEXT_VAL(int16_t);
	    break;
	case CPClipMask:
	    {
		pixman_image_t	    *mask;
		int	    clipType;

		mask = NEXT_PTR(pixman_image_t *);
		if (mask) {
		    clipType = CT_PIXMAP;
		    mask->refcnt++;
		} else {
		    clipType = CT_NONE;
		}
		error = pixman_image_change_clip (image, clipType,
					   (void *)mask, 0);
		break;
	    }
	case CPGraphicsExposure:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe <= xTrue)
		    image->graphicsExposures = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPSubwindowMode:
	    {
		unsigned int	news;
		news = NEXT_VAL(unsigned int);
		if (news == ClipByChildren || news == IncludeInferiors)
		    image->subWindowMode = news;
		else
		{
		    *error_value = news;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyEdge:
	    {
		unsigned int	newe;
		newe = NEXT_VAL(unsigned int);
		if (newe == PolyEdgeSharp || newe == PolyEdgeSmooth)
		    image->polyEdge = newe;
		else
		{
		    *error_value = newe;
		    error = BadValue;
		}
	    }
	    break;
	case CPPolyMode:
	    {
		unsigned int	newm;
		newm = NEXT_VAL(unsigned int);
		if (newm == PolyModePrecise || newm == PolyModeImprecise)
		    image->polyMode = newm;
		else
		{
		    *error_value = newm;
		    error = BadValue;
		}
	    }
	    break;
	case CPDither:
	    image->dither = NEXT_VAL(unsigned long);
	    break;
	case CPComponentAlpha:
	    {
		unsigned int	newca;

		newca = NEXT_VAL (unsigned int);
		if (newca <= xTrue)
		    image->componentAlpha = newca;
		else
		{
		    *error_value = newca;
		    error = BadValue;
		}
	    }
	    break;
	default:
	    *error_value = maskQ;
	    error = BadValue;
	    break;
	}
    }
    return error;
}
*/

/* XXX: Do we need this?
int
SetPictureClipRects (PicturePtr	pPicture,
		     int	xOrigin,
		     int	yOrigin,
		     int	nRect,
		     xRectangle	*rects)
{
    ScreenPtr		pScreen = pPicture->pDrawable->pScreen;
    PictureScreenPtr	ps = GetPictureScreen(pScreen);
    pixman_region16_t		*clientClip;
    int			result;

    clientClip = RECTS_TO_REGION(pScreen,
				 nRect, rects, CT_UNSORTED);
    if (!clientClip)
	return 1;
    result =(*ps->ChangePictureClip) (pPicture, CT_REGION, 
				      (void *) clientClip, 0);
    if (result == 0)
    {
	pPicture->clipOrigin.x = xOrigin;
	pPicture->clipOrigin.y = yOrigin;
	pPicture->stateChanges |= CPClipXOrigin|CPClipYOrigin|CPClipMask;
	pPicture->serialNumber |= GC_CHANGE_SERIAL_BIT;
    }
    return result;
}
*/

int
FbComputeCompositeRegion (pixman_region16_t	*region,
			  pixman_image_t	*iSrc,
			  pixman_image_t	*iMask,
			  pixman_image_t	*iDst,
			  int16_t		xSrc,
			  int16_t		ySrc,
			  int16_t		xMask,
			  int16_t		yMask,
			  int16_t		xDst,
			  int16_t		yDst,
			  uint16_t	width,
			  uint16_t	height)
{
    int		v;
    int x1, y1, x2, y2;

    /* XXX: This code previously directly set the extents of the
       region here. I need to decide whether removing that has broken
       this. Also, it might be necessary to just make the pixman_region16_t
       data structure transparent anyway in which case I can just put
       the code back. */
    x1 = xDst;
    v = xDst + width;
    x2 = BOUND(v);
    y1 = yDst;
    v = yDst + height;
    y2 = BOUND(v);
    /* Check for empty operation */
    if (x1 >= x2 ||
	y1 >= y2)
    {
	pixman_region_empty (region);
	return 1;
    }
    /* clip against src */
    if (!FbClipImageSrc (region, iSrc, xDst - xSrc, yDst - ySrc))
    {
	pixman_region_destroy (region);
	return 0;
    }
    if (iSrc->alphaMap)
    {
	if (!FbClipImageSrc (region, iSrc->alphaMap,
			     xDst - (xSrc + iSrc->alphaOrigin.x),
			     yDst - (ySrc + iSrc->alphaOrigin.y)))
	{
	    pixman_region_destroy (region);
	    return 0;
	}
    }
    /* clip against mask */
    if (iMask)
    {
	if (!FbClipImageSrc (region, iMask, xDst - xMask, yDst - yMask))
	{
	    pixman_region_destroy (region);
	    return 0;
	}	
	if (iMask->alphaMap)
	{
	    if (!FbClipImageSrc (region, iMask->alphaMap,
				 xDst - (xMask + iMask->alphaOrigin.x),
				 yDst - (yMask + iMask->alphaOrigin.y)))
	    {
		pixman_region_destroy (region);
		return 0;
	    }
	}
    }
    if (!FbClipImageReg (region, iDst->pCompositeClip, 0, 0))
    {
	pixman_region_destroy (region);
	return 0;
    }
    if (iDst->alphaMap)
    {
	if (!FbClipImageReg (region, iDst->alphaMap->pCompositeClip,
			     -iDst->alphaOrigin.x,
			     -iDst->alphaOrigin.y))
	{
	    pixman_region_destroy (region);
	    return 0;
	}
    }
    return 1;
}

int
miIsSolidAlpha (pixman_image_t *src)
{
    char	line[1];
    
    /* Alpha-only */
    if (PICT_FORMAT_TYPE (src->format_code) != PICT_TYPE_A)
	return 0;
    /* repeat */
    if (!src->repeat)
	return 0;
    /* 1x1 */
    if (src->pixels->width != 1 || src->pixels->height != 1)
	return 0;
    line[0] = 1;
    /* XXX: For the next line, fb has:
	(*pScreen->GetImage) (src->pixels, 0, 0, 1, 1, ZPixmap, ~0L, line);
       Is the following simple assignment sufficient?
    */
    line[0] = src->pixels->data[0];
    switch (src->pixels->bpp) {
    case 1:
	return (uint8_t) line[0] == 1 || (uint8_t) line[0] == 0x80;
    case 4:
	return (uint8_t) line[0] == 0xf || (uint8_t) line[0] == 0xf0;
    case 8:
	return (uint8_t) line[0] == 0xff;
    default:
	return 0;
    }
}
