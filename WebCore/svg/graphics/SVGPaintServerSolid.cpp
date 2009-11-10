/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#if ENABLE(SVG)
#include "SVGPaintServerSolid.h"

#include "GraphicsContext.h"
#include "RenderPath.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

SVGPaintServerSolid::SVGPaintServerSolid()
{
}

SVGPaintServerSolid::~SVGPaintServerSolid()
{
}

Color SVGPaintServerSolid::color() const
{
    return m_color;
}

void SVGPaintServerSolid::setColor(const Color& color)
{
    m_color = color;
}

TextStream& SVGPaintServerSolid::externalRepresentation(TextStream& ts) const
{
    ts << "[type=SOLID]"
        << " [color="<< color() << "]";
    return ts;
}

bool SVGPaintServerSolid::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    RenderStyle* style = object ? object->style() : 0;
    const SVGRenderStyle* svgStyle = style ? style->svgStyle() : 0;
    ColorSpace colorSpace = style ? style->colorSpace() : DeviceColorSpace;

    if ((type & ApplyToFillTargetType) && (!style || svgStyle->hasFill())) {
        context->setAlpha(style ? svgStyle->fillOpacity() : 1);
        context->setFillColor(color().rgb(), colorSpace);
        context->setFillRule(style ? svgStyle->fillRule() : RULE_NONZERO);

        if (isPaintingText)
            context->setTextDrawingMode(cTextFill);
    }

    if ((type & ApplyToStrokeTargetType) && (!style || svgStyle->hasStroke())) {
        context->setAlpha(style ? svgStyle->strokeOpacity() : 1);
        context->setStrokeColor(color().rgb(), colorSpace);

        if (style)
            applyStrokeStyleToContext(context, style, object);

        if (isPaintingText)
            context->setTextDrawingMode(cTextStroke);
    }

    return true;
}

} // namespace WebCore

#endif
