/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEImage.h"

#include "AffineTransform.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

FEImage::FEImage(RefPtr<Image> image, const SVGPreserveAspectRatio& preserveAspectRatio)
    : FilterEffect()
    , m_image(image)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

PassRefPtr<FEImage> FEImage::create(RefPtr<Image> image, const SVGPreserveAspectRatio& preserveAspectRatio)
{
    return adoptRef(new FEImage(image, preserveAspectRatio));
}

void FEImage::apply(Filter*)
{
    if (!m_image.get())
        return;

    GraphicsContext* filterContext = effectContext();
    if (!filterContext)
        return;

    FloatRect srcRect(FloatPoint(), m_image->size());
    FloatRect destRect(FloatPoint(), filterPrimitiveSubregion().size());

    m_preserveAspectRatio.transformRect(destRect, srcRect);

    filterContext->drawImage(m_image.get(), DeviceColorSpace, destRect, srcRect);
}

void FEImage::dump()
{
}

TextStream& FEImage::externalRepresentation(TextStream& ts, int indent) const
{
    ASSERT(m_image);
    IntSize imageSize = m_image->size();
    writeIndent(ts, indent);
    ts << "[feImage";
    FilterEffect::externalRepresentation(ts);
    ts << " image-size=\"" << imageSize.width() << "x" << imageSize.height() << "\"]\n";
    // FIXME: should this dump also object returned by SVGFEImage::image() ?
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
