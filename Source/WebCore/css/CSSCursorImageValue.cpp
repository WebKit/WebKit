/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "CSSCursorImageValue.h"

#include "CSSImageValue.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "StyleCachedImage.h"
#include "StyleImage.h"
#include "StylePendingImage.h"
#include "SVGCursorElement.h"
#include "SVGLengthContext.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "TreeScope.h"
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#include "StyleCachedImageSet.h"
#endif

namespace WebCore {

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, bool hasHotSpot, const IntPoint& hotSpot)
    : CSSValue(CursorImageClass)
    , m_imageValue(WTFMove(imageValue))
    , m_hasHotSpot(hasHotSpot)
    , m_hotSpot(hotSpot)
{
}

inline void CSSCursorImageValue::detachPendingImage()
{
    if (is<StylePendingImage>(m_image.get()))
        downcast<StylePendingImage>(*m_image).detachFromCSSValue();
}

CSSCursorImageValue::~CSSCursorImageValue()
{
    detachPendingImage();

    for (auto* element : m_cursorElements)
        element->removeClient(*this);
}

String CSSCursorImageValue::customCSSText() const
{
    StringBuilder result;
    result.append(m_imageValue.get().cssText());
    if (m_hasHotSpot) {
        result.append(' ');
        result.appendNumber(m_hotSpot.x());
        result.append(' ');
        result.appendNumber(m_hotSpot.y());
    }
    return result.toString();
}

SVGCursorElement* CSSCursorImageValue::updateCursorElement(const Document& document)
{
    if (!isSVGCursor())
        return nullptr;

    auto* element = SVGURIReference::targetElementFromIRIString(downcast<CSSImageValue>(m_imageValue.get()).url(), document);
    if (!is<SVGCursorElement>(element))
        return nullptr;

    auto& cursorElement = downcast<SVGCursorElement>(*element);
    if (m_cursorElements.add(&cursorElement).isNewEntry) {
        cursorElementChanged(cursorElement);
        cursorElement.addClient(*this);
    }
    return &cursorElement;
}

void CSSCursorImageValue::cursorElementRemoved(SVGCursorElement& cursorElement)
{
    m_cursorElements.remove(&cursorElement);
    clearCachedImage();
}

void CSSCursorImageValue::cursorElementChanged(SVGCursorElement& cursorElement)
{
    clearCachedImage();

    // FIXME: This will override hot spot specified in CSS, which is probably incorrect.
    SVGLengthContext lengthContext(nullptr);
    m_hasHotSpot = true;
    float x = std::round(cursorElement.x().value(lengthContext));
    m_hotSpot.setX(static_cast<int>(x));

    float y = std::round(cursorElement.y().value(lengthContext));
    m_hotSpot.setY(static_cast<int>(y));
}

StyleImage* CSSCursorImageValue::cachedImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
#if ENABLE(CSS_IMAGE_SET)
    if (is<CSSImageSetValue>(m_imageValue.get()))
        return downcast<CSSImageSetValue>(m_imageValue.get()).cachedImageSet(loader, options);
#endif

    auto* cursorElement = loader.document() ? updateCursorElement(*loader.document()) : nullptr;

    if (!m_isImageValid) {
        m_isImageValid = true;
        // For SVG images we need to lazily substitute in the correct URL. Rather than attempt
        // to change the URL of the CSSImageValue (which would then change behavior like cssText),
        // we create an alternate CSSImageValue to use.
        if (cursorElement) {
            detachPendingImage();
            Ref<CSSImageValue> svgImageValue(CSSImageValue::create(cursorElement->href()));
            StyleCachedImage* cachedImage = svgImageValue->cachedImage(loader, options);
            m_image = cachedImage;
            return cachedImage;
        }

        if (is<CSSImageValue>(m_imageValue.get())) {
            detachPendingImage();
            m_image = downcast<CSSImageValue>(m_imageValue.get()).cachedImage(loader, options);
        }
    }

    if (is<StyleCachedImage>(m_image.get()))
        return downcast<StyleCachedImage>(m_image.get());

    return nullptr;
}

StyleImage* CSSCursorImageValue::cachedOrPendingImage(Document& document)
{
#if ENABLE(CSS_IMAGE_SET)
    // Need to delegate completely so that changes in device scale factor can be handled appropriately.
    if (is<CSSImageSetValue>(m_imageValue.get()))
        return downcast<CSSImageSetValue>(m_imageValue.get()).cachedOrPendingImageSet(document);
#else
    UNUSED_PARAM(document);
#endif

    if (!m_image)
        m_image = StylePendingImage::create(this);

    return m_image.get();
}

bool CSSCursorImageValue::isSVGCursor() const
{
    if (is<CSSImageValue>(m_imageValue.get())) {
        URL kurl(ParsedURLString, downcast<CSSImageValue>(m_imageValue.get()).url());
        return kurl.hasFragmentIdentifier();
    }
    return false;
}

String CSSCursorImageValue::cachedImageURL()
{
    if (!is<StyleCachedImage>(m_image.get()))
        return String();
    return downcast<StyleCachedImage>(*m_image).cachedImage()->url();
}

void CSSCursorImageValue::clearCachedImage()
{
    detachPendingImage();
    m_image = nullptr;
    m_isImageValid = false;
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return m_hasHotSpot ? other.m_hasHotSpot && m_hotSpot == other.m_hotSpot : !other.m_hasHotSpot
        && compareCSSValue(m_imageValue, other.m_imageValue);
}

} // namespace WebCore
