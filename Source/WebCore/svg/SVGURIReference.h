/*
 * Copyright (C) 2004, 2005, 2008, 2009 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Document.h"
#include "QualifiedName.h"
#include "SVGAnimatedString.h"

namespace WebCore {

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeRegistry;

template<typename OwnerType, typename... BaseTypes>
class SVGAttributeOwnerProxyImpl;

class SVGURIReference {
    WTF_MAKE_NONCOPYABLE(SVGURIReference);
public:
    virtual ~SVGURIReference() = default;

    void parseAttribute(const QualifiedName&, const AtomicString&);

    static String fragmentIdentifierFromIRIString(const String&, const Document&);

    struct TargetElementResult {
        RefPtr<Element> element;
        String identifier;
    };
    static TargetElementResult targetElementFromIRIString(const String&, const Document&, RefPtr<Document> externalDocument = nullptr);

    static bool isExternalURIReference(const String& uri, const Document& document)
    {
        // Fragment-only URIs are always internal
        if (uri.startsWith('#'))
            return false;

        // If the URI matches our documents URL, we're dealing with a local reference.
        URL url = document.completeURL(uri);
        ASSERT(!url.protocolIsData());
        return !equalIgnoringFragmentIdentifier(url, document.url());
    }

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGURIReference>;
    using AttributeRegistry = SVGAttributeRegistry<SVGURIReference>;
    static AttributeRegistry& attributeRegistry();

    const String& href() const;
    RefPtr<SVGAnimatedString> hrefAnimated();

protected:
    SVGURIReference(SVGElement* contextElement);

    static bool isKnownAttribute(const QualifiedName& attributeName);

private:
    static void registerAttributes();

    std::unique_ptr<AttributeOwnerProxy> m_attributeOwnerProxy;
    SVGAnimatedStringAttribute m_href;
};

} // namespace WebCore
