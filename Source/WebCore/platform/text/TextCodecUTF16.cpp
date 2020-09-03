/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "TextCodecUTF16.h"

#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

inline TextCodecUTF16::TextCodecUTF16(bool littleEndian)
    : m_littleEndian(littleEndian)
{
}

void TextCodecUTF16::registerEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("UTF-16LE", "UTF-16LE");
    registrar("UTF-16BE", "UTF-16BE");

    registrar("ISO-10646-UCS-2", "UTF-16LE");
    registrar("UCS-2", "UTF-16LE");
    registrar("UTF-16", "UTF-16LE");
    registrar("Unicode", "UTF-16LE");
    registrar("csUnicode", "UTF-16LE");
    registrar("unicodeFEFF", "UTF-16LE");

    registrar("unicodeFFFE", "UTF-16BE");
}

void TextCodecUTF16::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-16LE", [] {
        return makeUnique<TextCodecUTF16>(true);
    });
    registrar("UTF-16BE", [] {
        return makeUnique<TextCodecUTF16>(false);
    });
}

// https://encoding.spec.whatwg.org/#shared-utf-16-decoder
String TextCodecUTF16::decode(const char* bytes, size_t length, bool flush, bool, bool& sawError)
{
    const auto* p = reinterpret_cast<const uint8_t*>(bytes);
    const auto* const end = p + length;
    const auto* const endMinusOneOrNull = end ? end - 1 : nullptr;

    StringBuilder result;
    result.reserveCapacity(length / 2);

    Function<void(UChar)> processBytesShared;
    processBytesShared = [&] (UChar codeUnit) {
        if (m_leadSurrogate) {
            auto leadSurrogate = *std::exchange(m_leadSurrogate, WTF::nullopt);
            if (U16_IS_TRAIL(codeUnit)) {
                result.appendCharacter(U16_GET_SUPPLEMENTARY(leadSurrogate, codeUnit));
                return;
            }
            sawError = true;
            result.append(replacementCharacter);
            processBytesShared(codeUnit);
            return;
        }
        if (U16_IS_LEAD(codeUnit)) {
            m_leadSurrogate = codeUnit;
            return;
        }
        if (U16_IS_TRAIL(codeUnit)) {
            sawError = true;
            result.append(replacementCharacter);
            return;
        }
        result.append(codeUnit);
    };
    auto processBytesLE = [&] (uint8_t first, uint8_t second) {
        processBytesShared(first | (second << 8));
    };
    auto processBytesBE = [&] (uint8_t first, uint8_t second) {
        processBytesShared((first << 8) | second);
    };

    if (m_leadByte && p < end) {
        auto leadByte = *std::exchange(m_leadByte, WTF::nullopt);
        if (m_littleEndian)
            processBytesLE(leadByte, p[0]);
        else
            processBytesBE(leadByte, p[0]);
        p++;
    }

    if (m_littleEndian) {
        while (p < endMinusOneOrNull) {
            processBytesLE(p[0], p[1]);
            p += 2;
        }
    } else {
        while (p < endMinusOneOrNull) {
            processBytesBE(p[0], p[1]);
            p += 2;
        }
    }

    if (p && p == endMinusOneOrNull) {
        ASSERT(!m_leadByte);
        m_leadByte = p[0];
    } else
        ASSERT(!p || p == end);
    
    if (flush && (m_leadByte || m_leadSurrogate)) {
        m_leadByte = WTF::nullopt;
        m_leadSurrogate = WTF::nullopt;
        sawError = true;
        result.append(replacementCharacter);
    }

    return result.toString();
}

Vector<uint8_t> TextCodecUTF16::encode(StringView string, UnencodableHandling)
{
    Vector<uint8_t> result(WTF::checkedProduct<size_t>(string.length(), 2).unsafeGet());
    auto* bytes = result.data();

    if (m_littleEndian) {
        for (auto character : string.codeUnits()) {
            *bytes++ = character;
            *bytes++ = character >> 8;
        }
    } else {
        for (auto character : string.codeUnits()) {
            *bytes++ = character >> 8;
            *bytes++ = character;
        }
    }

    return result;
}

} // namespace WebCore
