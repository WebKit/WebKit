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
#include "SVGPaintServerRadialGradient.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

SVGPaintServerRadialGradient::SVGPaintServerRadialGradient(const SVGGradientElement* owner)
    : SVGPaintServerGradient(owner)
    , m_radius(0.0f)
{
}

SVGPaintServerRadialGradient::~SVGPaintServerRadialGradient()
{
}


FloatPoint SVGPaintServerRadialGradient::gradientCenter() const
{
    return m_center;
}

void SVGPaintServerRadialGradient::setGradientCenter(const FloatPoint& center)
{
    m_center = center;
}

FloatPoint SVGPaintServerRadialGradient::gradientFocal() const
{
    return m_focal;
}

void SVGPaintServerRadialGradient::setGradientFocal(const FloatPoint& focal)
{
    m_focal = focal;
}

float SVGPaintServerRadialGradient::gradientRadius() const
{
    return m_radius;
}

void SVGPaintServerRadialGradient::setGradientRadius(float radius)
{
    m_radius = radius;
}

TextStream& SVGPaintServerRadialGradient::externalRepresentation(TextStream& ts) const
{
    ts << "[type=RADIAL-GRADIENT] ";
    SVGPaintServerGradient::externalRepresentation(ts);
    ts << " [center=" << gradientCenter() << "]"
        << " [focal=" << gradientFocal() << "]"
        << " [radius=" << gradientRadius() << "]";
    return ts;
}

} // namespace WebCore

#endif
