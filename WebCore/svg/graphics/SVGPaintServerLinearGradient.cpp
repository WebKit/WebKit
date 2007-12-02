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
#include "SVGPaintServerLinearGradient.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

SVGPaintServerLinearGradient::SVGPaintServerLinearGradient(const SVGGradientElement* owner)
    : SVGPaintServerGradient(owner)
{ 
}

SVGPaintServerLinearGradient::~SVGPaintServerLinearGradient()
{
}

FloatPoint SVGPaintServerLinearGradient::gradientStart() const
{
    return m_start;
}

void SVGPaintServerLinearGradient::setGradientStart(const FloatPoint& start)
{
    m_start = start;
}

FloatPoint SVGPaintServerLinearGradient::gradientEnd() const
{
    return m_end;
}

void SVGPaintServerLinearGradient::setGradientEnd(const FloatPoint& end)
{
    m_end = end;
}

TextStream& SVGPaintServerLinearGradient::externalRepresentation(TextStream& ts) const
{
    ts << "[type=LINEAR-GRADIENT] ";
    SVGPaintServerGradient::externalRepresentation(ts);
    ts  << " [start=" << gradientStart() << "]"
        << " [end=" << gradientEnd() << "]";
    return ts;
}

} // namespace WebCore

#endif
