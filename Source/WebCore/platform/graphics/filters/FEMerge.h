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

#pragma once

#include "FilterEffect.h"

namespace WebCore {

class FEMerge : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEMerge> create(unsigned numberOfEffectInputs);

    unsigned numberOfEffectInputs() const override { return m_numberOfEffectInputs; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEMerge>> decode(Decoder&);

private:
    FEMerge(unsigned numberOfEffectInputs);

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    unsigned m_numberOfEffectInputs { 0 };
};

template<class Encoder>
void FEMerge::encode(Encoder& encoder) const
{
    encoder << m_numberOfEffectInputs;
}

template<class Decoder>
std::optional<Ref<FEMerge>> FEMerge::decode(Decoder& decoder)
{
    std::optional<unsigned> numberOfEffectInputs;
    decoder >> numberOfEffectInputs;
    if (!numberOfEffectInputs)
        return std::nullopt;

    return FEMerge::create(*numberOfEffectInputs);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEMerge)
