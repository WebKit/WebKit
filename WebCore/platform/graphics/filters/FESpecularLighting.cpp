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

#include "config.h"

#if ENABLE(FILTERS)
#include "FESpecularLighting.h"

#include "LightSource.h"

namespace WebCore {

FESpecularLighting::FESpecularLighting(const Color& lightingColor, float surfaceScale,
    float specularConstant, float specularExponent, float kernelUnitLengthX,
    float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
    : FELighting(SpecularLighting, lightingColor, surfaceScale, 0, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, lightSource)
{
}

PassRefPtr<FESpecularLighting> FESpecularLighting::create(const Color& lightingColor,
    float surfaceScale, float specularConstant, float specularExponent,
    float kernelUnitLengthX, float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
{
    return adoptRef(new FESpecularLighting(lightingColor, surfaceScale, specularConstant, specularExponent,
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

void FESpecularLighting::setLightSource(PassRefPtr<LightSource> lightSource)
{
    m_lightSource = lightSource;
}

void FESpecularLighting::dump()
{
}

TextStream& FESpecularLighting::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feSpecularLighting";
    FilterEffect::externalRepresentation(ts);
    ts << " surfaceScale=\"" << m_surfaceScale << "\" "
       << "specualConstant=\"" << m_specularConstant << "\" "
       << "specularExponent=\"" << m_specularExponent << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
