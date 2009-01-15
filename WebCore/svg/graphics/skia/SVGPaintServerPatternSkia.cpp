/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SVG)
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "Pattern.h"
#include "SVGPatternElement.h"
#include "SVGPaintServerPattern.h"

#include "SkShader.h"
#include "SkiaUtils.h"

namespace WebCore {

bool SVGPaintServerPattern::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    RenderStyle* style = object->style();

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

    TransformationMatrix transform = patternTransform();
    transform.translate(patternBoundaries().x(), patternBoundaries().y());

    RefPtr<Pattern> pattern(Pattern::create(tile()->image(), true, true));

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        context->setFillPattern(pattern);
        if (isPaintingText) 
            context->setTextDrawingMode(cTextFill);
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        context->setStrokePattern(pattern);
        applyStrokeStyleToContext(context, style, object);
        if (isPaintingText) 
            context->setTextDrawingMode(cTextStroke);
    }

    return true;
}

} // namespace WebCore

#endif
