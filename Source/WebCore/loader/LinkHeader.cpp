/*
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Copyright (C) 2016 Akamai Technologies Inc. All rights reserved.
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
template <typename CharacterType>
static bool isSpaceOrTab(CharacterType chr)
{
    return (chr == ' ') || (chr == '\t');
}

template <typename CharacterType>
static bool isNotURLTerminatingChar(CharacterType chr)
{
    return (chr != '>');
}

template <typename CharacterType>
static bool isValidParameterNameChar(CharacterType chr)
{
    // A separator, CTL or '%', '*' or '\'' means the char is not valid.
    // Definition as attr-char at https://tools.ietf.org/html/rfc5987
    // CTL and separators are defined in https://tools.ietf.org/html/rfc2616#section-2.2
    // Valid chars are alpha-numeric and any of !#$&+-.^_`|~"
    if ((chr >= '^' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') || (chr >= '!' && chr <= '$') || chr == '&' || chr == '+' || chr == '-' || chr == '.')
        return true;
    return false;
}

template <typename CharacterType>
static bool isParameterValueEnd(CharacterType chr)
{
    return chr == ';' || chr == ',';
}

template <typename CharacterType>
static bool isParameterValueChar(CharacterType chr)
{
    return !isSpaceOrTab(chr) && !isParameterValueEnd(chr);
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
template <typename CharacterType>
static Optional<String> findURLBoundaries(CharacterType*& position, CharacterType* const end)
{
    ASSERT(position <= end);
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    if (!skipExactly<CharacterType>(position, end, '<'))
        return WTF::nullopt;
    skipWhile<CharacterType, isSpaceOrTab>(position, end);

    CharacterType* urlStart = position;
    skipWhile<CharacterType, isNotURLTerminatingChar>(position, end);
    CharacterType* urlEnd = position;
    skipUntil<CharacterType>(position, end, '>');
    if (!skipExactly<CharacterType>(position, end, '>'))
        return WTF::nullopt;

    return String(urlStart, urlEnd - urlStart);
}

template <typename CharacterType>
static bool invalidParameterDelimiter(CharacterType*& position, CharacterType* const end)
{
    ASSERT(position <= end);
    return (!skipExactly<CharacterType>(position, end, ';') && (position < end) && (*position != ','));
}

template <typename CharacterType>
static bool validFieldEnd(CharacterType*& position, CharacterType* const end)
{
    ASSERT(position <= end);
    return (position == end || *position == ',');
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
template <typename CharacterType>
static bool parseParameterDelimiter(CharacterType*& position, CharacterType* const end, bool& isValid)
{
    ASSERT(position <= end);
    isValid = true;
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    if (invalidParameterDelimiter(position, end)) {
        isValid = false;
        return false;
    }
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    if (validFieldEnd(position, end))
        return false;
    return true;
}

static LinkHeader::LinkParameterName paramterNameFromString(String name)
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
template <typename CharacterType>
static bool parseParameterName(CharacterType*& position, CharacterType* const end, LinkHeader::LinkParameterName& name)
{
    ASSERT(position <= end);
    CharacterType* nameStart = position;
    skipWhile<CharacterType, isValidParameterNameChar>(position, end);
    CharacterType* nameEnd = position;
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    bool hasEqual = skipExactly<CharacterType>(position, end, '=');
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    name = paramterNameFromString(String(nameStart, nameEnd - nameStart));
    if (hasEqual)
        return true;
    bool validParameterValueEnd = (position == end) || isParameterValueEnd(*position);
    return validParameterValueEnd && isExtensionParameter(name);
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
template <typename CharacterType>
static bool skipQuotesIfNeeded(CharacterType*& position, CharacterType* const end, bool& completeQuotes)
{
    ASSERT(position <= end);
    unsigned char quote;
    if (skipExactly<CharacterType>(position, end, '\''))
        quote = '\'';
    else if (skipExactly<CharacterType>(position, end, '"'))
        quote = '"';
    else
        return false;

    while (!completeQuotes && position < end) {
        skipUntil(position, end, static_cast<CharacterType>(quote));
        if (*(position - 1) != '\\')
            completeQuotes = true;
        completeQuotes = skipExactly(position, end, static_cast<CharacterType>(quote)) && completeQuotes;
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
template <typename CharacterType>
static bool parseParameterValue(CharacterType*& position, CharacterType* const end, String& value)
{
    ASSERT(position <= end);
    CharacterType* valueStart = position;
    CharacterType* valueEnd = position;
    bool completeQuotes = false;
    bool hasQuotes = skipQuotesIfNeeded(position, end, completeQuotes);
    if (!hasQuotes)
        skipWhile<CharacterType, isParameterValueChar>(position, end);
    valueEnd = position;
    skipWhile<CharacterType, isSpaceOrTab>(position, end);
    if ((!completeQuotes && valueStart == valueEnd) || (position != end && !isParameterValueEnd(*position))) {
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

void LinkHeader::setValue(LinkParameterName name, String value)
{
    switch (name) {
    case LinkParameterRel:
        if (!m_rel)
            m_rel = value;
        break;
    case LinkParameterAnchor:
        m_isValid = false;
        break;
    case LinkParameterCrossOrigin:
        m_crossOrigin = value;
        break;
    case LinkParameterAs:
        m_as = value;
        break;
    case LinkParameterType:
        m_mimeType = value;
        break;
    case LinkParameterMedia:
        m_media = value;
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

template <typename CharacterType>
static void findNextHeader(CharacterType*& position, CharacterType* const end)
{
    ASSERT(position <= end);
    skipUntil<CharacterType>(position, end, ',');
    skipExactly<CharacterType>(position, end, ',');
}

template <typename CharacterType>
LinkHeader::LinkHeader(CharacterType*& position, CharacterType* const end)
{
    ASSERT(position <= end);
    auto urlResult = findURLBoundaries(position, end);
    if (urlResult == WTF::nullopt) {
        m_isValid = false;
        findNextHeader(position, end);
        return;
    }
    m_url = urlResult.value();

    while (m_isValid && position < end) {
        if (!parseParameterDelimiter(position, end, m_isValid)) {
            findNextHeader(position, end);
            return;
        }

        LinkParameterName parameterName;
        if (!parseParameterName(position, end, parameterName)) {
            findNextHeader(position, end);
            m_isValid = false;
            return;
        }

        String parameterValue;
        if (!parseParameterValue(position, end, parameterValue) && !isExtensionParameter(parameterName)) {
            findNextHeader(position, end);
            m_isValid = false;
            return;
        }

        setValue(parameterName, parameterValue);
    }
    findNextHeader(position, end);
}

LinkHeaderSet::LinkHeaderSet(const String& header)
{
    if (header.isNull())
        return;

    if (header.is8Bit())
        init(header.characters8(), header.length());
    else
        init(header.characters16(), header.length());
}

template <typename CharacterType>
void LinkHeaderSet::init(CharacterType* headerValue, size_t length)
{
    CharacterType* position = headerValue;
    CharacterType* const end = headerValue + length;
    while (position < end)
        m_headerSet.append(LinkHeader(position, end));
}

} // namespace WebCore

