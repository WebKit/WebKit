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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <WebCore/CSSURL.h>
#include <WebCore/CSSValue.h>
#include <WebCore/CachedImage.h>
#include <WebCore/CachedResourceHandle.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <wtf/Function.h>
#include <wtf/Ref.h>

namespace WebCore {

class CachedImage;
class CachedResourceLoader;
class DeprecatedCSSOMValue;
class CSSStyleDeclaration;
class RenderElement;
class StyleImage;

namespace Style {
class BuilderState;
}

class CSSImageValue final : public CSSValue {
public:
    static Ref<CSSImageValue> create();
    static Ref<CSSImageValue> create(CSS::URL, AtomString initiatorType = { });
    static Ref<CSSImageValue> create(WTF::URL, AtomString initiatorType = { });
    ~CSSImageValue();

    Ref<CSSImageValue> copyForComputedStyle(const CSS::URL& resolvedURL) const;

    bool isPending() const;
    CachedImage* loadImage(CachedResourceLoader&, const ResourceLoaderOptions&);
    CachedImage* cachedImage() const { return m_cachedImage ? m_cachedImage.value().get() : nullptr; }

    // Take care when using this, and read https://drafts.csswg.org/css-values/#relative-urls
    const CSS::URL& url() const { return m_location; }

    String customCSSText(const CSS::SerializationContext&) const;

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

    bool customTraverseSubresources(NOESCAPE const Function<bool(const CachedResource&)>&) const;
    bool customMayDependOnBaseURL() const;

    bool equals(const CSSImageValue&) const;

    bool knownToBeOpaque(const RenderElement&) const;

    RefPtr<StyleImage> createStyleImage(const Style::BuilderState&) const;

    bool isLoadedFromOpaqueSource() const;

    IterationStatus customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (m_unresolvedValue) {
            if (func(*m_unresolvedValue) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSImageValue();
    CSSImageValue(CSS::URL&&, AtomString&&);

    CSS::URL m_location;
    std::optional<CachedResourceHandle<CachedImage>> m_cachedImage;
    AtomString m_initiatorType;
    RefPtr<CSSImageValue> m_unresolvedValue;
    bool m_isInvalid { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageValue, isImageValue())
