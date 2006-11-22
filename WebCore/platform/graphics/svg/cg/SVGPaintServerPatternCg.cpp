/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>

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
#include "SVGPaintServerPattern.h"

#include "KRenderingDeviceQuartz.h"
#include "RenderObject.h"
#include "SVGResourceImage.h"
#include "QuartzSupport.h"

namespace WebCore {

static void patternCallback(void* info, CGContextRef context)
{
    CGLayerRef layer = reinterpret_cast<SVGResourceImage*>(info)->cgLayer();
    CGContextDrawLayerAtPoint(context, CGPointZero, layer);
}

bool SVGPaintServerPattern::setup(KRenderingDeviceContext* context, const RenderObject* object, SVGPaintTargetType type) const
{
    if(listener()) // this seems like bad design to me, should be in a common baseclass. -- ecs 8/6/05
        listener()->resourceNotification();

    RenderStyle* style = object->style();

    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(context);
    CGContextRef contextRef = quartzContext->cgContext();

    RefPtr<SVGResourceImage> cell = tile();
    if (!cell)
        return false;

    CGContextSaveGState(contextRef);

    CGSize cellSize = CGSize(cell->size());

    CGFloat alpha = 1; // canvasStyle->opacity(); //which?

    // Patterns don't seem to resepect the CTM unless we make them...
    CGAffineTransform ctm = CGContextGetCTM(contextRef);
    CGAffineTransform transform = patternTransform();
    transform = CGAffineTransformConcat(transform, ctm);

    CGSize phase = CGSizeMake(bbox().x(), -bbox().y()); // Pattern space seems to start in the lower-left, so we flip the Y here.
    CGContextSetPatternPhase(contextRef, phase);

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

    CGContextSetAlpha(contextRef, style->opacity()); // or do I set the alpha above?

    m_patternSpace = CGColorSpaceCreatePattern(NULL);

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        CGContextSetFillColorSpace(contextRef, m_patternSpace);
        CGContextSetFillPattern(contextRef, m_pattern, &alpha);
        if (isPaintingText()) {
            const_cast<RenderObject*>(object)->style()->setColor(Color());
            CGContextSetTextDrawingMode(contextRef, kCGTextFill);
        }
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        CGContextSetStrokeColorSpace(contextRef, m_patternSpace);
        CGContextSetStrokePattern(contextRef, m_pattern, &alpha);
        applyStrokeStyleToContext(contextRef, style, object);
        if (isPaintingText()) {
            const_cast<RenderObject*>(object)->style()->setColor(Color());
            CGContextSetTextDrawingMode(contextRef, kCGTextStroke);
        }
    }

    return true;
}

void SVGPaintServerPattern::teardown(KRenderingDeviceContext* context, const RenderObject* object, SVGPaintTargetType type) const
{
    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(context);
    CGContextRef contextRef = quartzContext->cgContext();
    CGPatternRelease(m_pattern);
    CGColorSpaceRelease(m_patternSpace);
    CGContextRestoreGState(contextRef);
}

} // namespace WebCore

#endif

// vim:ts=4:noet
