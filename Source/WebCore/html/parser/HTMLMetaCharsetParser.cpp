/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include <pal/text/TextCodec.h>
#include <pal/text/TextEncodingRegistry.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLMetaCharsetParser);

using namespace HTMLNames;

HTMLMetaCharsetParser::HTMLMetaCharsetParser()
    : m_codec(newTextCodec(PAL::Latin1Encoding()))
{
}

static StringView extractCharset(StringView value)
{
    unsigned length = value.length();
    for (size_t pos = 0; pos < length; ) {
        pos = value.findIgnoringASCIICase("charset"_s, pos);
        if (pos == notFound)
            break;

        static const size_t charsetLength = sizeof("charset") - 1;
        pos += charsetLength;

        // Skip whitespace.
        while (pos < length && value[pos] <= ' ')
            ++pos;

        // Ensure we're in bounds before checking for '='.
        if (pos >= length)
            break;

        if (value[pos] != '=')
            continue;

        ++pos;

        while (pos < length && value[pos] <= ' ')
            ++pos;

        char16_t quoteMark = 0;
        if (pos < length && (value[pos] == '"' || value[pos] == '\''))
            quoteMark = value[pos++];

        if (pos == length)
            break;

        unsigned end = pos;
        while (end < length && ((quoteMark && value[end] != quoteMark) || (!quoteMark && value[end] > ' ' && value[end] != '"' && value[end] != '\'' && value[end] != ';')))
            ++end;

        if (quoteMark && (end == length))
            break; // Close quote not found.

        return value.substring(pos, end - pos);
    }
    return { };
}

bool HTMLMetaCharsetParser::processMeta(HTMLToken& token)
{
    auto attributes = token.attributes().map([](auto& attribute) {
        return std::pair { StringView { attribute.name.span() }, StringView { attribute.value.span() } };
    });
    m_encoding = encodingFromMetaAttributes(attributes);
    return m_encoding.isValid();
}

PAL::TextEncoding HTMLMetaCharsetParser::encodingFromMetaAttributes(std::span<const std::pair<StringView, StringView>> attributes)
{
    bool gotPragma = false;
    enum { None, Charset, Pragma } mode = None;
    StringView charset;

    for (auto& attribute : attributes) {
        auto& attributeName = attribute.first;
        auto& attributeValue = attribute.second;

        if (attributeName == "http-equiv"_s) {
            if (equalLettersIgnoringASCIICase(attributeValue, "content-type"_s))
                gotPragma = true;
        } else if (attributeName == "charset"_s) {
            charset = attributeValue;
            mode = Charset;
            // Charset attribute takes precedence
            break;
        } else if (attributeName == "content"_s) {
            charset = extractCharset(attributeValue);
            if (charset.length())
                mode = Pragma;
        }
    }

    if (mode == Charset || (mode == Pragma && gotPragma))
        return charset.trim(isASCIIWhitespace<char16_t>);

    return PAL::TextEncoding();
}

bool HTMLMetaCharsetParser::checkForMetaCharset(std::span<const uint8_t> data)
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
    m_input.append(m_codec->decode(data, false, false, ignoredSawErrorFlag));

    while (auto token = m_tokenizer.nextToken(m_input)) {
        bool isEnd = token->type() == HTMLToken::Type::EndTag;
        if (isEnd || token->type() == HTMLToken::Type::StartTag) {
            auto knownTagName = AtomString::lookUp(token->name().span());
            if (!isEnd) {
                m_tokenizer.updateStateFor(knownTagName);
                if (knownTagName == metaTag && processMeta(*token)) {
                    m_doneChecking = true;
                    return true;
                }
            }

            if (knownTagName != scriptTag && knownTagName != noscriptTag
                && knownTagName != styleTag && knownTagName != linkTag
                && knownTagName != metaTag && knownTagName != objectTag
                && knownTagName != titleTag && knownTagName != baseTag
                && (isEnd || knownTagName != htmlTag)
                && (isEnd || knownTagName != headTag)) {
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
