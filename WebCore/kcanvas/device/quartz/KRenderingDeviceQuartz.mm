/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#include "config.h"
#import "KRenderingDeviceQuartz.h"
#import "KCanvasResourcesQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "KRenderingPaintServerQuartz.h"
#import "QuartzSupport.h"

#import "kcanvas/KCanvas.h"
#import "KCanvasMatrix.h"
#import "KCanvasItemQuartz.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"

#import <AppKit/NSGraphicsContext.h>

#import "KWQLogging.h"


KCanvasQuartzPathData::KCanvasQuartzPathData()
{
	path = CGPathCreateMutable();
	hasValidBBox = false;
}

KCanvasQuartzPathData::~KCanvasQuartzPathData()
{
	CGPathRelease(path);
}

KRenderingDeviceContextQuartz::KRenderingDeviceContextQuartz(CGContextRef context) : m_cgContext(CGContextRetain(context)), m_nsGraphicsContext(0)
{
    ASSERT(m_cgContext);
}

KRenderingDeviceContextQuartz::~KRenderingDeviceContextQuartz()
{
    CGContextRelease(m_cgContext);
    [m_nsGraphicsContext release];
}

KCanvasMatrix KRenderingDeviceContextQuartz::concatCTM(const KCanvasMatrix &worldMatrix)
{
    KCanvasMatrix ret = ctm();
    CGAffineTransform wMatrix = CGAffineTransformMake(worldMatrix.a(), worldMatrix.b(), worldMatrix.c(),
                                                     worldMatrix.d(), worldMatrix.e(), worldMatrix.f());
    CGContextConcatCTM(m_cgContext, wMatrix);
    return ret;
}

KCanvasMatrix KRenderingDeviceContextQuartz::ctm() const
{
    CGAffineTransform contextCTM = CGContextGetCTM(m_cgContext);
    return KCanvasMatrix(contextCTM.a, contextCTM.b, contextCTM.c, contextCTM.d, contextCTM.tx, contextCTM.ty);
}

QRect KRenderingDeviceContextQuartz::mapFromVisual(const QRect &rect)
{
	NSLog(@"mapFromVisual not yet for Quartz");
	return QRect();
}

QRect KRenderingDeviceContextQuartz::mapToVisual(const QRect &rect)
{
	NSLog(@"mapToVisual not yet for Quartz");
	return QRect();
}

NSGraphicsContext *KRenderingDeviceContextQuartz::nsGraphicsContext()
{
    if (!m_nsGraphicsContext && m_cgContext)
        m_nsGraphicsContext = [[NSGraphicsContext graphicsContextWithGraphicsPort:m_cgContext flipped:NO] retain];
    return m_nsGraphicsContext;
}

static bool __useFilters = true;

bool KRenderingDeviceQuartz::filtersEnabled()
{
    return __useFilters;
}

void KRenderingDeviceQuartz::setFiltersEnabled(bool enabled)
{
    __useFilters = enabled;
}

static bool __useHardwareRendering = true;

bool KRenderingDeviceQuartz::hardwareRenderingEnabled()
{
    return __useHardwareRendering;
}

void KRenderingDeviceQuartz::setHardwareRenderingEnabled(bool enabled)
{
    __useHardwareRendering = enabled;
}

#pragma mark -
#pragma mark Context Management

KRenderingDeviceContextQuartz *KRenderingDeviceQuartz::quartzContext() const
{
	return static_cast<KRenderingDeviceContextQuartz *>(currentContext());
}

CGContextRef KRenderingDeviceQuartz::currentCGContext() const
{
    ASSERT(quartzContext());
    return quartzContext()->cgContext();
}

void KRenderingDeviceQuartz::pushContext(KRenderingDeviceContext *context)
{
    ASSERT(context);
    KRenderingDevice::pushContext(context);
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:quartzContext()->nsGraphicsContext()];
    ASSERT(quartzContext()->nsGraphicsContext() == [NSGraphicsContext currentContext]);
    ASSERT(currentCGContext() == QPainter().currentContext());
}

KRenderingDeviceContext *KRenderingDeviceQuartz::popContext()
{
    [NSGraphicsContext restoreGraphicsState];
    KRenderingDeviceContext *poppedContext = KRenderingDevice::popContext();
    ASSERT(!currentContext() || (currentCGContext() == QPainter().currentContext()));
    return poppedContext;
}


#pragma mark -
#pragma mark Path Management

KRenderingDeviceContext *KRenderingDeviceQuartz::contextForImage(KCanvasImage *image) const
{
    KCanvasImageQuartz *quartzImage = static_cast<KCanvasImageQuartz *>(image);
    CGLayerRef cgLayer = quartzImage->cgLayer();
    if (!cgLayer) {
        // FIXME: we might not get back a layer if this is a loaded image
        // maybe this logic should go into KCanvasImage?
        cgLayer = CGLayerCreateWithContext(currentCGContext(), CGSize(image->size() + QSize(1,1)), NULL);  // FIXME + 1 is a hack
        // FIXME: we should composite the original image onto the layer...
        quartzImage->setCGLayer(cgLayer);
        CGLayerRelease(cgLayer);
    }
    return new KRenderingDeviceContextQuartz(CGLayerGetContext(cgLayer));
}

void KRenderingDeviceQuartz::deletePath(KCanvasUserData pathData)
{
	KCanvasQuartzPathData *data = static_cast<KCanvasQuartzPathData *>(pathData);
	ASSERT(data != 0);
	
	delete data;
}

void KRenderingDeviceQuartz::startPath()
{
	setCurrentPath(new KCanvasQuartzPathData());
}

void KRenderingDeviceQuartz::endPath()
{
	// NOOP for Quartz.
}

void KRenderingDeviceQuartz::moveTo(double x, double y)
{
	CGMutablePathRef path = static_cast<KCanvasQuartzPathData *>(currentPath())->path;
	ASSERT(path != 0);

	CGPathMoveToPoint(path,NULL,x, y);
}

void KRenderingDeviceQuartz::lineTo(double x, double y)
{
	KCanvasQuartzPathData *pathData = static_cast<KCanvasQuartzPathData *>(currentPath());
	CGMutablePathRef path = pathData->path;
	ASSERT(path != 0);

	CGPathAddLineToPoint(path,NULL,x, y);
	pathData->hasValidBBox = false;
}

void KRenderingDeviceQuartz::curveTo(double x1, double y1, double x2, double y2, double endX, double endY)
{
	KCanvasQuartzPathData *pathData = static_cast<KCanvasQuartzPathData *>(currentPath());
	CGMutablePathRef path = pathData->path;
	ASSERT(path != 0);

	CGPathAddCurveToPoint(path, NULL, x1, y1, x2, y2, endX, endY);
	pathData->hasValidBBox = false;
}

void KRenderingDeviceQuartz::closeSubpath()
{
	CGMutablePathRef path = static_cast<KCanvasQuartzPathData *>(currentPath())->path;
	ASSERT(path != 0);

	CGPathCloseSubpath(path);
}

KCanvasUserData KRenderingDeviceQuartz::pathForRect(const QRect &rect) const
{
	KCanvasQuartzPathData *data = new KCanvasQuartzPathData();
	CGPathAddRect(data->path, NULL, CGRectMake(rect.x(),rect.y(),rect.width(),rect.height()));
	return data;
}

QString KRenderingDeviceQuartz::stringForPath(KCanvasUserData path)
{
    QString result;
    if (path) {
        CGMutablePathRef cgPath = static_cast<KCanvasQuartzPathData *>(path)->path;
        CFStringRef pathString = CFStringFromCGPath(cgPath);
        result = QString::fromCFString(pathString);
        CFRelease(pathString);
    }
    return result;
}

#pragma mark -
#pragma mark Resource Creation

KRenderingPaintServer *KRenderingDeviceQuartz::createPaintServer(const KCPaintServerType &type) const
{
	KRenderingPaintServer *newServer = NULL;
	switch(type)
	{
		case PS_SOLID: newServer = new KRenderingPaintServerSolidQuartz(); break;
		case PS_PATTERN: newServer = new KRenderingPaintServerPatternQuartz(); break;
		case PS_IMAGE: newServer = new KRenderingPaintServerImageQuartz(); break;
		case PS_LINEAR_GRADIENT: newServer = new KRenderingPaintServerLinearGradientQuartz(); break;
		case PS_RADIAL_GRADIENT: newServer = new KRenderingPaintServerRadialGradientQuartz(); break;
	}
	return newServer;
}
 
KCanvasContainer *KRenderingDeviceQuartz::createContainer(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) const
{
    return new (arena) KCanvasContainerQuartz(node);
}

RenderPath *KRenderingDeviceQuartz::createItem(RenderArena *arena, khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node, KCanvasUserData path) const
{
    RenderPath *item = new (arena) KCanvasItemQuartz(style, node);
    item->changePath(path);
    return item;
}

KCanvasResource *KRenderingDeviceQuartz::createResource(const KCResourceType &type) const
{
	switch(type)
	{
		case RS_CLIPPER: return new KCanvasClipperQuartz();
		case RS_MARKER: return new KCanvasMarker(); // Use default implementation...
		case RS_IMAGE: return new KCanvasImageQuartz();
		case RS_FILTER: return new KCanvasFilterQuartz();
	}
	NSLog(@"ERROR: Failed to create resource of type: %i", type);
	return NULL;
}

KCanvasFilterEffect *KRenderingDeviceQuartz::createFilterEffect(const KCFilterEffectType &type) const
{
	switch(type)
	{
//	case FE_DISTANT_LIGHT: 
//	case FE_POINT_LIGHT: 
//	case FE_SPOT_LIGHT: 
	case FE_BLEND: return new KCanvasFEBlendQuartz();
	case FE_COLOR_MATRIX: return new KCanvasFEColorMatrixQuartz();
//	case FE_COMPONENT_TRANSFER: 
	case FE_COMPOSITE: return new KCanvasFECompositeQuartz();
//	case FE_CONVOLVE_MATRIX: 
//	case FE_DIFFUSE_LIGHTING: 
//	case FE_DISPLACEMENT_MAP: 
	case FE_FLOOD: return new KCanvasFEFloodQuartz();
	case FE_GAUSSIAN_BLUR: return new KCanvasFEGaussianBlurQuartz();
	case FE_IMAGE: return new KCanvasFEImageQuartz();
	case FE_MERGE: return new KCanvasFEMergeQuartz();
//	case FE_MORPHOLOGY: 
	case FE_OFFSET: return new KCanvasFEOffsetQuartz();
//	case FE_SPECULAR_LIGHTING: 
	case FE_TILE: return new KCanvasFETileQuartz();
//	case FE_TURBULENCE: 
	default:
		NSLog(@"Unsupported filter of type: %i!", type);
		return NULL;
	}
}

