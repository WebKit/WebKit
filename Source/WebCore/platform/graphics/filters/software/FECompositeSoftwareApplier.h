/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

namespace WebCore {

class FEComposite;

class FECompositeSoftwareApplier final : public FilterEffectConcreteApplier<FEComposite> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = FilterEffectConcreteApplier<FEComposite>;

public:
    using Base::Base;

private:
    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;

    static uint8_t clampByte(int);

    template <int b1, int b4>
    static inline void computeArithmeticPixels(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    template <int b1, int b4>
    static inline void computeArithmeticPixelsUnclamped(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    static inline void applyPlatformArithmetic(unsigned char* source, unsigned char* destination, int pixelArrayLength, float k1, float k2, float k3, float k4);

    bool applyArithmetic(FilterImage& input, FilterImage& input2, FilterImage& result) const;
    bool applyNonArithmetic(FilterImage& input, FilterImage& input2, FilterImage& result) const;
};

} // namespace WebCore
