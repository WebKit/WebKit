/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2008 Dirk Schulze <krit@webkit.org>
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
#include "SVGPaintServerPattern.h"

#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "RenderObject.h"
#include "SVGPatternElement.h"
#include "SVGRenderTreeAsText.h"
#include "TransformationMatrix.h"

using namespace std;

namespace WebCore {

SVGPaintServerPattern::SVGPaintServerPattern(const SVGPatternElement* owner)
    : m_ownerElement(owner)
    , m_pattern(0)
{
    ASSERT(owner);
}

SVGPaintServerPattern::~SVGPaintServerPattern()
{
}

FloatRect SVGPaintServerPattern::patternBoundaries() const
{
    return m_patternBoundaries;
}

void SVGPaintServerPattern::setPatternBoundaries(const FloatRect& rect)
{
    m_patternBoundaries = rect;
}

ImageBuffer* SVGPaintServerPattern::tile() const
{
    return m_tile.get();
}

void SVGPaintServerPattern::setTile(PassOwnPtr<ImageBuffer> tile)
{
    m_tile = tile;
}

TransformationMatrix SVGPaintServerPattern::patternTransform() const
{
    return m_patternTransform;
}

void SVGPaintServerPattern::setPatternTransform(const TransformationMatrix& transform)
{
    m_patternTransform = transform;
}

TextStream& SVGPaintServerPattern::externalRepresentation(TextStream& ts) const
{
    // Gradients/patterns aren't setup, until they are used for painting. Work around that fact.
    m_ownerElement->buildPattern(FloatRect(0.0f, 0.0f, 1.0f, 1.0f));

    ts << "[type=PATTERN]"
        << " [bbox=" << patternBoundaries() << "]";
    if (!patternTransform().isIdentity())
        ts << " [pattern transform=" << patternTransform() << "]";
    return ts;
}

bool SVGPaintServerPattern::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    FloatRect targetRect = object->objectBoundingBox();

    const SVGRenderStyle* style = object->style()->svgStyle();
    bool isFilled = (type & ApplyToFillTargetType) && style->hasFill();
    bool isStroked = (type & ApplyToStrokeTargetType) && style->hasStroke();

    ASSERT(isFilled && !isStroked || !isFilled && isStroked);

    m_ownerElement->buildPattern(targetRect);
    if (!tile())
        return false;

    context->save();

    ASSERT(!m_pattern);

    IntRect tileRect = tile()->image()->rect();
    if (tileRect.width() > patternBoundaries().width() || tileRect.height() > patternBoundaries().height()) {
        // Draw the first cell of the pattern manually to support overflow="visible" on all platforms.
        int tileWidth = static_cast<int>(patternBoundaries().width() + 0.5f);
        int tileHeight = static_cast<int>(patternBoundaries().height() + 0.5f);
        OwnPtr<ImageBuffer> tileImage = ImageBuffer::create(IntSize(tileWidth, tileHeight), false);
  
        GraphicsContext* tileImageContext = tileImage->context();

        int numY = static_cast<int>(ceilf(tileRect.height() / tileHeight)) + 1;
        int numX = static_cast<int>(ceilf(tileRect.width() / tileWidth)) + 1;

        tileImageContext->save();
        tileImageContext->translate(-patternBoundaries().width() * numX, -patternBoundaries().height() * numY);
        for (int i = numY; i > 0; i--) {
            tileImageContext->translate(0, patternBoundaries().height());
            for (int j = numX; j > 0; j--) {
                tileImageContext->translate(patternBoundaries().width(), 0);
                tileImageContext->drawImage(tile()->image(), tileRect, tileRect);
            }
            tileImageContext->translate(-patternBoundaries().width() * numX, 0);
        }
        tileImageContext->restore();

        m_pattern = Pattern::create(tileImage->image(), true, true);
    }
    else
        m_pattern = Pattern::create(tile()->image(), true, true);

    if (isFilled) {
        context->setAlpha(style->fillOpacity());
        context->setFillPattern(m_pattern);
        context->setFillRule(style->fillRule());
    }
    if (isStroked) {
        context->setAlpha(style->strokeOpacity());
        context->setStrokePattern(m_pattern);
        applyStrokeStyleToContext(context, object->style(), object);
    }

    TransformationMatrix matrix;
    matrix.translate(patternBoundaries().x(), patternBoundaries().y());
    matrix.multiply(patternTransform());
    m_pattern->setPatternSpaceTransform(matrix);

    if (isPaintingText) {
        context->setTextDrawingMode(isFilled ? cTextFill : cTextStroke);
#if PLATFORM(CG)
        if (isFilled)
            context->applyFillPattern();
        else
            context->applyStrokePattern();
#endif
    }

    return true;
}

void SVGPaintServerPattern::teardown(GraphicsContext*& context, const RenderObject*, SVGPaintTargetType, bool) const
{
    m_pattern = 0;

    context->restore();
}

} // namespace WebCore

#endif
