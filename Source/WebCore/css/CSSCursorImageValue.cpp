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

static inline SVGCursorElement* resourceReferencedByCursorElement(const String& url, Document& document)
{
    Element* element = SVGURIReference::targetElementFromIRIString(url, document);
    if (is<SVGCursorElement>(element))
        return downcast<SVGCursorElement>(element);

    return nullptr;
}

CSSCursorImageValue::CSSCursorImageValue(Ref<CSSValue>&& imageValue, bool hasHotSpot, const IntPoint& hotSpot)
    : CSSValue(CursorImageClass)
    , m_imageValue(WTFMove(imageValue))
    , m_hasHotSpot(hasHotSpot)
    , m_hotSpot(hotSpot)
    , m_accessedImage(false)
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

    if (!isSVGCursor())
        return;

    HashSet<SVGElement*>::const_iterator it = m_referencedElements.begin();
    HashSet<SVGElement*>::const_iterator end = m_referencedElements.end();

    for (; it != end; ++it) {
        SVGElement* referencedElement = *it;
        referencedElement->cursorImageValueRemoved();
        if (SVGCursorElement* cursorElement = resourceReferencedByCursorElement(downcast<CSSImageValue>(m_imageValue.get()).url(), referencedElement->document()))
            cursorElement->removeClient(referencedElement);
    }
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

bool CSSCursorImageValue::updateIfSVGCursorIsUsed(Element* element)
{
    if (!element || !element->isSVGElement())
        return false;

    if (!isSVGCursor())
        return false;

    if (SVGCursorElement* cursorElement = resourceReferencedByCursorElement(downcast<CSSImageValue>(m_imageValue.get()).url(), element->document())) {
        // FIXME: This will override hot spot specified in CSS, which is probably incorrect.
        SVGLengthContext lengthContext(0);
        m_hasHotSpot = true;
        float x = roundf(cursorElement->x().value(lengthContext));
        m_hotSpot.setX(static_cast<int>(x));

        float y = roundf(cursorElement->y().value(lengthContext));
        m_hotSpot.setY(static_cast<int>(y));

        if (cachedImageURL() != element->document().completeURL(cursorElement->href()))
            clearCachedImage();

        SVGElement& svgElement = downcast<SVGElement>(*element);
        m_referencedElements.add(&svgElement);
        svgElement.setCursorImageValue(this);
        cursorElement->addClient(&svgElement);
        return true;
    }

    return false;
}

StyleImage* CSSCursorImageValue::cachedImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
#if ENABLE(CSS_IMAGE_SET)
    if (is<CSSImageSetValue>(m_imageValue.get()))
        return downcast<CSSImageSetValue>(m_imageValue.get()).cachedImageSet(loader, options);
#endif

    if (!m_accessedImage) {
        m_accessedImage = true;

        // For SVG images we need to lazily substitute in the correct URL. Rather than attempt
        // to change the URL of the CSSImageValue (which would then change behavior like cssText),
        // we create an alternate CSSImageValue to use.
        if (isSVGCursor() && loader.document()) {
            // FIXME: This will fail if the <cursor> element is in a shadow DOM (bug 59827)
            if (SVGCursorElement* cursorElement = resourceReferencedByCursorElement(downcast<CSSImageValue>(m_imageValue.get()).url(), *loader.document())) {
                detachPendingImage();
                Ref<CSSImageValue> svgImageValue(CSSImageValue::create(cursorElement->href()));
                StyleCachedImage* cachedImage = svgImageValue->cachedImage(loader, options);
                m_image = cachedImage;
                return cachedImage;
            }
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
    m_accessedImage = false;
}

void CSSCursorImageValue::removeReferencedElement(SVGElement* element)
{
    m_referencedElements.remove(element);
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const
{
    return m_hasHotSpot ? other.m_hasHotSpot && m_hotSpot == other.m_hotSpot : !other.m_hasHotSpot
        && compareCSSValue(m_imageValue, other.m_imageValue);
}

} // namespace WebCore
