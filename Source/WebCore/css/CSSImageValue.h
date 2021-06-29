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

#include "CSSValue.h"
#include "CachedResourceHandle.h"
#include "ResourceLoaderOptions.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>

namespace WebCore {

class CachedImage;
class CachedResourceLoader;
class DeprecatedCSSOMValue;
class CSSStyleDeclaration;
class RenderElement;

namespace Style {
class BuilderState;
}

struct ResolvedURL {
    String specifiedURLString;
    URL resolvedURL;
};

class CSSImageValue final : public CSSValue {
public:
    static Ref<CSSImageValue> create(ResolvedURL&&, LoadedFromOpaqueSource);
    static Ref<CSSImageValue> create(URL&&, LoadedFromOpaqueSource);
    static Ref<CSSImageValue> create(CachedImage&);
    ~CSSImageValue();

    bool isPending() const;
    CachedImage* loadImage(CachedResourceLoader&, const ResourceLoaderOptions&);
    CachedImage* cachedImage() const { return m_cachedImage ? m_cachedImage.value().get() : nullptr; }

    const URL& imageURL() const { return m_location.resolvedURL; }

    String customCSSText() const;

    Ref<DeprecatedCSSOMValue> createDeprecatedCSSOMWrapper(CSSStyleDeclaration&) const;

    bool traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const;

    bool equals(const CSSImageValue&) const;

    bool knownToBeOpaque(const RenderElement&) const;

    void setInitiator(const AtomString& name) { m_initiatorName = name; }

    Ref<CSSImageValue> valueWithStylesResolved(Style::BuilderState&);

private:
    CSSImageValue(ResolvedURL&&, LoadedFromOpaqueSource);
    explicit CSSImageValue(CachedImage&);

    URL reresolvedURL(const Document&) const;

    ResolvedURL m_location;
    std::optional<CachedResourceHandle<CachedImage>> m_cachedImage;
    AtomString m_initiatorName;
    LoadedFromOpaqueSource m_loadedFromOpaqueSource { LoadedFromOpaqueSource::No };
    RefPtr<CSSImageValue> m_unresolvedValue;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageValue, isImageValue())
