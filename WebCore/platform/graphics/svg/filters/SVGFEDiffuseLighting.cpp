/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGLightSource.h"
#include "SVGFEDiffuseLighting.h"
#include "TextStream.h"

namespace WebCore {

SVGFEDiffuseLighting::SVGFEDiffuseLighting()
    : m_lightSource(0)
{
}

SVGFEDiffuseLighting::~SVGFEDiffuseLighting()
{
    delete m_lightSource;
}

Color SVGFEDiffuseLighting::lightingColor() const
{
    return m_lightingColor;
}

void SVGFEDiffuseLighting::setLightingColor(const Color& lightingColor)
{
    m_lightingColor = lightingColor;
}

float SVGFEDiffuseLighting::surfaceScale() const 
{
    return m_surfaceScale;
}

void SVGFEDiffuseLighting::setSurfaceScale(float surfaceScale)
{
    m_surfaceScale = surfaceScale;
}

float SVGFEDiffuseLighting::diffuseConstant() const
{
    return m_diffuseConstant;
}

void SVGFEDiffuseLighting::setDiffuseConstant(float diffuseConstant)
{
    m_diffuseConstant = diffuseConstant;
}

float SVGFEDiffuseLighting::kernelUnitLengthX() const
{
    return m_kernelUnitLengthX;
}

void SVGFEDiffuseLighting::setKernelUnitLengthX(float kernelUnitLengthX)
{
    m_kernelUnitLengthX = kernelUnitLengthX;
}

float SVGFEDiffuseLighting::kernelUnitLengthY() const
{
    return m_kernelUnitLengthY;
}

void SVGFEDiffuseLighting::setKernelUnitLengthY(float kernelUnitLengthY)
{
    m_kernelUnitLengthY = kernelUnitLengthY;
}

const SVGLightSource* SVGFEDiffuseLighting::lightSource() const
{
    return m_lightSource;
}

void SVGFEDiffuseLighting::setLightSource(SVGLightSource* lightSource)
{    
    if (m_lightSource != lightSource) {
        delete m_lightSource;
        m_lightSource = lightSource;
    }
}

TextStream& SVGFEDiffuseLighting::externalRepresentation(TextStream& ts) const
{
    ts << "[type=DIFFUSE-LIGHTING] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [diffuse constant=" << m_diffuseConstant << "]"
        << " [kernel unit length " << m_kernelUnitLengthX << ", " << m_kernelUnitLengthY << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
