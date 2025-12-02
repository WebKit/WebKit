/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "StyleSVGMarkerResource.h"
#include "XLinkNames.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGURIReference);

SVGURIReference::SVGURIReference(SVGElement* contextElement)
    : m_href(SVGAnimatedString::create(contextElement, IsHrefProperty::Yes))
{
    static bool didRegistration = false;
    if (!didRegistration) [[unlikely]] {
        didRegistration = true;
        PropertyRegistry::registerProperty<SVGNames::hrefAttr, &SVGURIReference::m_href>();
        PropertyRegistry::registerProperty<XLinkNames::hrefAttr, &SVGURIReference::m_href>();
    }
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
    size_t start = url.find('#');
    if (start == notFound)
        return emptyAtom();

    if (!start)
        return StringView(url).substring(1).toAtomString();

    URL base = URL(document.baseURL(), url.left(start));
    String fragmentIdentifier = url.substring(start);
    URL urlWithFragment(base, fragmentIdentifier);
    if (equalIgnoringFragmentIdentifier(urlWithFragment, document.url()))
        return StringView(fragmentIdentifier).substring(1).toAtomString();

    // The url doesn't have any fragment identifier.
    return emptyAtom();
}

AtomString SVGURIReference::fragmentIdentifierFromIRIString(const Style::URL& url, const Document& document)
{
    return fragmentIdentifierFromIRIString(url.resolved.string(), document);
}

AtomString SVGURIReference::fragmentIdentifierFromIRIString(const Style::SVGMarkerResource& markerResource, const Document& document)
{
    if (auto url = markerResource.tryURL())
        return fragmentIdentifierFromIRIString(*url, document);
    return emptyAtom();
}

auto SVGURIReference::targetElementFromIRIString(const String& iri, const TreeScope& treeScope, RefPtr<Document> externalDocument) -> TargetElementResult
{
    // If there's no fragment identifier contained within the IRI string, we can't lookup an element.
    size_t startOfFragmentIdentifier = iri.find('#');
    if (startOfFragmentIdentifier == notFound)
        return { };

    // Exclude the '#' character when determining the fragmentIdentifier.
    auto id = StringView(iri).substring(startOfFragmentIdentifier + 1).toAtomString();
    if (id.isEmpty())
        return { };

    Ref document = treeScope.documentScope();
    auto url = document->completeURL(iri);
    if (externalDocument) {
        // Enforce that the referenced url matches the url of the document that we've loaded for it!
        ASSERT(equalIgnoringFragmentIdentifier(url, externalDocument->url()));
        return { externalDocument->getElementById(id), WTFMove(id) };
    }

    if (url.protocolIsData()) {
        // FIXME: We need to load the data url in a Document to be able to get the target element.
        if (!equalIgnoringFragmentIdentifier(url, document->url()))
            return { nullptr, WTFMove(id) };
    }

    // Exit early if the referenced url is external, and we have no externalDocument given.
    if (isExternalURIReference(iri, document))
        return { nullptr, WTFMove(id) };

    RefPtr shadowHost = treeScope.rootNode().shadowHost();
    if (is<SVGUseElement>(shadowHost))
        return { shadowHost->treeScope().getElementById(id), WTFMove(id) };

    return { treeScope.getElementById(id), WTFMove(id) };
}

auto SVGURIReference::targetElementFromIRIString(const Style::URL& iri, const TreeScope& treeScope, RefPtr<Document> externalDocument) -> TargetElementResult
{
    return targetElementFromIRIString(iri.resolved.string(), treeScope, WTFMove(externalDocument));
}

bool SVGURIReference::haveLoadedRequiredResources() const
{
    if (href().isEmpty())
        return true;
    if (contextElement().protectedDocument()->completeURL(href()).protocolIsData())
        return true;
    if (!isExternalURIReference(href(), contextElement().protectedDocument()))
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
