/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 *               2006 Alexander Kellett <lypanov@kde.org>
 *               2006 Rob Buis <buis@kde.org>
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

#ifdef SVG_SUPPORT
#include "SVGResourceImage.h"
#include "KRenderingPaintServerQuartz.h"
#include "RenderPath.h"
#include "QuartzSupport.h"
#include "KRenderingDeviceQuartz.h"

#include "KCanvasRenderingStyle.h"
#include "KRenderingPaintServer.h"
#include "KRenderingDevice.h"

#include "Logging.h"

namespace WebCore {

void KRenderingPaintServerQuartzHelper::strokePath(CGContextRef context, const RenderPath* renderPath)
{
    CGContextStrokePath(context);
}

void KRenderingPaintServerQuartzHelper::clipToStrokePath(CGContextRef context, const RenderPath* renderPath)
{
    CGContextReplacePathWithStrokedPath(context);
    CGContextClip(context);
}    

void KRenderingPaintServerQuartzHelper::fillPath(CGContextRef context, const RenderPath* renderPath)
{
    if (renderPath->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
}

void KRenderingPaintServerQuartzHelper::clipToFillPath(CGContextRef context, const RenderPath* renderPath)
{
    if (renderPath->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOClip(context);
    else
        CGContextClip(context);
}    

void KRenderingPaintServerSolidQuartz::draw(KRenderingDeviceContext* renderingContext, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(renderingContext, path, type))
        return;
    renderPath(renderingContext, path, type);
    teardown(renderingContext, path, type);
}

bool KRenderingPaintServerSolidQuartz::setup(KRenderingDeviceContext* renderingContext, const RenderObject* renderObject, KCPaintTargetType type) const
{
    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(renderingContext);
    CGContextRef context = quartzContext->cgContext();
    RenderStyle* renderStyle = renderObject->style();

    CGContextSetAlpha(context, renderStyle->opacity());
    
    static CGColorSpaceRef deviceRGBColorSpace = CGColorSpaceCreateDeviceRGB(); // This should be shared from GraphicsContext, or some other central location

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill()) {
        CGFloat colorComponents[4];
        color().getRGBA(colorComponents[0], colorComponents[1], colorComponents[2], colorComponents[3]);
        ASSERT(!color().hasAlpha());
        colorComponents[3] = renderStyle->svgStyle()->fillOpacity(); // SVG/CSS colors are not specified w/o alpha
        CGContextSetFillColorSpace(context, deviceRGBColorSpace);
        CGContextSetFillColor(context, colorComponents);
        if (isPaintingText()) {
            const_cast<RenderObject*>(renderObject)->style()->setColor(color());
            CGContextSetTextDrawingMode(context, kCGTextFill);
        }
    }

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke()) {
        CGFloat colorComponents[4];
        color().getRGBA(colorComponents[0], colorComponents[1], colorComponents[2], colorComponents[3]);
        ASSERT(!color().hasAlpha());
        colorComponents[3] = renderStyle->svgStyle()->strokeOpacity(); // SVG/CSS colors are not specified w/o alpha
        CGContextSetStrokeColorSpace(context, deviceRGBColorSpace);
        CGContextSetStrokeColor(context, colorComponents);
        applyStrokeStyleToContext(context, renderStyle, renderObject);
        if (isPaintingText()) {
            const_cast<RenderObject*>(renderObject)->style()->setColor(color());
            CGContextSetTextDrawingMode(context, kCGTextStroke);
        }
    }
    
    return true;
}

void KRenderingPaintServerSolidQuartz::renderPath(KRenderingDeviceContext* renderingContext, const RenderPath* renderPath, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = renderPath->style();
    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(renderingContext);
    CGContextRef context = quartzContext->cgContext();    
    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        KRenderingPaintServerQuartzHelper::fillPath(context, renderPath);
    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        KRenderingPaintServerQuartzHelper::strokePath(context, renderPath);
}

void KRenderingPaintServerSolidQuartz::teardown(KRenderingDeviceContext* renderingContext, const RenderObject* renderObject, KCPaintTargetType type) const
{
}

void patternCallback(void *info, CGContextRef context)
{
    CGLayerRef layer = reinterpret_cast<SVGResourceImage*>(info)->cgLayer();
    CGContextDrawLayerAtPoint(context, CGPointZero, layer);
}

void KRenderingPaintServerPatternQuartz::draw(KRenderingDeviceContext* renderingContext, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(renderingContext, path, type))
        return;
    renderPath(renderingContext, path, type);
    teardown(renderingContext, path, type);
}

bool KRenderingPaintServerPatternQuartz::setup(KRenderingDeviceContext* renderingContext, const RenderObject* renderObject, KCPaintTargetType type) const
{
    if(listener()) // this seems like bad design to me, should be in a common baseclass. -- ecs 8/6/05
        listener()->resourceNotification();

    RenderStyle* renderStyle = renderObject->style();

    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(renderingContext);
    CGContextRef context = quartzContext->cgContext();

    RefPtr<SVGResourceImage> cell = tile();
    if (!cell)
        return false;

    CGContextSaveGState(context);

    CGSize cellSize = CGSize(cell->size());

    CGFloat alpha = 1; // canvasStyle->opacity(); //which?
            
    // Patterns don't seem to resepect the CTM unless we make them...
    CGAffineTransform ctm = CGContextGetCTM(context);
    CGAffineTransform transform = patternTransform();
    transform = CGAffineTransformConcat(transform, ctm);

    CGSize phase = CGSizeMake(bbox().x(), -bbox().y()); // Pattern space seems to start in the lower-left, so we flip the Y here.
    CGContextSetPatternPhase(context, phase);

    CGPatternCallbacks callbacks = {0, patternCallback, NULL};
    m_pattern = CGPatternCreate(
        tile(),
        CGRectMake(0, 0, cellSize.width, cellSize.height),
        transform,
        bbox().width(), //cellSize.width,
        bbox().height(), //cellSize.height,
        kCGPatternTilingConstantSpacing,  // FIXME: should ask CG guys.
        true, // has color
        &callbacks);

    CGContextSetAlpha(context, renderStyle->opacity()); // or do I set the alpha above?

    m_patternSpace = CGColorSpaceCreatePattern(NULL);

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill()) {
        CGContextSetFillColorSpace(context, m_patternSpace);
        CGContextSetFillPattern(context, m_pattern, &alpha);
        if (isPaintingText()) {
            const_cast<RenderObject*>(renderObject)->style()->setColor(Color());
            CGContextSetTextDrawingMode(context, kCGTextFill);
        }
    }
    
    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke()) {
        CGContextSetStrokeColorSpace(context, m_patternSpace);
        CGContextSetStrokePattern(context, m_pattern, &alpha);
        applyStrokeStyleToContext(context, renderStyle, renderObject);
        if (isPaintingText()) {
            const_cast<RenderObject*>(renderObject)->style()->setColor(Color());
            CGContextSetTextDrawingMode(context, kCGTextStroke);
        }
    }
    
    return true;
}

void KRenderingPaintServerPatternQuartz::renderPath(KRenderingDeviceContext* renderingContext, const RenderPath* renderPath, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = renderPath->style();

    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(renderingContext);
    CGContextRef context = quartzContext->cgContext();

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        KRenderingPaintServerQuartzHelper::fillPath(context, renderPath);
    
    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        KRenderingPaintServerQuartzHelper::strokePath(context, renderPath);
}

void KRenderingPaintServerPatternQuartz::teardown(KRenderingDeviceContext* renderingContext, const RenderObject* renderObject, KCPaintTargetType type) const
{
    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(renderingContext);
    CGContextRef context = quartzContext->cgContext();
    CGPatternRelease(m_pattern);
    CGColorSpaceRelease(m_patternSpace);
    CGContextRestoreGState(context);
}

}

#endif // SVG_SUPPORT
