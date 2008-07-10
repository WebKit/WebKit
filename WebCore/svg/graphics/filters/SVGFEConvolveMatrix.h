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
#include "FilterEffect.h"
#include "FloatPoint.h"
#include "FloatSize.h"

#include <wtf/Vector.h>

namespace WebCore {

    enum EdgeModeType {
        EDGEMODE_UNKNOWN   = 0,
        EDGEMODE_DUPLICATE = 1,
        EDGEMODE_WRAP      = 2,
        EDGEMODE_NONE      = 3
    };

    class FEConvolveMatrix : public FilterEffect {
    public:
        static PassRefPtr<FEConvolveMatrix> create(FilterEffect*, FilterEffect*, const FloatSize&, 
                const float&, const float&, const FloatSize&, EdgeModeType, const FloatPoint&,
                bool, const Vector<float>&);

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

        EdgeModeType edgeMode() const;
        void setEdgeMode(EdgeModeType);

        FloatPoint kernelUnitLength() const;
        void setKernelUnitLength(FloatPoint);

        bool preserveAlpha() const;
        void setPreserveAlpha(bool);

        virtual void apply();
        virtual void dump();
        TextStream& externalRepresentation(TextStream& ts) const;

    private:
        FEConvolveMatrix(FilterEffect*, FilterEffect*, const FloatSize&, const float&, const float&,
                const FloatSize&, EdgeModeType, const FloatPoint&, bool, const Vector<float>&);

        RefPtr<FilterEffect> m_in;
        RefPtr<FilterEffect> m_in2;
        FloatSize m_kernelSize;
        float m_divisor;
        float m_bias;
        FloatSize m_targetOffset;
        EdgeModeType m_edgeMode;
        FloatPoint m_kernelUnitLength;
        bool m_preserveAlpha;
        Vector<float> m_kernelMatrix;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEConvolveMatrix_h
