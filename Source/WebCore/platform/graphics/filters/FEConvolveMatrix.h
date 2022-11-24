/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
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
#include "FloatPoint.h"
#include <wtf/Vector.h>

namespace WebCore {

enum class EdgeModeType {
    Unknown,
    Duplicate,
    Wrap,
    None
};

class FEConvolveMatrix : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEConvolveMatrix> create(const IntSize& kernelSize, float divisor, float bias, const IntPoint& targetOffset, EdgeModeType, const FloatPoint& kernelUnitLength, bool preserveAlpha, const Vector<float>& kernelMatrix);

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

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEConvolveMatrix>> decode(Decoder&);

private:
    FEConvolveMatrix(const IntSize& kernelSize, float divisor, float bias, const IntPoint& targetOffset, EdgeModeType, const FloatPoint& kernelUnitLength, bool preserveAlpha, const Vector<float>& kernelMatrix);

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    IntSize m_kernelSize;
    float m_divisor;
    float m_bias;
    IntPoint m_targetOffset;
    EdgeModeType m_edgeMode;
    FloatPoint m_kernelUnitLength;
    bool m_preserveAlpha;
    Vector<float> m_kernelMatrix;
};

template<class Encoder>
void FEConvolveMatrix::encode(Encoder& encoder) const
{
    encoder << m_kernelSize;
    encoder << m_divisor;
    encoder << m_bias;
    encoder << m_targetOffset;
    encoder << m_edgeMode;
    encoder << m_kernelUnitLength;
    encoder << m_preserveAlpha;
    encoder << m_kernelMatrix;
}

template<class Decoder>
std::optional<Ref<FEConvolveMatrix>> FEConvolveMatrix::decode(Decoder& decoder)
{
    std::optional<IntSize> kernelSize;
    decoder >> kernelSize;
    if (!kernelSize)
        return std::nullopt;

    std::optional<float> divisor;
    decoder >> divisor;
    if (!divisor)
        return std::nullopt;

    std::optional<float> bias;
    decoder >> bias;
    if (!bias)
        return std::nullopt;

    std::optional<IntPoint> targetOffset;
    decoder >> targetOffset;
    if (!targetOffset)
        return std::nullopt;

    std::optional<EdgeModeType> edgeMode;
    decoder >> edgeMode;
    if (!edgeMode)
        return std::nullopt;

    std::optional<FloatPoint> kernelUnitLength;
    decoder >> kernelUnitLength;
    if (!kernelUnitLength)
        return std::nullopt;

    std::optional<bool> preserveAlpha;
    decoder >> preserveAlpha;
    if (!kernelUnitLength)
        return std::nullopt;

    std::optional<Vector<float>> kernelMatrix;
    decoder >> kernelMatrix;
    if (!kernelMatrix)
        return std::nullopt;

    return FEConvolveMatrix::create(*kernelSize, *divisor, *bias, *targetOffset, *edgeMode, *kernelUnitLength, *preserveAlpha, WTFMove(*kernelMatrix));
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::EdgeModeType> {
    using values = EnumValues<
        WebCore::EdgeModeType,

        WebCore::EdgeModeType::Unknown,
        WebCore::EdgeModeType::Duplicate,
        WebCore::EdgeModeType::Wrap,
        WebCore::EdgeModeType::None
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEConvolveMatrix)
