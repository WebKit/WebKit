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
#if SVG_SUPPORT
#import "KRenderingDeviceQuartz.h"

#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "KCanvasFilterQuartz.h"
#import "KCanvasItemQuartz.h"
#import "KCanvasMaskerQuartz.h"
#import "KCanvasMatrix.h"
#import "KCanvasPathQuartz.h"
#import "KCanvasResourcesQuartz.h"
#import "KRenderingFillPainter.h"
#import "KRenderingPaintServerQuartz.h"
#import "KRenderingStrokePainter.h"
#import "Logging.h"
#import "QuartzSupport.h"
#import "RenderCanvas.h"
#import <Cocoa/Cocoa.h>

namespace WebCore {

KRenderingDeviceContextQuartz::KRenderingDeviceContextQuartz(CGContextRef context) : m_cgContext(CGContextRetain(context)), m_nsGraphicsContext(0)
{
    ASSERT(m_cgContext);
}

KRenderingDeviceContextQuartz::~KRenderingDeviceContextQuartz()
{
    CGContextRelease(m_cgContext);
    KWQRelease(m_nsGraphicsContext);
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

IntRect KRenderingDeviceContextQuartz::mapFromVisual(const IntRect &rect)
{
    return IntRect();
}

IntRect KRenderingDeviceContextQuartz::mapToVisual(const IntRect &rect)
{
    return IntRect();
}

void KRenderingDeviceContextQuartz::clearPath()
{
    CGContextBeginPath(m_cgContext);
}

void KRenderingDeviceContextQuartz::addPath(const KCanvasPath *path)
{
    CGContextAddPath(m_cgContext, static_cast<const KCanvasPathQuartz*>(path)->cgPath());
}

NSGraphicsContext *KRenderingDeviceContextQuartz::nsGraphicsContext()
{
    if (!m_nsGraphicsContext && m_cgContext)
        m_nsGraphicsContext = KWQRetain([NSGraphicsContext graphicsContextWithGraphicsPort:m_cgContext flipped:YES]);
    return m_nsGraphicsContext;
}

GraphicsContext* KRenderingDeviceContextQuartz::createGraphicsContext()
{
    return new GraphicsContext(m_cgContext);
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
}

KRenderingDeviceContext *KRenderingDeviceQuartz::popContext()
{
    [NSGraphicsContext restoreGraphicsState];
    KRenderingDeviceContext *poppedContext = KRenderingDevice::popContext();
    ASSERT(!currentContext() || currentCGContext() == [[NSGraphicsContext currentContext] graphicsPort]);
    return poppedContext;
}

KRenderingDeviceContext *KRenderingDeviceQuartz::contextForImage(KCanvasImage *image) const
{
    KCanvasImageQuartz *quartzImage = static_cast<KCanvasImageQuartz *>(image);
    CGLayerRef cgLayer = quartzImage->cgLayer();
    if (!cgLayer) {
        // FIXME: we might not get back a layer if this is a loaded image
        // maybe this logic should go into KCanvasImage?
        cgLayer = CGLayerCreateWithContext(currentCGContext(), CGSize(image->size() + IntSize(1,1)), NULL);  // FIXME + 1 is a hack
        // FIXME: we should composite the original image onto the layer...
        quartzImage->setCGLayer(cgLayer);
        CGLayerRelease(cgLayer);
    }
    return new KRenderingDeviceContextQuartz(CGLayerGetContext(cgLayer));
}

#pragma mark -
#pragma mark Path Management

KCanvasPath* KRenderingDeviceQuartz::createPath() const
{
    return new KCanvasPathQuartz();
}

DeprecatedString KRenderingDeviceQuartz::stringForPath(const KCanvasPath *path)
{
    DeprecatedString result;
    if (path) {
        CGPathRef cgPath = static_cast<const KCanvasPathQuartz *>(path)->cgPath();
        CFStringRef pathString = CFStringFromCGPath(cgPath);
        result = DeprecatedString::fromCFString(pathString);
        CFRelease(pathString);
    }
    return result;
}

#pragma mark -
#pragma mark Resource Creation

KRenderingPaintServer *KRenderingDeviceQuartz::createPaintServer(const KCPaintServerType &type) const
{
    KRenderingPaintServer *newServer = NULL;
    switch(type) {
    case PS_SOLID:
        newServer = new KRenderingPaintServerSolidQuartz();
        break;
    case PS_PATTERN:
        newServer = new KRenderingPaintServerPatternQuartz();
        break;
    case PS_LINEAR_GRADIENT:
        newServer = new KRenderingPaintServerLinearGradientQuartz();
        break;
    case PS_RADIAL_GRADIENT:
        newServer = new KRenderingPaintServerRadialGradientQuartz();
        break;
    }
    return newServer;
}

RenderPath *KRenderingDeviceQuartz::createItem(RenderArena *arena, RenderStyle *style, SVGStyledElement *node, KCanvasPath* path) const
{
    RenderPath *item = new (arena) KCanvasItemQuartz(style, node);
    item->setPath(path);
    return item;
}

KCanvasResource *KRenderingDeviceQuartz::createResource(const KCResourceType &type) const
{
    switch (type) {
    case RS_CLIPPER:
        return new KCanvasClipperQuartz();
    case RS_MARKER:
        return new KCanvasMarker();
    case RS_IMAGE:
        return new KCanvasImageQuartz();
    case RS_FILTER:
        return new KCanvasFilterQuartz();
    case RS_MASKER:
        return new KCanvasMaskerQuartz();
    }
    LOG_ERROR("Failed to create resource of type: %i", type);
    return 0;
}

KCanvasFilterEffect *KRenderingDeviceQuartz::createFilterEffect(const KCFilterEffectType &type) const
{
    switch(type)
    {
    /* Light sources are contained by the diffuse/specular light blocks 
    case FE_DISTANT_LIGHT: 
    case FE_POINT_LIGHT: 
    case FE_SPOT_LIGHT: 
    */
    case FE_BLEND: return new KCanvasFEBlendQuartz();
    case FE_COLOR_MATRIX: return new KCanvasFEColorMatrixQuartz();
    case FE_COMPONENT_TRANSFER: return new KCanvasFEComponentTransferQuartz();
    case FE_COMPOSITE: return new KCanvasFECompositeQuartz();
//  case FE_CONVOLVE_MATRIX: 
    case FE_DIFFUSE_LIGHTING: return new KCanvasFEDiffuseLightingQuartz();
    case FE_DISPLACEMENT_MAP: return new KCanvasFEDisplacementMapQuartz();
    case FE_FLOOD: return new KCanvasFEFloodQuartz();
    case FE_GAUSSIAN_BLUR: return new KCanvasFEGaussianBlurQuartz();
    case FE_IMAGE: return new KCanvasFEImageQuartz();
    case FE_MERGE: return new KCanvasFEMergeQuartz();
//  case FE_MORPHOLOGY: 
    case FE_OFFSET: return new KCanvasFEOffsetQuartz();
    case FE_SPECULAR_LIGHTING: return new KCanvasFESpecularLightingQuartz();
    case FE_TILE: return new KCanvasFETileQuartz();
//  case FE_TURBULENCE: 
    default:
        return 0;
    }
}

KRenderingDevice* renderingDevice()
{
    static KRenderingDevice* sharedRenderingDevice = new KRenderingDeviceQuartz();
    return sharedRenderingDevice;
}

}

#endif // SVG_SUPPORT
