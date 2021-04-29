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
#include <wtf/Vector.h>

namespace WebCore {

enum ColorMatrixType {
    FECOLORMATRIX_TYPE_UNKNOWN          = 0,
    FECOLORMATRIX_TYPE_MATRIX           = 1,
    FECOLORMATRIX_TYPE_SATURATE         = 2,
    FECOLORMATRIX_TYPE_HUEROTATE        = 3,
    FECOLORMATRIX_TYPE_LUMINANCETOALPHA = 4
};

class FEColorMatrix : public FilterEffect {
public:
    static Ref<FEColorMatrix> create(Filter&, ColorMatrixType, Vector<float>&&);

    ColorMatrixType type() const { return m_type; }
    bool setType(ColorMatrixType);

    const Vector<float>& values() const { return m_values; }
    bool setValues(const Vector<float>&);

    static inline void calculateSaturateComponents(float* components, float value);
    static inline void calculateHueRotateComponents(float* components, float value);

private:
    FEColorMatrix(Filter&, ColorMatrixType, Vector<float>&&);

    const char* filterName() const final { return "FEColorMatrix"; }

    void platformApplySoftware() override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, RepresentationType) const override;

    ColorMatrixType m_type;
    Vector<float> m_values;
};

inline void FEColorMatrix::calculateSaturateComponents(float* components, float value)
{
    components[0] = (0.213 + 0.787 * value);
    components[1] = (0.715 - 0.715 * value);
    components[2] = (0.072 - 0.072 * value);
    components[3] = (0.213 - 0.213 * value);
    components[4] = (0.715 + 0.285 * value);
    components[5] = (0.072 - 0.072 * value);
    components[6] = (0.213 - 0.213 * value);
    components[7] = (0.715 - 0.715 * value);
    components[8] = (0.072 + 0.928 * value);
}

inline void FEColorMatrix::calculateHueRotateComponents(float* components, float value)
{
    float cosHue = cos(value * piFloat / 180);
    float sinHue = sin(value * piFloat / 180);
    components[0] = 0.213 + cosHue * 0.787 - sinHue * 0.213;
    components[1] = 0.715 - cosHue * 0.715 - sinHue * 0.715;
    components[2] = 0.072 - cosHue * 0.072 + sinHue * 0.928;
    components[3] = 0.213 - cosHue * 0.213 + sinHue * 0.143;
    components[4] = 0.715 + cosHue * 0.285 + sinHue * 0.140;
    components[5] = 0.072 - cosHue * 0.072 - sinHue * 0.283;
    components[6] = 0.213 - cosHue * 0.213 - sinHue * 0.787;
    components[7] = 0.715 - cosHue * 0.715 + sinHue * 0.715;
    components[8] = 0.072 + cosHue * 0.928 + sinHue * 0.072;
}


} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::FEColorMatrix)
    static bool isType(const WebCore::FilterEffect& effect) { return effect.filterEffectClassType() == WebCore::FilterEffect::Type::ColorMatrix; }
SPECIALIZE_TYPE_TRAITS_END()

