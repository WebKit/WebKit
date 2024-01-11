/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "config.h"
#include "FELighting.h"

#include "FELightingNeonParallelApplier.h"
#include "FELightingSoftwareParallelApplier.h"
#include "Filter.h"

namespace WebCore {

FELighting::FELighting(Type type, const Color& lightingColor, float surfaceScale, float diffuseConstant, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource, DestinationColorSpace colorSpace)
    : FilterEffect(type, colorSpace)
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_diffuseConstant(std::max(diffuseConstant, 0.0f))
    , m_specularConstant(std::max(specularConstant, 0.0f))
    , m_specularExponent(clampTo<float>(specularExponent, 1.0f, 128.0f))
    , m_kernelUnitLengthX(kernelUnitLengthX)
    , m_kernelUnitLengthY(kernelUnitLengthY)
    , m_lightSource(WTFMove(lightSource))
{
}

bool FELighting::operator==(const FELighting& other) const
{
    return FilterEffect::operator==(other)
        && m_lightingColor == other.m_lightingColor
        && m_surfaceScale == other.m_surfaceScale
        && m_diffuseConstant == other.m_diffuseConstant
        && m_specularConstant == other.m_specularConstant
        && m_specularExponent == other.m_specularExponent
        && m_kernelUnitLengthX == other.m_kernelUnitLengthX
        && m_kernelUnitLengthY == other.m_kernelUnitLengthY
        && m_lightSource.get() == other.m_lightSource.get();
}

bool FELighting::setSurfaceScale(float surfaceScale)
{
    if (m_surfaceScale == surfaceScale)
        return false;

    m_surfaceScale = surfaceScale;
    return true;
}

bool FELighting::setLightingColor(const Color& lightingColor)
{
    if (m_lightingColor == lightingColor)
        return false;

    m_lightingColor = lightingColor;
    return true;
}

bool FELighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    if (m_kernelUnitLengthX == kernelUnitLengthX)
        return false;

    m_kernelUnitLengthX = kernelUnitLengthX;
    return true;
}

bool FELighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    if (m_kernelUnitLengthY == kernelUnitLengthY)
        return false;

    m_kernelUnitLengthY = kernelUnitLengthY;
    return true;
}

FloatRect FELighting::calculateImageRect(const Filter& filter, std::span<const FloatRect>, const FloatRect& primitiveSubregion) const
{
    return filter.maxEffectRect(primitiveSubregion);
}

std::unique_ptr<FilterEffectApplier> FELighting::createSoftwareApplier() const
{
#if (CPU(ARM_NEON) && CPU(ARM_TRADITIONAL) && COMPILER(GCC_COMPATIBLE))
    return FilterEffectApplier::create<FELightingNeonParallelApplier>(*this);
#else
    return FilterEffectApplier::create<FELightingSoftwareParallelApplier>(*this);
#endif
}

} // namespace WebCore
