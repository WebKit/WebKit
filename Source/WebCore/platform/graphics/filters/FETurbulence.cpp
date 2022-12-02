/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Renata Hodovan <reni@inf.u-szeged.hu>
 * Copyright (C) 2011 Gabor Loki <loki@webkit.org>
 * Copyright (C) 2017-2022 Apple Inc.  All rights reserved.
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
#include "FETurbulence.h"

#include "FETurbulenceSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FETurbulence> FETurbulence::create(TurbulenceType type, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles)
{
    return adoptRef(*new FETurbulence(type, baseFrequencyX, baseFrequencyY, numOctaves, seed, stitchTiles));
}

FETurbulence::FETurbulence(TurbulenceType type, float baseFrequencyX, float baseFrequencyY, int numOctaves, float seed, bool stitchTiles)
    : FilterEffect(FilterEffect::Type::FETurbulence)
    , m_type(type)
    , m_baseFrequencyX(baseFrequencyX)
    , m_baseFrequencyY(baseFrequencyY)
    , m_numOctaves(numOctaves)
    , m_seed(seed)
    , m_stitchTiles(stitchTiles)
{
}

bool FETurbulence::setType(TurbulenceType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

bool FETurbulence::setBaseFrequencyY(float baseFrequencyY)
{
    if (m_baseFrequencyY == baseFrequencyY)
        return false;
    m_baseFrequencyY = baseFrequencyY;
    return true;
}

bool FETurbulence::setBaseFrequencyX(float baseFrequencyX)
{
    if (m_baseFrequencyX == baseFrequencyX)
        return false;
    m_baseFrequencyX = baseFrequencyX;
    return true;
}

bool FETurbulence::setSeed(float seed)
{
    if (m_seed == seed)
        return false;
    m_seed = seed;
    return true;
}

bool FETurbulence::setNumOctaves(int numOctaves)
{
    if (m_numOctaves == numOctaves)
        return false;
    m_numOctaves = numOctaves;
    return true;
}

bool FETurbulence::setStitchTiles(bool stitch)
{
    if (m_stitchTiles == stitch)
        return false;
    m_stitchTiles = stitch;
    return true;
}

FloatRect FETurbulence::calculateImageRect(const Filter& filter, Span<const FloatRect>, const FloatRect& primitiveSubregion) const
{
    return filter.maxEffectRect(primitiveSubregion);
}

std::unique_ptr<FilterEffectApplier> FETurbulence::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FETurbulenceSoftwareApplier>(*this);
}

static TextStream& operator<<(TextStream& ts, TurbulenceType type)
{
    switch (type) {
    case TurbulenceType::Unknown:
        ts << "UNKNOWN";
        break;
    case TurbulenceType::Turbulence:
        ts << "TURBULENCE";
        break;
    case TurbulenceType::FractalNoise:
        ts << "NOISE";
        break;
    }
    return ts;
}

TextStream& FETurbulence::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feTurbulence";
    FilterEffect::externalRepresentation(ts, representation);
    
    ts << " type=\"" << type() << "\"";
    ts << " baseFrequency=\"" << baseFrequencyX() << ", " << baseFrequencyY() << "\"";
    ts << " seed=\"" << seed() << "\"";
    ts << " numOctaves=\"" << numOctaves() << "\"";
    ts << " stitchTiles=\"" << stitchTiles() << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
