/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "FEConvolveMatrix.h"
#include "Filter.h"
#include "FilterEffect.h"

namespace WebCore {

class FEGaussianBlur : public FilterEffect {
public:
    static Ref<FEGaussianBlur> create(Filter&, float, float, EdgeModeType);

    float stdDeviationX() const { return m_stdX; }
    void setStdDeviationX(float);

    float stdDeviationY() const { return m_stdY; }
    void setStdDeviationY(float);

    EdgeModeType edgeMode() const { return m_edgeMode; }
    void setEdgeMode(EdgeModeType);

    static IntSize calculateKernelSize(const Filter&, FloatSize stdDeviation);
    static IntSize calculateUnscaledKernelSize(FloatSize stdDeviation);

private:
    FEGaussianBlur(Filter&, float, float, EdgeModeType);

    const char* filterName() const final { return "FEGaussianBlur"; }

    static const int s_minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs

    template<typename Type>
    friend class ParallelJobs;

    struct PlatformApplyParameters {
        FEGaussianBlur* filter;
        RefPtr<Uint8ClampedArray> ioPixelArray;
        RefPtr<Uint8ClampedArray> tmpPixelArray;
        int width;
        int height;
        unsigned kernelSizeX;
        unsigned kernelSizeY;
    };

    void platformApplySoftware() override;

    void determineAbsolutePaintRect() override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    static void platformApplyWorker(PlatformApplyParameters*);
    void platformApply(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize);
    void platformApplyGeneric(Uint8ClampedArray& ioBuffer, Uint8ClampedArray& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize);

    float m_stdX;
    float m_stdY;
    EdgeModeType m_edgeMode;
};

} // namespace WebCore

