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
#include "TextStream.h"

namespace WebCore {

SVGFESpecularLighting::SVGFESpecularLighting(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
    , m_lightingColor()
    , m_surfaceScale(0.0f)
    , m_specularConstant(0.0f)
    , m_specularExponent(0.0f)
    , m_kernelUnitLengthX(0.0f)
    , m_kernelUnitLengthY(0.0f)
    , m_lightSource(0)
{
}

SVGFESpecularLighting::~SVGFESpecularLighting()
{
    delete m_lightSource;
}

Color SVGFESpecularLighting::lightingColor() const
{
    return m_lightingColor;
}

void SVGFESpecularLighting::setLightingColor(const Color& lightingColor)
{
    m_lightingColor = lightingColor;
}

float SVGFESpecularLighting::surfaceScale() const
{
    return m_surfaceScale;
}

void SVGFESpecularLighting::setSurfaceScale(float surfaceScale)
{
    m_surfaceScale = surfaceScale;
}

float SVGFESpecularLighting::specularConstant() const
{
    return m_specularConstant;
}

void SVGFESpecularLighting::setSpecularConstant(float specularConstant)
{
    m_specularConstant = specularConstant;
}

float SVGFESpecularLighting::specularExponent() const
{
    return m_specularExponent;
}

void SVGFESpecularLighting::setSpecularExponent(float specularExponent)
{
    m_specularExponent = specularExponent;
}

float SVGFESpecularLighting::kernelUnitLengthX() const
{
    return m_kernelUnitLengthX;
}

void SVGFESpecularLighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    m_kernelUnitLengthX = kernelUnitLengthX;
}

float SVGFESpecularLighting::kernelUnitLengthY() const
{
    return m_kernelUnitLengthY;
}

void SVGFESpecularLighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    m_kernelUnitLengthY = kernelUnitLengthY;
}

const SVGLightSource* SVGFESpecularLighting::lightSource() const
{
    return m_lightSource;
}

void SVGFESpecularLighting::setLightSource(SVGLightSource* lightSource)
{
    if (m_lightSource != lightSource) {
        delete m_lightSource;
        m_lightSource = lightSource;
    }
}

TextStream& SVGFESpecularLighting::externalRepresentation(TextStream& ts) const
{
    ts << "[type=SPECULAR-LIGHTING] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [specual constant=" << m_specularConstant << "]"
        << " [specular exponent=" << m_specularExponent << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
