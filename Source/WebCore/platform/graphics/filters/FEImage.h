/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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
#include "Image.h"
#include "ImageBuffer.h"
#include "SVGPreserveAspectRatioValue.h"
#include "SourceImage.h"

namespace WebCore {

class Image;
class ImageBuffer;

class FEImage final : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEImage> create(SourceImage&&, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue&);

    const SourceImage& sourceImage() const { return m_sourceImage; }
    void setImageSource(SourceImage&& sourceImage) { m_sourceImage = WTFMove(sourceImage); }

    FloatRect sourceImageRect() const { return m_sourceImageRect; }
    const SVGPreserveAspectRatioValue& preserveAspectRatio() const { return m_preserveAspectRatio; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEImage>> decode(Decoder&);

private:
    FEImage(SourceImage&&, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue&);

    unsigned numberOfEffectInputs() const override { return 0; }

    // FEImage results are always in DestinationColorSpace::SRGB()
    void setOperatingColorSpace(const DestinationColorSpace&) override { }

    FloatRect calculateImageRect(const Filter&, Span<const FloatRect> inputImageRects, const FloatRect& primitiveSubregion) const override;

    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const final;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const final;

    SourceImage m_sourceImage;
    FloatRect m_sourceImageRect;
    SVGPreserveAspectRatioValue m_preserveAspectRatio;
};

template<class Encoder>
void FEImage::encode(Encoder& encoder) const
{
    encoder << m_sourceImage;
    encoder << m_sourceImageRect;
    encoder << m_preserveAspectRatio;
}

template<class Decoder>
std::optional<Ref<FEImage>> FEImage::decode(Decoder& decoder)
{
    std::optional<SourceImage> sourceImage;
    decoder >> sourceImage;
    if (!sourceImage)
        return std::nullopt;

    std::optional<FloatRect> sourceImageRect;
    decoder >> sourceImageRect;
    if (!sourceImageRect)
        return std::nullopt;

    std::optional<SVGPreserveAspectRatioValue> preserveAspectRatio;
    decoder >> preserveAspectRatio;
    if (!preserveAspectRatio)
        return std::nullopt;

    return FEImage::create(WTFMove(*sourceImage), *sourceImageRect, *preserveAspectRatio);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEImage)
