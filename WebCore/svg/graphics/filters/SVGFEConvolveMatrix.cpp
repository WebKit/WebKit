/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGRenderTreeAsText.h"
#include "SVGFEConvolveMatrix.h"

namespace WebCore {

SVGFEConvolveMatrix::SVGFEConvolveMatrix(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_kernelSize()
    , m_divisor(0.0f)
    , m_bias(0.0f)
    , m_targetOffset()
    , m_edgeMode(SVG_EDGEMODE_UNKNOWN)
    , m_kernelUnitLength()
    , m_preserveAlpha(false)
{
}

FloatSize SVGFEConvolveMatrix::kernelSize() const
{
    return m_kernelSize;
}

void SVGFEConvolveMatrix::setKernelSize(FloatSize kernelSize)
{
    m_kernelSize = kernelSize; 
}

const Vector<float>& SVGFEConvolveMatrix::kernel() const
{
    return m_kernelMatrix; 
}

void SVGFEConvolveMatrix::setKernel(const Vector<float>& kernel)
{
    m_kernelMatrix = kernel; 
}

float SVGFEConvolveMatrix::divisor() const
{
    return m_divisor; 
}

void SVGFEConvolveMatrix::setDivisor(float divisor)
{
    m_divisor = divisor; 
}

float SVGFEConvolveMatrix::bias() const
{
    return m_bias; 
}

void SVGFEConvolveMatrix::setBias(float bias)
{
    m_bias = bias; 
}

FloatSize SVGFEConvolveMatrix::targetOffset() const
{
    return m_targetOffset; 
}

void SVGFEConvolveMatrix::setTargetOffset(FloatSize targetOffset)
{
    m_targetOffset = targetOffset; 
}

SVGEdgeModeType SVGFEConvolveMatrix::edgeMode() const
{
    return m_edgeMode; 
}

void SVGFEConvolveMatrix::setEdgeMode(SVGEdgeModeType edgeMode)
{
    m_edgeMode = edgeMode; 
}

FloatPoint SVGFEConvolveMatrix::kernelUnitLength() const
{
    return m_kernelUnitLength; 
}

void SVGFEConvolveMatrix::setKernelUnitLength(FloatPoint kernelUnitLength)
{
    m_kernelUnitLength = kernelUnitLength; 
}

bool SVGFEConvolveMatrix::preserveAlpha() const
{
    return m_preserveAlpha; 
}

void SVGFEConvolveMatrix::setPreserveAlpha(bool preserveAlpha)
{
    m_preserveAlpha = preserveAlpha; 
}

static TextStream& operator<<(TextStream& ts, SVGEdgeModeType t)
{
    switch (t)
    {
        case SVG_EDGEMODE_UNKNOWN:
            ts << "UNKNOWN";break;
        case SVG_EDGEMODE_DUPLICATE:
            ts << "DUPLICATE";break;
        case SVG_EDGEMODE_WRAP:
            ts << "WRAP"; break;
        case SVG_EDGEMODE_NONE:
            ts << "NONE"; break;
    }
    return ts;
}

TextStream& SVGFEConvolveMatrix::externalRepresentation(TextStream& ts) const
{
    ts << "[type=CONVOLVE-MATRIX] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [order " << m_kernelSize << "]"
        << " [kernel matrix=" << m_kernelMatrix  << "]"
        << " [divisor=" << m_divisor << "]"
        << " [bias=" << m_bias << "]"
        << " [target " << m_targetOffset << "]"
        << " [edge mode=" << m_edgeMode << "]"
        << " [kernel unit length " << m_kernelUnitLength << "]"
        << " [preserve alpha=" << m_preserveAlpha << "]";
    return ts;
}

}; // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
