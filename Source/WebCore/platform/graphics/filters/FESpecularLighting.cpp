/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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

#include "ImageBuffer.h"
#include "LightSource.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FESpecularLighting> FESpecularLighting::create(const Color& lightingColor, float surfaceScale, float, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
{
    return create(lightingColor, surfaceScale, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource));
}

Ref<FESpecularLighting> FESpecularLighting::create(const Color& lightingColor, float surfaceScale, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
{
    return adoptRef(*new FESpecularLighting(lightingColor, surfaceScale, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource)));
}

FESpecularLighting::FESpecularLighting(const Color& lightingColor, float surfaceScale, float specularConstant, float specularExponent, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
    : FELighting(FilterEffect::Type::FESpecularLighting, lightingColor, surfaceScale, 0, specularConstant, specularExponent, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource))
{
}

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

TextStream& FESpecularLighting::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feSpecularLighting";
    FilterEffect::externalRepresentation(ts, representation);
    
    ts << " surfaceScale=\"" << m_surfaceScale << "\"";
    ts << " specualConstant=\"" << m_specularConstant << "\"";
    ts << " specularExponent=\"" << m_specularExponent << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
