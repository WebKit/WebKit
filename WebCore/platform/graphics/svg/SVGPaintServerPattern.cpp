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

#ifdef SVG_SUPPORT
#include "SVGPaintServerPattern.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

SVGPaintServerPattern::SVGPaintServerPattern()
    : m_boundingBoxMode(true)
    , m_listener(0)
{
}

SVGPaintServerPattern::~SVGPaintServerPattern()
{
}

FloatRect SVGPaintServerPattern::bbox() const
{
    return m_bbox;
}

void SVGPaintServerPattern::setBbox(const FloatRect& rect)
{
    m_bbox = rect;
}

bool SVGPaintServerPattern::boundingBoxMode() const
{
    return m_boundingBoxMode;
}

void SVGPaintServerPattern::setBoundingBoxMode(bool mode)
{
    m_boundingBoxMode = mode;
}

ImageBuffer* SVGPaintServerPattern::tile() const
{
    return m_tile.get();
}

void SVGPaintServerPattern::setTile(ImageBuffer* tile)
{
    m_tile.set(tile);
}

AffineTransform SVGPaintServerPattern::patternTransform() const
{
    return m_patternTransform;
}

void SVGPaintServerPattern::setPatternTransform(const AffineTransform& transform)
{
    m_patternTransform = transform;
}

SVGResourceListener* SVGPaintServerPattern::listener() const
{
    return m_listener;
}

void SVGPaintServerPattern::setListener(SVGResourceListener* listener)
{
    m_listener = listener;
}

TextStream& SVGPaintServerPattern::externalRepresentation(TextStream& ts) const
{
    ts << "[type=PATTERN]"
        << " [bbox=" << bbox() << "]";
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";
    if (!patternTransform().isIdentity())
        ts << " [pattern transform=" << patternTransform() << "]";
    return ts;
}

} // namespace WebCore

#endif
