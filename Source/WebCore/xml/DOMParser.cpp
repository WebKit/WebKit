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

#include "HTMLDocument.h"
#include "SVGDocument.h"
#include "SecurityOriginPolicy.h"
#include "XMLDocument.h"

namespace WebCore {

inline DOMParser::DOMParser()
{
}

DOMParser::~DOMParser() = default;

Ref<DOMParser> DOMParser::create()
{
    return adoptRef(*new DOMParser());
}

ExceptionOr<Ref<Document>> DOMParser::parseFromString(Document& contextDocument, const String& string, const String& contentType)
{
    auto url = contextDocument.url();
    auto& settings = contextDocument.settings();
    RefPtr<Document> document;
    if (contentType == "text/html"_s)
        document = HTMLDocument::create(nullptr, settings, url);
    else if (contentType == "application/xhtml+xml"_s)
        document = XMLDocument::createXHTML(nullptr, settings, url);
    else if (contentType == "image/svg+xml"_s)
        document = SVGDocument::create(nullptr, settings, url);
    else if (contentType == "text/xml"_s || contentType == "application/xml"_s) {
        document = XMLDocument::create(nullptr, settings, url);
        document->overrideMIMEType(contentType);
    } else
        return Exception { ExceptionCode::TypeError };

    document->setContextDocument(contextDocument);
    document->setMarkupUnsafe(string, { });
    document->setSecurityOriginPolicy(contextDocument.securityOriginPolicy());
    return document.releaseNonNull();
}

} // namespace WebCore
