/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFESpecularLighting.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

FESpecularLighting::FESpecularLighting(FilterEffect* in, const Color& lightingColor, const float& surfaceScale,
    const float& specularConstant, const float& specularExponent, const float& kernelUnitLengthX,
    const float& kernelUnitLengthY, LightSource* lightSource)
    : FilterEffect()
    , m_in(in)
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_specularConstant(specularConstant)
    , m_specularExponent(specularExponent)
    , m_kernelUnitLengthX(kernelUnitLengthX)
    , m_kernelUnitLengthY(kernelUnitLengthY)
    , m_lightSource(lightSource)
{
}

PassRefPtr<FESpecularLighting> FESpecularLighting::create(FilterEffect* in, const Color& lightingColor,
    const float& surfaceScale, const float& specularConstant, const float& specularExponent,
    const float& kernelUnitLengthX, const float& kernelUnitLengthY, LightSource* lightSource)
{
    return adoptRef(new FESpecularLighting(in, lightingColor, surfaceScale, specularConstant, specularExponent,
        kernelUnitLengthX, kernelUnitLengthY, lightSource));
}

FESpecularLighting::~FESpecularLighting()
{
}

Color FESpecularLighting::lightingColor() const
{
    return m_lightingColor;
}

void FESpecularLighting::setLightingColor(const Color& lightingColor)
{
    m_lightingColor = lightingColor;
}

float FESpecularLighting::surfaceScale() const
{
    return m_surfaceScale;
}

void FESpecularLighting::setSurfaceScale(float surfaceScale)
{
    m_surfaceScale = surfaceScale;
}

float FESpecularLighting::specularConstant() const
{
    return m_specularConstant;
}

void FESpecularLighting::setSpecularConstant(float specularConstant)
{
    m_specularConstant = specularConstant;
}

float FESpecularLighting::specularExponent() const
{
    return m_specularExponent;
}

void FESpecularLighting::setSpecularExponent(float specularExponent)
{
    m_specularExponent = specularExponent;
}

float FESpecularLighting::kernelUnitLengthX() const
{
    return m_kernelUnitLengthX;
}

void FESpecularLighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    m_kernelUnitLengthX = kernelUnitLengthX;
}

float FESpecularLighting::kernelUnitLengthY() const
{
    return m_kernelUnitLengthY;
}

void FESpecularLighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    m_kernelUnitLengthY = kernelUnitLengthY;
}

const LightSource* FESpecularLighting::lightSource() const
{
    return m_lightSource.get();
}

void FESpecularLighting::setLightSource(LightSource* lightSource)
{
    m_lightSource = lightSource;
}

void FESpecularLighting::apply()
{
}

void FESpecularLighting::dump()
{
}

TextStream& FESpecularLighting::externalRepresentation(TextStream& ts) const
{
    ts << "[type=SPECULAR-LIGHTING] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [specual constant=" << m_specularConstant << "]"
        << " [specular exponent=" << m_specularExponent << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
