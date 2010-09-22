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

#ifndef FEConvolveMatrix_h
#define FEConvolveMatrix_h

#if ENABLE(FILTERS)
#include "FilterEffect.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "Filter.h"
#include <wtf/AlwaysInline.h>
#include <wtf/Vector.h>

namespace WebCore {

enum EdgeModeType {
    EDGEMODE_UNKNOWN   = 0,
    EDGEMODE_DUPLICATE = 1,
    EDGEMODE_WRAP      = 2,
    EDGEMODE_NONE      = 3
};

class CanvasPixelArray;

class FEConvolveMatrix : public FilterEffect {
public:
    static PassRefPtr<FEConvolveMatrix> create(const IntSize&,
            float, float, const IntPoint&, EdgeModeType, const FloatPoint&,
            bool, const Vector<float>&);

    IntSize kernelSize() const;
    void setKernelSize(IntSize);

    const Vector<float>& kernel() const;
    void setKernel(const Vector<float>&);

    float divisor() const;
    void setDivisor(float);

    float bias() const;
    void setBias(float);

    IntPoint targetOffset() const;
    void setTargetOffset(IntPoint);

    EdgeModeType edgeMode() const;
    void setEdgeMode(EdgeModeType);

    FloatPoint kernelUnitLength() const;
    void setKernelUnitLength(FloatPoint);

    bool preserveAlpha() const;
    void setPreserveAlpha(bool);

    virtual void apply(Filter*);
    virtual void dump();

    virtual TextStream& externalRepresentation(TextStream&, int indention) const;

private:
    FEConvolveMatrix(const IntSize&, float, float,
            const IntPoint&, EdgeModeType, const FloatPoint&, bool, const Vector<float>&);

    struct PaintingData {
        CanvasPixelArray* srcPixelArray;
        CanvasPixelArray* dstPixelArray;
        int width;
        int height;
        float bias;
    };

    template<bool preserveAlphaValues>
    ALWAYS_INLINE void fastSetInteriorPixels(PaintingData&, int clipRight, int clipBottom);

    ALWAYS_INLINE int getPixelValue(PaintingData&, int x, int y);

    template<bool preserveAlphaValues>
    void fastSetOuterPixels(PaintingData&, int x1, int y1, int x2, int y2);

    // Wrapper functions
    ALWAYS_INLINE void setInteriorPixels(PaintingData& paintingData, int clipRight, int clipBottom);
    ALWAYS_INLINE void setOuterPixels(PaintingData& paintingData, int x1, int y1, int x2, int y2);

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

#endif // ENABLE(FILTERS)

#endif // FEConvolveMatrix_h
