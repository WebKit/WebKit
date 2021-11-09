/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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
#include "SVGFEImage.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEImage> FEImage::create(Filter& filter, Ref<Image>&& image, const SVGPreserveAspectRatioValue& preserveAspectRatio)
{
    auto imageRect = FloatRect { { }, image->size() };
    return create(filter, WTFMove(image), imageRect, preserveAspectRatio);
}

Ref<FEImage> FEImage::create(Filter& filter, SourceImage&& sourceImage, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue& preserveAspectRatio)
{
    return adoptRef(*new FEImage(filter, WTFMove(sourceImage), sourceImageRect, preserveAspectRatio));
}

FEImage::FEImage(Filter& filter, SourceImage&& sourceImage, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue& preserveAspectRatio)
    : FilterEffect(filter, Type::Image)
    , m_sourceImage(WTFMove(sourceImage))
    , m_sourceImageRect(sourceImageRect)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

void FEImage::determineAbsolutePaintRect()
{
    auto primitiveSubregion = filterPrimitiveSubregion();

    auto imageRect = WTF::switchOn(m_sourceImage,
        [&] (const Ref<Image>&) {
            auto imageRect = primitiveSubregion;
            auto srcRect = m_sourceImageRect;
            m_preserveAspectRatio.transformRect(imageRect, srcRect);
            return imageRect;
        },
        [&] (const Ref<ImageBuffer>&) {
            return primitiveSubregion;
        }
    );

    imageRect = filter().absoluteTransform().mapRect(imageRect);

    if (clipsToBounds())
        imageRect.intersect(maxEffectRect());
    else
        imageRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(imageRect));
}

void FEImage::platformApplySoftware()
{
    // FEImage results are always in DestinationColorSpace::SRGB()
    setResultColorSpace(DestinationColorSpace::SRGB());

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    auto primitiveSubregion = filterPrimitiveSubregion();
    auto& context = resultImage->context();

    WTF::switchOn(m_sourceImage,
        [&] (const Ref<Image>& image) {
            auto imageRect = primitiveSubregion;
            auto srcRect = m_sourceImageRect;
            m_preserveAspectRatio.transformRect(imageRect, srcRect);
            imageRect = filter().absoluteTransform().mapRect(imageRect);
            imageRect = drawingRegionOfInputImage(IntRect(imageRect));
            context.drawImage(image, imageRect, srcRect);
        },
        [&] (const Ref<ImageBuffer>& imageBuffer) {
            auto imageRect = primitiveSubregion;
            imageRect.moveBy(m_sourceImageRect.location());
            imageRect = filter().absoluteTransform().mapRect(imageRect);
            imageRect = drawingRegionOfInputImage(IntRect(imageRect));
            context.drawImageBuffer(imageBuffer, imageRect.location());
        }
    );
}

TextStream& FEImage::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feImage";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " image-size=\"" << m_sourceImageRect.width() << "x" << m_sourceImageRect.height() << "\"]\n";
    // FIXME: should this dump also object returned by SVGFEImage::image() ?
    return ts;
}

} // namespace WebCore
