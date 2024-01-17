/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015-2022 Apple, Inc. All rights reserved.
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

#include "FilterEffectApplier.h"
#include "IntSize.h"
#include "PixelBuffer.h"
#include <JavaScriptCore/Forward.h>

namespace WebCore {

class FEGaussianBlur;
enum class EdgeModeType : uint8_t;

class FEGaussianBlurSoftwareApplier final : public FilterEffectConcreteApplier<FEGaussianBlur> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = FilterEffectConcreteApplier<FEGaussianBlur>;

public:
    using Base::Base;

private:
    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;

    struct ApplyParameters {
        RefPtr<PixelBuffer> ioBuffer;
        RefPtr<PixelBuffer> tempBuffer;
        int width;
        int height;
        unsigned kernelSizeX;
        unsigned kernelSizeY;
        bool isAlphaImage;
        EdgeModeType edgeMode;
    };

    static inline void kernelPosition(int blurIteration, unsigned& radius, int& deltaLeft, int& deltaRight);

    static inline void boxBlurAlphaOnly(const PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned dx, int& dxLeft, int& dxRight, int& stride, int& strideLine, int& effectWidth, int& effectHeight, const int& maxKernelSize);
    static inline void boxBlur(const PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned dx, int dxLeft, int dxRight, int stride, int strideLine, int effectWidth, int effectHeight, bool alphaImage, EdgeModeType);

    static inline void boxBlurAccelerated(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSize, int stride, int effectWidth, int effectHeight);
    static inline void boxBlurUnaccelerated(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, int stride, IntSize& paintSize, bool isAlphaImage, EdgeModeType);

    static inline void boxBlurGeneric(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize, bool isAlphaImage, EdgeModeType);
    static inline void boxBlurWorker(ApplyParameters*);

    static inline void applyPlatform(PixelBuffer& ioBuffer, PixelBuffer& tempBuffer, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize, bool isAlphaImage, EdgeModeType);
};

} // namespace WebCore
