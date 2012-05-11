/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "CSSImageValue.h"

#include "CSSCursorImageValue.h"
#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "MemoryCache.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "StyleCachedImage.h"
#include "StylePendingImage.h"

namespace WebCore {

CSSImageValue::CSSImageValue(ClassType classType, const String& url)
    : CSSValue(classType)
    , m_url(url)
    , m_accessedImage(false)
{
}

CSSImageValue::CSSImageValue(const String& url)
    : CSSValue(ImageClass)
    , m_url(url)
    , m_accessedImage(false)
{
}

CSSImageValue::CSSImageValue(const String& url, StyleImage* image)
    : CSSValue(ImageClass)
    , m_url(url)
    , m_image(image)
    , m_accessedImage(true)
{
}

CSSImageValue::~CSSImageValue()
{
}

StyleImage* CSSImageValue::cachedOrPendingImage()
{
    if (!m_image)
        m_image = StylePendingImage::create(this);

    return m_image.get();
}

StyleCachedImage* CSSImageValue::cachedImage(CachedResourceLoader* loader)
{
    if (isCursorImageValue())
        return static_cast<CSSCursorImageValue*>(this)->cachedImage(loader);
    return cachedImage(loader, m_url);
}

StyleCachedImage* CSSImageValue::cachedImage(CachedResourceLoader* loader, const String& url)
{
    ASSERT(loader);

    if (!m_accessedImage) {
        m_accessedImage = true;

        ResourceRequest request(loader->document()->completeURL(url));
        if (CachedResourceHandle<CachedImage> cachedImage = loader->requestImage(request))
            m_image = StyleCachedImage::create(cachedImage.get());
    }

    return (m_image && m_image->isCachedImage()) ? static_cast<StyleCachedImage*>(m_image.get()) : 0;
}

String CSSImageValue::cachedImageURL()
{
    if (!m_image || !m_image->isCachedImage())
        return String();
    return static_cast<StyleCachedImage*>(m_image.get())->cachedImage()->url();
}

void CSSImageValue::clearCachedImage()
{
    m_image = 0;
    m_accessedImage = false;
}

String CSSImageValue::customCssText() const
{
    return "url(" + quoteCSSURLIfNeeded(m_url) + ")";
}

PassRefPtr<CSSValue> CSSImageValue::cloneForCSSOM() const
{
    // NOTE: We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    RefPtr<CSSPrimitiveValue> uriValue = CSSPrimitiveValue::create(m_url, CSSPrimitiveValue::CSS_URI);
    uriValue->setCSSOMSafe();
    return uriValue.release();
}

} // namespace WebCore
