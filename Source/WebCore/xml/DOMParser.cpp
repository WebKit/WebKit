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

inline DOMParser::DOMParser(Document& contextDocument)
    : m_contextDocument(contextDocument)
    , m_settings(contextDocument.settings())
{
}

DOMParser::~DOMParser() = default;

Ref<DOMParser> DOMParser::create(Document& contextDocument)
{
    return adoptRef(*new DOMParser(contextDocument));
}

ExceptionOr<Ref<Document>> DOMParser::parseFromString(const String& string, const String& contentType)
{
    URL url = { };
    if (m_contextDocument)
        url = m_contextDocument->url();
    RefPtr<Document> document;
    if (contentType == "text/html"_s)
        document = HTMLDocument::create(nullptr, m_settings, url);
    else if (contentType == "application/xhtml+xml"_s)
        document = XMLDocument::createXHTML(nullptr, m_settings, url);
    else if (contentType == "image/svg+xml"_s)
        document = SVGDocument::create(nullptr, m_settings, url);
    else if (contentType == "text/xml"_s || contentType == "application/xml"_s) {
        document = XMLDocument::create(nullptr, m_settings, url);
        document->overrideMIMEType(contentType);
    } else
        return Exception { ExceptionCode::TypeError };

    if (m_contextDocument)
        document->setContextDocument(*m_contextDocument.get());
    document->setMarkupUnsafe(string, { });
    if (m_contextDocument)
        document->setSecurityOriginPolicy(m_contextDocument->securityOriginPolicy());
    return document.releaseNonNull();
}

} // namespace WebCore
