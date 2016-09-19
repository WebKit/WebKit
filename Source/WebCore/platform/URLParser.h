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
    WEBCORE_EXPORT URL parse(const String&, const URL& = { }, const TextEncoding& = UTF8Encoding());
    WEBCORE_EXPORT URL parseSerializedURL(const String&);
    WEBCORE_EXPORT static bool allValuesEqual(const URL&, const URL&);

    WEBCORE_EXPORT static bool enabled();
    WEBCORE_EXPORT static void setEnabled(bool);
    
    typedef Vector<std::pair<String, String>> URLEncodedForm;
    static URLEncodedForm parseURLEncodedForm(StringView);
    static String serialize(const URLEncodedForm&);

private:
    URL m_url;
    Vector<LChar> m_asciiBuffer;
    Vector<UChar32> m_unicodeFragmentBuffer;
    bool m_urlIsSpecial { false };
    bool m_hostHasPercentOrNonASCII { false };

    template<bool serialized, typename CharacterType> URL parse(const CharacterType*, const unsigned length, const URL&, const TextEncoding&);
    template<bool serialized, typename CharacterType> void parseAuthority(CodePointIterator<CharacterType>);
    template<bool serialized, typename CharacterType> bool parseHost(CodePointIterator<CharacterType>);
    template<bool serialized, typename CharacterType> bool parsePort(CodePointIterator<CharacterType>&);
    template<typename CharacterType> URL failure(const CharacterType*, unsigned length);

    enum class URLPart;
    void copyURLPartsUntil(const URL& base, URLPart);
    static size_t urlLengthUntilPart(const URL&, URLPart);
    void popPath();
};

}
