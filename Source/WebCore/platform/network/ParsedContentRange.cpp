/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ParsedContentRange.h"

#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static bool areContentRangeValuesValid(int64_t firstBytePosition, int64_t lastBytePosition, int64_t instanceLength)
{
    // From <http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html>
    // 14.16 Content-Range
    // A byte-content-range-spec with a byte-range-resp-spec whose last- byte-pos value is less than its first-byte-pos value,
    // or whose instance-length value is less than or equal to its last-byte-pos value, is invalid.
    if (firstBytePosition < 0)
        return false;
    ASSERT(firstBytePosition >= 0);

    if (lastBytePosition < firstBytePosition)
        return false;
    ASSERT(lastBytePosition >= 0);

    if (instanceLength == ParsedContentRange::unknownLength)
        return true;

    return lastBytePosition < instanceLength;
}

static bool parseContentRange(StringView headerValue, int64_t& firstBytePosition, int64_t& lastBytePosition, int64_t& instanceLength)
{
    // From <http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html>
    // 14.16 Content-Range
    //
    // Content-Range = "Content-Range" ":" content-range-spec
    // content-range-spec      = byte-content-range-spec
    // byte-content-range-spec = bytes-unit SP
    //                          byte-range-resp-spec "/"
    //                          ( instance-length | "*" )
    // byte-range-resp-spec = (first-byte-pos "-" last-byte-pos)
    //                               | "*"
    // instance-length           = 1*DIGIT

    static constexpr auto prefix = "bytes "_s;
    static constexpr size_t prefixLength = 6;

    if (!headerValue.startsWith(prefix))
        return false;

    size_t byteSeparatorTokenLoc = headerValue.find('-', prefixLength);
    if (byteSeparatorTokenLoc == notFound)
        return false;

    size_t instanceLengthSeparatorToken = headerValue.find('/', byteSeparatorTokenLoc + 1);
    if (instanceLengthSeparatorToken == notFound)
        return false;

    auto firstByteString = headerValue.substring(prefixLength, byteSeparatorTokenLoc - prefixLength);
    if (!firstByteString.isAllSpecialCharacters<isASCIIDigit>())
        return false;

    auto optionalFirstBytePosition = parseInteger<int64_t>(firstByteString);
    if (!optionalFirstBytePosition)
        return false;
    firstBytePosition = *optionalFirstBytePosition;

    auto lastByteString = headerValue.substring(byteSeparatorTokenLoc + 1, instanceLengthSeparatorToken - (byteSeparatorTokenLoc + 1));
    if (!lastByteString.isAllSpecialCharacters<isASCIIDigit>())
        return false;

    auto optionalLastBytePosition = parseInteger<int64_t>(lastByteString);
    if (!optionalLastBytePosition)
        return false;
    lastBytePosition = *optionalLastBytePosition;

    auto instanceString = headerValue.substring(instanceLengthSeparatorToken + 1);
    if (instanceString == "*"_s)
        instanceLength = ParsedContentRange::unknownLength;
    else {
        if (!instanceString.isAllSpecialCharacters<isASCIIDigit>())
            return false;

        auto optionalInstanceLength = parseInteger<int64_t>(instanceString);
        if (!optionalInstanceLength)
            return false;
        instanceLength = *optionalInstanceLength;
    }

    return areContentRangeValuesValid(firstBytePosition, lastBytePosition, instanceLength);
}

ParsedContentRange::ParsedContentRange(const String& headerValue)
{
    if (!parseContentRange(StringView(headerValue), m_firstBytePosition, m_lastBytePosition, m_instanceLength))
        m_instanceLength = invalidLength;
}

ParsedContentRange::ParsedContentRange(int64_t firstBytePosition, int64_t lastBytePosition, int64_t instanceLength)
    : m_firstBytePosition(firstBytePosition)
    , m_lastBytePosition(lastBytePosition)
    , m_instanceLength(instanceLength)
{
    if (!areContentRangeValuesValid(m_firstBytePosition, m_lastBytePosition, m_instanceLength))
        m_instanceLength = invalidLength;
}

String ParsedContentRange::headerValue() const
{
    if (!isValid())
        return String();
    if (m_instanceLength == unknownLength)
        return makeString("bytes ", m_firstBytePosition, '-', m_lastBytePosition, "/*");
    return makeString("bytes ", m_firstBytePosition, '-', m_lastBytePosition, '/', m_instanceLength);
}

}
