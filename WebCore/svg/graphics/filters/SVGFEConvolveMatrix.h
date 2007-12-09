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

#ifndef SVGFEConvolveMatrix_h
#define SVGFEConvolveMatrix_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFilterEffect.h"

namespace WebCore {

enum SVGEdgeModeType {
    SVG_EDGEMODE_UNKNOWN   = 0,
    SVG_EDGEMODE_DUPLICATE = 1,
    SVG_EDGEMODE_WRAP      = 2,
    SVG_EDGEMODE_NONE      = 3
};

class SVGFEConvolveMatrix : public SVGFilterEffect {
public:
    SVGFEConvolveMatrix(SVGResourceFilter*);

    FloatSize kernelSize() const;
    void setKernelSize(FloatSize);

    const Vector<float>& kernel() const;
    void setKernel(const Vector<float>&);

    float divisor() const;
    void setDivisor(float);

    float bias() const;
    void setBias(float);

    FloatSize targetOffset() const;
    void setTargetOffset(FloatSize);

    SVGEdgeModeType edgeMode() const;
    void setEdgeMode(SVGEdgeModeType);

    FloatPoint kernelUnitLength() const;
    void setKernelUnitLength(FloatPoint);

    bool preserveAlpha() const;
    void setPreserveAlpha(bool);

    virtual TextStream& externalRepresentation(TextStream&) const;

private:
    FloatSize m_kernelSize;
    float m_divisor;
    float m_bias;
    FloatSize m_targetOffset;
    SVGEdgeModeType m_edgeMode;
    FloatPoint m_kernelUnitLength;
    bool m_preserveAlpha;
    Vector<float> m_kernelMatrix; // maybe should be a real matrix?
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEConvolveMatrix_h
