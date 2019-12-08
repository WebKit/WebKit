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
#include "SVGFEImage.h"

#include "AffineTransform.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "RenderElement.h"
#include "RenderTreeAsText.h"
#include "SVGElement.h"
#include "SVGRenderingContext.h"
#include "SVGURIReference.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FEImage::FEImage(Filter& filter, RefPtr<Image> image, const SVGPreserveAspectRatioValue& preserveAspectRatio)
    : FilterEffect(filter)
    , m_image(image)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

FEImage::FEImage(Filter& filter, TreeScope& treeScope, const String& href, const SVGPreserveAspectRatioValue& preserveAspectRatio)
    : FilterEffect(filter)
    , m_treeScope(&treeScope)
    , m_href(href)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

Ref<FEImage> FEImage::createWithImage(Filter& filter, RefPtr<Image> image, const SVGPreserveAspectRatioValue& preserveAspectRatio)
{
    return adoptRef(*new FEImage(filter, image, preserveAspectRatio));
}

Ref<FEImage> FEImage::createWithIRIReference(Filter& filter, TreeScope& treeScope, const String& href, const SVGPreserveAspectRatioValue& preserveAspectRatio)
{
    return adoptRef(*new FEImage(filter, treeScope, href, preserveAspectRatio));
}

void FEImage::determineAbsolutePaintRect()
{
    FloatRect paintRect = filter().absoluteTransform().mapRect(filterPrimitiveSubregion());
    FloatRect srcRect;
    if (m_image) {
        srcRect.setSize(m_image->size());
        m_preserveAspectRatio.transformRect(paintRect, srcRect);
    } else if (RenderElement* renderer = referencedRenderer())
        srcRect = filter().absoluteTransform().mapRect(renderer->repaintRectInLocalCoordinates());

    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

RenderElement* FEImage::referencedRenderer() const
{
    if (!m_treeScope)
        return nullptr;
    auto target = SVGURIReference::targetElementFromIRIString(m_href, *m_treeScope);
    if (!is<SVGElement>(target.element))
        return nullptr;
    return target.element->renderer();
}

void FEImage::platformApplySoftware()
{
    RenderElement* renderer = referencedRenderer();
    if (!m_image && !renderer)
        return;

    // FEImage results are always in ColorSpace::SRGB
    setResultColorSpace(ColorSpace::SRGB);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    FloatRect destRect = filter().absoluteTransform().mapRect(filterPrimitiveSubregion());

    FloatRect srcRect;
    if (renderer)
        srcRect = filter().absoluteTransform().mapRect(renderer->repaintRectInLocalCoordinates());
    else {
        srcRect = FloatRect(FloatPoint(), m_image->size());
        m_preserveAspectRatio.transformRect(destRect, srcRect);
    }

    IntPoint paintLocation = absolutePaintRect().location();
    destRect.move(-paintLocation.x(), -paintLocation.y());

    if (renderer) {
        const AffineTransform& absoluteTransform = filter().absoluteTransform();
        resultImage->context().concatCTM(absoluteTransform);

        auto contextNode = makeRefPtr(downcast<SVGElement>(renderer->element()));
        if (contextNode->hasRelativeLengths()) {
            SVGLengthContext lengthContext(contextNode.get());
            FloatSize viewportSize;

            // If we're referencing an element with percentage units, eg. <rect with="30%"> those values were resolved against the viewport.
            // Build up a transformation that maps from the viewport space to the filter primitive subregion.
            if (lengthContext.determineViewport(viewportSize))
                resultImage->context().concatCTM(makeMapBetweenRects(FloatRect(FloatPoint(), viewportSize), destRect));
        }

        AffineTransform contentTransformation;
        SVGRenderingContext::renderSubtreeToImageBuffer(resultImage, *renderer, contentTransformation);
        return;
    }

    resultImage->context().drawImage(*m_image, destRect, srcRect);
}

TextStream& FEImage::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    FloatSize imageSize;
    if (m_image)
        imageSize = m_image->size();
    else if (RenderObject* renderer = referencedRenderer())
        imageSize = enclosingIntRect(renderer->repaintRectInLocalCoordinates()).size();
    ts << indent << "[feImage";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " image-size=\"" << imageSize.width() << "x" << imageSize.height() << "\"]\n";
    // FIXME: should this dump also object returned by SVGFEImage::image() ?
    return ts;
}

} // namespace WebCore
