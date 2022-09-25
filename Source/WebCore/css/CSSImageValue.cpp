/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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
#include "CachedResourceRequestInitiators.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "Document.h"
#include "Element.h"
#include "StyleBuilderState.h"

namespace WebCore {

static bool operator==(const ResolvedURL& a, const ResolvedURL& b)
{
    return a.specifiedURLString == b.specifiedURLString && a.resolvedURL == b.resolvedURL;
}

// https://drafts.csswg.org/css-values/#url-local-url-flag
bool ResolvedURL::isLocalURL() const
{
    return specifiedURLString.startsWith('#');
}

static ResolvedURL makeResolvedURL(URL&& resolvedURL)
{
    auto string = resolvedURL.string();
    return { WTFMove(string), WTFMove(resolvedURL) };
}

CSSImageValue::CSSImageValue(ResolvedURL&& location, LoadedFromOpaqueSource loadedFromOpaqueSource)
    : CSSValue(ImageClass)
    , m_location(WTFMove(location))
    , m_loadedFromOpaqueSource(loadedFromOpaqueSource)
{
}

CSSImageValue::CSSImageValue(CachedImage& image)
    : CSSValue(ImageClass)
    , m_location { image.url().string(), image.url() }
    , m_cachedImage(&image)
{
}

Ref<CSSImageValue> CSSImageValue::create(ResolvedURL&& location, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return adoptRef(*new CSSImageValue(WTFMove(location), loadedFromOpaqueSource));
}

Ref<CSSImageValue> CSSImageValue::create(URL&& imageURL, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return create(makeResolvedURL(WTFMove(imageURL)), loadedFromOpaqueSource);
}

Ref<CSSImageValue> CSSImageValue::create(CachedImage& image)
{
    return adoptRef(*new CSSImageValue(image));
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

Ref<CSSImageValue> CSSImageValue::valueWithStylesResolved(Style::BuilderState& state)
{
    auto location = makeResolvedURL(reresolvedURL(state.document()));
    if (m_location == location)
        return *this;
    auto result = create(WTFMove(location), m_loadedFromOpaqueSource);
    result->m_cachedImage = m_cachedImage;
    result->m_initiatorName = m_initiatorName;
    result->m_unresolvedValue = this;
    return result;
}

CachedImage* CSSImageValue::loadImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    if (!m_cachedImage) {
        ResourceLoaderOptions loadOptions = options;
        loadOptions.loadedFromOpaqueSource = m_loadedFromOpaqueSource;
        CachedResourceRequest request(ResourceRequest(reresolvedURL(*loader.document())), loadOptions);
        if (m_initiatorName.isEmpty())
            request.setInitiator(cachedResourceRequestInitiators().css);
        else
            request.setInitiator(m_initiatorName);
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

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_location == other.m_location;
}

String CSSImageValue::customCSSText() const
{
    return serializeURL(m_location.specifiedURLString);
}

Ref<DeprecatedCSSOMValue> CSSImageValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    // NOTE: We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    return DeprecatedCSSOMPrimitiveValue::create(CSSPrimitiveValue::create(m_location.resolvedURL.string(), CSSUnitType::CSS_URI), styleDeclaration);
}

bool CSSImageValue::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage.value_or(nullptr) && (**m_cachedImage).currentFrameKnownToBeOpaque(&renderer);
}

} // namespace WebCore
