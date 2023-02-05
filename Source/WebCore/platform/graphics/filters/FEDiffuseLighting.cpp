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
#include "FEDiffuseLighting.h"

#include "ImageBuffer.h"
#include "LightSource.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEDiffuseLighting> FEDiffuseLighting::create(const Color& lightingColor, float surfaceScale, float diffuseConstant, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
{
    return adoptRef(*new FEDiffuseLighting(lightingColor, surfaceScale, diffuseConstant, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource)));
}

FEDiffuseLighting::FEDiffuseLighting(const Color& lightingColor, float surfaceScale, float diffuseConstant, float kernelUnitLengthX, float kernelUnitLengthY, Ref<LightSource>&& lightSource)
    : FELighting(FilterEffect::Type::FEDiffuseLighting, lightingColor, surfaceScale, diffuseConstant, 0, 0, kernelUnitLengthX, kernelUnitLengthY, WTFMove(lightSource))
{
}

bool FEDiffuseLighting::setDiffuseConstant(float diffuseConstant)
{
    if (m_diffuseConstant == diffuseConstant)
        return false;
    m_diffuseConstant = diffuseConstant;
    return true;
}

TextStream& FEDiffuseLighting::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feDiffuseLighting";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " surfaceScale=\"" << m_surfaceScale << "\"";
    ts << " diffuseConstant=\"" << m_diffuseConstant << "\"";
    ts << " kernelUnitLength=\"" << m_kernelUnitLengthX << ", " << m_kernelUnitLengthY << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
