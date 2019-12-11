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
#include <wtf/text/WTFString.h>

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

String TextCodecUTF16::decode(const char* bytes, size_t length, bool, bool, bool&)
{
    if (!length)
        return String();

    // FIXME: This should generate an error if there is an unpaired surrogate.

    const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes);
    size_t numBytes = length + m_haveBufferedByte;
    size_t numCodeUnits = numBytes / 2;
    RELEASE_ASSERT(numCodeUnits <= std::numeric_limits<unsigned>::max());

    UChar* q;
    auto result = String::createUninitialized(numCodeUnits, q);

    if (m_haveBufferedByte) {
        UChar c;
        if (m_littleEndian)
            c = m_bufferedByte | (p[0] << 8);
        else
            c = (m_bufferedByte << 8) | p[0];
        *q++ = c;
        m_haveBufferedByte = false;
        p += 1;
        numCodeUnits -= 1;
    }

    if (m_littleEndian) {
        for (size_t i = 0; i < numCodeUnits; ++i) {
            UChar c = p[0] | (p[1] << 8);
            p += 2;
            *q++ = c;
        }
    } else {
        for (size_t i = 0; i < numCodeUnits; ++i) {
            UChar c = (p[0] << 8) | p[1];
            p += 2;
            *q++ = c;
        }
    }

    if (numBytes & 1) {
        ASSERT(!m_haveBufferedByte);
        m_haveBufferedByte = true;
        m_bufferedByte = p[0];
    }

    return result;
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
