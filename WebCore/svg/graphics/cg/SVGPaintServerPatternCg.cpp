/*
    Copyright (C) 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>

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
    CGContextDrawImage(context, CGRect(FloatRect(FloatPoint(), patternImage->size())), patternImage->cgImage());
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
        targetRect = object->relativeBBox(false);

    m_ownerElement->buildPattern(targetRect);

    if (!tile())
        return false;

    context->save();

    // Respect local pattern transformation
    context->concatCTM(patternTransform());

    // Apply pattern space transformation
    context->translate(patternBoundaries().x(), patternBoundaries().y());

    // Crude hack to support overflow="visible".
    // When the patternBoundaries() size is smaller than the actual tile() size, we run into a problem:
    // Our tile contains content which is larger than the pattern cell size. We just draw the pattern
    // "out of" cell boundaries, to draw the overflown content, instead of clipping it away. The uppermost
    // cell doesn't include the overflown content of the cell right above it though -> that's why we're moving
    // down the phase by a very small amount, so we're sure the "cell right above"'s overflown content gets drawn.
    CGContextSetPatternPhase(contextRef, CGSizeMake(0.0f, -0.01f));

    RenderStyle* style = object->style();
    CGContextSetAlpha(contextRef, style->opacity());

    CGPatternCallbacks callbacks = {0, patternCallback, 0};

    ASSERT(!m_pattern);
    m_pattern = CGPatternCreate(tile(),
                                CGRect(FloatRect(FloatPoint(), tile()->size())),
                                CGContextGetCTM(contextRef),
                                patternBoundaries().width(),
                                patternBoundaries().height(),
                                kCGPatternTilingConstantSpacing,
                                true, // has color
                                &callbacks);

    if (!m_patternSpace)
        m_patternSpace = CGColorSpaceCreatePattern(0);

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        CGFloat alpha = style->svgStyle()->fillOpacity();
        CGContextSetFillColorSpace(contextRef, m_patternSpace);
        CGContextSetFillPattern(contextRef, m_pattern, &alpha);
 
        if (isPaintingText) 
            context->setTextDrawingMode(cTextFill);
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        CGFloat alpha = style->svgStyle()->strokeOpacity();
        CGContextSetStrokeColorSpace(contextRef, m_patternSpace);
        CGContextSetStrokePattern(contextRef, m_pattern, &alpha);
        applyStrokeStyleToContext(context, style, object);

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
