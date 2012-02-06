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

Element* SVGURIReference::targetElementFromIRIString(const String& iri, Document* document, String* fragmentIdentifier)
{
    String id = fragmentIdentifierFromIRIString(iri, document);
    if (fragmentIdentifier)
        *fragmentIdentifier = id;
    // FIXME: Handle external references (Bug 65344).
    return document->getElementById(id);
}

void SVGURIReference::addSupportedAttributes(HashSet<QualifiedName>& supportedAttributes)
{
    supportedAttributes.add(XLinkNames::hrefAttr);
}

}

#endif // ENABLE(SVG)
