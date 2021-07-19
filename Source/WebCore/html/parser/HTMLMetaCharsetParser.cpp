/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "HTMLMetaCharsetParser.h"

#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "TextCodec.h"
#include "TextEncodingRegistry.h"

namespace WebCore {

using namespace HTMLNames;

HTMLMetaCharsetParser::HTMLMetaCharsetParser()
    : m_codec(newTextCodec(Latin1Encoding()))
{
}

static StringView extractCharset(const String& value)
{
    unsigned length = value.length();
    for (size_t pos = 0; pos < length; ) {
        pos = value.findIgnoringASCIICase("charset", pos);
        if (pos == notFound)
            break;

        static const size_t charsetLength = sizeof("charset") - 1;
        pos += charsetLength;

        // Skip whitespace.
        while (pos < length && value[pos] <= ' ')
            ++pos;

        if (value[pos] != '=')
            continue;

        ++pos;

        while (pos < length && value[pos] <= ' ')
            ++pos;

        UChar quoteMark = 0;
        if (pos < length && (value[pos] == '"' || value[pos] == '\''))
            quoteMark = value[pos++];

        if (pos == length)
            break;

        unsigned end = pos;
        while (end < length && ((quoteMark && value[end] != quoteMark) || (!quoteMark && value[end] > ' ' && value[end] != '"' && value[end] != '\'' && value[end] != ';')))
            ++end;

        if (quoteMark && (end == length))
            break; // Close quote not found.

        return StringView(value).substring(pos, end - pos);
    }
    return StringView();
}

bool HTMLMetaCharsetParser::processMeta(HTMLToken& token)
{
    AttributeList attributes;
    for (auto& attribute : token.attributes()) {
        String attributeName = StringImpl::create8BitIfPossible(attribute.name);
        String attributeValue = StringImpl::create8BitIfPossible(attribute.value);
        attributes.append(std::make_pair(attributeName, attributeValue));
    }

    m_encoding = encodingFromMetaAttributes(attributes);
    return m_encoding.isValid();
}

TextEncoding HTMLMetaCharsetParser::encodingFromMetaAttributes(const AttributeList& attributes)
{
    bool gotPragma = false;
    enum { None, Charset, Pragma } mode = None;
    StringView charset;

    for (auto& attribute : attributes) {
        const String& attributeName = attribute.first;
        const String& attributeValue = attribute.second;

        if (attributeName == http_equivAttr) {
            if (equalLettersIgnoringASCIICase(attributeValue, "content-type"))
                gotPragma = true;
        } else if (attributeName == charsetAttr) {
            charset = attributeValue;
            mode = Charset;
            // Charset attribute takes precedence
            break;
        } else if (attributeName == contentAttr) {
            charset = extractCharset(attributeValue);
            if (charset.length())
                mode = Pragma;
        }
    }

    if (mode == Charset || (mode == Pragma && gotPragma))
        return TextEncoding(stripLeadingAndTrailingHTMLSpaces(charset.toStringWithoutCopying()));

    return TextEncoding();
}

bool HTMLMetaCharsetParser::checkForMetaCharset(const char* data, size_t length)
{
    if (m_doneChecking)
        return true;

    ASSERT(!m_encoding.isValid());

    // We still don't have an encoding, and are in the head.
    // The following tags are allowed in <head>:
    // SCRIPT|STYLE|META|LINK|OBJECT|TITLE|BASE
    //
    // We stop scanning when a tag that is not permitted in <head>
    // is seen, rather when </head> is seen, because that more closely
    // matches behavior in other browsers; more details in
    // <http://bugs.webkit.org/show_bug.cgi?id=3590>.
    //
    // Additionally, we ignore things that looks like tags in <title>, <script>
    // and <noscript>; see <http://bugs.webkit.org/show_bug.cgi?id=4560>,
    // <http://bugs.webkit.org/show_bug.cgi?id=12165> and
    // <http://bugs.webkit.org/show_bug.cgi?id=12389>.
    //
    // Since many sites have charset declarations after <body> or other tags
    // that are disallowed in <head>, we don't bail out until we've checked at
    // least bytesToCheckUnconditionally bytes of input.

    constexpr int bytesToCheckUnconditionally = 1024;

    bool ignoredSawErrorFlag;
    m_input.append(m_codec->decode(data, length, false, false, ignoredSawErrorFlag));

    while (auto token = m_tokenizer.nextToken(m_input)) {
        bool isEnd = token->type() == HTMLToken::EndTag;
        if (isEnd || token->type() == HTMLToken::StartTag) {
            AtomString tagName(token->name());
            if (!isEnd) {
                m_tokenizer.updateStateFor(tagName);
                if (tagName == metaTag && processMeta(*token)) {
                    m_doneChecking = true;
                    return true;
                }
            }

            if (tagName != scriptTag && tagName != noscriptTag
                && tagName != styleTag && tagName != linkTag
                && tagName != metaTag && tagName != objectTag
                && tagName != titleTag && tagName != baseTag
                && (isEnd || tagName != htmlTag)
                && (isEnd || tagName != headTag)) {
                m_inHeadSection = false;
            }
        }

        if (!m_inHeadSection && m_input.numberOfCharactersConsumed() >= bytesToCheckUnconditionally) {
            m_doneChecking = true;
            return true;
        }
    }

    return false;
}

}
