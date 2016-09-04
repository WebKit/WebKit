/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "Element.h"
#include "MemoryCache.h"

namespace WebCore {

CSSImageValue::CSSImageValue(const String& url)
    : CSSValue(ImageClass)
    , m_url(url)
    , m_accessedImage(false)
{
}

CSSImageValue::CSSImageValue(CachedImage& image)
    : CSSValue(ImageClass)
    , m_url(image.url())
    , m_cachedImage(&image)
    , m_accessedImage(true)
{
}


CSSImageValue::~CSSImageValue()
{
}

bool CSSImageValue::isPending() const
{
    return !m_accessedImage;
}

CachedImage* CSSImageValue::loadImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    if (!m_accessedImage) {
        m_accessedImage = true;

        CachedResourceRequest request(ResourceRequest(loader.document()->completeURL(m_url)), options);
        if (m_initiatorName.isEmpty())
            request.setInitiator(cachedResourceRequestInitiators().css);
        else
            request.setInitiator(m_initiatorName);

        if (options.mode == FetchOptions::Mode::Cors) {
            ASSERT(loader.document()->securityOrigin());
            updateRequestForAccessControl(request.mutableResourceRequest(), *loader.document()->securityOrigin(), options.allowCredentials);
        }
        m_cachedImage = loader.requestImage(request);
    }
    return m_cachedImage.get();
}

bool CSSImageValue::traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const
{
    if (!m_cachedImage)
        return false;
    return handler(*m_cachedImage);
}

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_url == other.m_url;
}

String CSSImageValue::customCSSText() const
{
    return "url(" + quoteCSSURLIfNeeded(m_url) + ')';
}

Ref<CSSValue> CSSImageValue::cloneForCSSOM() const
{
    // NOTE: We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    Ref<CSSPrimitiveValue> uriValue = CSSPrimitiveValue::create(m_url, CSSPrimitiveValue::CSS_URI);
    uriValue->setCSSOMSafe();
    return WTFMove(uriValue);
}

bool CSSImageValue::knownToBeOpaque(const RenderElement* renderer) const
{
    if (!m_cachedImage)
        return false;
    return m_cachedImage->currentFrameKnownToBeOpaque(renderer);
}

} // namespace WebCore
