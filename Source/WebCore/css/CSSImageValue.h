/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSImageValue_h
#define CSSImageValue_h

#include "CSSValue.h"
#include "CachedResourceHandle.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CachedImage;
class CachedResourceLoader;
class Element;
class RenderElement;
struct ResourceLoaderOptions;

class CSSImageValue final : public CSSValue {
public:
    static Ref<CSSImageValue> create(const String& url) { return adoptRef(*new CSSImageValue(url)); }
    static Ref<CSSImageValue> create(CachedImage& image) { return adoptRef(*new CSSImageValue(image)); }
    ~CSSImageValue();

    bool isPending() const;
    CachedImage* loadImage(CachedResourceLoader&, const ResourceLoaderOptions&);
    CachedImage* cachedImage() const { return m_cachedImage.get(); }

    const String& url() const { return m_url; }

    String customCSSText() const;

    Ref<CSSValue> cloneForCSSOM() const;

    bool traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const;

    bool equals(const CSSImageValue&) const;

    bool knownToBeOpaque(const RenderElement*) const;

    void setInitiator(const AtomicString& name) { m_initiatorName = name; }

private:
    explicit CSSImageValue(const String& url);
    explicit CSSImageValue(CachedImage&);

    String m_url;
    CachedResourceHandle<CachedImage> m_cachedImage;
    bool m_accessedImage;
    AtomicString m_initiatorName;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSImageValue, isImageValue())

#endif // CSSImageValue_h
