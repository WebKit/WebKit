/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "Color.h"
#include "DistantLightSource.h"
#include "FilterEffect.h"
#include "PointLightSource.h"
#include "SpotLightSource.h"

namespace WebCore {

struct FELightingPaintingDataForNeon;

class FELighting : public FilterEffect {
public:
    const Color& lightingColor() const { return m_lightingColor; }
    bool setLightingColor(const Color&);

    float surfaceScale() const { return m_surfaceScale; }
    bool setSurfaceScale(float);

    float diffuseConstant() const { return m_diffuseConstant; }
    float specularConstant() const { return m_specularConstant; }
    float specularExponent() const { return m_specularExponent; }

    float kernelUnitLengthX() const { return m_kernelUnitLengthX; }
    bool setKernelUnitLengthX(float);

    float kernelUnitLengthY() const { return m_kernelUnitLengthY; }
    bool setKernelUnitLengthY(float);

    const LightSource& lightSource() const { return m_lightSource.get(); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder, class ClassName> static std::optional<Ref<ClassName>> decode(Decoder&);

protected:
    FELighting(Type, const Color& lightingColor, float surfaceScale, float diffuseConstant, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&&);

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

#if CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE)
    static int getPowerCoefficients(float exponent);
    inline void platformApplyNeon(const LightingData&, const LightSource::PaintingData&);
#endif

    Color m_lightingColor;
    float m_surfaceScale;
    float m_diffuseConstant;
    float m_specularConstant;
    float m_specularExponent;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
    Ref<LightSource> m_lightSource;
};

template<class Encoder>
void FELighting::encode(Encoder& encoder) const
{
    encoder << m_lightingColor;
    encoder << m_surfaceScale;
    encoder << m_diffuseConstant;
    encoder << m_specularConstant;
    encoder << m_specularExponent;
    encoder << m_kernelUnitLengthX;
    encoder << m_kernelUnitLengthY;
    
    encoder << m_lightSource->type();
    switch (m_lightSource->type()) {
    case LS_DISTANT:
        downcast<DistantLightSource>(m_lightSource.get()).encode(encoder);
        break;
    case LS_POINT:
        downcast<PointLightSource>(m_lightSource.get()).encode(encoder);
        break;
    case LS_SPOT:
        downcast<SpotLightSource>(m_lightSource.get()).encode(encoder);
        break;
    }
}

template<class Decoder, class ClassName>
std::optional<Ref<ClassName>> FELighting::decode(Decoder& decoder)
{
    std::optional<Color> lightingColor;
    decoder >> lightingColor;
    if (!lightingColor)
        return std::nullopt;

    std::optional<float> surfaceScale;
    decoder >> surfaceScale;
    if (!surfaceScale)
        return std::nullopt;

    std::optional<float> diffuseConstant;
    decoder >> diffuseConstant;
    if (!diffuseConstant)
        return std::nullopt;

    std::optional<float> specularConstant;
    decoder >> specularConstant;
    if (!specularConstant)
        return std::nullopt;

    std::optional<float> specularExponent;
    decoder >> specularExponent;
    if (!specularExponent)
        return std::nullopt;

    std::optional<float> kernelUnitLengthX;
    decoder >> kernelUnitLengthX;
    if (!kernelUnitLengthX)
        return std::nullopt;

    std::optional<float> kernelUnitLengthY;
    decoder >> kernelUnitLengthY;
    if (!kernelUnitLengthY)
        return std::nullopt;

    std::optional<LightType> lightSourceType;
    decoder >> lightSourceType;
    if (!lightSourceType)
        return std::nullopt;

    std::optional<Ref<LightSource>> lightSource;
    switch (*lightSourceType) {
    case LS_DISTANT:
        lightSource = DistantLightSource::decode(decoder);
        break;
    case LS_POINT:
        lightSource = PointLightSource::decode(decoder);
        break;
    case LS_SPOT:
        lightSource = SpotLightSource::decode(decoder);
        break;
    }

    if (!lightSource)
        return std::nullopt;

    return ClassName::create(*lightingColor, *surfaceScale, *diffuseConstant, *specularConstant, *specularExponent, *kernelUnitLengthX, *kernelUnitLengthY, WTFMove(*lightSource));
}

} // namespace WebCore
