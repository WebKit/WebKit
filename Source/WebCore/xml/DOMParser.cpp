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

#include "DOMImplementation.h"
#include "SecurityOriginPolicy.h"

namespace WebCore {

inline DOMParser::DOMParser(Document& contextDocument)
    : m_contextDocument(makeWeakPtr(contextDocument))
{
}

Ref<DOMParser> DOMParser::create(Document& contextDocument)
{
    return adoptRef(*new DOMParser(contextDocument));
}

ExceptionOr<Ref<Document>> DOMParser::parseFromString(const String& string, const String& contentType)
{
    if (contentType != "text/html" && contentType != "text/xml" && contentType != "application/xml" && contentType != "application/xhtml+xml" && contentType != "image/svg+xml")
        return Exception { TypeError };
    auto document = DOMImplementation::createDocument(contentType, nullptr, URL { });
    if (m_contextDocument)
        document->setContextDocument(*m_contextDocument.get());
    document->setContent(string);
    if (m_contextDocument) {
        document->setURL(m_contextDocument->url());
        document->setSecurityOriginPolicy(m_contextDocument->securityOriginPolicy());
    }
    return document;
}

} // namespace WebCore
