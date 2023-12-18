/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "FEMerge.h"

#include "FEMergeSoftwareApplier.h"
#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEMerge> FEMerge::create(unsigned numberOfEffectInputs, DestinationColorSpace colorSpace)
{
    return adoptRef(*new FEMerge(numberOfEffectInputs, colorSpace));
}

FEMerge::FEMerge(unsigned numberOfEffectInputs, DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::FEMerge, colorSpace)
    , m_numberOfEffectInputs(numberOfEffectInputs)
{
}

bool FEMerge::operator==(const FEMerge& other) const
{
    return FilterEffect::operator==(other) && m_numberOfEffectInputs == other.m_numberOfEffectInputs;
}

std::unique_ptr<FilterEffectApplier> FEMerge::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEMergeSoftwareApplier>(*this);
}

TextStream& FEMerge::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feMerge";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " mergeNodes=\"" << m_numberOfEffectInputs << "\"";

    ts << "]\n";
    return ts;
}

} // namespace WebCore
