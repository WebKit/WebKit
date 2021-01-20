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
#include "FESpecularLighting.h"

#include "LightSource.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FESpecularLighting::FESpecularLighting(Filter& filter, const Color& lightingColor, float surfaceScale, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
    : FELighting(filter, SpecularLighting, lightingColor, surfaceScale, 0, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource), Type::SpecularLighting)
{
}

Ref<FESpecularLighting> FESpecularLighting::create(Filter& filter, const Color& lightingColor, float surfaceScale, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
{
    return adoptRef(*new FESpecularLighting(filter, lightingColor, surfaceScale, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource)));
}

FESpecularLighting::~FESpecularLighting() = default;

bool FESpecularLighting::setSpecularConstant(float specularConstant)
{
    if (m_specularConstant == specularConstant)
        return false;
    m_specularConstant = specularConstant;
    return true;
}

bool FESpecularLighting::setSpecularExponent(float specularExponent)
{
    if (m_specularExponent == specularExponent)
        return false;
    m_specularExponent = specularExponent;
    return true;
}

TextStream& FESpecularLighting::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feSpecularLighting";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " surfaceScale=\"" << m_surfaceScale << "\" "
       << "specualConstant=\"" << m_specularConstant << "\" "
       << "specularExponent=\"" << m_specularExponent << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
