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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGLightSource.h"
#include "SVGFEDiffuseLighting.h"
#include "SVGRenderTreeAsText.h"
#include "Filter.h"

namespace WebCore {

FEDiffuseLighting::FEDiffuseLighting(FilterEffect* in , const Color& lightingColor, const float& surfaceScale,
    const float& diffuseConstant, const float& kernelUnitLengthX, const float& kernelUnitLengthY, LightSource* lightSource)
    : FilterEffect()
    , m_in(in)
    , m_lightingColor(lightingColor)
    , m_surfaceScale(surfaceScale)
    , m_diffuseConstant(diffuseConstant)
    , m_kernelUnitLengthX(kernelUnitLengthX)
    , m_kernelUnitLengthY(kernelUnitLengthY)
    , m_lightSource(lightSource)
{
}

PassRefPtr<FEDiffuseLighting> FEDiffuseLighting::create(FilterEffect* in , const Color& lightingColor,
    const float& surfaceScale, const float& diffuseConstant, const float& kernelUnitLengthX,
    const float& kernelUnitLengthY, LightSource* lightSource)
{
    return adoptRef(new FEDiffuseLighting(in, lightingColor, surfaceScale, diffuseConstant, kernelUnitLengthX, kernelUnitLengthY, lightSource));
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

void FEDiffuseLighting::setLightSource(LightSource* lightSource)
{    
    m_lightSource = lightSource;
}

void FEDiffuseLighting::apply(Filter*)
{
}

void FEDiffuseLighting::dump()
{
}

TextStream& FEDiffuseLighting::externalRepresentation(TextStream& ts) const
{
    ts << "[type=DIFFUSE-LIGHTING] ";
    FilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [diffuse constant=" << m_diffuseConstant << "]"
        << " [kernel unit length " << m_kernelUnitLengthX << ", " << m_kernelUnitLengthY << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
