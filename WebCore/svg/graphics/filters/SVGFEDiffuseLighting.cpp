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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEDiffuseLighting.h"

#include "SVGLightSource.h"
#include "SVGRenderTreeAsText.h"

namespace WebCore {

FEDiffuseLighting::FEDiffuseLighting(const Color& lightingColor, float surfaceScale,
    float diffuseConstant, float kernelUnitLengthX, float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
    : FELighting(DiffuseLighting, lightingColor, surfaceScale, diffuseConstant, 0, 0, kernelUnitLengthX, kernelUnitLengthY, lightSource)
{
}

PassRefPtr<FEDiffuseLighting> FEDiffuseLighting::create(const Color& lightingColor,
    float surfaceScale, float diffuseConstant, float kernelUnitLengthX,
    float kernelUnitLengthY, PassRefPtr<LightSource> lightSource)
{
    return adoptRef(new FEDiffuseLighting(lightingColor, surfaceScale, diffuseConstant, kernelUnitLengthX, kernelUnitLengthY, lightSource));
}

FEDiffuseLighting::~FEDiffuseLighting()
{
}

Color FEDiffuseLighting::lightingColor() const
{
    return m_lightingColor;
}

void FEDiffuseLighting::setLightingColor(const Color& lightingColor)
{
    m_lightingColor = lightingColor;
}

float FEDiffuseLighting::surfaceScale() const 
{
    return m_surfaceScale;
}

void FEDiffuseLighting::setSurfaceScale(float surfaceScale)
{
    m_surfaceScale = surfaceScale;
}

float FEDiffuseLighting::diffuseConstant() const
{
    return m_diffuseConstant;
}

void FEDiffuseLighting::setDiffuseConstant(float diffuseConstant)
{
    m_diffuseConstant = diffuseConstant;
}

float FEDiffuseLighting::kernelUnitLengthX() const
{
    return m_kernelUnitLengthX;
}

void FEDiffuseLighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    m_kernelUnitLengthX = kernelUnitLengthX;
}

float FEDiffuseLighting::kernelUnitLengthY() const
{
    return m_kernelUnitLengthY;
}

void FEDiffuseLighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    m_kernelUnitLengthY = kernelUnitLengthY;
}

const LightSource* FEDiffuseLighting::lightSource() const
{
    return m_lightSource.get();
}

void FEDiffuseLighting::setLightSource(PassRefPtr<LightSource> lightSource)
{    
    m_lightSource = lightSource;
}

void FEDiffuseLighting::dump()
{
}

TextStream& FEDiffuseLighting::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feDiffuseLighting";
    FilterEffect::externalRepresentation(ts);
    ts << " surfaceScale=\"" << m_surfaceScale << "\" "
       << "diffuseConstant=\"" << m_diffuseConstant << "\" "
       << "kernelUnitLength=\"" << m_kernelUnitLengthX << ", " << m_kernelUnitLengthY << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
