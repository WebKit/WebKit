/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "SVGElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGUseElement.h"
#include "XLinkNames.h"
#include <wtf/URL.h>

namespace WebCore {

SVGURIReference::SVGURIReference(SVGElement* contextElement)
    : m_href(SVGAnimatedString::create(contextElement))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::hrefAttr, &SVGURIReference::m_href>();
        PropertyRegistry::registerProperty<XLinkNames::hrefAttr, &SVGURIReference::m_href>();
    });
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attributeName)
{
    return PropertyRegistry::isKnownAttribute(attributeName);
}

SVGElement& SVGURIReference::contextElement() const
{
    return *m_href->contextElement();
}

void SVGURIReference::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name.matches(SVGNames::hrefAttr))
        m_href->setBaseValInternal(value.isNull() ? contextElement().getAttribute(XLinkNames::hrefAttr) : value);
    else if (name.matches(XLinkNames::hrefAttr) && !contextElement().hasAttribute(SVGNames::hrefAttr))
        m_href->setBaseValInternal(value);
}

AtomString SVGURIReference::fragmentIdentifierFromIRIString(const String& url, const Document& document)
{
    auto trimmedURL = url.trim(isASCIIWhitespace<UChar>);
    size_t start = trimmedURL.find('#');
    if (start == notFound)
        return emptyAtom();

    if (!start)
        return StringView(trimmedURL).substring(1).toAtomString();

    URL base = URL(document.baseURL(), trimmedURL.left(start));
    String fragmentIdentifier = trimmedURL.substring(start);
    URL urlWithFragment(base, fragmentIdentifier);
    if (equalIgnoringFragmentIdentifier(urlWithFragment, document.url()))
        return StringView(fragmentIdentifier).substring(1).trim(isASCIIWhitespace<UChar>).toAtomString();

    // The url doesn't have any fragment identifier.
    return emptyAtom();
}

auto SVGURIReference::targetElementFromIRIString(const String& iri, const TreeScope& treeScope, RefPtr<Document> externalDocument) -> TargetElementResult
{
    // If there's no fragment identifier contained within the IRI string, we can't lookup an element.
    auto trimmedIri = iri.trim(isASCIIWhitespace<UChar>);
    size_t startOfFragmentIdentifier = trimmedIri.find('#');
    if (startOfFragmentIdentifier == notFound)
        return { };

    // Exclude the '#' character when determining the fragmentIdentifier.
    auto id = StringView(trimmedIri).substring(startOfFragmentIdentifier + 1).toAtomString();
    if (id.isEmpty())
        return { };

    Ref document = treeScope.documentScope();
    auto url = document->completeURL(trimmedIri);
    if (externalDocument) {
        // Enforce that the referenced url matches the url of the document that we've loaded for it!
        ASSERT(equalIgnoringFragmentIdentifier(url, externalDocument->url()));
        return { externalDocument->getElementById(id), WTFMove(id) };
    }

    // Exit early if the referenced url is external, and we have no externalDocument given.
    if (isExternalURIReference(trimmedIri, document))
        return { nullptr, WTFMove(id) };

    RefPtr shadowHost = treeScope.rootNode().shadowHost();
    if (is<SVGUseElement>(shadowHost))
        return { shadowHost->treeScope().getElementById(id), WTFMove(id) };

    return { treeScope.getElementById(id), WTFMove(id) };
}

bool SVGURIReference::haveLoadedRequiredResources() const
{
    if (href().isEmpty() || !isExternalURIReference(href(), contextElement().protectedDocument()))
        return true;
    return errorOccurred() || haveFiredLoadEvent();
}

void SVGURIReference::dispatchLoadEvent()
{
    if (haveFiredLoadEvent())
        return;

    // Dispatch the load event
    setHaveFiredLoadEvent(true);
    ASSERT(contextElement().haveLoadedRequiredResources());

    contextElement().sendLoadEventIfPossible();
}

}
