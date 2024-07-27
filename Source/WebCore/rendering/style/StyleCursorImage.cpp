/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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
 *
 */

#include "config.h"
#include "StyleCursorImage.h"

#include "CSSCursorImageValue.h"
#include "CSSImageValue.h"
#include "CachedImage.h"
#include "FloatSize.h"
#include "RenderElement.h"
#include "SVGCursorElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLengthContext.h"
#include "SVGURIReference.h"
#include "StyleBuilderState.h"
#include "StyleCachedImage.h"
#include "StyleImageSet.h"
#include "StyleSameDocumentSVGResourceImage.h"

namespace WebCore {

Ref<StyleCursorImage> StyleCursorImage::create(Ref<StyleImage>&& image, const std::optional<IntPoint>& hotSpot, const URL& originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
{ 
    return adoptRef(*new StyleCursorImage(WTFMove(image), hotSpot, originalURL, loadedFromOpaqueSource));
}

StyleCursorImage::StyleCursorImage(Ref<StyleImage>&& image, const std::optional<IntPoint>& hotSpot, const URL& originalURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : StyleMultiImage { Type::CursorImage }
    , m_image { WTFMove(image) }
    , m_hotSpot { hotSpot }
    , m_originalURL { originalURL }
    , m_loadedFromOpaqueSource { loadedFromOpaqueSource }
{
}

StyleCursorImage::~StyleCursorImage()
{
    for (auto& element : m_cursorElements)
        element.removeClient(*this);
}

bool StyleCursorImage::operator==(const StyleImage& other) const
{
    auto* otherCursorImage = dynamicDowncast<StyleCursorImage>(other);
    return otherCursorImage && equals(*otherCursorImage);
}

bool StyleCursorImage::equals(const StyleCursorImage& other) const
{
    return equalInputImages(other) && StyleMultiImage::equals(other);
}

bool StyleCursorImage::equalInputImages(const StyleCursorImage& other) const
{
    return m_image.get() == other.m_image.get();
}

Ref<CSSValue> StyleCursorImage::computedStyleValue(const RenderStyle& style) const
{ 
    return CSSCursorImageValue::create(m_image->computedStyleValue(style), m_hotSpot, m_originalURL, m_loadedFromOpaqueSource );
}

ImageWithScale StyleCursorImage::selectBestFitImage(const Document& document)
{
    if (RefPtr imageSet = dynamicDowncast<StyleImageSet>(m_image.get()))
        return imageSet->selectBestFitImage(document);

    auto replacementImageFromSVGCursorResource = [&](auto& existingImage) -> RefPtr<StyleImage> {
        if (RefPtr cursorElement = updateCursorElement(document)) {
            auto existingImageURL = existingImage->imageURL();
            auto replacementImageURL = document.completeURL(cursorElement->href());

            if (existingImageURL != replacementImageURL)
                return StyleCachedImage::create(CSSImageValue::create(WTFMove(replacementImageURL), m_loadedFromOpaqueSource));
        }
        return nullptr;
    };

    if (RefPtr cachedImage = dynamicDowncast<StyleCachedImage>(m_image.get())) {
        if (auto replacementImage = replacementImageFromSVGCursorResource(cachedImage))
            m_image = replacementImage.releaseNonNull();
    } else if (RefPtr sameDocumentResourceImage = dynamicDowncast<StyleSameDocumentSVGResourceImage>(m_image.get())) {
        if (auto replacementImage = replacementImageFromSVGCursorResource(sameDocumentResourceImage))
            m_image = replacementImage.releaseNonNull();
    }

    return { m_image.ptr(), 1, String() };
}

RefPtr<SVGCursorElement> StyleCursorImage::updateCursorElement(const Document& document)
{
    RefPtr cursorElement = dynamicDowncast<SVGCursorElement>(SVGURIReference::targetElementFromIRIString(m_originalURL.string(), document).element);
    if (!cursorElement)
        return nullptr;

    // FIXME: Not right to keep old cursor elements as clients. The new one should replace the old, not join it in a set.
    if (m_cursorElements.add(*cursorElement).isNewEntry) {
        cursorElementChanged(*cursorElement);
        cursorElement->addClient(*this);
    }
    return cursorElement;
}

void StyleCursorImage::cursorElementRemoved(SVGCursorElement& cursorElement)
{
    // FIXME: Not right to stay a client of a cursor element until the element is destroyed. We'd want to stop being a client once it's no longer a valid target, like when it's disconnected.
    m_cursorElements.remove(cursorElement);
}

void StyleCursorImage::cursorElementChanged(SVGCursorElement& cursorElement)
{
    // FIXME: Seems wrong that changing an old cursor element, one that that is no longer the target, changes the hot spot.
    // FIXME: This will override a hot spot that was specified in CSS, which is probably incorrect.
    // FIXME: Should we clamp from float to int instead of just casting here?
    SVGLengthContext lengthContext(nullptr);
    m_hotSpot = IntPoint {
        static_cast<int>(std::round(cursorElement.x().value(lengthContext))),
        static_cast<int>(std::round(cursorElement.y().value(lengthContext)))
    };

    // FIXME: Why doesn't this funtion check for a change to the href of the cursor element? Why would we dynamically track changes to x/y but not href?
}

void StyleCursorImage::setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const URL& url)
{
    if (!hasCachedImage())
        return;
    cachedImage()->setContainerContextForClient(renderer, LayoutSize(containerSize), containerZoom, url);
}

bool StyleCursorImage::usesDataProtocol() const
{
    return m_originalURL.protocolIsData();
}

} // namespace WebCore
