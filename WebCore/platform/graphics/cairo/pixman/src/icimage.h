/*
 * Copyright © 2000 Keith Packard
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

/* XXX: This whole file should be moved up into incint.h (and cleaned
   up considerably as well) */

#ifndef _ICIMAGE_H_
#define _ICIMAGE_H_

/* #include "glyphstr.h" */
/* #include "scrnintstr.h" */

/* XXX: Hmmm... what's needed from here?
#include "resource.h"
*/

/*
typedef struct _IndexFormat {
    VisualPtr	    pVisual; 
    ColormapPtr	    pColormap;
    int		    nvalues;
    xIndexValue	    *pValues;
    void	    *devPrivate;
} IndexFormatRec;
*/

/*
typedef struct pixman_format {
    uint32_t	    id;
    uint32_t	    format;
    unsigned char   type;
    unsigned char   depth;
    DirectFormatRec direct;
    IndexFormatRec  index;
} pixman_format_t;
*/

#define PICT_GRADIENT_STOPTABLE_SIZE 1024

#define SourcePictTypeSolidFill 0
#define SourcePictTypeLinear    1
#define SourcePictTypeRadial    2
#define SourcePictTypeConical   3

#define SourcePictClassUnknown    0
#define SourcePictClassHorizontal 1
#define SourcePictClassVertical   2

typedef struct _pixman_solid_fill_image {
    unsigned int type;
    unsigned int class;
    uint32_t	 color;
} pixman_solid_fill_image_t;

typedef struct _pixman_gradient_image {
    unsigned int	   type;
    unsigned int	   class;
    pixman_gradient_stop_t *stops;
    int			   nstops;
    int			   stopRange;
    uint32_t		   *colorTable;
    int			   colorTableSize;
} pixman_gradient_image_t;

typedef struct _pixman_linear_gradient_image {
    unsigned int	   type;
    unsigned int	   class;
    pixman_gradient_stop_t *stops;
    int			   nstops;
    int			   stopRange;
    uint32_t		   *colorTable;
    int			   colorTableSize;
    pixman_point_fixed_t   p1;
    pixman_point_fixed_t   p2;
} pixman_linear_gradient_image_t;

typedef struct _pixman_radial_gradient_image {
    unsigned int	   type;
    unsigned int	   class;
    pixman_gradient_stop_t *stops;
    int			   nstops;
    int			   stopRange;
    uint32_t		   *colorTable;
    int			   colorTableSize;
    double		   fx;
    double		   fy;
    double		   dx;
    double		   dy;
    double		   a;
    double		   m;
    double		   b;
} pixman_radial_gradient_image_t;

typedef struct _pixman_conical_gradient_image {
    unsigned int	   type;
    unsigned int	   class;
    pixman_gradient_stop_t *stops;
    int			   nstops;
    int			   stopRange;
    uint32_t		   *colorTable;
    int			   colorTableSize;
    pixman_point_fixed_t   center;
    pixman_fixed16_16_t	   angle;
} pixman_conical_gradient_image_t;

typedef union _pixman_source_image {
    unsigned int		    type;
    pixman_solid_fill_image_t	    solidFill;
    pixman_gradient_image_t	    gradient;
    pixman_linear_gradient_image_t  linear;
    pixman_radial_gradient_image_t  radial;
    pixman_conical_gradient_image_t conical;
} pixman_source_image_t;

typedef pixman_source_image_t *SourcePictPtr;

struct pixman_image {
    FbPixels	    *pixels;
    pixman_format_t	    image_format;
    int		    format_code;
    int		    refcnt;
    
    unsigned int    repeat : 2;
    unsigned int    graphicsExposures : 1;
    unsigned int    subWindowMode : 1;
    unsigned int    polyEdge : 1;
    unsigned int    polyMode : 1;
    unsigned int    freeCompClip : 1;
    unsigned int    freeSourceClip : 1;
    unsigned int    clientClipType : 2;
    unsigned int    componentAlpha : 1;
    unsigned int    compositeClipSource : 1;
    unsigned int    unused : 20;

    struct pixman_image *alphaMap;
    FbPoint	    alphaOrigin;

    FbPoint 	    clipOrigin;
    void	   *clientClip;

    unsigned long   dither;

    unsigned long   stateChanges;
    unsigned long   serialNumber;

    pixman_region16_t	    *pCompositeClip;
    pixman_region16_t	    *pSourceClip;
    
    pixman_transform_t     *transform;

    pixman_filter_t	    filter;
    pixman_fixed16_16_t    *filter_params;
    int		    filter_nparams;

    int		    owns_pixels;

    pixman_source_image_t *pSourcePict;
};

#endif /* _ICIMAGE_H_ */

#ifndef _IC_MIPICT_H_
#define _IC_MIPICT_H_

#define IC_MAX_INDEXED	256 /* XXX depth must be <= 8 */

#if IC_MAX_INDEXED <= 256
typedef uint8_t FbIndexType;
#endif

/* XXX: We're not supporting indexed operations, right? */
typedef struct _FbIndexed {
    int	color;
    uint32_t	rgba[IC_MAX_INDEXED];
    FbIndexType	ent[32768];
} FbIndexedRec, *FbIndexedPtr;

#define FbCvtR8G8B8to15(s) ((((s) >> 3) & 0x001f) | \
			     (((s) >> 6) & 0x03e0) | \
			     (((s) >> 9) & 0x7c00))
#define FbIndexToEnt15(icf,rgb15) ((icf)->ent[rgb15])
#define FbIndexToEnt24(icf,rgb24) FbIndexToEnt15(icf,FbCvtR8G8B8to15(rgb24))

#define FbIndexToEntY24(icf,rgb24) ((icf)->ent[CvtR8G8B8toY15(rgb24)])

/*
pixman_private int
FbCreatePicture (PicturePtr pPicture);
*/

pixman_private void
pixman_image_init (pixman_image_t *image);

pixman_private void
pixman_image_destroyClip (pixman_image_t *image);

/*
pixman_private void
FbValidatePicture (PicturePtr pPicture,
		   Mask       mask);
*/


/* XXX: What should this be?
pixman_private int
FbClipPicture (pixman_region16_t    *region,
	       pixman_image_t	    *image,
	       int16_t	    xReg,
	       int16_t	    yReg,
	       int16_t	    xPict,
	       int16_t	    yPict);
*/

pixman_private int
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
			  uint16_t	height);

int
miIsSolidAlpha (pixman_image_t *src);

/*
pixman_private int
FbPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);
*/

/*
pixman_private void
FbGlyphs (pixman_operator_t	op,
	  PicturePtr	pSrc,
	  PicturePtr	pDst,
	  PictFormatPtr	maskFormat,
	  int16_t		xSrc,
	  int16_t		ySrc,
	  int		nlist,
	  GlyphListPtr	list,
	  GlyphPtr	*glyphs);
*/

/*
pixman_private void
pixman_compositeRects (pixman_operator_t	op,
		  PicturePtr	pDst,
		  xRenderColor  *color,
		  int		nRect,
		  xRectangle    *rects);
*/

pixman_private pixman_image_t *
FbCreateAlphaPicture (pixman_image_t	*dst,
		      pixman_format_t	*format,
		      uint16_t	width,
		      uint16_t	height);

typedef void	(*CompositeFunc) (pixman_operator_t   op,
				  pixman_image_t    *iSrc,
				  pixman_image_t    *iMask,
				  pixman_image_t    *iDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height);

typedef struct _FbCompositeOperand FbCompositeOperand;

typedef uint32_t (*pixman_compositeFetch)(FbCompositeOperand *op);
typedef void (*pixman_compositeStore) (FbCompositeOperand *op, uint32_t value);

typedef void (*pixman_compositeStep) (FbCompositeOperand *op);
typedef void (*pixman_compositeSet) (FbCompositeOperand *op, int x, int y);

struct _FbCompositeOperand {
    union {
	struct {
	    pixman_bits_t		*top_line;
	    int			left_offset;
	    
	    int			start_offset;
	    pixman_bits_t		*line;
	    uint32_t		offset;
	    FbStride		stride;
	    int			bpp;
	} drawable;
	struct {
	    int			alpha_dx;
	    int			alpha_dy;
	} external;
	struct {
	    int			top_y;
	    int			left_x;
	    int			start_x;
	    int			x;
	    int			y;
	    pixman_transform_t		*transform;
	    pixman_filter_t		filter;
	    int                         repeat;
	    int                         width;
	    int                         height;
	} transform;
    } u;
    pixman_compositeFetch	fetch;
    pixman_compositeFetch	fetcha;
    pixman_compositeStore	store;
    pixman_compositeStep	over;
    pixman_compositeStep	down;
    pixman_compositeSet	set;
/* XXX: We're not supporting indexed operations, right?
    FbIndexedPtr	indexed;
*/
    pixman_region16_t		*dst_clip;
    pixman_region16_t		*src_clip;
};

typedef void (*FbCombineFunc) (FbCompositeOperand	*src,
			       FbCompositeOperand	*msk,
			       FbCompositeOperand	*dst);

typedef struct _FbAccessMap {
    uint32_t		format_code;
    pixman_compositeFetch	fetch;
    pixman_compositeFetch	fetcha;
    pixman_compositeStore	store;
} FbAccessMap;

/* iccompose.c */

typedef struct _FbCompSrc {
    uint32_t	value;
    uint32_t	alpha;
} FbCompSrc;

pixman_private int
fbBuildCompositeOperand (pixman_image_t	    *image,
			 FbCompositeOperand op[4],
			 int16_t		    x,
			 int16_t		    y,
			 int		    transform,
			 int		    alpha);

pixman_private void
pixman_compositeGeneral (pixman_operator_t	op,
			 pixman_image_t	*iSrc,
			 pixman_image_t	*iMask,
			 pixman_image_t	*iDst,
			 int16_t	xSrc,
			 int16_t	ySrc,
			 int16_t	xMask,
			 int16_t	yMask,
			 int16_t	xDst,
			 int16_t	yDst,
			 uint16_t	width,
			 uint16_t	height);

#endif /* _IC_MIPICT_H_ */
