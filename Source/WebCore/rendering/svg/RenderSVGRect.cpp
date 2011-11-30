/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGRect.h"

#include "SVGNames.h"
#include "SVGRectElement.h"
#include <wtf/Platform.h>

namespace WebCore {

RenderSVGRect::RenderSVGRect(SVGRectElement* node)
    : RenderSVGShape(node)
{
}

RenderSVGRect::~RenderSVGRect()
{
}

void RenderSVGRect::createShape()
{
    // Before creating a new object we need to clear the cached bounding box
    // to avoid using garbage.
    m_boundingBox = FloatRect();
    m_innerStrokeRect = FloatRect();
    m_outerStrokeRect = FloatRect();
    SVGRectElement* rect = static_cast<SVGRectElement*>(node());
    ASSERT(rect);

    bool nonScalingStroke = style()->svgStyle()->vectorEffect() == VE_NON_SCALING_STROKE;
    // Fallback to RenderSVGShape if rect has rounded corners.
    if (rect->hasAttribute(SVGNames::rxAttr) || rect->hasAttribute(SVGNames::ryAttr) || nonScalingStroke) {
       RenderSVGShape::createShape();
       setIsPaintingFallback(true);
       return;
    }

    SVGLengthContext lengthContext(rect);
    FloatSize boundingBoxSize(rect->width().value(lengthContext), rect->height().value(lengthContext));
    if (boundingBoxSize.isEmpty())
        return;

    m_boundingBox = FloatRect(FloatPoint(rect->x().value(lengthContext), rect->y().value(lengthContext)), boundingBoxSize);

    // To decide if the stroke contains a point we create two rects which represent the inner and
    // the outer stroke borders. A stroke contains the point, if the point is between them.
    m_innerStrokeRect = m_boundingBox;
    m_outerStrokeRect = m_boundingBox;

    if (style()->svgStyle()->hasStroke()) {
        float strokeWidth = this->strokeWidth();
        m_innerStrokeRect.inflate(-strokeWidth / 2);
        m_outerStrokeRect.inflate(strokeWidth / 2);
    }
}

FloatRect RenderSVGRect::objectBoundingBox() const
{
    if (isPaintingFallback())
        return RenderSVGShape::objectBoundingBox();
    return m_boundingBox;
}

FloatRect RenderSVGRect::strokeBoundingBox() const
{
    if (isPaintingFallback())
        return RenderSVGShape::strokeBoundingBox();
    return m_outerStrokeRect;
}

void RenderSVGRect::fillShape(GraphicsContext* context) const
{
    if (!isPaintingFallback()) {
#if USE(CG)
        // FIXME: CG implementation of GraphicsContextCG::fillRect has an own
        // shadow drawing method, which draws an extra shadow.
        // This is a workaround for switching off the extra shadow.
        // https://bugs.webkit.org/show_bug.cgi?id=68899
        if (context->hasShadow()) {
            GraphicsContextStateSaver stateSaver(*context);
            context->clearShadow();
            context->fillRect(m_boundingBox);
            return;
        }
#endif
        context->fillRect(m_boundingBox);
        return;
    }
    RenderSVGShape::fillShape(context);
}

void RenderSVGRect::strokeShape(GraphicsContext* context) const
{
    if (!isPaintingFallback()) {
        context->strokeRect(m_boundingBox, strokeWidth());
        return;
    }
    RenderSVGShape::strokeShape(context);
}

bool RenderSVGRect::shapeDependentStrokeContains(const FloatPoint& point) const
{
    return m_outerStrokeRect.contains(point, FloatRect::InsideOrOnStroke) && !m_innerStrokeRect.contains(point, FloatRect::InsideButNotOnStroke);
}

bool RenderSVGRect::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    if (isPaintingFallback())
        return RenderSVGShape::shapeDependentFillContains(point, fillRule);
    return m_boundingBox.contains(point.x(), point.y());
}

}

#endif // ENABLE(SVG)
