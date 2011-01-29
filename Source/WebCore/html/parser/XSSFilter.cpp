/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XSSFilter.h"

#include "Document.h"
#include "Frame.h"
#include "HTMLDocumentParser.h"
#include "HTMLNames.h"
#include "Settings.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include <wtf/text/CString.h>

// This preprocesssor macro is a temporary scaffold while this code is still an experiment.
#define XSS_DETECTOR_ENABLED 0

namespace WebCore {

using namespace HTMLNames;

namespace {

bool hasName(const HTMLToken& token, const QualifiedName& name)
{
    return equalIgnoringNullity(token.name(), static_cast<const String&>(name.localName()));
}

bool findAttributeWithName(const HTMLToken& token, const QualifiedName& name, size_t& indexOfMatchingAttribute)
{
    for (size_t i = 0; i < token.attributes().size(); ++i) {
        if (equalIgnoringNullity(token.attributes().at(i).m_name, name.localName())) {
            indexOfMatchingAttribute = i;
            return true;
        }
    }
    return false;
}

bool isNameOfInlineEventHandler(const Vector<UChar, 32>& name)
{
    const size_t lengthOfShortestInlineEventHandlerName = 5; // To wit: oncut.
    if (name.size() < lengthOfShortestInlineEventHandlerName)
        return false;
    return name[0] == 'o' && name[1] == 'n';
}

String decodeURL(const String& string, const TextEncoding& encoding)
{
    String workingString = string;
    workingString.replace('+', ' ');
    workingString = decodeURLEscapeSequences(workingString);
    CString workingStringUTF8 = workingString.utf8();
    String decodedString = encoding.decode(workingStringUTF8.data(), workingStringUTF8.length());
    // FIXME: Is this check necessary?
    if (decodedString.isEmpty())
        return workingString;
    return decodedString;
}

}

XSSFilter::XSSFilter(HTMLDocumentParser* parser)
    : m_parser(parser)
    , m_isEnabled(false)
    , m_state(Initial)
{
    ASSERT(m_parser);
    if (Frame* frame = parser->document()->frame()) {
        if (Settings* settings = frame->settings())
            m_isEnabled = settings->xssAuditorEnabled();
    }
}

void XSSFilter::filterToken(HTMLToken& token)
{
#if !XSS_DETECTOR_ENABLED
    ASSERT_UNUSED(token, &token);
    return;
#else
    if (!m_isEnabled)
        return;

    bool didBlockScript = false;

    switch (m_state) {
    case Initial: 
        didBlockScript = filterTokenInitial(token);
        break;
    case AfterScriptStartTag:
        didBlockScript = filterTokenAfterScriptStartTag(token);
        ASSERT(m_state == Initial);
        m_cachedSnippet = String();
        break;
    }

    if (didBlockScript) {
        // FIXME: Consider using a more helpful console message.
        DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute a JavaScript script. Source code of script found within request.\n"));
        // FIXME: We should add the real line number to the console.
        m_parser->document()->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());
    }
#endif
}

bool XSSFilter::filterTokenInitial(HTMLToken& token)
{
    ASSERT(m_state == Initial);

    if (token.type() != HTMLToken::StartTag)
        return false;

    bool didBlockScript = eraseInlineEventHandlersIfInjected(token);

    if (hasName(token, scriptTag))
        didBlockScript |= filterScriptToken(token);
    else if (hasName(token, objectTag))
        didBlockScript |= filterObjectToken(token);
    else if (hasName(token, embedTag))
        didBlockScript |= filterEmbedToken(token);
    else if (hasName(token, appletTag))
        didBlockScript |= filterAppletToken(token);
    else if (hasName(token, metaTag))
        didBlockScript |= filterMetaToken(token);
    else if (hasName(token, baseTag))
        didBlockScript |= filterBaseToken(token);

    return didBlockScript;
}

bool XSSFilter::filterTokenAfterScriptStartTag(HTMLToken& token)
{
    ASSERT(m_state == AfterScriptStartTag);
    m_state = Initial;

    if (token.type() != HTMLToken::Character) {
        ASSERT(token.type() == HTMLToken::EndTag || token.type() == HTMLToken::EndOfFile);
        return false;
    }

    int start = 0;
    // FIXME: We probably want to grab only the first few characters of the
    //        contents of the script element.
    int end = token.endIndex() - token.startIndex();
    if (isContainedInRequest(m_cachedSnippet + snippetForRange(token, start, end))) {
        token.eraseCharacters();
        token.appendToCharacter(' '); // Technically, character tokens can't be empty.
        return true;
    }
    return false;
}

bool XSSFilter::filterScriptToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, scriptTag));

    if (eraseAttributeIfInjected(token, srcAttr))
        return true;

    m_state = AfterScriptStartTag;
    m_cachedSnippet = m_parser->sourceForToken(token);
    return false;
}

bool XSSFilter::filterObjectToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, objectTag));

    bool didBlockScript = false;

    didBlockScript |= eraseAttributeIfInjected(token, dataAttr);
    didBlockScript |= eraseAttributeIfInjected(token, typeAttr);
    didBlockScript |= eraseAttributeIfInjected(token, classidAttr);

    return didBlockScript;
}

bool XSSFilter::filterEmbedToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, embedTag));

    bool didBlockScript = false;

    didBlockScript |= eraseAttributeIfInjected(token, srcAttr);
    didBlockScript |= eraseAttributeIfInjected(token, typeAttr);

    return didBlockScript;
}

bool XSSFilter::filterAppletToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, appletTag));

    bool didBlockScript = false;

    didBlockScript |= eraseAttributeIfInjected(token, codeAttr);
    didBlockScript |= eraseAttributeIfInjected(token, objectAttr);

    return didBlockScript;
}

bool XSSFilter::filterMetaToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, metaTag));

    return eraseAttributeIfInjected(token, http_equivAttr);
}

bool XSSFilter::filterBaseToken(HTMLToken& token)
{
    ASSERT(m_state == Initial);
    ASSERT(token.type() == HTMLToken::StartTag);
    ASSERT(hasName(token, baseTag));

    return eraseAttributeIfInjected(token, hrefAttr);
}

bool XSSFilter::eraseInlineEventHandlersIfInjected(HTMLToken& token)
{
    bool didBlockScript = false;
    for (size_t i = 0; i < token.attributes().size(); ++i) {
        const HTMLToken::Attribute& attribute = token.attributes().at(i);
        if (!isNameOfInlineEventHandler(attribute.m_name))
            continue;
        if (!isContainedInRequest(snippetForAttribute(token, attribute)))
            continue;
        token.eraseValueOfAttribute(i);
        didBlockScript = true;
    }
    return didBlockScript;
}

bool XSSFilter::eraseAttributeIfInjected(HTMLToken& token, const QualifiedName& attributeName)
{
    size_t indexOfAttribute;
    if (findAttributeWithName(token, attributeName, indexOfAttribute)) {
        const HTMLToken::Attribute& attribute = token.attributes().at(indexOfAttribute);
        if (isContainedInRequest(snippetForAttribute(token, attribute)))
            token.eraseValueOfAttribute(indexOfAttribute);
        return true;
    }
    return false;
}

String XSSFilter::snippetForRange(const HTMLToken& token, int start, int end)
{
    // FIXME: There's an extra allocation here that we could save by
    //        passing the range to the parser.
    return m_parser->sourceForToken(token).substring(start, end - start);
}

String XSSFilter::snippetForAttribute(const HTMLToken& token, const HTMLToken::Attribute& attribute)
{
    // FIXME: We should grab one character before the name also.
    int start = attribute.m_nameRange.m_start - token.startIndex();
    // FIXME: We probably want to grab only the first few characters of the attribute value.
    int end = attribute.m_valueRange.m_end - token.startIndex();
    return snippetForRange(token, start, end);
}

bool XSSFilter::isContainedInRequest(const String& snippet)
{
    String url = m_parser->document()->url().string();
    String decodedURL = decodeURL(url, m_parser->document()->decoder()->encoding());
    if (decodedURL.find(snippet, 0, false) != notFound)
        return true; // We've found the string in the GET data.
    // FIXME: Look in form data.
    return false;
}

}
