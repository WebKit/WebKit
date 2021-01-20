/*
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Copyright (C) 2016 Akamai Technologies Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LinkHeader.h"

#include "ParsingUtilities.h"

namespace WebCore {

// LWSP definition in https://www.ietf.org/rfc/rfc0822.txt
template<typename CharacterType> static bool isSpaceOrTab(CharacterType character)
{
    return character == ' ' || character == '\t';
}

template<typename CharacterType> static bool isNotURLTerminatingChar(CharacterType character)
{
    return character != '>';
}

template<typename CharacterType> static bool isValidParameterNameChar(CharacterType character)
{
    // A separator, CTL or '%', '*' or '\'' means the char is not valid.
    // Definition as attr-char at https://tools.ietf.org/html/rfc5987
    // CTL and separators are defined in https://tools.ietf.org/html/rfc2616#section-2.2
    // Valid chars are alpha-numeric and any of !#$&+-.^_`|~"
    if ((character >= '^' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || (character >= '!' && character <= '$') || character == '&' || character == '+' || character == '-' || character == '.')
        return true;
    return false;
}

template<typename CharacterType> static bool isParameterValueEnd(CharacterType character)
{
    return character == ';' || character == ',';
}

template<typename CharacterType> static bool isParameterValueChar(CharacterType character)
{
    return !isSpaceOrTab(character) && !isParameterValueEnd(character);
}

// Verify that the parameter is a link-extension which according to spec doesn't have to have a value.
static bool isExtensionParameter(LinkHeader::LinkParameterName name)
{
    return name >= LinkHeader::LinkParameterUnknown;
}

// Before:
//
// <cat.jpg>; rel=preload
// ^                     ^
// position              end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload
//          ^            ^
//          position     end
template<typename CharacterType> static Optional<String> findURLBoundaries(StringParsingBuffer<CharacterType>& buffer)
{
    skipWhile<isSpaceOrTab>(buffer);
    if (!skipExactly(buffer, '<'))
        return WTF::nullopt;
    skipWhile<isSpaceOrTab>(buffer);

    auto urlStart = buffer.position();
    skipWhile<isNotURLTerminatingChar>(buffer);
    auto urlEnd = buffer.position();
    skipUntil(buffer, '>');
    if (!skipExactly(buffer, '>'))
        return WTF::nullopt;

    return String(urlStart, urlEnd - urlStart);
}

template<typename CharacterType> static bool invalidParameterDelimiter(StringParsingBuffer<CharacterType>& buffer)
{
    return !skipExactly(buffer, ';') && buffer.hasCharactersRemaining() && *buffer != ',';
}

template<typename CharacterType> static bool validFieldEnd(StringParsingBuffer<CharacterType>& buffer)
{
    return buffer.atEnd() || *buffer == ',';
}

// Before:
//
// <cat.jpg>; rel=preload
//          ^            ^
//          position     end
//
// After (if successful: otherwise the method returns false, and modifies the isValid boolean accordingly)
//
// <cat.jpg>; rel=preload
//            ^          ^
//            position  end
template<typename CharacterType> static bool parseParameterDelimiter(StringParsingBuffer<CharacterType>& buffer, bool& isValid)
{
    isValid = true;
    skipWhile<isSpaceOrTab>(buffer);
    if (invalidParameterDelimiter(buffer)) {
        isValid = false;
        return false;
    }
    skipWhile<isSpaceOrTab>(buffer);
    if (validFieldEnd(buffer))
        return false;
    return true;
}

static LinkHeader::LinkParameterName paramterNameFromString(StringView name)
{
    if (equalLettersIgnoringASCIICase(name, "rel"))
        return LinkHeader::LinkParameterRel;
    if (equalLettersIgnoringASCIICase(name, "anchor"))
        return LinkHeader::LinkParameterAnchor;
    if (equalLettersIgnoringASCIICase(name, "crossorigin"))
        return LinkHeader::LinkParameterCrossOrigin;
    if (equalLettersIgnoringASCIICase(name, "title"))
        return LinkHeader::LinkParameterTitle;
    if (equalLettersIgnoringASCIICase(name, "media"))
        return LinkHeader::LinkParameterMedia;
    if (equalLettersIgnoringASCIICase(name, "type"))
        return LinkHeader::LinkParameterType;
    if (equalLettersIgnoringASCIICase(name, "rev"))
        return LinkHeader::LinkParameterRev;
    if (equalLettersIgnoringASCIICase(name, "hreflang"))
        return LinkHeader::LinkParameterHreflang;
    if (equalLettersIgnoringASCIICase(name, "as"))
        return LinkHeader::LinkParameterAs;
    if (equalLettersIgnoringASCIICase(name, "imagesrcset"))
        return LinkHeader::LinkParameterImageSrcSet;
    if (equalLettersIgnoringASCIICase(name, "imagesizes"))
        return LinkHeader::LinkParameterImageSizes;
    return LinkHeader::LinkParameterUnknown;
}

// Before:
//
// <cat.jpg>; rel=preload
//            ^          ^
//            position   end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload
//                ^      ^
//            position  end
template<typename CharacterType> static Optional<LinkHeader::LinkParameterName> parseParameterName(StringParsingBuffer<CharacterType>& buffer)
{
    auto nameStart = buffer.position();
    skipWhile<isValidParameterNameChar>(buffer);
    auto nameEnd = buffer.position();
    skipWhile<isSpaceOrTab>(buffer);
    bool hasEqual = skipExactly(buffer, '=');
    skipWhile<isSpaceOrTab>(buffer);
    auto name = paramterNameFromString(StringView { nameStart, static_cast<unsigned>(nameEnd - nameStart) });
    if (hasEqual)
        return name;
    bool validParameterValueEnd = buffer.atEnd() || isParameterValueEnd(*buffer);
    if (validParameterValueEnd && isExtensionParameter(name))
        return name;
    return WTF::nullopt;
}

// Before:
//
// <cat.jpg>; rel="preload"; type="image/jpeg";
//                ^                            ^
//            position                        end
//
// After (if the parameter starts with a quote, otherwise the method returns false)
//
// <cat.jpg>; rel="preload"; type="image/jpeg";
//                         ^                   ^
//                     position               end
template<typename CharacterType> static bool skipQuotesIfNeeded(StringParsingBuffer<CharacterType>& buffer, bool& completeQuotes)
{
    unsigned char quote;
    if (skipExactly(buffer, '\''))
        quote = '\'';
    else if (skipExactly(buffer, '"'))
        quote = '"';
    else
        return false;

    while (!completeQuotes && buffer.hasCharactersRemaining()) {
        skipUntil(buffer, static_cast<CharacterType>(quote));
        if (*(buffer.position() - 1) != '\\')
            completeQuotes = true;
        completeQuotes = skipExactly(buffer, static_cast<CharacterType>(quote)) && completeQuotes;
    }
    return true;
}

// Before:
//
// <cat.jpg>; rel=preload; foo=bar
//                ^               ^
//            position            end
//
// After (if successful: otherwise the method returns false)
//
// <cat.jpg>; rel=preload; foo=bar
//                       ^        ^
//                   position     end
template<typename CharacterType> static bool parseParameterValue(StringParsingBuffer<CharacterType>& buffer, String& value)
{
    auto valueStart = buffer.position();
    auto valueEnd = buffer.position();
    bool completeQuotes = false;
    bool hasQuotes = skipQuotesIfNeeded(buffer, completeQuotes);
    if (!hasQuotes)
        skipWhile<isParameterValueChar>(buffer);
    valueEnd = buffer.position();
    skipWhile<isSpaceOrTab>(buffer);
    if ((!completeQuotes && valueStart == valueEnd) || (!buffer.atEnd() && !isParameterValueEnd(*buffer))) {
        value = emptyString();
        return false;
    }
    if (hasQuotes)
        ++valueStart;
    if (completeQuotes)
        --valueEnd;
    ASSERT(valueEnd >= valueStart);
    value = String(valueStart, valueEnd - valueStart);
    return !hasQuotes || completeQuotes;
}

void LinkHeader::setValue(LinkParameterName name, String&& value)
{
    switch (name) {
    case LinkParameterRel:
        if (!m_rel)
            m_rel = WTFMove(value);
        break;
    case LinkParameterAnchor:
        m_isValid = false;
        break;
    case LinkParameterCrossOrigin:
        m_crossOrigin = WTFMove(value);
        break;
    case LinkParameterAs:
        m_as = WTFMove(value);
        break;
    case LinkParameterType:
        m_mimeType = WTFMove(value);
        break;
    case LinkParameterMedia:
        m_media = WTFMove(value);
        break;
    case LinkParameterImageSrcSet:
        m_imageSrcSet = WTFMove(value);
        break;
    case LinkParameterImageSizes:
        m_imageSizes = WTFMove(value);
        break;
    case LinkParameterTitle:
    case LinkParameterRev:
    case LinkParameterHreflang:
    case LinkParameterUnknown:
        // These parameters are not yet supported, so they are currently ignored.
        break;
    }
    // FIXME: Add support for more header parameters as neccessary.
}

template<typename CharacterType> static void findNextHeader(StringParsingBuffer<CharacterType>& buffer)
{
    skipUntil(buffer, ',');
    skipExactly(buffer, ',');
}

template<typename CharacterType> LinkHeader::LinkHeader(StringParsingBuffer<CharacterType>& buffer)
{
    auto urlResult = findURLBoundaries(buffer);
    if (urlResult == WTF::nullopt) {
        m_isValid = false;
        findNextHeader(buffer);
        return;
    }
    m_url = urlResult.value();

    while (m_isValid && buffer.hasCharactersRemaining()) {
        if (!parseParameterDelimiter(buffer, m_isValid)) {
            findNextHeader(buffer);
            return;
        }

        auto parameterName = parseParameterName(buffer);
        if (!parameterName) {
            findNextHeader(buffer);
            m_isValid = false;
            return;
        }

        String parameterValue;
        if (!parseParameterValue(buffer, parameterValue) && !isExtensionParameter(*parameterName)) {
            findNextHeader(buffer);
            m_isValid = false;
            return;
        }

        setValue(*parameterName, WTFMove(parameterValue));
    }
    findNextHeader(buffer);
}

LinkHeaderSet::LinkHeaderSet(const String& header)
{
    readCharactersForParsing(header, [&](auto buffer) {
        while (buffer.hasCharactersRemaining())
            m_headerSet.append(LinkHeader { buffer });
    });
}

} // namespace WebCore

