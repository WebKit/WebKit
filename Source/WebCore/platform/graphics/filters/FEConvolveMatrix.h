/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FilterEffect.h"
#include "FloatPoint.h"
#include "Filter.h"
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
    static Ref<FEConvolveMatrix> create(Filter&, const IntSize&,
            float, float, const IntPoint&, EdgeModeType, const FloatPoint&,
            bool, const Vector<float>&);

    IntSize kernelSize() const { return m_kernelSize; }
    void setKernelSize(const IntSize&);

    const Vector<float>& kernel() const { return m_kernelMatrix; }
    void setKernel(const Vector<float>&);

    float divisor() const { return m_divisor; }
    bool setDivisor(float);

    float bias() const { return m_bias; }
    bool setBias(float);

    IntPoint targetOffset() const { return m_targetOffset; }
    bool setTargetOffset(const IntPoint&);

    EdgeModeType edgeMode() const { return m_edgeMode; }
    bool setEdgeMode(EdgeModeType);

    FloatPoint kernelUnitLength() const { return m_kernelUnitLength; }
    bool setKernelUnitLength(const FloatPoint&);

    bool preserveAlpha() const { return m_preserveAlpha; }
    bool setPreserveAlpha(bool);

private:

    struct PaintingData {
        const Uint8ClampedArray& srcPixelArray;
        Uint8ClampedArray& dstPixelArray;
        int width;
        int height;
        float bias;
        Vector<float> kernelMatrix;
    };

    FEConvolveMatrix(Filter&, const IntSize&, float, float,
            const IntPoint&, EdgeModeType, const FloatPoint&, bool, const Vector<float>&);

    const char* filterName() const final { return "FEConvolveMatrix"; }

    void determineAbsolutePaintRect() override { setAbsolutePaintRect(enclosingIntRect(maxEffectRect())); }

    void platformApplySoftware() override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    template<bool preserveAlphaValues>
    ALWAYS_INLINE void fastSetInteriorPixels(PaintingData&, int clipRight, int clipBottom, int yStart, int yEnd);

    ALWAYS_INLINE int getPixelValue(PaintingData&, int x, int y);

    template<bool preserveAlphaValues>
    void fastSetOuterPixels(PaintingData&, int x1, int y1, int x2, int y2);

    // Wrapper functions
    ALWAYS_INLINE void setInteriorPixels(PaintingData&, int clipRight, int clipBottom, int yStart, int yEnd);
    ALWAYS_INLINE void setOuterPixels(PaintingData&, int x1, int y1, int x2, int y2);

    // Parallelization parts
    static const int s_minimalRectDimension = (100 * 100); // Empirical data limit for parallel jobs

    IntSize m_kernelSize;
    float m_divisor;
    float m_bias;
    IntPoint m_targetOffset;
    EdgeModeType m_edgeMode;
    FloatPoint m_kernelUnitLength;
    bool m_preserveAlpha;
    Vector<float> m_kernelMatrix;
};

} // namespace WebCore

