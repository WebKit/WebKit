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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServerPattern.h"

#include "CgSupport.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "SVGPatternElement.h"

namespace WebCore {

static void patternCallback(void* info, CGContextRef context)
{
    ImageBuffer* patternImage = reinterpret_cast<ImageBuffer*>(info);

    IntSize patternContentSize = patternImage->size();
    CGContextDrawImage(context, CGRectMake(0, 0, patternContentSize.width(), patternContentSize.height()), patternImage->cgImage());
}

bool SVGPaintServerPattern::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    CGContextRef contextRef = context->platformContext();

    // Build pattern tile, passing destination object bounding box
    FloatRect targetRect;
    if (isPaintingText) {
        IntRect textBoundary = const_cast<RenderObject*>(object)->absoluteBoundingBoxRect();
        targetRect = object->absoluteTransform().inverse().mapRect(textBoundary);
    } else
        targetRect = CGContextGetPathBoundingBox(contextRef);

    m_ownerElement->buildPattern(targetRect);

    if (!tile())
        return false;

    CGSize cellSize = CGSize(tile()->size());
    CGFloat alpha = 1; // canvasStyle->opacity(); //which?

    context->save();

    // Repesct local pattern transformations
    CGContextConcatCTM(contextRef, patternTransform());

    // Pattern space seems to start in the lower-left, so we flip the Y here. 
    CGSize phase = CGSizeMake(patternBoundaries().x(), -patternBoundaries().y());
    CGContextSetPatternPhase(contextRef, phase);

    RenderStyle* style = object->style();
    CGContextSetAlpha(contextRef, style->opacity()); // or do I set the alpha above?

    ASSERT(!m_pattern);
    CGPatternCallbacks callbacks = {0, patternCallback, NULL};
    m_pattern = CGPatternCreate(tile(),
                                CGRectMake(0, 0, cellSize.width, cellSize.height),
                                CGContextGetCTM(contextRef),
                                patternBoundaries().width(),
                                patternBoundaries().height(),
                                kCGPatternTilingConstantSpacing, // FIXME: should ask CG guys.
                                true, // has color
                                &callbacks);

    if (!m_patternSpace)
        m_patternSpace = CGColorSpaceCreatePattern(0);

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        CGContextSetFillColorSpace(contextRef, m_patternSpace);
        CGContextSetFillPattern(contextRef, m_pattern, &alpha);
 
        if (isPaintingText) 
            context->setTextDrawingMode(cTextFill);
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        CGContextSetStrokeColorSpace(contextRef, m_patternSpace);
        CGContextSetStrokePattern(contextRef, m_pattern, &alpha);
        applyStrokeStyleToContext(contextRef, style, object);

        if (isPaintingText) 
            context->setTextDrawingMode(cTextStroke);
    }

    return true;
}

void SVGPaintServerPattern::teardown(GraphicsContext*& context, const RenderObject*, SVGPaintTargetType, bool) const
{
    CGPatternRelease(m_pattern);
    m_pattern = 0;

    context->restore();
}

} // namespace WebCore

#endif

// vim:ts=4:noet
