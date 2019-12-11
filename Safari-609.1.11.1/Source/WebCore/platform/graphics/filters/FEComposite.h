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

#include "FilterEffect.h"

#include "Filter.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

enum CompositeOperationType {
    FECOMPOSITE_OPERATOR_UNKNOWN    = 0, 
    FECOMPOSITE_OPERATOR_OVER       = 1,
    FECOMPOSITE_OPERATOR_IN         = 2,
    FECOMPOSITE_OPERATOR_OUT        = 3,
    FECOMPOSITE_OPERATOR_ATOP       = 4,
    FECOMPOSITE_OPERATOR_XOR        = 5,
    FECOMPOSITE_OPERATOR_ARITHMETIC = 6,
    FECOMPOSITE_OPERATOR_LIGHTER    = 7
};

class FEComposite : public FilterEffect {
public:
    static Ref<FEComposite> create(Filter&, const CompositeOperationType&, float, float, float, float);

    CompositeOperationType operation() const { return m_type; }
    bool setOperation(CompositeOperationType);

    float k1() const { return m_k1; }
    bool setK1(float);

    float k2() const { return m_k2; }
    bool setK2(float);

    float k3() const { return m_k3; }
    bool setK3(float);

    float k4() const { return m_k4; }
    bool setK4(float);

protected:
    bool requiresValidPreMultipliedPixels() override { return m_type != FECOMPOSITE_OPERATOR_ARITHMETIC; }

private:
    FEComposite(Filter&, const CompositeOperationType&, float, float, float, float);

    const char* filterName() const final { return "FEComposite"; }

    void correctFilterResultIfNeeded() override;
    void determineAbsolutePaintRect() override;

    void platformApplySoftware() override;
    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    inline void platformArithmeticSoftware(const Uint8ClampedArray& source, Uint8ClampedArray& destination, float k1, float k2, float k3, float k4);

#if HAVE(ARM_NEON_INTRINSICS)
    template <int b1, int b4>
    static inline void computeArithmeticPixelsNeon(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4);

    static inline void platformArithmeticNeon(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4);
#endif

    CompositeOperationType m_type;
    float m_k1;
    float m_k2;
    float m_k3;
    float m_k4;
};

} // namespace WebCore

