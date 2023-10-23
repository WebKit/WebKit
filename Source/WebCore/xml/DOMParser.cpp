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
#include "DocumentFragment.h"
#include "FragmentScriptingPermission.h"
#include "HTMLBodyElement.h"
#include "HTMLDocumentParserFastPath.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
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
    OptionSet<ParserContentPolicy> parsingOptions;
    if (options.includeShadowRoots && document->settings().declarativeShadowDOMInDOMParserEnabled())
        parsingOptions = { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent, ParserContentPolicy::AllowDeclarativeShadowDOM };
    else
        parsingOptions = { ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent };
    document->setParserContentPolicy(parsingOptions);
    bool usedFastPath = false;
    if (contentType == "text/html"_s) {
        auto body = HTMLBodyElement::create(document);
        usedFastPath = tryFastParsingHTMLFragment(StringView { string }.substring(string.find(isNotASCIIWhitespace<UChar>)), document, body, body, parsingOptions);
        if (LIKELY(usedFastPath)) {
            auto html = HTMLHtmlElement::create(document);
            document->appendChild(html);
            auto head = HTMLHeadElement::create(document);
            html->appendChild(head);
            html->appendChild(body);
        }
    }
    if (!usedFastPath)
        document->setContent(string);
    if (m_contextDocument) {
        document->setURL(m_contextDocument->url());
        document->setSecurityOriginPolicy(m_contextDocument->securityOriginPolicy());
    }
    return document;
}

} // namespace WebCore
