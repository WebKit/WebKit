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
#include "Settings.h"

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

ExceptionOr<Ref<Document>> DOMParser::parseFromString(const String& string, const String& contentType, ParseFromStringOptions options)
{
    if (contentType != "text/html"_s && contentType != "text/xml"_s && contentType != "application/xml"_s && contentType != "application/xhtml+xml"_s && contentType != "image/svg+xml"_s)
        return Exception { TypeError };
    auto document = DOMImplementation::createDocument(contentType, nullptr, m_settings, URL { });
    if (m_contextDocument)
        document->setContextDocument(*m_contextDocument.get());
    if (options.includeShadowRoots)
        document->setParserContentPolicy({ ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent, ParserContentPolicy::AllowDeclarativeShadowDOM });
    else
        document->setParserContentPolicy({ ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    document->setContent(string);
    if (m_contextDocument) {
        document->setURL(m_contextDocument->url());
        document->setSecurityOriginPolicy(m_contextDocument->securityOriginPolicy());
    }
    return document;
}

} // namespace WebCore
