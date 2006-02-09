/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *               2006 Alexander Kellett <lypanov@kde.org>
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
#import "KRenderingPaintServerQuartz.h"
#import "QuartzSupport.h"

#import "render_object.h"

#import "kcanvas/KCanvas.h"
#import "KCanvasRenderingStyle.h"
#import "KRenderingPaintServer.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasMatrix.h"
#import "KRenderingDeviceQuartz.h"

#import "KCanvasResourcesQuartz.h"
#import "KCanvasImage.h"


#import <kxmlcore/Assertions.h>


//typedef vector unsigned char vUInt8;
//vector float vec_loadAndSplatScalar( float *scalarPtr )
//{
//      vUInt8 splatMap = vec_lvsl( 0, scalarPtr );
//      vector float result = vec_lde( 0, scalarPtr );
//      splatMap = (vUInt8) vec_splat( (vector float) splatMap, 0 );
//      return vec_perm( result, result, splatMap );
//}
    
static void cgGradientCallback(void *info, const float *inValues, float *outColor)
{
    const KRenderingPaintServerGradientQuartz *server = (const KRenderingPaintServerGradientQuartz *)info;
    QuartzGradientStop *stops = server->m_stopsCache;
    int stopsCount = server->m_stopsCount;
    
    float inValue = inValues[0];
    
    if (!stopsCount) {
        outColor[0] = 0;
        outColor[1] = 0;
        outColor[2] = 0;
        outColor[3] = 1;
        return;
    } else if (stopsCount == 1) {
        memcpy(outColor, stops[0].colorArray, 4 * sizeof(float));
        return;
    }
    
    if (!(inValue > stops[0].offset)) {
        memcpy(outColor, stops[0].colorArray, 4 * sizeof(float));
    } else if (!(inValue < stops[stopsCount-1].offset)) {
        memcpy(outColor, stops[stopsCount-1].colorArray, 4 * sizeof(float));
    } else {
        int nextStopIndex = 0;
        while ( (nextStopIndex < stopsCount) && (stops[nextStopIndex].offset < inValue) ) {
            nextStopIndex++;
        }
        
        //float nextOffset = stops[nextStopIndex].offset;
        float *nextColorArray = stops[nextStopIndex].colorArray;
        float *previousColorArray = stops[nextStopIndex-1].colorArray;
        //float totalDelta = nextOffset - previousOffset;
        float diffFromPrevious = inValue - stops[nextStopIndex-1].offset;
        //float percent = diffFromPrevious / totalDelta;
        float percent = diffFromPrevious * stops[nextStopIndex].previousDeltaInverse;
        
#if 1
        outColor[0] = ((1.0f - percent) * previousColorArray[0] + percent * nextColorArray[0]);
        outColor[1] = ((1.0f - percent) * previousColorArray[1] + percent * nextColorArray[1]);
        outColor[2] = ((1.0f - percent) * previousColorArray[2] + percent * nextColorArray[2]);
        outColor[3] = ((1.0f - percent) * previousColorArray[3] + percent * nextColorArray[3]);
#else
        // load up vPercent = {percent, percent, percent, percent}
        vector float vPercent = vec_loadAndSplatScalar(percent);
        // result = -( arg1 * arg2 - arg3 ) = arg3 - arg2 * arg1
        vector float vPrevResult = vec_nmsub(vPrevColor, vPercent, vPrevColor);
        // result = arg1 * arg2 + arg3
        vector float vResult = vec_madd(vPercent, vNextColor, vPrevResult);
        vec_st(vResult, 0, outColor);
#endif
    }
    // FIXME: have to handle the spreadMethod()s here SPREADMETHOD_REPEAT, etc.
}


static CGShadingRef CGShadingRefForLinearGradient(const KRenderingPaintServerLinearGradientQuartz *server)
{
    CGPoint start = CGPoint(server->gradientStart());
    CGPoint end = CGPoint(server->gradientEnd());
    
    CGFunctionCallbacks callbacks = { 0, cgGradientCallback, NULL };
    float domainLimits[2] = { 0.0f, 1.0f };
    float rangeLimits[8] = { 0,1, 0,1, 0,1, 0,1 };
    const KRenderingPaintServerGradientQuartz *castServer = static_cast<const KRenderingPaintServerGradientQuartz *>(server);
    CGFunctionRef shadingFunction = CGFunctionCreate((void *)castServer, 1, domainLimits, 4, rangeLimits, &callbacks);
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGShadingRef shading = CGShadingCreateAxial(colorSpace, start, end, shadingFunction, true, true);
    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(shadingFunction);
    return shading;
}

static CGShadingRef CGShadingRefForRadialGradient(const KRenderingPaintServerRadialGradientQuartz *server)
{
    CGPoint center = CGPoint(server->gradientCenter());
    CGPoint focus = CGPoint(server->gradientFocal());
    double radius = server->gradientRadius();
    
    double fdx = focus.x - center.x;
    double fdy = focus.y - center.y;
    
    // Spec: If (fx, fy) lies outside the circle defined by (cx, cy) and r, set (fx, fy)
    // to the point of intersection of the line through (fx, fy) and the circle.
    if(sqrt(fdx*fdx + fdy*fdy) > radius)
    {
        double angle = atan2(focus.y, focus.x);
        focus.x = int(cos(angle) * radius) - 1;
        focus.y = int(sin(angle) * radius) - 1;
    }
    
    CGFunctionCallbacks callbacks = { 0, cgGradientCallback, NULL };
    float domainLimits[2] = { 0.0f, 1.0f };
    float rangeLimits[8] = { 0,1, 0,1, 0,1, 0,1 };
    const KRenderingPaintServerGradientQuartz *castServer = static_cast<const KRenderingPaintServerGradientQuartz *>(server);
    CGFunctionRef shadingFunction = CGFunctionCreate((void *)castServer, 1, domainLimits, 4, rangeLimits, &callbacks);
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGShadingRef shading = CGShadingCreateRadial(colorSpace, focus, 0, center, radius, shadingFunction, true, true);
    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(shadingFunction);
    return shading;
}

KRenderingPaintServerGradientQuartz::KRenderingPaintServerGradientQuartz() :
m_stopsCache(0), m_stopsCount(0), m_shadingCache(0), m_maskImage(0)
{
}

KRenderingPaintServerGradientQuartz::~KRenderingPaintServerGradientQuartz()
{
    delete m_stopsCache;
    CGShadingRelease(m_shadingCache);
    delete m_maskImage;
}

void KRenderingPaintServerGradientQuartz::updateQuartzGradientCache(const KRenderingPaintServerGradient *server)
{
    // cache our own copy of the stops for faster access.
    // this is legacy code, probably could be reworked.
    if (!m_stopsCache)
        updateQuartzGradientStopsCache(server->gradientStops());
    
    if (!m_stopsCount)
        NSLog(@"Warning, no gradient stops, gradient (%p) will be all black!", this);
    
    if (m_shadingCache)
        CGShadingRelease(m_shadingCache);
    if (server->type() == PS_RADIAL_GRADIENT) {
        const KRenderingPaintServerRadialGradientQuartz *radial = static_cast<const KRenderingPaintServerRadialGradientQuartz *>(server);
        m_shadingCache = CGShadingRefForRadialGradient(radial);
    } else if (server->type() == PS_LINEAR_GRADIENT) {
        const KRenderingPaintServerLinearGradientQuartz *linear = static_cast<const KRenderingPaintServerLinearGradientQuartz *>(server);
        m_shadingCache = CGShadingRefForLinearGradient(linear);
    }
}

void KRenderingPaintServerGradientQuartz::updateQuartzGradientStopsCache(const Vector<KCGradientStop>& stops)
{
    delete m_stopsCache;

    m_stopsCount = stops.size();
    m_stopsCache = new QuartzGradientStop[m_stopsCount];
    
    float previousOffset = 0.0f;
    for (unsigned i = 0; i < stops.size(); ++i) {
        m_stopsCache[i].offset = stops[i].first;
        m_stopsCache[i].previousDeltaInverse = 1.0f / (stops[i].first - previousOffset);
        previousOffset = stops[i].first;
        float *ca = m_stopsCache[i].colorArray;
        stops[i].second.getRgbaF(ca, ca+1, ca+2, ca+3);
    }
}

void KRenderingPaintServerGradientQuartz::invalidateCaches()
{
    delete m_stopsCache;
    CGShadingRelease(m_shadingCache);
    
    m_stopsCache = 0;
    m_shadingCache = 0;
}

void KRenderingPaintServerLinearGradientQuartz::invalidate()
{
    invalidateCaches();
    KRenderingPaintServerLinearGradient::invalidate();
}

void KRenderingPaintServerRadialGradientQuartz::invalidate()
{
    invalidateCaches();
    KRenderingPaintServerRadialGradient::invalidate();
}

void KRenderingPaintServerGradientQuartz::draw(const KRenderingPaintServerGradient* server, KRenderingDeviceContext* renderingContext, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(server, renderingContext, path, type))
        return;
    renderPath(server, renderingContext, path, type);
    teardown(server, renderingContext, path, type);
}

bool KRenderingPaintServerGradientQuartz::setup(const KRenderingPaintServerGradient* server, KRenderingDeviceContext* renderingContext, const khtml::RenderObject* renderObject, KCPaintTargetType type) const
{
    if (server->listener()) // this seems like bad design to me, should be in a common baseclass. -- ecs 8/6/05
        server->listener()->resourceNotification();
    
    delete m_maskImage;
    m_maskImage = 0;

    // FIXME: total const HACK!
    // We need a hook to call this when the gradient gets updated, before drawn.
    if (!m_shadingCache)
        const_cast<KRenderingPaintServerGradientQuartz*>(this)->updateQuartzGradientCache(server);
    
    KRenderingDeviceQuartz* quartzDevice = static_cast<KRenderingDeviceQuartz*>(QPainter::renderingDevice());
    CGContextRef context = quartzDevice->currentCGContext();
    khtml::RenderStyle* renderStyle = renderObject->style();
    ASSERT(context != NULL);
    
    CGContextSaveGState(context);
    // make the gradient fit in the bbox if necessary.
    if (server->boundingBoxMode() && renderObject->isRenderPath()) { // no support for bounding boxes around text yet!
        // get the object bbox
        CGRect objectBBox = CGContextGetPathBoundingBox(context);
        CGRect gradientBBox = CGRectMake(0,0,100,100); // FIXME - this is arbitrary no?
        // generate a transform to map between the two.
        CGAffineTransform gradientIntoObjectBBox = CGAffineTransformMakeMapBetweenRects(gradientBBox, objectBBox);
        CGContextConcatCTM(context, gradientIntoObjectBBox);
    }
    
    // apply the gradient's own transform
    CGAffineTransform gradientTransform = CGAffineTransform(server->gradientTransform().qmatrix());
    CGContextConcatCTM(context, gradientTransform);
    
    CGContextSetAlpha(context, renderStyle->opacity());
    
    if ((type & APPLY_TO_FILL) && KSVG::KSVGPainterFactory::isFilled(renderStyle)) {
        CGContextSaveGState(context);
        if (server->isPaintingText())
            CGContextSetTextDrawingMode(context, kCGTextClip);
    }

    if ((type & APPLY_TO_STROKE) && KSVG::KSVGPainterFactory::isStroked(renderStyle)) {
        CGContextSaveGState(context);
        applyStrokeStyleToContext(context, renderStyle, renderObject); // FIXME: this seems like the wrong place for this.
        if (server->isPaintingText()) {
            m_maskImage = static_cast<KCanvasImage *>(quartzDevice->createResource(RS_IMAGE));
            int width  = 2048;
            int height = 2048; // FIXME???
            IntSize size = IntSize(width, height);
            m_maskImage->init(size);
            KRenderingDeviceContext* maskImageContext = quartzDevice->contextForImage(m_maskImage);
            quartzDevice->pushContext(maskImageContext);
            CGContextRef maskContext = static_cast<KRenderingDeviceContextQuartz*>(maskImageContext)->cgContext();
            const_cast<khtml::RenderObject*>(renderObject)->style()->setColor(Color(255, 255, 255));
            CGContextSetTextDrawingMode(maskContext, kCGTextStroke);
        }
    }
    return true;
}

void KRenderingPaintServerGradientQuartz::renderPath(const KRenderingPaintServerGradient* server, KRenderingDeviceContext* renderingContext, const RenderPath* path, KCPaintTargetType type) const
{    
    KRenderingDeviceQuartz* quartzDevice = static_cast<KRenderingDeviceQuartz*>(QPainter::renderingDevice());
    CGContextRef context = quartzDevice->currentCGContext();
    khtml::RenderStyle* renderStyle = path->style();
    ASSERT(context != NULL);
    
    if ((type & APPLY_TO_FILL) && KSVG::KSVGPainterFactory::isFilled(renderStyle))
        KRenderingPaintServerQuartzHelper::clipToFillPath(context, path);
    if ((type & APPLY_TO_STROKE) && KSVG::KSVGPainterFactory::isStroked(renderStyle))
        KRenderingPaintServerQuartzHelper::clipToStrokePath(context, path);
}

void KRenderingPaintServerGradientQuartz::teardown(const KRenderingPaintServerGradient *server, KRenderingDeviceContext* renderingContext, const khtml::RenderObject* renderObject, KCPaintTargetType type) const
{ 
    CGShadingRef shading = m_shadingCache;
    KCanvasImage* maskImage = m_maskImage;
    KRenderingDeviceQuartz* quartzDevice = static_cast<KRenderingDeviceQuartz*>(QPainter::renderingDevice());
    CGContextRef context = quartzDevice->currentCGContext();
    khtml::RenderStyle* renderStyle = renderObject->style();
    ASSERT(context != NULL);
    
    if ((type & APPLY_TO_FILL) && KSVG::KSVGPainterFactory::isFilled(renderStyle)) {
        CGContextDrawShading(context, shading);
        CGContextRestoreGState(context);
    }
    
    if ((type & APPLY_TO_STROKE) && KSVG::KSVGPainterFactory::isStroked(renderStyle)) {
        if (server->isPaintingText()) {
            int width  = 2048;
            int height = 2048; // FIXME??? SEE ABOVE
            delete quartzDevice->popContext();
            context = quartzDevice->currentCGContext();
            void* imageBuffer = fastMalloc(width * height);
            CGColorSpaceRef grayColorSpace = CGColorSpaceCreateDeviceGray();
            CGContextRef grayscaleContext = CGBitmapContextCreate(imageBuffer, width, height, 8, width, grayColorSpace, kCGImageAlphaNone);
            CGColorSpaceRelease(grayColorSpace);
            KCanvasImageQuartz* qMaskImage = static_cast<KCanvasImageQuartz *>(maskImage);
            CGContextDrawLayerAtPoint(grayscaleContext, CGPointMake(0, 0), qMaskImage->cgLayer());
            CGImageRef grayscaleImage = CGBitmapContextCreateImage(grayscaleContext);
            CGContextClipToMask(context, CGRectMake(0, 0, width, height), grayscaleImage);
            CGContextRelease(grayscaleContext);
            CGImageRelease(grayscaleImage);
        }
        CGContextDrawShading(context, shading);
        CGContextRestoreGState(context);
    }
    
    CGContextRestoreGState(context);
}

#endif // SVG_SUPPORT
