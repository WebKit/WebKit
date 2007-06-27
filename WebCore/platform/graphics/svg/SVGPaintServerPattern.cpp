/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "ImageBuffer.h"
#include "SVGPatternElement.h"
#include "SVGRenderTreeAsText.h"

using namespace std;

namespace WebCore {

SVGPaintServerPattern::SVGPaintServerPattern(const SVGPatternElement* owner)
    : m_ownerElement(owner)
#if PLATFORM(CG)
    , m_patternSpace(0)
    , m_pattern(0)
#endif
{
    ASSERT(owner);
}

SVGPaintServerPattern::~SVGPaintServerPattern()
{
#if PLATFORM(CG)
    CGPatternRelease(m_pattern);
    CGColorSpaceRelease(m_patternSpace);
#endif
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

void SVGPaintServerPattern::setTile(auto_ptr<ImageBuffer> tile)
{
    m_tile.set(tile.release());
}

AffineTransform SVGPaintServerPattern::patternTransform() const
{
    return m_patternTransform;
}

void SVGPaintServerPattern::setPatternTransform(const AffineTransform& transform)
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

} // namespace WebCore

#endif
