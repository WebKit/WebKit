/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMParser.h"

#include "CommonAtomStrings.h"
#include "ExceptionOr.h"
#include "HTMLDocument.h"
#include "SVGDocument.h"
#include "SecurityOriginPolicy.h"
#include "TrustedType.h"
#include "XMLDocument.h"

namespace WebCore {

inline DOMParser::DOMParser(Document& contextDocument)
    : m_contextDocument(contextDocument)
{
}

DOMParser::~DOMParser() = default;

Ref<DOMParser> DOMParser::create(Document& contextDocument)
{
    return adoptRef(*new DOMParser(contextDocument));
}

ExceptionOr<Ref<Document>> DOMParser::parseFromString(Variant<Ref<TrustedHTML>, String>&& string, const AtomString& contentType)
{
    Ref contextDocument = *m_contextDocument;
    auto stringValueHolder = trustedTypeCompliantString(protect(contextDocument->contextDocument()), WTF::move(string), "DOMParser parseFromString"_s);

    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    RefPtr<Document> document;
    if (contentType == textHTMLContentTypeAtom())
        document = HTMLDocument::create(contextDocument, URL { }, contextDocument->securityOriginPolicy());
    else if (contentType == applicationXHTMLContentTypeAtom())
        document = XMLDocument::createXHTML(contextDocument, URL { }, contextDocument->securityOriginPolicy());
    else if (contentType == imageSVGContentTypeAtom())
        document = SVGDocument::create(contextDocument, URL { }, contextDocument->securityOriginPolicy());
    else if (contentType == textXMLContentTypeAtom() || contentType == applicationXMLContentTypeAtom()) {
        document = XMLDocument::create(contextDocument, URL { }, contextDocument->securityOriginPolicy());
        document->overrideMIMEType(contentType);
    } else
        return Exception { ExceptionCode::TypeError };

    document->setURL(URL { contextDocument->url() });
    document->setMarkupUnsafe(stringValueHolder.releaseReturnValue(), { });
    return document.releaseNonNull();
}

} // namespace WebCore
