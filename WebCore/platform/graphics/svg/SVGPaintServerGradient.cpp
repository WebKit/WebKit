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
#include "SVGPaintServerGradient.h"

#include "SVGGradientElement.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

TextStream& operator<<(TextStream& ts, SVGGradientSpreadMethod m)
{
    switch (m) {
        case SPREADMETHOD_PAD:
            ts << "PAD"; break;
        case SPREADMETHOD_REPEAT:
            ts << "REPEAT"; break;
        case SPREADMETHOD_REFLECT:
            ts << "REFLECT"; break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const Vector<SVGGradientStop>& l)
{
    ts << "[";
    for (Vector<SVGGradientStop>::const_iterator it = l.begin(); it != l.end(); ++it) {
        ts << "(" << it->first << "," << it->second << ")";
        if (it + 1 != l.end())
            ts << ", ";
    }
    ts << "]";
    return ts;
}

SVGPaintServerGradient::SVGPaintServerGradient(const SVGGradientElement* owner)
    : m_spreadMethod(SPREADMETHOD_PAD)
    , m_boundingBoxMode(true)
    , m_ownerElement(owner)

#if PLATFORM(CG)
    , m_stopsCache(0)
    , m_shadingCache(0)
    , m_savedContext(0)
    , m_imageBuffer(0)
#endif
{
    ASSERT(owner);
}

SVGPaintServerGradient::~SVGPaintServerGradient()
{
#if PLATFORM(CG)
    CGShadingRelease(m_shadingCache);
#endif
}

const Vector<SVGGradientStop>& SVGPaintServerGradient::gradientStops() const
{
    return m_stops;
}

void SVGPaintServerGradient::setGradientStops(const Vector<SVGGradientStop>& stops)
{
    m_stops = stops;
}

SVGGradientSpreadMethod SVGPaintServerGradient::spreadMethod() const
{
    return m_spreadMethod;
}

void SVGPaintServerGradient::setGradientSpreadMethod(const SVGGradientSpreadMethod& method)
{
    m_spreadMethod = method;
}

bool SVGPaintServerGradient::boundingBoxMode() const
{
    return m_boundingBoxMode;
}

void SVGPaintServerGradient::setBoundingBoxMode(bool mode)
{
    m_boundingBoxMode = mode;
}

AffineTransform SVGPaintServerGradient::gradientTransform() const
{
    return m_gradientTransform;
}

void SVGPaintServerGradient::setGradientTransform(const AffineTransform& transform)
{
    m_gradientTransform = transform;
}

TextStream& SVGPaintServerGradient::externalRepresentation(TextStream& ts) const
{
    // Gradients/patterns aren't setup, until they are used for painting. Work around that fact.
    m_ownerElement->buildGradient();

    // abstract, don't stream type
    ts  << "[stops=" << gradientStops() << "]";
    if (spreadMethod() != SPREADMETHOD_PAD)
        ts << "[method=" << spreadMethod() << "]";
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";
    if (!gradientTransform().isIdentity())
        ts << " [transform=" << gradientTransform() << "]";

    return ts;
}

} // namespace WebCore

#endif
