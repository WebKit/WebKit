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

#include "config.h"
#include "FEImage.h"

#include "FEImageSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEImage> FEImage::create(SourceImage&& sourceImage, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue& preserveAspectRatio)
{
    return adoptRef(*new FEImage(WTFMove(sourceImage), sourceImageRect, preserveAspectRatio));
}

FEImage::FEImage(SourceImage&& sourceImage, const FloatRect& sourceImageRect, const SVGPreserveAspectRatioValue& preserveAspectRatio)
    : FilterEffect(Type::FEImage)
    , m_sourceImage(WTFMove(sourceImage))
    , m_sourceImageRect(sourceImageRect)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

FloatRect FEImage::calculateImageRect(const Filter& filter, Span<const FloatRect>, const FloatRect& primitiveSubregion) const
{
    if (m_sourceImage.nativeImageIfExists()) {
        auto imageRect = primitiveSubregion;
        auto srcRect = m_sourceImageRect;
        m_preserveAspectRatio.transformRect(imageRect, srcRect);
        return filter.clipToMaxEffectRect(imageRect, primitiveSubregion);
    }

    if (m_sourceImage.imageBufferIfExists())
        return filter.maxEffectRect(primitiveSubregion);

    ASSERT_NOT_REACHED();
    return FloatRect();
}

std::unique_ptr<FilterEffectApplier> FEImage::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEImageSoftwareApplier>(*this);
}

TextStream& FEImage::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feImage";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " image-size=\"" << m_sourceImageRect.width() << "x" << m_sourceImageRect.height() << "\"";
    // FIXME: should this dump also object returned by FEImage::image() ?

    ts << "]\n";
    return ts;
}

} // namespace WebCore
