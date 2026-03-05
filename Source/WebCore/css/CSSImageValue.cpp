/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#include "CSSURLValue.h"
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
    : CSSValue(ClassType::Image)
    , m_isInvalid(true)
{
}

CSSImageValue::CSSImageValue(CSS::URL&& location, AtomString&& initiatorType)
    : CSSValue(ClassType::Image)
    , m_location(WTF::move(location))
    , m_initiatorType(WTF::move(initiatorType))
{
}

CSSImageValue::CSSImageValue(CachedImage& cachedImage)
    : CSSValue(ClassType::Image)
    , m_location(CSS::URL { .specified = cachedImage.url().string(), .resolved = cachedImage.url(), .modifiers = { } })
    , m_initiatorType(cachedImage.initiatorType())
    , m_cachedImage(cachedImage)
{
}

Ref<CSSImageValue> CSSImageValue::create()
{
    return adoptRef(*new CSSImageValue);
}

Ref<CSSImageValue> CSSImageValue::create(CSS::URL location, AtomString initiatorType)
{
    return adoptRef(*new CSSImageValue(WTF::move(location), WTF::move(initiatorType)));
}

Ref<CSSImageValue> CSSImageValue::create(WTF::URL imageURL, AtomString initiatorType)
{
    return create(CSS::URL { .specified = imageURL.string(), .resolved = WTF::move(imageURL), .modifiers = { } }, WTF::move(initiatorType));
}

Ref<CSSImageValue> CSSImageValue::create(CachedImage& cachedImage)
{
    return adoptRef(*new CSSImageValue(cachedImage));
}

CSSImageValue::~CSSImageValue() = default;

Ref<CSSImageValue> CSSImageValue::copyForComputedStyle(const CSS::URL& resolvedURL) const
{
    if (resolvedURL == m_location)
        return const_cast<CSSImageValue&>(*this);

    auto result = create(resolvedURL);
    result->m_cachedImage = m_cachedImage;
    result->m_initiatorType = m_initiatorType;
    result->m_unresolvedValue = const_cast<CSSImageValue*>(this);
    return result;
}

bool CSSImageValue::isLoadedFromOpaqueSource() const
{
    return m_location.modifiers.loadedFromOpaqueSource == LoadedFromOpaqueSource::Yes;
}

bool CSSImageValue::isPending() const
{
    return !m_cachedImage;
}

RefPtr<Style::Image> CSSImageValue::createStyleImage(const Style::BuilderState& state) const
{
    auto styleLocation = Style::toStyle(m_location, state);
    if (styleLocation.resolved == m_location.resolved)
        return Style::CachedImage::create(WTF::move(styleLocation), const_cast<CSSImageValue&>(*this));

    // FIXME: This case can only happen when a element from a document with no baseURL has an inline style with a relative image URL in it and has been moved to a document with a non-null baseURL. Instead of re-resolving in this case, moved elements with this kind of inline style should have their inline style re-parsed.

    auto newLocation = m_location;
    newLocation.resolved = styleLocation.resolved;
    auto result = create(WTF::move(newLocation));
    result->m_cachedImage = m_cachedImage;
    result->m_initiatorType = m_initiatorType;
    result->m_unresolvedValue = const_cast<CSSImageValue*>(this);
    return Style::CachedImage::create(WTF::move(styleLocation), WTF::move(result));
}

CachedImage* CSSImageValue::loadImage(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    if (!m_cachedImage) {
        ASSERT(loader.document());

        ResourceLoaderOptions loadOptions = options;
        CSS::applyModifiersToLoaderOptions(m_location.modifiers, loadOptions);

        CachedResourceRequest request(ResourceRequest(URL { m_location.resolved }), loadOptions);
        if (m_initiatorType.isEmpty())
            request.setInitiatorType(cachedResourceRequestInitiatorTypes().css);
        else
            request.setInitiatorType(m_initiatorType);
        if (options.mode == FetchOptions::Mode::Cors)
            request.updateForAccessControl(*protect(loader.document()));
        m_cachedImage = loader.requestImage(WTF::move(request)).value_or(nullptr);
        for (RefPtr<CSSImageValue> imageValue = this; (imageValue = imageValue->m_unresolvedValue.get()); )
            imageValue->m_cachedImage = m_cachedImage;
    }
    return m_cachedImage.value().get();
}

bool CSSImageValue::customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>& handler) const
{
    return m_cachedImage && *m_cachedImage && handler(**m_cachedImage);
}

bool CSSImageValue::customMayDependOnBaseURL() const
{
    return WebCore::CSS::mayDependOnBaseURL(m_location);
}

bool CSSImageValue::equals(const CSSImageValue& other) const
{
    return m_location == other.m_location;
}

String CSSImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    if (m_isInvalid)
        return ""_s;

    return CSS::serializationForCSS(context, m_location);
}

Ref<DeprecatedCSSOMValue> CSSImageValue::createDeprecatedCSSOMWrapper(CSSStyleDeclaration& styleDeclaration) const
{
    // We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    return DeprecatedCSSOMPrimitiveValue::create(CSSURLValue::create(m_location), styleDeclaration);
}

bool CSSImageValue::knownToBeOpaque(const RenderElement& renderer) const
{
    return m_cachedImage.value_or(nullptr) && (**m_cachedImage).currentFrameKnownToBeOpaque(&renderer);
}

} // namespace WebCore
