/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "SVGImageForContainer.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "NativeImage.h"

namespace WebCore {

SVGImageForContainer::SVGImageForContainer(SVGImage* image, SVGImage::ContainerContext&& containerContext)
    : m_image(image)
    , m_containerContext(WTF::move(containerContext))
{
}

FloatSize SVGImageForContainer::size(ImageOrientation) const
{
    return m_containerContext.zoomedContainerSize();
}

ImageDrawResult SVGImageForContainer::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr<SVGImage> image = m_image.get();
    if (!image)
        return ImageDrawResult::DidNothing;

    if (!image->displayListCacheEnabled())
        return image->drawForContainer(context, m_containerContext, dstRect, srcRect, options);

    if (!m_displayList) {
        m_displayList = image->recordContentForContainer(m_containerContext, context.colorSpace());
        if (!m_displayList)
            return ImageDrawResult::DidNothing;
    }

    return image->drawRecordedContentForContainer(context, m_containerContext, protect(*m_displayList), dstRect, srcRect, options);
}

void SVGImageForContainer::drawPattern(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    protect(m_image)->drawPatternForContainer(context, m_containerContext, srcRect, patternTransform, phase, spacing, dstRect, options);
}

RefPtr<NativeImage> SVGImageForContainer::currentNativeImage()
{
    return protect(m_image)->nativeImage(size());
}

} // namespace WebCore
