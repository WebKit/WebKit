/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Mozilla Corporation
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 *
 * Contributor(s):
 *      Vladimir Vukicevic <vladimir@mozilla.com>
 */

#include <Carbon/Carbon.h>

#include <AGL/agl.h>
#include <OpenGL/gl.h>

#include "cairoint.h"
#include "cairo-private.h"
#include "cairo-quartz2.h"

#include "cairo-quartz-private.h"

/* This method is private, but it exists.  Its params are are exposed
 * as args to the NS* method, but not as CG.
 */
enum PrivateCGCompositeMode {
    kPrivateCGCompositeClear            = 0,
    kPrivateCGCompositeCopy             = 1,
    kPrivateCGCompositeSourceOver       = 2,
    kPrivateCGCompositeSourceIn         = 3,
    kPrivateCGCompositeSourceOut        = 4,
    kPrivateCGCompositeSourceAtop       = 5,
    kPrivateCGCompositeDestinationOver  = 6,
    kPrivateCGCompositeDestinationIn    = 7,
    kPrivateCGCompositeDestinationOut   = 8,
    kPrivateCGCompositeDestinationAtop  = 9,
    kPrivateCGCompositeXOR              = 10,
    kPrivateCGCompositePlusDarker       = 11, // (max (0, (1-d) + (1-s)))
    kPrivateCGCompositePlusLighter      = 12, // (min (1, s + d))
};
typedef enum PrivateCGCompositeMode PrivateCGCompositeMode;
CG_EXTERN void CGContextSetCompositeOperation (CGContextRef, PrivateCGCompositeMode);
CG_EXTERN void CGContextResetCTM (CGContextRef);
CG_EXTERN void CGContextSetCTM (CGContextRef, CGAffineTransform);
CG_EXTERN void CGContextResetClip (CGContextRef);
CG_EXTERN CGSize CGContextGetPatternPhase (CGContextRef);

typedef struct cairo_quartzgl_surface {
    cairo_surface_t base;

    void *imageData;

    cairo_bool_t y_grows_down;

    AGLContext aglContext;
    CGContextRef cgContext;

    cairo_rectangle_t extents;

    /* These are stored while drawing operations are in place, set up
     * by quartzgl_setup_source() and quartzgl_finish_source()
     */
    CGImageRef sourceImage;
    CGShadingRef sourceShading;
    CGPatternRef sourcePattern;
} cairo_quartzgl_surface_t;

/**
 ** Utility functions
 **/

/*
 * Cairo path -> Quartz path conversion helpers
 */

/* cairo path -> mutable path */
static cairo_status_t
_cairo_path_to_quartz_path_move_to (void *closure, cairo_point_t *point)
{
    CGPathMoveToPoint ((CGMutablePathRef) closure, NULL, _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_path_line_to (void *closure, cairo_point_t *point)
{
    CGPathAddLineToPoint ((CGMutablePathRef) closure, NULL, _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_path_curve_to (void *closure, cairo_point_t *p0, cairo_point_t *p1, cairo_point_t *p2)
{
    CGPathAddCurveToPoint ((CGMutablePathRef) closure, NULL,
			   _cairo_fixed_to_double(p0->x), _cairo_fixed_to_double(p0->y),
			   _cairo_fixed_to_double(p1->x), _cairo_fixed_to_double(p1->y),
			   _cairo_fixed_to_double(p2->x), _cairo_fixed_to_double(p2->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_path_close_path (void *closure)
{
    CGPathCloseSubpath ((CGMutablePathRef) closure);
    return CAIRO_STATUS_SUCCESS;
}

/* cairo path -> execute in context */
static cairo_status_t
_cairo_path_to_quartz_context_move_to (void *closure, cairo_point_t *point)
{
    //fprintf (stderr, "moveto: %f %f\n", _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    CGContextMoveToPoint ((CGContextRef) closure, _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_line_to (void *closure, cairo_point_t *point)
{
    //fprintf (stderr, "lineto: %f %f\n",  _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    if (CGContextIsPathEmpty ((CGContextRef) closure))
	CGContextMoveToPoint ((CGContextRef) closure, _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    else
	CGContextAddLineToPoint ((CGContextRef) closure, _cairo_fixed_to_double(point->x), _cairo_fixed_to_double(point->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_curve_to (void *closure, cairo_point_t *p0, cairo_point_t *p1, cairo_point_t *p2)
{
    //fprintf (stderr, "curveto: %f,%f %f,%f %f,%f\n",
    //		   _cairo_fixed_to_double(p0->x), _cairo_fixed_to_double(p0->y),
    //		   _cairo_fixed_to_double(p1->x), _cairo_fixed_to_double(p1->y),
    //		   _cairo_fixed_to_double(p2->x), _cairo_fixed_to_double(p2->y));

    CGContextAddCurveToPoint ((CGContextRef) closure,
			      _cairo_fixed_to_double(p0->x), _cairo_fixed_to_double(p0->y),
			      _cairo_fixed_to_double(p1->x), _cairo_fixed_to_double(p1->y),
			      _cairo_fixed_to_double(p2->x), _cairo_fixed_to_double(p2->y));
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_to_quartz_context_close_path (void *closure)
{
    //fprintf (stderr, "closepath\n");
    CGContextClosePath ((CGContextRef) closure);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartzgl_cairo_path_to_quartz_path (cairo_path_fixed_t *path,
					  CGMutablePathRef cgPath)
{
    return _cairo_path_fixed_interpret (path,
					CAIRO_DIRECTION_FORWARD,
					_cairo_path_to_quartz_path_move_to,
					_cairo_path_to_quartz_path_line_to,
					_cairo_path_to_quartz_path_curve_to,
					_cairo_path_to_quartz_path_close_path,
					cgPath);
}

static cairo_status_t
_cairo_quartzgl_cairo_path_to_quartz_context (cairo_path_fixed_t *path,
					     CGContextRef cgc)
{
    return _cairo_path_fixed_interpret (path,
					CAIRO_DIRECTION_FORWARD,
					_cairo_path_to_quartz_context_move_to,
					_cairo_path_to_quartz_context_line_to,
					_cairo_path_to_quartz_context_curve_to,
					_cairo_path_to_quartz_context_close_path,
					cgc);
}

/*
 * Misc helpers/callbacks
 */

static PrivateCGCompositeMode
_cairo_quartzgl_cairo_operator_to_quartz (cairo_operator_t op)
{
    switch (op) {
	case CAIRO_OPERATOR_CLEAR:
	    return kPrivateCGCompositeClear;
	case CAIRO_OPERATOR_SOURCE:
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_OVER:
	    return kPrivateCGCompositeSourceOver;
	case CAIRO_OPERATOR_IN:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeSourceIn;
	case CAIRO_OPERATOR_OUT:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeSourceOut;
	case CAIRO_OPERATOR_ATOP:
	    return kPrivateCGCompositeSourceAtop;

	case CAIRO_OPERATOR_DEST:
	    /* XXX this is handled specially (noop)! */
	    return kPrivateCGCompositeCopy;
	case CAIRO_OPERATOR_DEST_OVER:
	    return kPrivateCGCompositeDestinationOver;
	case CAIRO_OPERATOR_DEST_IN:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeDestinationIn;
	case CAIRO_OPERATOR_DEST_OUT:
	    return kPrivateCGCompositeDestinationOut;
	case CAIRO_OPERATOR_DEST_ATOP:
	    /* XXX This doesn't match image output */
	    return kPrivateCGCompositeDestinationAtop;

	case CAIRO_OPERATOR_XOR:
	    return kPrivateCGCompositeXOR; /* This will generate strange results */
	case CAIRO_OPERATOR_ADD:
	    return kPrivateCGCompositePlusLighter;
	case CAIRO_OPERATOR_SATURATE:
	    /* XXX This doesn't match image output for SATURATE; there's no equivalent */
	    return kPrivateCGCompositePlusDarker;  /* ??? */
    }


    return kPrivateCGCompositeCopy;
}

static CGLineCap
_cairo_quartzgl_cairo_line_cap_to_quartz (cairo_line_cap_t ccap)
{
    switch (ccap) {
	case CAIRO_LINE_CAP_BUTT: return kCGLineCapButt; break;
	case CAIRO_LINE_CAP_ROUND: return kCGLineCapRound; break;
	case CAIRO_LINE_CAP_SQUARE: return kCGLineCapSquare; break;
    }

    return kCGLineCapButt;
}

static CGLineJoin
_cairo_quartzgl_cairo_line_join_to_quartz (cairo_line_join_t cjoin)
{
    switch (cjoin) {
	case CAIRO_LINE_JOIN_MITER: return kCGLineJoinMiter; break;
	case CAIRO_LINE_JOIN_ROUND: return kCGLineJoinRound; break;
	case CAIRO_LINE_JOIN_BEVEL: return kCGLineJoinBevel; break;
    }

    return kCGLineJoinMiter;
}

static void
_cairo_quartzgl_cairo_matrix_to_quartz (const cairo_matrix_t *src,
					CGAffineTransform *dst)
{
    dst->a = src->xx;
    dst->b = src->xy;
    dst->c = src->yx;
    dst->d = src->yy;
    dst->tx = src->x0;
    dst->ty = src->y0;
}

/**
 ** Source -> Quartz setup and finish functions
 **/

static void
ComputeGradientValue (void *info, const float *in, float *out)
{
    float fdist = *in; /* 0.0 .. 1.0 */
    cairo_fixed_16_16_t fdist_fix = _cairo_fixed_from_double(*in);
    cairo_gradient_pattern_t *grad = (cairo_gradient_pattern_t*) info;
    int i;

    for (i = 0; i < grad->n_stops; i++) {
	if (grad->stops[i].x > fdist_fix)
	    break;
    }

    //fprintf (stderr, "in: %f i: %d n_stops: %d\n", fdist, i, grad->n_stops);

    if (i == 0 || i == grad->n_stops) {
	if (i == grad->n_stops)
	    --i;
	out[0] = grad->stops[i].color.red / 65535.;
	out[1] = grad->stops[i].color.green / 65535.;
	out[2] = grad->stops[i].color.blue / 65535.;
	out[3] = grad->stops[i].color.alpha / 65535.;
    } else {
	float ax = _cairo_fixed_to_double(grad->stops[i-1].x);
	float bx = _cairo_fixed_to_double(grad->stops[i].x) - ax;
	fdist -= ax;

	float bp = fdist/bx;
	float ap = 1.0 - bp;

	//fprintf (stderr, "ax: %f bx: %f fdist: %f bp: %f ap: %f\n", ax, bx, fdist, bp, ap);
	out[0] =
	    (grad->stops[i-1].color.red / 65535.) * ap +
	    (grad->stops[i].color.red / 65535.) * bp;
	out[1] =
	    (grad->stops[i-1].color.green / 65535.) * ap +
	    (grad->stops[i].color.green / 65535.) * bp;
	out[2] =
	    (grad->stops[i-1].color.blue / 65535.) * ap +
	    (grad->stops[i].color.blue / 65535.) * bp;
	out[3] =
	    (grad->stops[i-1].color.alpha / 65535.) * ap +
	    (grad->stops[i].color.alpha / 65535.) * bp;
    }

    //fprintf (stderr, "out: %f %f %f %f\n", out[0], out[1], out[2], out[3]);
}

static CGFunctionRef
CreateGradientFunction (cairo_gradient_pattern_t *gpat)
{
    static const float input_value_range[2] = { 0.f, 1.f };
    static const float output_value_ranges[8] = { 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f };
    static const CGFunctionCallbacks callbacks = {
	0, ComputeGradientValue, (CGFunctionReleaseInfoCallback) cairo_pattern_destroy
    };

    return CGFunctionCreate (gpat,
			     1,
			     input_value_range,
			     4,
			     output_value_ranges,
			     &callbacks);
}

static CGShadingRef
_cairo_quartzgl_cairo_gradient_pattern_to_quartz (cairo_pattern_t *abspat)
{
    cairo_matrix_t mat;
    double x0, y0;

    if (abspat->type != CAIRO_PATTERN_LINEAR &&
	abspat->type != CAIRO_PATTERN_RADIAL)
	return NULL;

    /* We can only do this if we have an identity pattern matrix;
     * otherwise fall back through to the generic pattern case.
     * XXXperf we could optimize this by creating a pattern with the shading;
     * but we'd need to know the extents to do that.
     */
    cairo_pattern_get_matrix (abspat, &mat);
    if (mat.xx != 1.0 || mat.yy != 1.0 || mat.xy != 0.0 || mat.yx != 0.0)
	return NULL;

    x0 = mat.x0;
    y0 = mat.y0;

    if (abspat->type == CAIRO_PATTERN_LINEAR) {
	cairo_linear_pattern_t *lpat = (cairo_linear_pattern_t*) abspat;
	CGShadingRef shading;
	CGPoint start, end;
	CGFunctionRef gradFunc;
	CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();

	start = CGPointMake (_cairo_fixed_to_double (lpat->gradient.p1.x) - x0,
			     _cairo_fixed_to_double (lpat->gradient.p1.y) - y0);
	end = CGPointMake (_cairo_fixed_to_double (lpat->gradient.p2.x) - x0,
			   _cairo_fixed_to_double (lpat->gradient.p2.y) - y0);

	cairo_pattern_reference (abspat);
	gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) lpat);
	shading = CGShadingCreateAxial (rgb,
					start, end,
					gradFunc,
					true, true);
	CGColorSpaceRelease(rgb);
	CGFunctionRelease(gradFunc);

	return shading;
    }

    if (abspat->type == CAIRO_PATTERN_RADIAL) {
	cairo_radial_pattern_t *rpat = (cairo_radial_pattern_t*) abspat;
	CGShadingRef shading;
	CGPoint start, end;
	CGFunctionRef gradFunc;
	CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();

	start = CGPointMake (_cairo_fixed_to_double (rpat->gradient.inner.x) - x0,
			     _cairo_fixed_to_double (rpat->gradient.inner.y) - y0);
	end = CGPointMake (_cairo_fixed_to_double (rpat->gradient.outer.x) - x0,
			   _cairo_fixed_to_double (rpat->gradient.outer.y) - y0);

	cairo_pattern_reference (abspat);
	gradFunc = CreateGradientFunction ((cairo_gradient_pattern_t*) rpat);
	shading = CGShadingCreateRadial (rgb,
					 start,
					 _cairo_fixed_to_double (rpat->gradient.inner.radius),
					 end,
					 _cairo_fixed_to_double (rpat->gradient.outer.radius),
					 gradFunc,
					 true, true);
	CGColorSpaceRelease(rgb);
	CGFunctionRelease(gradFunc);

	return shading;
    }

    /* Shouldn't be reached */
    return NULL;
}


/* Generic cairo_pattern -> CGPattern function */
static void
SurfacePatternDrawFunc (void *info, CGContextRef context)
{
    cairo_surface_pattern_t *spat = (cairo_surface_pattern_t *) info;
    cairo_surface_t *pat_surf = spat->surface;
    cairo_rectangle_t extents;
    cairo_status_t status;

    cairo_quartzgl_surface_t *quartz_surf = NULL;

    /* Coordinates in this function are always in pattern space, which
     * uses the opengl Y axis orientation.
     */
    CGImageRef img;

    if (!cairo_surface_is_quartzgl (pat_surf)) {
	//fprintf (stderr, "-- source is not quartzgl surface\n");
	/* This sucks; we should really store a dummy quartzgl surface
	 * for passing in here */

	cairo_surface_t *dummy = cairo_quartzgl_surface_create (CAIRO_FORMAT_ARGB32, 1, 1, TRUE);

	cairo_rectangle_t rect;
	_cairo_surface_get_extents (pat_surf, &rect);

	cairo_surface_t *new_surf = NULL;

	_cairo_surface_clone_similar (dummy, pat_surf, &new_surf);

	cairo_surface_destroy(dummy);

	quartz_surf = (cairo_quartzgl_surface_t *) new_surf;
    } else {
	//fprintf (stderr, "-- source is quartzgl surface\n");
	/* If it's a quartzgl surface, we can try to see if it's a CGBitmapContext;
	 * we do this when we call CGBitmapContextCreateImage below.
	 */
	cairo_surface_reference (pat_surf);
	quartz_surf = (cairo_quartzgl_surface_t*) pat_surf;
    }

    /* this is a 10.4 API */
    img = CGBitmapContextCreateImage (quartz_surf->cgContext);
    if (!img)
	//fprintf (stderr, "CGBitmapContextCreateImage failed\n");

    {
	CGAffineTransform xform = CGContextGetCTM(context);
	//fprintf (stderr, "  pattern draw xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n", xform.tx, xform.ty, xform.a, xform.b, xform.c, xform.d);
    }

    CGRect imageBounds;
    imageBounds.size = CGSizeMake (CGImageGetWidth(img), CGImageGetHeight(img));
    imageBounds.origin.x = 0;
    imageBounds.origin.y = 0;

    //fprintf (stderr, "bounds: %f %f\n", imageBounds.size.width, imageBounds.size.height);
    CGContextDrawImage (context, imageBounds, img);

    //CGContextSetRGBFillColor (context, 0, 1, 1, 1);
    //CGContextFillRect (context, imageBounds);

    CGImageRelease (img);

    cairo_surface_destroy ((cairo_surface_t*) quartz_surf);
}

static CGPatternRef
_cairo_quartzgl_cairo_repeating_surface_pattern_to_quartz (cairo_quartzgl_surface_t *dest,
							   cairo_pattern_t *abspat)
{
    cairo_surface_pattern_t *spat;
    cairo_surface_t *pat_surf;
    cairo_rectangle_t extents;

    CGRect pbounds;
    CGAffineTransform ptransform, stransform;
    CGPatternCallbacks cb = { 0,
			      SurfacePatternDrawFunc,
			      (CGFunctionReleaseInfoCallback) cairo_pattern_destroy };
    CGPatternRef cgpat;
    float rw, rh;

    /* SURFACE is the only type we'll handle here */
    if (abspat->type != CAIRO_PATTERN_SURFACE)
	return NULL;

    spat = (cairo_surface_pattern_t *) abspat;
    pat_surf = spat->surface;

    _cairo_surface_get_extents (pat_surf, &extents);
    pbounds.origin.x = 0;
    pbounds.origin.y = 0;
    pbounds.size.width = extents.width;
    pbounds.size.height = extents.height;

    cairo_matrix_t m = spat->base.matrix;
    cairo_matrix_invert(&m);
    _cairo_quartzgl_cairo_matrix_to_quartz (&m, &stransform);

    //fprintf (stderr, "  cairo inv pattern xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n",
    //     stransform.tx, stransform.ty, stransform.a, stransform.b, stransform.c, stransform.d);

    ptransform = CGContextGetCTM (dest->cgContext);
    ptransform = CGAffineTransformConcat (stransform, ptransform);

    //fprintf (stderr, "  pattern xform: t: %f %f xx: %f xy: %f yx: %f yy: %f\n",
    //     ptransform.tx, ptransform.ty, ptransform.a, ptransform.b, ptransform.c, ptransform.d);

    // kjs seems to indicate this should work (setting to 0,0 to avoid tiling); however,
    // the pattern CTM scaling ends up being NaN in the pattern draw function if
    // either rw or rh are 0.
#if 0
    if (spat->base.extend == CAIRO_EXTEND_NONE) {
	/* XXX wasteful; this will keep drawing the pattern in the original
	 * location.  We need to set up the clip region instead to do this right.
	 */
	rw = 0;
	rh = 0;
    } else if (spat->base.extend == CAIRO_EXTEND_REPEAT) {
	rw = extents.width;
	rh = extents.height;
    } else if (spat->base.extend == CAIRO_EXTEND_REFLECT) {
	/* XXX broken; need to emulate by reflecting the image into 4 quadrants
	 * and then tiling that
	 */
	rw = extents.width;
	rh = extents.height;
    } else {
	/* CAIRO_EXTEND_PAD */
	/* XXX broken. */
	rw = 0;
	rh = 0;
    }
#else
    rw = extents.width;
    rh = extents.height;
#endif

    cairo_pattern_reference (abspat);
    cgpat = CGPatternCreate (abspat,
			     pbounds,
			     ptransform,
			     rw, rh,
			     kCGPatternTilingConstantSpacing, /* kCGPatternTilingNoDistortion, */
			     TRUE,
			     &cb);
    return cgpat;
}

typedef enum {
    DO_SOLID,
    DO_SHADING,
    DO_PATTERN,
    DO_UNSUPPORTED
} cairo_quartzgl_action_t;

static cairo_quartzgl_action_t
_cairo_quartzgl_setup_source (cairo_quartzgl_surface_t *surface,
			      cairo_pattern_t *source)
{
    assert (!(surface->sourceImage || surface->sourceShading || surface->sourcePattern));

    if (source->type == CAIRO_PATTERN_SOLID) {
	cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) source;

	CGContextSetRGBStrokeColor (surface->cgContext,
				    solid->color.red,
				    solid->color.green,
				    solid->color.blue,
				    solid->color.alpha);
	CGContextSetRGBFillColor (surface->cgContext,
				  solid->color.red,
				  solid->color.green,
				  solid->color.blue,
				  solid->color.alpha);
    } else if (source->type == CAIRO_PATTERN_LINEAR ||
	       source->type == CAIRO_PATTERN_RADIAL)
    {
	CGShadingRef shading = _cairo_quartzgl_cairo_gradient_pattern_to_quartz (source);
	if (!shading)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	surface->sourceShading = shading;
    } else if (source->type == CAIRO_PATTERN_SURFACE) {
	CGPatternRef pattern = _cairo_quartzgl_cairo_repeating_surface_pattern_to_quartz (surface, source);
	if (!pattern)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	float patternAlpha = 1.0;
        CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
        CGContextSetFillColorSpace (surface->cgContext, patternSpace);
	CGContextSetFillPattern (surface->cgContext, pattern, &patternAlpha);
	CGColorSpaceRelease (patternSpace);

	/* Quartz likes to munge the pattern phase (as yet unexplained why); force
	 * it to 0,0 otherwise things break badly. */

	CGContextSetPatternPhase (surface->cgContext, CGSizeMake(0,0));
	surface->sourcePattern = pattern;
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartzgl_teardown_source (cairo_quartzgl_surface_t *surface,
				 cairo_pattern_t *source)
{
    if (surface->sourceImage) {
	// nothing to do; we don't use sourceImage yet
    }

    if (surface->sourceShading) {
	CGShadingRelease(surface->sourceShading);
	surface->sourceShading = NULL;
    }

    if (surface->sourcePattern) {
	CGPatternRelease(surface->sourcePattern);
	surface->sourcePattern = NULL;
    }
}

/**
 ** get source/dest image implementation
 **/

static void
ImageDataReleaseFunc(void *info, const void *data, size_t size)
{
    if (data != NULL) {
        free((void *) data);
    }
}

/* Read the image from the surface's front buffer */
static cairo_int_status_t
_cairo_quartzgl_get_image (cairo_quartzgl_surface_t *surface,
			   cairo_image_surface_t **image_out,
			   unsigned char **data_out)
{
    unsigned char *imageData;
    cairo_image_surface_t *isurf;

    /* If we weren't constructed with an AGL Context
     * or a CCGBitmapContext, then we have no way
     * of doing this
     */
    if (surface->aglContext) {
	AGLContext oldContext;
	cairo_format_masks_t masks = { 32, 0xff << 24, 0xff << 16, 0xff << 8, 0xff << 0 };
	unsigned int i;

	oldContext = aglGetCurrentContext();
	if (oldContext != surface->aglContext)
	    aglSetCurrentContext(surface->aglContext);

	imageData = (unsigned char *) malloc (surface->extents.width * surface->extents.height * 4);
	if (!imageData)
	    return CAIRO_STATUS_NO_MEMORY;

	glReadBuffer (GL_FRONT);
	glReadPixels (0, 0, surface->extents.width, surface->extents.height,
		      GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		      imageData);

	/* swap lines */
	/* XXX find some fast code to do this */
	unsigned int lineSize = surface->extents.width * 4;
	unsigned char *tmpLine = malloc(lineSize);
	for (i = 0; i < surface->extents.height / 2; i++) {
	    unsigned char *l0 = imageData + lineSize * i;
	    unsigned char *l1 = imageData + (lineSize * (surface->extents.height - i - 1));
	    memcpy (tmpLine, l0, lineSize);
	    memcpy (l0, l1, lineSize);
	    memcpy (l1, tmpLine, lineSize);
	}
	free (tmpLine);

	if (oldContext && oldContext != surface->aglContext)
	    aglSetCurrentContext(oldContext);

	isurf = (cairo_image_surface_t *)_cairo_image_surface_create_with_masks
	    (imageData,
	     &masks,
	     surface->extents.width,
	     surface->extents.height,
	     surface->extents.width * 4);

	if (data_out)
	    *data_out = imageData;
	else
	    _cairo_image_surface_assume_ownership_of_data (isurf);
    } else if (CGBitmapContextGetBitmapInfo(surface->cgContext) != 0) {
	unsigned int stride;
	unsigned int bitinfo;
	unsigned int bpc, bpp;
	CGColorSpaceRef colorspace;
	unsigned int color_comps;

	imageData = (unsigned char *) CGBitmapContextGetData(surface->cgContext);
	bitinfo = CGBitmapContextGetBitmapInfo (surface->cgContext);
	stride = CGBitmapContextGetBytesPerRow (surface->cgContext);
	bpp = CGBitmapContextGetBitsPerPixel (surface->cgContext);
	bpc = CGBitmapContextGetBitsPerComponent (surface->cgContext);

	// let's hope they don't add YUV under us
	colorspace = CGBitmapContextGetColorSpace (surface->cgContext);
	color_comps = CGColorSpaceGetNumberOfComponents(colorspace);

	// XXX TODO: We can handle all of these by converting to
	// pixman masks, including non-native-endian masks
	if (bpc != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp != 32 && bpp != 8)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (color_comps != 3 && color_comps != 1)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (bpp == 32 && color_comps == 3 &&
	    (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaPremultipliedFirst &&
	    (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_ARGB32,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 32 && color_comps == 3 &&
		   (bitinfo & kCGBitmapAlphaInfoMask) == kCGImageAlphaNoneSkipFirst &&
		   (bitinfo & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Host)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_RGB24,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else if (bpp == 8 && color_comps == 1)
	{
	    isurf = (cairo_image_surface_t *)
		cairo_image_surface_create_for_data (imageData,
						     CAIRO_FORMAT_A8,
						     surface->extents.width,
						     surface->extents.height,
						     stride);
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *image_out = isurf;
    return CAIRO_STATUS_SUCCESS;
}

/**
 ** Cairo surface backend implementations
 **/

static cairo_status_t
_cairo_quartzgl_surface_finish (void *abstract_surface)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_finish\n", surface);

    if (surface->aglContext)
	aglSetCurrentContext(surface->aglContext);

    CGContextFlush (surface->cgContext);
    CGContextRelease (surface->cgContext);
    surface->cgContext = NULL;

    if (surface->aglContext)
	glFlush();

    surface->aglContext = NULL;

    if (surface->imageData) {
	free (surface->imageData);
	surface->imageData = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_quartzgl_surface_acquire_source_image (void *abstract_surface,
					      cairo_image_surface_t **image_out,
					      void **image_extra)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_acquire_source_image\n", surface);

    *image_extra = NULL;

    return _cairo_quartzgl_get_image (surface, image_out, NULL);
}

static cairo_status_t
_cairo_quartzgl_surface_acquire_dest_image (void *abstract_surface,
					    cairo_rectangle_t *interest_rect,
					    cairo_image_surface_t **image_out,
					    cairo_rectangle_t *image_rect,
					    void **image_extra)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_int_status_t status;
    unsigned char *data;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_acquire_dest_image\n", surface);

    *image_rect = surface->extents;

    status = _cairo_quartzgl_get_image (surface, image_out, &data);
    if (status)
        return status;

    *image_extra = data;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_quartzgl_surface_release_dest_image (void *abstract_surface,
					    cairo_rectangle_t *interest_rect,
					    cairo_image_surface_t *image,
					    cairo_rectangle_t *image_rect,
					    void *image_extra)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    unsigned char *imageData = (unsigned char *) image_extra;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_release_dest_image\n", surface);

    if (!CGBitmapContextGetData (surface->cgContext)) {
	CGDataProviderRef dataProvider;
	CGImageRef img;

	dataProvider = CGDataProviderCreateWithData (NULL, imageData,
						     surface->extents.width * surface->extents.height * 4,
						     ImageDataReleaseFunc);
	    
	img = CGImageCreate (surface->extents.width, surface->extents.height,
			     8, 32,
			     surface->extents.width * 4,
			     CGColorSpaceCreateDeviceRGB(),
			     kCGImageAlphaPremultipliedFirst,
			     dataProvider,
			     NULL,
			     false,
			     kCGRenderingIntentDefault);

	/* Insert screaming here */
	/* We have a y-inverting CTM set on the cgContext.  However,
	 * images are still drawn top-down, and we can't flip an image
	 * by drawing a negative width or height.  We need to undo
	 * that transform so that we can draw our image back sanely.
	 */
	if (surface->y_grows_down) {
	    CGContextSaveGState (surface->cgContext);
	    CGContextScaleCTM (surface->cgContext, 1.0, -1.0);
	    CGContextTranslateCTM (surface->cgContext, 0.0, -surface->extents.height);
	}

	CGContextSetCompositeOperation (surface->cgContext, kPrivateCGCompositeCopy);

	CGContextDrawImage (surface->cgContext,
			    CGRectMake (0, 0, surface->extents.width, surface->extents.height),
			    img);

	if (surface->y_grows_down) {
	    CGContextRestoreGState (surface->cgContext);
	}

	CGImageRelease (img);
	CGDataProviderRelease (dataProvider);
    }

    cairo_surface_destroy ((cairo_surface_t *) image);
}

static cairo_surface_t *
_cairo_quartzgl_surface_create_similar (void *abstract_surface,
					cairo_content_t content,
					int width,
					int height)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    cairo_format_t format;

    if (content == CAIRO_CONTENT_COLOR_ALPHA)
	format = CAIRO_FORMAT_ARGB32;
    else if (content == CAIRO_CONTENT_COLOR)
	format = CAIRO_FORMAT_RGB24;
    else if (content == CAIRO_CONTENT_ALPHA)
	format = CAIRO_FORMAT_A8;
    else
	return NULL;

    return cairo_quartzgl_surface_create (format, width, height, surface->y_grows_down);
}

static cairo_status_t
_cairo_quartzgl_surface_clone_similar (void *abstract_surface,
				       cairo_surface_t *src,
				       cairo_surface_t **clone_out)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_quartzgl_surface_t *new_surface = NULL;
    cairo_format_t new_format;

    CGImageRef quartz_image = NULL;
    cairo_surface_t *surface_to_release = NULL;

    if (cairo_surface_is_quartzgl (src)) {
	cairo_quartzgl_surface_t *qsurf = (cairo_quartzgl_surface_t *) src;
	quartz_image = CGBitmapContextCreateImage (qsurf->cgContext);
	new_format = CAIRO_FORMAT_ARGB32;  /* XXX bogus; recover a real format from the image */
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *isurf = (cairo_image_surface_t *) src;
	CGDataProviderRef dataProvider;
	CGColorSpaceRef cgColorspace;
	CGBitmapInfo bitinfo;
	int bitsPerComponent, bitsPerPixel;

	if (isurf->format == CAIRO_FORMAT_ARGB32) {
	    cgColorspace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	} else if (isurf->format == CAIRO_FORMAT_RGB24) {
	    cgColorspace = CGColorSpaceCreateDeviceRGB();
	    bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
	    bitsPerComponent = 8;
	    bitsPerPixel = 32;
	} else if (isurf->format == CAIRO_FORMAT_A8) {
	    cgColorspace = CGColorSpaceCreateDeviceGray();
	    bitinfo = kCGImageAlphaNone;
	    bitsPerComponent = 8;
	    bitsPerPixel = 8;
	} else {
	    /* SUPPORT A1, maybe */
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	new_format = isurf->format;

	dataProvider = CGDataProviderCreateWithData (NULL,
						     isurf->data,
						     isurf->height * isurf->stride,
						     NULL);

	quartz_image = CGImageCreate (isurf->width, isurf->height,
				      bitsPerComponent,
				      bitsPerPixel,
				      isurf->stride,
				      cgColorspace,
				      bitinfo,
				      dataProvider,
				      NULL,
				      false,
				      kCGRenderingIntentDefault);
	CGDataProviderRelease (dataProvider);
	CGColorSpaceRelease (cgColorspace);
    } else {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (!quartz_image)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    new_surface = (cairo_quartzgl_surface_t *)
	cairo_quartzgl_surface_create (new_format,
				       CGImageGetWidth (quartz_image),
				       CGImageGetHeight (quartz_image),
				       surface->y_grows_down);
    if (!new_surface || new_surface->base.status)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    CGContextSetCompositeOperation (new_surface->cgContext,
				    kPrivateCGCompositeCopy);

    CGContextTranslateCTM (new_surface->cgContext, 0.0, new_surface->extents.height);
    CGContextScaleCTM (new_surface->cgContext, 1.0, -1.0);

    CGContextDrawImage (new_surface->cgContext,
			CGRectMake (0, 0, CGImageGetWidth (quartz_image), CGImageGetHeight (quartz_image)),
			quartz_image);
    CGImageRelease (quartz_image);

    *clone_out = (cairo_surface_t*) new_surface;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartzgl_surface_get_extents (void *abstract_surface,
				     cairo_rectangle_t *extents)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    *extents = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_quartzgl_surface_paint (void *abstract_surface,
			       cairo_operator_t op,
			       cairo_pattern_t *source)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartzgl_action_t action;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_paint op %d source->type %d\n", surface, op, source->type);

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartzgl_cairo_operator_to_quartz (op));

    CGRect bounds = CGContextGetClipBoundingBox (surface->cgContext);

    action = _cairo_quartzgl_setup_source (surface, source);

    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextFillRect (surface->cgContext, bounds);
    } else if (action == DO_SHADING) {
	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _cairo_quartzgl_teardown_source (surface, source);

    return rv;
}

static cairo_int_status_t
_cairo_quartzgl_surface_fill (void *abstract_surface,
			      cairo_operator_t op,
			      cairo_pattern_t *source,
			      cairo_path_fixed_t *path,
			      cairo_fill_rule_t fill_rule,
			      double tolerance,
			      cairo_antialias_t antialias)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartzgl_action_t action;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_fill op %d source->type %d\n", surface, op, source->type);

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSaveGState (surface->cgContext);

    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartzgl_cairo_operator_to_quartz (op));

    action = _cairo_quartzgl_setup_source (surface, source);
    if (action == DO_UNSUPPORTED) {
	CGContextRestoreGState (surface->cgContext);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextBeginPath (surface->cgContext);
    _cairo_quartzgl_cairo_path_to_quartz_context (path, surface->cgContext);

    if (action == DO_SOLID || action == DO_PATTERN) {
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextFillPath (surface->cgContext);
	else
	    CGContextEOFillPath (surface->cgContext);
    } else if (action == DO_SHADING) {

	// we have to clip and then paint the shading; we can't fill
	// with the shading
	if (fill_rule == CAIRO_FILL_RULE_WINDING)
	    CGContextClip (surface->cgContext);
	else
	    CGContextEOClip (surface->cgContext);

	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    _cairo_quartzgl_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

    return rv;
}

static cairo_int_status_t
_cairo_quartzgl_surface_stroke (void *abstract_surface,
				cairo_operator_t op,
				cairo_pattern_t *source,
				cairo_path_fixed_t *path,
				cairo_stroke_style_t *style,
				cairo_matrix_t *ctm,
				cairo_matrix_t *ctm_inverse,
				double tolerance,
				cairo_antialias_t antialias)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartzgl_action_t action;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_stroke op %d source->type %d\n", surface, op, source->type);

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    CGContextSaveGState (surface->cgContext);

    CGContextSetShouldAntialias (surface->cgContext, (antialias != CAIRO_ANTIALIAS_NONE));
    CGContextSetLineWidth (surface->cgContext, style->line_width);
    CGContextSetLineCap (surface->cgContext, _cairo_quartzgl_cairo_line_cap_to_quartz (style->line_cap));
    CGContextSetLineJoin (surface->cgContext, _cairo_quartzgl_cairo_line_join_to_quartz (style->line_join));
    CGContextSetMiterLimit (surface->cgContext, style->miter_limit);

    if (style->dash && style->num_dashes) {
#define STATIC_DASH 32
	float sdash[STATIC_DASH];
	float *fdash = sdash;
	int k;
	if (style->num_dashes > STATIC_DASH)
	    fdash = malloc (sizeof(float)*style->num_dashes);

	for (k = 0; k < style->num_dashes; k++)
	    fdash[k] = (float) style->dash[k];
	
	CGContextSetLineDash (surface->cgContext, style->dash_offset, fdash, style->num_dashes);

	if (fdash != sdash)
	    free (fdash);
    }

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartzgl_cairo_operator_to_quartz (op));

    action = _cairo_quartzgl_setup_source (surface, source);
    if (action == DO_UNSUPPORTED) {
	CGContextRestoreGState (surface->cgContext);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextBeginPath (surface->cgContext);
    _cairo_quartzgl_cairo_path_to_quartz_context (path, surface->cgContext);

    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextStrokePath (surface->cgContext);
    } else if (action == DO_SHADING) {
	// we have to clip and then paint the shading; first we have to convert
	// the stroke to a path that we can fill

	// this is a 10.4-only method; we could probably fall back to
	// CGPattern usage if it's not available
	CGContextReplacePathWithStrokedPath (surface->cgContext);
	CGContextClip (surface->cgContext);

	CGContextDrawShading (surface->cgContext, surface->sourceShading);
    } else {
	rv = CAIRO_INT_STATUS_UNSUPPORTED;
    }

    CGContextRestoreGState (surface->cgContext);
    return rv;
}

static cairo_int_status_t
_cairo_quartzgl_surface_show_glyphs (void *abstract_surface,
				     cairo_operator_t op,
				     cairo_pattern_t *source,
				     const cairo_glyph_t *glyphs,
				     int num_glyphs,
				     cairo_scaled_font_t *scaled_font)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;
    cairo_int_status_t rv = CAIRO_STATUS_SUCCESS;
    cairo_quartzgl_action_t action;

    if (num_glyphs <= 0)
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    if (!_cairo_scaled_font_is_atsui (scaled_font))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    CGContextSaveGState (surface->cgContext);

    action = _cairo_quartzgl_setup_source (surface, source);
    if (action == DO_SOLID || action == DO_PATTERN) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextFill);
    } else if (action == DO_SHADING) {
	CGContextSetTextDrawingMode (surface->cgContext, kCGTextClip);
    } else {
	/* Unsupported */
	CGContextRestoreGState (surface->cgContext);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    double y_height = surface->extents.height;
#if 0
    if (surface->y_grows_down) {
	/* Undo this, because we can just fix the glyph offsets ourselves,
	 * and it's a pain to manage both coodinate spaces in my head */
	CGContextScaleCTM (surface->cgContext, 1.0, -1.0);
	CGContextTranslateCTM (surface->cgContext, 0.0, - y_height);
    }
#endif

    CGContextSetCompositeOperation (surface->cgContext, _cairo_quartzgl_cairo_operator_to_quartz (op));

    ATSUFontID fid = _cairo_atsui_scaled_font_get_atsu_font_id (scaled_font);
    ATSFontRef atsfref = FMGetATSFontRefFromFont (fid);
    CGFontRef cgfref = CGFontCreateWithPlatformFont (&atsfref);

    CGContextSetFont (surface->cgContext, cgfref);
    CGFontRelease (cgfref);

    /* So this should include the size; I don't know if I need to extract the
     * size from this and call CGContextSetFontSize.. will I get crappy hinting
     * with this 1.0 size business?  Or will CG just multiply that size into the
     * text matrix?
     */
    //fprintf (stderr, "show_glyphs: glyph 0 at: %f, %f\n", glyphs[0].x, glyphs[0].y);
    CGAffineTransform textTransform;
    _cairo_quartzgl_cairo_matrix_to_quartz (&scaled_font->font_matrix, &textTransform);

    if (surface->y_grows_down) {
	textTransform = CGAffineTransformTranslate (textTransform, glyphs[0].x, y_height - glyphs[0].y);
    } else {
	textTransform = CGAffineTransformTranslate (textTransform, glyphs[0].x, glyphs[0].y);
	// Uhh, this is just straight up broken.  No idea why this is needed
	// for the cocoa non-y-grows-down business.
	textTransform = CGAffineTransformScale (textTransform, 1.0, -1.0);
    }

    CGContextSetTextMatrix (surface->cgContext, textTransform);
    CGContextSetFontSize (surface->cgContext, 1.0);

#if 1
    /* We should be using the ShowGlyphsWithAdvances version,
     * but I'm clearly not smart enough for it */
    if (surface->y_grows_down) {
	int i;
	for (i = 0; i < num_glyphs; i++) {
	    CGGlyph g = glyphs[i].index;
	    CGContextShowGlyphsAtPoint (surface->cgContext,
					glyphs[i].x, y_height - glyphs[i].y,
					&g,
					1);
	}
    } else {
	int i;
	for (i = 0; i < num_glyphs; i++) {
	    CGGlyph g = glyphs[i].index;
	    CGContextShowGlyphsAtPoint (surface->cgContext,
					glyphs[i].x, glyphs[i].y,
					&g,
					1);
	}
    }

#else
    // XXX stack storage
    CGGlyph *cg_glyphs = (CGGlyph*) malloc(sizeof(CGGlyph) * num_glyphs);
    CGSize *cg_advances = (CGSize*) malloc(sizeof(CGSize) * num_glyphs);

    double xprev = 0.0, yprev = 0.0;

    for (i = 0; i < num_glyphs; i++) {
	//fprintf (stderr, "%d: gx: %f gy: %f xp: %f yp: %f\n", i, glyphs[i].x, glyphs[i].y, xprev, yprev);
	cg_glyphs[i] = glyphs[i].index;
	if (i) {
	    cg_advances[i-1].width = glyphs[i].x - xprev;
	    cg_advances[i-1].height = - (glyphs[i].y - yprev);
	} else {
	    cg_advances[i].width = 0;
	    cg_advances[i].height = 0;
	}
	xprev = glyphs[i].x;
	yprev = glyphs[i].y;
    }

    cg_advances[num_glyphs-1].width = 0;
    cg_advances[num_glyphs-1].height = 0;

    for (i = 0; i < num_glyphs; i++) {
	//fprintf (stderr, "[%d: %d %f,%f]\n", i, cg_glyphs[i], cg_advances[i].width, cg_advances[i].height);
    }

    CGContextShowGlyphsWithAdvances (surface->cgContext,
				     cg_glyphs,
				     cg_advances,
				     num_glyphs);

    free (cg_glyphs);
    free (cg_advances);
#endif

    if (action == DO_SHADING)
	CGContextDrawShading (surface->cgContext, surface->sourceShading);

    _cairo_quartzgl_teardown_source (surface, source);

    CGContextRestoreGState (surface->cgContext);

    return rv;
}

static cairo_int_status_t
_cairo_quartzgl_surface_mask (void *abstract_surface,
			      cairo_operator_t op,
			      cairo_pattern_t *source,
			      cairo_pattern_t *mask)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    return CAIRO_STATUS_SUCCESS;
    /*return CAIRO_INT_STATUS_UNSUPPORTED;*/
}

static cairo_int_status_t
_cairo_quartzgl_surface_intersect_clip_path (void *abstract_surface,
					     cairo_path_fixed_t *path,
					     cairo_fill_rule_t fill_rule,
					     double tolerance,
					     cairo_antialias_t antialias)
{
    cairo_quartzgl_surface_t *surface = (cairo_quartzgl_surface_t *) abstract_surface;

    //fprintf (stderr, "%p _cairo_quartzgl_surface_intersect_clip_path path: %p\n", surface, path);

    if (path == NULL) {
	/* If we're being asked to reset the clip, we can only do it by
	 * restoring the gstate to our previous saved one, and saving it again
	 */

	CGContextRestoreGState (surface->cgContext);
	CGContextSaveGState (surface->cgContext);

	return CAIRO_STATUS_SUCCESS;
    }

#if 0
    CGContextSetRGBStrokeColor (surface->cgContext, 1.0, 0.0, 0.0, 1.0);
    CGContextBeginPath (surface->cgContext);
    _cairo_quartzgl_cairo_path_to_quartz_context (path, surface->cgContext);
    CGContextStrokePath (surface->cgContext);
#endif

    CGContextBeginPath (surface->cgContext);
    _cairo_quartzgl_cairo_path_to_quartz_context (path, surface->cgContext);
    if (fill_rule == CAIRO_FILL_RULE_WINDING)
	CGContextClip (surface->cgContext);
    else
	CGContextEOClip (surface->cgContext);

    return CAIRO_STATUS_SUCCESS;
}

static const struct _cairo_surface_backend cairo_quartzgl_surface_backend = {
    _cairo_quartzgl_surface_create_similar,
    _cairo_quartzgl_surface_finish,
    _cairo_quartzgl_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_quartzgl_surface_acquire_dest_image,
    _cairo_quartzgl_surface_release_dest_image,
    _cairo_quartzgl_surface_clone_similar,
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_quartzgl_surface_intersect_clip_path,
    _cairo_quartzgl_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    _cairo_quartzgl_surface_paint,
    _cairo_quartzgl_surface_mask,
    _cairo_quartzgl_surface_stroke,
    _cairo_quartzgl_surface_fill,
    _cairo_quartzgl_surface_show_glyphs,

    NULL, /* snapshot */
};

cairo_bool_t
cairo_surface_is_quartzgl (cairo_surface_t *surf)
{
    return (surf->backend == &cairo_quartzgl_surface_backend);
}

static cairo_quartzgl_surface_t *
_cairo_quartzgl_surface_create_internal (CGContextRef cgContext,
					 AGLContext aglContext,
					 unsigned int width,
					 unsigned int height,
					 cairo_bool_t y_grows_down)
{
    cairo_quartzgl_surface_t *surface;

    /* Init the base surface */
    surface = malloc(sizeof(cairo_quartzgl_surface_t));
    if (surface == NULL) {
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    memset(surface, 0, sizeof(cairo_quartzgl_surface_t));

    _cairo_surface_init(&surface->base, &cairo_quartzgl_surface_backend);

    /* Save our extents */
    surface->extents.x = surface->extents.y = 0;
    surface->extents.width = width;
    surface->extents.height = height;

    if (y_grows_down) {
	/* Then make the CGContext sane */
	CGContextTranslateCTM (cgContext, 0.0, surface->extents.height);
	CGContextScaleCTM (cgContext, 1.0, -1.0);
    }

    /* Save so we can always get back to a known-good CGContext */
    CGContextSaveGState (cgContext);

    surface->y_grows_down = y_grows_down;
    surface->aglContext = aglContext;
    surface->cgContext = cgContext;
    surface->imageData = NULL;

    return surface;
}
					 
cairo_surface_t *
cairo_quartzgl_surface_create_for_agl_context (AGLContext aglContext,
					       unsigned int width,
					       unsigned int height,
					       cairo_bool_t y_grows_down)
{
    cairo_quartzgl_surface_t *surf;
    CGSize sz;

    /* Make our CGContext from the AGL context */
    sz.width = width;
    sz.height = height;

    CGContextRef cgc = CGGLContextCreate (aglContext, sz, NULL /* device RGB colorspace */);
    fprintf (stderr, "cgc: %p\n", cgc);

    surf = _cairo_quartzgl_surface_create_internal (cgc, aglContext, width, height, y_grows_down);
    if (!surf) {
	CGContextRelease (cgc);
	// create_internal will have set an error
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return (cairo_surface_t *) surf;
}

cairo_surface_t *
cairo_quartzgl_surface_create_for_cg_context (CGContextRef cgContext,
					      unsigned int width,
					      unsigned int height,
					      cairo_bool_t y_grows_down)
{
    cairo_quartzgl_surface_t *surf;

    CGContextRetain (cgContext);

    surf = _cairo_quartzgl_surface_create_internal (cgContext, NULL, width, height, y_grows_down);
    if (!surf) {
	CGContextRelease (cgContext);
	// create_internal will have set an error
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    return (cairo_surface_t *) surf;
}

cairo_surface_t *
cairo_quartzgl_surface_create (cairo_format_t format,
			       unsigned int width,
			       unsigned int height,
			       cairo_bool_t y_grows_down)
{
    cairo_quartzgl_surface_t *surf;
    CGContextRef cgc;
    CGColorSpaceRef cgColorspace;
    CGBitmapInfo bitinfo;
    void *imageData;
    int stride;
    int bitsPerComponent;

    if (format == CAIRO_FORMAT_ARGB32) {
	cgColorspace = CGColorSpaceCreateDeviceRGB();
	stride = width * 4;
	bitinfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_RGB24) {
	cgColorspace = CGColorSpaceCreateDeviceRGB();
	stride = width * 4;
	bitinfo = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_A8) {
	cgColorspace = CGColorSpaceCreateDeviceGray();
	if (width % 4 == 0)
	    stride = width;
	else
	    stride = (width & 3) + 1;
	bitinfo = kCGImageAlphaNone;
	bitsPerComponent = 8;
    } else if (format == CAIRO_FORMAT_A1) {
	/* I don't think we can usefully support this, as defined by
	 * cairo_format_t -- these are 1-bit pixels stored in 32-bit
	 * quantities.
	 */
	_cairo_error (CAIRO_STATUS_INVALID_FORMAT);
        return (cairo_surface_t*) &_cairo_surface_nil;
    } else {
	_cairo_error (CAIRO_STATUS_INVALID_FORMAT);
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    imageData = malloc (height * stride);
    if (!imageData) {
	CGColorSpaceRelease (cgColorspace);
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    cgc = CGBitmapContextCreate (imageData,
				 width,
				 height,
				 bitsPerComponent,
				 stride,
				 cgColorspace,
				 bitinfo);
    CGColorSpaceRelease (cgColorspace);

    surf = _cairo_quartzgl_surface_create_internal (cgc, NULL, width, height, y_grows_down);
    if (!surf) {
	CGContextRelease (cgc);
	// create_internal will have set an error
        return (cairo_surface_t*) &_cairo_surface_nil;
    }

    surf->imageData = imageData;

    return (cairo_surface_t *) surf;
}
