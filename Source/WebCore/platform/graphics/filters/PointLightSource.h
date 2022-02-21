/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
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

#pragma once

#include "LightSource.h"
#include <wtf/Ref.h>

namespace WebCore {

class PointLightSource : public LightSource {
public:
    WEBCORE_EXPORT static Ref<PointLightSource> create(const FloatPoint3D& position);

    const FloatPoint3D& position() const { return m_userSpacePosition; }
    bool setX(float) override;
    bool setY(float) override;
    bool setZ(float) override;

    void initPaintingData(const Filter&, const FilterImage& result, PaintingData&) const override;
    ComputedLightingData computePixelLightingData(const PaintingData&, int x, int y, float z) const final;

    WTF::TextStream& externalRepresentation(WTF::TextStream&) const override;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<PointLightSource>> decode(Decoder&);

private:
    PointLightSource(const FloatPoint3D& position);

    FloatPoint3D m_userSpacePosition;
    mutable FloatPoint3D m_bufferPosition;
};

template<class Encoder>
void PointLightSource::encode(Encoder& encoder) const
{
    encoder << m_userSpacePosition;
}

template<class Decoder>
std::optional<Ref<PointLightSource>> PointLightSource::decode(Decoder& decoder)
{
    std::optional<FloatPoint3D> userSpacePosition;
    decoder >> userSpacePosition;
    if (!userSpacePosition)
        return std::nullopt;

    return PointLightSource::create(*userSpacePosition);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_LIGHTSOURCE(PointLightSource, LS_POINT)
