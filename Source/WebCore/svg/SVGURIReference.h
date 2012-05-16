/*
 * Copyright (C) 2004, 2005, 2008, 2009 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGURIReference_h
#define SVGURIReference_h

#if ENABLE(SVG)
#include "SVGElement.h"
#include "XLinkNames.h"

namespace WebCore {

class Attribute;
class Document;
class Element;

class SVGURIReference {
public:
    virtual ~SVGURIReference() { }

    bool parseAttribute(const Attribute&);
    bool isKnownAttribute(const QualifiedName&);
    void addSupportedAttributes(HashSet<QualifiedName>&);

    static String fragmentIdentifierFromIRIString(const String&, Document*);
    static Element* targetElementFromIRIString(const String&, Document*, String* = 0, Document* = 0);

    static inline bool isExternalURIReference(const String& uri, Document* document)
    {
        // If the URI matches our documents URL, early exit, we're dealing with a local reference.
        ASSERT(document);
        KURL url = document->completeURL(uri);
        if (equalIgnoringFragmentIdentifier(url, document->url()))
            return false;

        // If the URI doesn't contain a base string, just see if it starts with a fragment-identifier.
        return uri.find('#') != notFound;
    }

protected:
    virtual void setHrefBaseValue(const String&) = 0;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGURIReference_h
