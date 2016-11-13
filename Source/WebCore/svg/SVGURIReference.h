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

#pragma once

#include "Document.h"
#include "QualifiedName.h"

namespace WebCore {

class SVGURIReference {
public:
    virtual ~SVGURIReference() { }

    void parseAttribute(const QualifiedName&, const AtomicString&);
    static bool isKnownAttribute(const QualifiedName&);
    static void addSupportedAttributes(HashSet<QualifiedName>&);

    static String fragmentIdentifierFromIRIString(const String&, const Document&);
    static Element* targetElementFromIRIString(const String&, const Document&, String* fragmentIdentifier = nullptr, const Document* externalDocument = nullptr);

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

protected:
    virtual String& hrefBaseValue() const = 0;
    virtual void setHrefBaseValue(const String&, const bool validValue = true) = 0;
};

} // namespace WebCore
