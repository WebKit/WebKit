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

#include "TextEncoding.h"
#include "URL.h"
#include <wtf/Forward.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

template<typename CharacterType> class CodePointIterator;

class URLParser {
public:
    WEBCORE_EXPORT URLParser(const String&, const URL& = { }, const TextEncoding& = UTF8Encoding());
    URL result() { return m_url; }

    WEBCORE_EXPORT static bool allValuesEqual(const URL&, const URL&);
    WEBCORE_EXPORT static bool internalValuesConsistent(const URL&);

    WEBCORE_EXPORT static bool enabled();
    WEBCORE_EXPORT static void setEnabled(bool);
    
    typedef Vector<std::pair<String, String>> URLEncodedForm;
    static URLEncodedForm parseURLEncodedForm(StringView);
    static String serialize(const URLEncodedForm&);

private:
    URL m_url;
    Vector<LChar> m_asciiBuffer;
    Vector<UChar> m_unicodeFragmentBuffer;
    bool m_urlIsSpecial { false };
    bool m_hostHasPercentOrNonASCII { false };
    String m_inputString;

    template<typename CharacterType> void parse(const CharacterType*, const unsigned length, const URL&, const TextEncoding&);
    template<typename CharacterType> void parseAuthority(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parseHostAndPort(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parsePort(CodePointIterator<CharacterType>&);

    void failure();
    template<typename CharacterType> void incrementIteratorSkippingTabAndNewLine(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void syntaxError(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> bool isWindowsDriveLetter(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool shouldCopyFileURL(CodePointIterator<CharacterType>);
    template<typename CharacterType> void checkWindowsDriveLetter(CodePointIterator<CharacterType>&);
    template<bool(*isInCodeSet)(UChar32)> void utf8PercentEncode(UChar32);
    void utf8QueryEncode(UChar32);
    void percentEncodeByte(uint8_t);
    void appendToASCIIBuffer(UChar32);
    void appendToASCIIBuffer(const char*, size_t);
    void appendToASCIIBuffer(const LChar* characters, size_t size) { appendToASCIIBuffer(reinterpret_cast<const char*>(characters), size); }
    void encodeQuery(const Vector<UChar>& source, const TextEncoding&);
    void copyASCIIStringUntil(const String&, size_t lengthIf8Bit, size_t lengthIf16Bit);

    enum class URLPart;
    void copyURLPartsUntil(const URL& base, URLPart);
    static size_t urlLengthUntilPart(const URL&, URLPart);
    void popPath();
};

}
