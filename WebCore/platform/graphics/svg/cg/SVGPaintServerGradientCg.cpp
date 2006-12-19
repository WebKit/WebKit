/*
    Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGPaintServerGradient.h"

#include "GraphicsContext.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"
#include "RenderPath.h"
#include "CgSupport.h"

namespace WebCore {

static void cgGradientCallback(void* info, const CGFloat* inValues, CGFloat* outColor)
{
    const SVGPaintServerGradient* server = (const SVGPaintServerGradient*)info;
    SVGPaintServerGradient::QuartzGradientStop* stops = server->m_stopsCache;
    int stopsCount = server->m_stopsCount;

    CGFloat inValue = inValues[0];

    if (!stopsCount) {
        outColor[0] = 0;
        outColor[1] = 0;
        outColor[2] = 0;
        outColor[3] = 1;
        return;
    } else if (stopsCount == 1) {
        memcpy(outColor, stops[0].colorArray, 4 * sizeof(CGFloat));
        return;
    }

    if (!(inValue > stops[0].offset))
        memcpy(outColor, stops[0].colorArray, 4 * sizeof(CGFloat));
    else if (!(inValue < stops[stopsCount-1].offset))
        memcpy(outColor, stops[stopsCount-1].colorArray, 4 * sizeof(CGFloat));
    else {
        int nextStopIndex = 0;
        while ((nextStopIndex < stopsCount) && (stops[nextStopIndex].offset < inValue))
            nextStopIndex++;

        //float nextOffset = stops[nextStopIndex].offset;
        CGFloat *nextColorArray = stops[nextStopIndex].colorArray;
        CGFloat *previousColorArray = stops[nextStopIndex-1].colorArray;
        //float totalDelta = nextOffset - previousOffset;
        CGFloat diffFromPrevious = inValue - stops[nextStopIndex-1].offset;
        //float percent = diffFromPrevious / totalDelta;
        CGFloat percent = diffFromPrevious * stops[nextStopIndex].previousDeltaInverse;

        outColor[0] = ((1.0 - percent) * previousColorArray[0] + percent * nextColorArray[0]);
        outColor[1] = ((1.0 - percent) * previousColorArray[1] + percent * nextColorArray[1]);
        outColor[2] = ((1.0 - percent) * previousColorArray[2] + percent * nextColorArray[2]);
        outColor[3] = ((1.0 - percent) * previousColorArray[3] + percent * nextColorArray[3]);
    }
    // FIXME: have to handle the spreadMethod()s here SPREADMETHOD_REPEAT, etc.
}

static CGShadingRef CGShadingRefForLinearGradient(const SVGPaintServerLinearGradient* server)
{
    CGPoint start = CGPoint(server->gradientStart());
    CGPoint end = CGPoint(server->gradientEnd());

    CGFunctionCallbacks callbacks = {0, cgGradientCallback, NULL};
    CGFloat domainLimits[2] = {0, 1};
    CGFloat rangeLimits[8] = {0, 1, 0, 1, 0, 1, 0, 1};
    CGFunctionRef shadingFunction = CGFunctionCreate((void *)server, 1, domainLimits, 4, rangeLimits, &callbacks);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGShadingRef shading = CGShadingCreateAxial(colorSpace, start, end, shadingFunction, true, true);
    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(shadingFunction);
    return shading;
}

static CGShadingRef CGShadingRefForRadialGradient(const SVGPaintServerRadialGradient* server)
{
    CGPoint center = CGPoint(server->gradientCenter());
    CGPoint focus = CGPoint(server->gradientFocal());
    double radius = server->gradientRadius();

    double fdx = focus.x - center.x;
    double fdy = focus.y - center.y;

    // Spec: If (fx, fy) lies outside the circle defined by (cx, cy) and r, set (fx, fy)
    // to the point of intersection of the line through (fx, fy) and the circle.
    if (sqrt(fdx*fdx + fdy*fdy) > radius) {
        double angle = atan2(focus.y, focus.x);
        focus.x = int(cos(angle) * radius) - 1;
        focus.y = int(sin(angle) * radius) - 1;
    }

    CGFunctionCallbacks callbacks = {0, cgGradientCallback, NULL};
    CGFloat domainLimits[2] = {0, 1};
    CGFloat rangeLimits[8] = {0, 1, 0, 1, 0, 1, 0, 1};
    CGFunctionRef shadingFunction = CGFunctionCreate((void *)server, 1, domainLimits, 4, rangeLimits, &callbacks);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGShadingRef shading = CGShadingCreateRadial(colorSpace, focus, 0, center, radius, shadingFunction, true, true);
    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(shadingFunction);
    return shading;
}

void SVGPaintServerGradient::invalidateCaches()
{
    delete m_stopsCache;
    CGShadingRelease(m_shadingCache);

    m_stopsCache = 0;
    m_shadingCache = 0;
}

void SVGPaintServerGradient::updateQuartzGradientStopsCache(const Vector<SVGGradientStop>& stops)
{
    delete m_stopsCache;

    m_stopsCount = stops.size();
    m_stopsCache = new SVGPaintServerGradient::QuartzGradientStop[m_stopsCount];

    CGFloat previousOffset = 0.0;
    for (unsigned i = 0; i < stops.size(); ++i) {
        m_stopsCache[i].offset = stops[i].first;
        m_stopsCache[i].previousDeltaInverse = 1.0 / (stops[i].first - previousOffset);
        previousOffset = stops[i].first;
        CGFloat *ca = m_stopsCache[i].colorArray;
        stops[i].second.getRGBA(ca[0], ca[1], ca[2], ca[3]);
    }
}

void SVGPaintServerGradient::updateQuartzGradientCache(const SVGPaintServerGradient* server)
{
    // cache our own copy of the stops for faster access.
    // this is legacy code, probably could be reworked.
    if (!m_stopsCache)
        updateQuartzGradientStopsCache(gradientStops());

    if (m_shadingCache)
        CGShadingRelease(m_shadingCache);

    if (type() == RadialGradientPaintServer) {
        const SVGPaintServerRadialGradient* radial = static_cast<const SVGPaintServerRadialGradient*>(server);
        m_shadingCache = CGShadingRefForRadialGradient(radial);
    } else if (type() == LinearGradientPaintServer) {
        const SVGPaintServerLinearGradient* linear = static_cast<const SVGPaintServerLinearGradient*>(server);
        m_shadingCache = CGShadingRefForLinearGradient(linear);
    }
}

void SVGPaintServerGradient::teardown(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type) const
{
    CGShadingRef shading = m_shadingCache;
    CGContextRef contextRef = context->platformContext();
    RenderStyle* style = object->style();
    ASSERT(contextRef);

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        // workaround for filling the entire screen with the shading in the case that no text was intersected with the clip
        if (!isPaintingText() || (object->width() > 0 && object->height() > 0))
            CGContextDrawShading(contextRef, shading);
        context->restore();     
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        if (isPaintingText()) {
            IntRect maskRect = const_cast<RenderObject*>(object)->absoluteBoundingBoxRect();
            maskRect = object->absoluteTransform().inverse().mapRect(maskRect);

            // Translate from 0x0 image origin to actual rendering position
            m_savedContext->translate(maskRect.x(), maskRect.y());

            // Clip current context to mask image (gradient)
            CGContextClipToMask(m_savedContext->platformContext(), CGRectMake(0, 0, maskRect.width(), maskRect.height()), m_imageBuffer->cgImage());
            m_savedContext->translate(-maskRect.x(), -maskRect.y());

            // Restore on-screen drawing context, after we got the image of the gradient
            delete m_imageBuffer;
            context = m_savedContext;
            contextRef = context->platformContext();
            m_savedContext = 0;
        }

        CGContextDrawShading(contextRef, shading);
        context->restore();
    }

    context->restore();
}

void SVGPaintServerGradient::renderPath(GraphicsContext*& context, const RenderPath* path, SVGPaintTargetType type) const
{
    CGContextRef contextRef = context->platformContext();
    RenderStyle* style = path->style();
    ASSERT(contextRef);

    CGRect objectBBox;
    if (boundingBoxMode())
        objectBBox = CGContextGetPathBoundingBox(contextRef);
    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill())
        clipToFillPath(contextRef, path);
    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke())
        clipToStrokePath(contextRef, path);
    // make the gradient fit in the bbox if necessary.
    if (boundingBoxMode()) { // no support for bounding boxes around text yet!
        // get the object bbox
        CGRect gradientBBox = CGRectMake(0,0,100,100); // FIXME - this is arbitrary no?
        // generate a transform to map between the two.
        CGAffineTransform gradientIntoObjectBBox = CGAffineTransformMakeMapBetweenRects(gradientBBox, objectBBox);
        CGContextConcatCTM(contextRef, gradientIntoObjectBBox);
    }

    // apply the gradient's own transform
    CGAffineTransform transform = gradientTransform();
    CGContextConcatCTM(contextRef, transform);
}

bool SVGPaintServerGradient::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type) const
{
    if (listener()) // this seems like bad design to me, should be in a common baseclass. -- ecs 8/6/05
        listener()->resourceNotification();

    // FIXME: total const HACK!
    // We need a hook to call this when the gradient gets updated, before drawn.
    if (!m_shadingCache)
        const_cast<SVGPaintServerGradient*>(this)->updateQuartzGradientCache(this);

    CGContextRef contextRef = context->platformContext();
    RenderStyle* style = object->style();
    ASSERT(contextRef);

    context->save();
    CGContextSetAlpha(contextRef, style->opacity());

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        context->save();      
        if (isPaintingText())
            context->setTextDrawingMode(cTextClip);
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        context->save();
        applyStrokeStyleToContext(contextRef, style, object); // FIXME: this seems like the wrong place for this.
        if (isPaintingText()) {
            IntRect maskRect = const_cast<RenderObject*>(object)->absoluteBoundingBoxRect();
            maskRect = object->absoluteTransform().inverse().mapRect(maskRect);

            ImageBuffer* maskImage(GraphicsContext::createImageBuffer(IntSize(maskRect.width(), maskRect.height()), false));
            GraphicsContext* maskImageContext = maskImage->context();

            maskImageContext->save();
            maskImageContext->translate(-maskRect.x(), -maskRect.y());

            const_cast<RenderObject*>(object)->style()->setColor(Color(255, 255, 255));
            maskImageContext->setTextDrawingMode(cTextStroke);

            m_imageBuffer = maskImage;
            m_savedContext = context;
            context = maskImageContext;
        }
    }

    return true;
}

void SVGPaintServerGradient::invalidate()
{
    invalidateCaches();
}

} // namespace WebCore

#endif

// vim:ts=4:noet
