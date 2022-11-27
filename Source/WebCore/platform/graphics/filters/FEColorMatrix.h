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

#include "FilterEffect.h"
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
    WEBCORE_EXPORT static Ref<FEColorMatrix> create(ColorMatrixType, Vector<float>&&);

    ColorMatrixType type() const { return m_type; }
    bool setType(ColorMatrixType);

    const Vector<float>& values() const { return m_values; }
    bool setValues(const Vector<float>&);

    static void calculateSaturateComponents(float* components, float value);
    static void calculateHueRotateComponents(float* components, float value);
    static Vector<float> normalizedFloats(const Vector<float>& values);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEColorMatrix>> decode(Decoder&);

private:
    FEColorMatrix(ColorMatrixType, Vector<float>&&);

    bool resultIsAlphaImage(const FilterImageVector& inputs) const override;

    OptionSet<FilterRenderingMode> supportedFilterRenderingModes() const override;

    std::unique_ptr<FilterEffectApplier> createAcceleratedApplier() const override;
    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;
    std::optional<GraphicsStyle> createGraphicsStyle(const Filter&) const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    ColorMatrixType m_type;
    Vector<float> m_values;
};

template<class Encoder>
void FEColorMatrix::encode(Encoder& encoder) const
{
    encoder << m_type;
    encoder << m_values;
}

template<class Decoder>
std::optional<Ref<FEColorMatrix>> FEColorMatrix::decode(Decoder& decoder)
{
    std::optional<ColorMatrixType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<Vector<float>> values;
    decoder >> values;
    if (!values)
        return std::nullopt;

    return FEColorMatrix::create(*type, WTFMove(*values));
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ColorMatrixType> {
    using values = EnumValues<
        WebCore::ColorMatrixType,

        WebCore::FECOLORMATRIX_TYPE_UNKNOWN,
        WebCore::FECOLORMATRIX_TYPE_MATRIX,
        WebCore::FECOLORMATRIX_TYPE_SATURATE,
        WebCore::FECOLORMATRIX_TYPE_HUEROTATE,
        WebCore::FECOLORMATRIX_TYPE_LUMINANCETOALPHA
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEColorMatrix)
