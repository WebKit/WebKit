/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGURIReference.h"

#include "Attribute.h"
#include "Document.h"
#include "Element.h"
#include "KURL.h"

namespace WebCore {

bool SVGURIReference::parseAttribute(Attribute* attr)
{
    if (attr->name().matches(XLinkNames::hrefAttr)) {
        setHrefBaseValue(attr->value());
        return true;
    }

    return false;
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attrName)
{
    return attrName.matches(XLinkNames::hrefAttr);
}

String SVGURIReference::fragmentIdentifierFromIRIString(const String& url, Document* document)
{
    ASSERT(document);
    size_t start = url.find('#');
    if (start == notFound)
        return emptyString();

    KURL base = start ? KURL(document->baseURI(), url.substring(0, start)) : document->baseURI();
    String fragmentIdentifier = url.substring(start);
    KURL kurl(base, fragmentIdentifier);
    if (equalIgnoringFragmentIdentifier(kurl, document->url()))
        return fragmentIdentifier.substring(1);

    // The url doesn't have any fragment identifier.
    return emptyString();
}

static inline KURL urlFromIRIStringWithFragmentIdentifier(const String& url, Document* document, String& fragmentIdentifier)
{
    ASSERT(document);
    size_t startOfFragmentIdentifier = url.find('#');
    if (startOfFragmentIdentifier == notFound)
        return KURL();

    // Exclude the '#' character when determining the fragmentIdentifier.
    fragmentIdentifier = url.substring(startOfFragmentIdentifier + 1);
    if (startOfFragmentIdentifier) {
        KURL base(document->baseURI(), url.substring(0, startOfFragmentIdentifier));
        return KURL(base, url.substring(startOfFragmentIdentifier));
    }

    return KURL(document->baseURI(), url.substring(startOfFragmentIdentifier));
}

Element* SVGURIReference::targetElementFromIRIString(const String& iri, Document* document, String* fragmentIdentifier, Document* externalDocument)
{
    // If there's no fragment identifier contained within the IRI string, we can't lookup an element.
    String id;
    KURL url = urlFromIRIStringWithFragmentIdentifier(iri, document, id);
    if (url == KURL())
        return 0;

    // If we're requesting an external resources, and externalDocument is non-zero, the load already succeeded.
    // Go ahead and check if the externalDocuments URL matches the expected URL, that we resolved using the
    // host document before in urlFromIRIStringWithFragmentIdentifier(). For internal resources, the same
    // assumption must hold true, just with the host documents URL, not the external documents URL.
    if (!equalIgnoringFragmentIdentifier(url, externalDocument ? externalDocument->url() : document->url()))
        return 0;

    if (fragmentIdentifier)
        *fragmentIdentifier = id;

    if (id.isEmpty())
        return 0;

    if (externalDocument)
        return externalDocument->getElementById(id);
    if (isExternalURIReference(iri, document))
        return 0; // Non-existing external resource

    return document->getElementById(id);
}

void SVGURIReference::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(XLinkNames::hrefAttr);
}

}

#endif // ENABLE(SVG)
