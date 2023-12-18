/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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

#include "CSSMarkup.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "Document.h"
#include "Element.h"
#include "StyleBuilderState.h"
#include "StyleCachedImage.h"

namespace WebCore {

CSSImageValue::CSSImageValue()
    : CSSValue(ImageClass)
    , m_isInvalid(true)
{
}

CSSImageValue::CSSImageValue(ResolvedURL&& location, LoadedFromOpaqueSource loadedFromOpaqueSource, AtomString&& initiatorType)
    : CSSValue(ImageClass)
    , m_location(WTFMove(location))
    , m_initiatorType(WTFMove(initiatorType))
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

Ref<CSSImageValue> CSSImageValue::create()
{
    return adoptRef(*new CSSImageValue);
}

Ref<CSSImageValue> CSSImageValue::create(ResolvedURL location, LoadedFromOpaqueSource loadedFromOpaqueSource, AtomString initiatorType)
{
    return adoptRef(*new CSSImageValue(WTFMove(location), loadedFromOpaqueSource, WTFMove(initiatorType)));
}

Ref<CSSImageValue> CSSImageValue::create(URL imageURL, LoadedFromOpaqueSource loadedFromOpaqueSource, AtomString initiatorType)
{
    return create(makeResolvedURL(WTFMove(imageURL)), loadedFromOpaqueSource, WTFMove(initiatorType));
}

CSSImageValue::~CSSImageValue() = default;

bool CSSImageValue::isPending() const
{
    return !m_cachedImage;
}

URL CSSImageValue::reresolvedURL(const Document& document) const
{
    if (isCSSLocalURL(m_location.resolvedURL.string()))
        return m_location.resolvedURL;

    // Re-resolving the URL is important for cases where resolvedURL is still not an absolute URL.
    // This can happen if there was no absolute base URL when the value was created, like a style from a document without a base URL.
    if (m_location.isLocalURL())
        return document.completeURL(m_location.specifiedURLString, URL());

    return document.completeURL(m_location.resolvedURL.string());
}

RefPtr<StyleImage> CSSImageValue::createStyleImage(Style::BuilderState& state) const
{
    auto location = makeResolvedURL(reresolvedURL(state.document()));
    if (m_location == location)
        return StyleCachedImage::create(const_cast<CSSImageValue&>(*this));
    auto result = create(WTFMove(location), m_loadedFromOpaqueSource);
    result->m_cachedImage = m_cachedImage;
    result->m_initiatorType = m_initiatorType;
    result->m_unresolvedValue = const_cast<CSSImageValue*>(this);
    return StyleCachedImage::create(WTFMove(result));
}

CachedImage* CSSImageValue::loadImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    if (!m_cachedImage) {
        ResourceLoaderOptions loadOptions = options;
        loadOptions.loadedFromOpaqueSource = m_loadedFromOpaqueSource;
        CachedResourceRequest request(ResourceRequest(reresolvedURL(*loader.document())), loadOptions);
        if (m_initiatorType.isEmpty())
            request.setInitiatorType(cachedResourceRequestInitiatorTypes().css);
        else
            request.setInitiatorType(m_initiatorType);
        if (options.mode == FetchOptions::Mode::Cors) {
            ASSERT(loader.document());
            request.updateForAccessControl(*loader.document());
        }
        m_cachedImage = loader.requestImage(WTFMove(request)).value_or(nullptr);
        for (auto imageValue = this; (imageValue = imageValue->m_unresolvedValue.get()); )
            imageValue->m_cachedImage = m_cachedImage;
    }
    return m_cachedImage.value().get();
}

bool CSSImageValue::customTraverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    return m_cachedImage && *m_cachedImage && handler(**m_cachedImage);
}

void CSSImageValue::customSetReplacementURLForSubresources(const HashMap<String, String>& replacementURLStrings)
{
    auto replacementURLString = replacementURLStrings.get(m_location.resolvedURL.string());
    if (!replacementURLString.isNull())
        m_replacementURLString = replacementURLString;
}

void CSSImageValue::customClearReplacementURLForSubresources()
{
    m_replacementURLString = { };
}

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_location == other.m_location;
}

String CSSImageValue::customCSSText() const
{
    if (m_isInvalid)
        return ""_s;

    if (!m_replacementURLString.isEmpty())
        return serializeURL(m_replacementURLString);

    return serializeURL(m_location.specifiedURLString);
}

Ref<DeprecatedCSSOMValue> CSSImageValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    // We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::createURI(m_location.resolvedURL.string()), styleDeclaration);
}

bool CSSImageValue::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage.value_or(nullptr) && (**m_cachedImage).currentFrameKnownToBeOpaque(&renderer);
}

} // namespace WebCore
