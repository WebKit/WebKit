/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "URL.h"
#include <wtf/Expected.h>
#include <wtf/Forward.h>

struct UIDNA;

namespace WebCore {

template<typename CharacterType> class CodePointIterator;

class URLParser {
public:
    WEBCORE_EXPORT URLParser(const String&, const URL& = { }, const URLTextEncoding* = nullptr);
    URL result() { return m_url; }

    WEBCORE_EXPORT static bool allValuesEqual(const URL&, const URL&);
    WEBCORE_EXPORT static bool internalValuesConsistent(const URL&);
    
    typedef Vector<WTF::KeyValuePair<String, String>> URLEncodedForm;
    WEBCORE_EXPORT static URLEncodedForm parseURLEncodedForm(StringView);
    static String serialize(const URLEncodedForm&);

    static const UIDNA& internationalDomainNameTranscoder();
    static bool isInUserInfoEncodeSet(UChar);

    WEBCORE_EXPORT static bool isSpecialScheme(const String& scheme);
    WEBCORE_EXPORT static std::optional<String> maybeCanonicalizeScheme(const String& scheme);

private:
    static std::optional<uint16_t> defaultPortForProtocol(StringView);
    friend std::optional<uint16_t> defaultPortForProtocol(StringView);

    URL m_url;
    Vector<LChar> m_asciiBuffer;
    bool m_urlIsSpecial { false };
    bool m_urlIsFile { false };
    bool m_hostHasPercentOrNonASCII { false };
    String m_inputString;
    const void* m_inputBegin { nullptr };

    bool m_didSeeSyntaxViolation { false };
    static constexpr size_t defaultInlineBufferSize = 2048;
    using LCharBuffer = Vector<LChar, defaultInlineBufferSize>;

    template<typename CharacterType> void parse(const CharacterType*, const unsigned length, const URL&, const URLTextEncoding*);
    template<typename CharacterType> void parseAuthority(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parseHostAndPort(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parsePort(CodePointIterator<CharacterType>&);

    void failure();
    enum class ReportSyntaxViolation { No, Yes };
    template<typename CharacterType, ReportSyntaxViolation reportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>& iterator) { advance<CharacterType, reportSyntaxViolation>(iterator, iterator); }
    template<typename CharacterType, ReportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>&, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    template<typename CharacterType> bool takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>);
    template<typename CharacterType> void syntaxViolation(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> bool isPercentEncodedDot(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isWindowsDriveLetter(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isSingleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isDoubleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool shouldCopyFileURL(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool checkLocalhostCodePoint(CodePointIterator<CharacterType>&, UChar32);
    template<typename CharacterType> bool isAtLocalhost(CodePointIterator<CharacterType>);
    bool isLocalhost(StringView);
    template<typename CharacterType> void consumeSingleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void consumeDoubleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void appendWindowsDriveLetter(CodePointIterator<CharacterType>&);
    template<typename CharacterType> size_t currentPosition(const CodePointIterator<CharacterType>&);
    template<typename UnsignedIntegerType> void appendNumberToASCIIBuffer(UnsignedIntegerType);
    template<bool(*isInCodeSet)(UChar32), typename CharacterType> void utf8PercentEncode(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> void utf8QueryEncode(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> std::optional<LCharBuffer> domainToASCII(StringImpl&, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    template<typename CharacterType> LCharBuffer percentDecode(const LChar*, size_t, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    static LCharBuffer percentDecode(const LChar*, size_t);
    static std::optional<String> formURLDecode(StringView input);
    static bool hasForbiddenHostCodePoint(const LCharBuffer&);
    void percentEncodeByte(uint8_t);
    void appendToASCIIBuffer(UChar32);
    void appendToASCIIBuffer(const char*, size_t);
    void appendToASCIIBuffer(const LChar* characters, size_t size) { appendToASCIIBuffer(reinterpret_cast<const char*>(characters), size); }
    template<typename CharacterType> void encodeNonUTF8Query(const Vector<UChar>& source, const URLTextEncoding&, CodePointIterator<CharacterType>);
    void copyASCIIStringUntil(const String&, size_t length);
    bool copyBaseWindowsDriveLetter(const URL&);
    StringView parsedDataView(size_t start, size_t length);
    UChar parsedDataView(size_t position);

    using IPv4Address = uint32_t;
    void serializeIPv4(IPv4Address);
    enum class IPv4ParsingError;
    enum class IPv4PieceParsingError;
    template<typename CharacterTypeForSyntaxViolation, typename CharacterType> Expected<IPv4Address, IPv4ParsingError> parseIPv4Host(const CodePointIterator<CharacterTypeForSyntaxViolation>&, CodePointIterator<CharacterType>);
    template<typename CharacterType> Expected<uint32_t, URLParser::IPv4PieceParsingError> parseIPv4Piece(CodePointIterator<CharacterType>&, bool& syntaxViolation);
    using IPv6Address = std::array<uint16_t, 8>;
    template<typename CharacterType> std::optional<IPv6Address> parseIPv6Host(CodePointIterator<CharacterType>);
    template<typename CharacterType> std::optional<uint32_t> parseIPv4PieceInsideIPv6(CodePointIterator<CharacterType>&);
    template<typename CharacterType> std::optional<IPv4Address> parseIPv4AddressInsideIPv6(CodePointIterator<CharacterType>);
    void serializeIPv6Piece(uint16_t piece);
    void serializeIPv6(IPv6Address);

    enum class URLPart;
    template<typename CharacterType> void copyURLPartsUntil(const URL& base, URLPart, const CodePointIterator<CharacterType>&, const URLTextEncoding*&);
    static size_t urlLengthUntilPart(const URL&, URLPart);
    void popPath();
    bool shouldPopPath(unsigned);
};

}
