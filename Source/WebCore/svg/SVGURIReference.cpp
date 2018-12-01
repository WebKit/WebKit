/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "config.h"
#include "SVGURIReference.h"

#include "Document.h"
#include "Element.h"
#include "SVGAttributeOwnerProxy.h"
#include <wtf/URL.h>
#include "XLinkNames.h"

namespace WebCore {

SVGURIReference::SVGURIReference(SVGElement* contextElement)
    : m_attributeOwnerProxy(std::make_unique<AttributeOwnerProxy>(*this, *contextElement))
{
    registerAttributes();
}

void SVGURIReference::registerAttributes()
{
    auto& registry = attributeRegistry();
    if (!registry.isEmpty())
        return;
    registry.registerAttribute<SVGNames::hrefAttr, &SVGURIReference::m_href>();
    registry.registerAttribute<XLinkNames::hrefAttr, &SVGURIReference::m_href>();
}

SVGURIReference::AttributeRegistry& SVGURIReference::attributeRegistry()
{
    return AttributeOwnerProxy::attributeRegistry();
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attributeName)
{
    return AttributeOwnerProxy::isKnownAttribute(attributeName);
}

void SVGURIReference::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (isKnownAttribute(name))
        m_href.setValue(value);
}

const String& SVGURIReference::href() const
{
    return m_href.currentValue(*m_attributeOwnerProxy);
}

RefPtr<SVGAnimatedString> SVGURIReference::hrefAnimated()
{
    return m_href.animatedProperty(*m_attributeOwnerProxy);
}

String SVGURIReference::fragmentIdentifierFromIRIString(const String& url, const Document& document)
{
    size_t start = url.find('#');
    if (start == notFound)
        return emptyString();

    URL base = start ? URL(document.baseURL(), url.substring(0, start)) : document.baseURL();
    String fragmentIdentifier = url.substring(start);
    URL kurl(base, fragmentIdentifier);
    if (equalIgnoringFragmentIdentifier(kurl, document.url()))
        return fragmentIdentifier.substring(1);

    // The url doesn't have any fragment identifier.
    return emptyString();
}

auto SVGURIReference::targetElementFromIRIString(const String& iri, const TreeScope& treeScope, RefPtr<Document> externalDocument) -> TargetElementResult
{
    // If there's no fragment identifier contained within the IRI string, we can't lookup an element.
    size_t startOfFragmentIdentifier = iri.find('#');
    if (startOfFragmentIdentifier == notFound)
        return { };

    // Exclude the '#' character when determining the fragmentIdentifier.
    auto id = iri.substring(startOfFragmentIdentifier + 1);
    if (id.isEmpty())
        return { };

    auto& document = treeScope.documentScope();
    auto url = document.completeURL(iri);
    if (externalDocument) {
        // Enforce that the referenced url matches the url of the document that we've loaded for it!
        ASSERT(equalIgnoringFragmentIdentifier(url, externalDocument->url()));
        return { externalDocument->getElementById(id), WTFMove(id) };
    }

    // Exit early if the referenced url is external, and we have no externalDocument given.
    if (isExternalURIReference(iri, document))
        return { nullptr, WTFMove(id) };

    return { treeScope.getElementById(id), WTFMove(id) };
}

}
