/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "GraphicsTypes.h"

namespace WebCore {

class FEBlend : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEBlend> create(BlendMode);

    BlendMode blendMode() const { return m_mode; }
    bool setBlendMode(BlendMode);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEBlend>> decode(Decoder&);

private:
    FEBlend(BlendMode);

    unsigned numberOfEffectInputs() const override { return 2; }

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    BlendMode m_mode;
};

template<class Encoder>
void FEBlend::encode(Encoder& encoder) const
{
    encoder << m_mode;
}

template<class Decoder>
std::optional<Ref<FEBlend>> FEBlend::decode(Decoder& decoder)
{
    std::optional<BlendMode> mode;
    decoder >> mode;
    if (!mode)
        return std::nullopt;

    return FEBlend::create(*mode);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEBlend)
