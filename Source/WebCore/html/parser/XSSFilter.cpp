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
#include "HTMLDocumentParser.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include <wtf/text/CString.h>

// This preprocesssor macro is a temporary scaffold while this code is still an experiment.
#define XSS_DETECTOR_ENABLED 0

namespace WebCore {

namespace {

bool isNameOfScriptCarryingAttribute(const Vector<UChar, 32>& name)
{
    const size_t lengthOfShortestScriptCarryingAttribute = 5; // To wit: oncut.
    if (name.size() < lengthOfShortestScriptCarryingAttribute)
        return false;
    if (name[0] != 'o' && name[0] != 'O')
        return false;
    if (name[1] != 'n' && name[0] != 'N')
        return false;
    return true;
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
{
    ASSERT(m_parser);
}

void XSSFilter::filterToken(HTMLToken& token)
{
#if !XSS_DETECTOR_ENABLED
    ASSERT_UNUSED(token, &token);
    return;
#else
    if (token.type() != HTMLToken::StartTag)
        return;

    HTMLToken::AttributeList::const_iterator iter = token.attributes().begin();
    for (; iter != token.attributes().end(); ++iter) {
        if (!isNameOfScriptCarryingAttribute(iter->m_name))
            continue;
        if (!isContainedInRequest(snippetForAttribute(token, *iter)))
            continue;
        iter->m_value.clear();
    }
#endif
}

String XSSFilter::snippetForAttribute(const HTMLToken& token, const HTMLToken::Attribute& attribute)
{
    // FIXME: We should grab one character before the name also.
    int start = attribute.m_nameRange.m_start - token.startIndex();
    // FIXME: We probably want to grab only the first few characters of the attribute value.
    int end = attribute.m_valueRange.m_end - token.startIndex();

    // FIXME: There's an extra allocation here that we could save by
    //        passing the range to the parser.
    return m_parser->sourceForToken(token).substring(start, end - start);
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
