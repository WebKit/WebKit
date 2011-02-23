/*
 * Copyright (C) 2004, 2006, 2008, 2010, 2011 Apple Inc. All rights reserved.
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
#include "TextCodecUTF16.h"

#include "PlatformString.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/unicode/CharacterNames.h>

using namespace WTF::Unicode;
using namespace std;

namespace WebCore {

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

static PassOwnPtr<TextCodec> newStreamingTextDecoderUTF16LE(const TextEncoding&, const void*)
{
    return adoptPtr(new TextCodecUTF16(true));
}

static PassOwnPtr<TextCodec> newStreamingTextDecoderUTF16BE(const TextEncoding&, const void*)
{
    return adoptPtr(new TextCodecUTF16(false));
}

void TextCodecUTF16::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-16LE", newStreamingTextDecoderUTF16LE, 0);
    registrar("UTF-16BE", newStreamingTextDecoderUTF16BE, 0);
}

String TextCodecUTF16::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    // FIXME: This should buffer surrogates, not just single bytes,
    // and should generate an error if there is an unpaired surrogate.

    const uint8_t* source = reinterpret_cast<const uint8_t*>(bytes);
    size_t numBytes = length + m_haveBufferedByte;
    size_t numCharacters = numBytes / 2;

    StringBuffer buffer(numCharacters + (flush && (numBytes & 1)));
    UChar* destination = buffer.characters();

    if (length) {
        if (m_haveBufferedByte) {
            UChar character;
            if (m_littleEndian)
                *destination++ = m_bufferedByte | (source[0] << 8);
            else
                *destination++ = (m_bufferedByte << 8) | source[0];
            m_haveBufferedByte = false;
            ++source;
            --numCharacters;
        }

        if (m_littleEndian) {
            for (size_t i = 0; i < numCharacters; ++i) {
                *destination++ = source[0] | (source[1] << 8);
                source += 2;
            }
        } else {
            for (size_t i = 0; i < numCharacters; ++i) {
                *destination++ = (source[0] << 8) | source[1];
                source += 2;
            }
        }
    }

    if (numBytes & 1) {
        if (flush) {
            sawError = true;
            if (!stopOnError)
                *destination++ = replacementCharacter;
        } else {
            ASSERT(!m_haveBufferedByte);
            m_haveBufferedByte = true;
            m_bufferedByte = source[0];
        }
    }
    
    buffer.shrink(destination - buffer.characters());

    return String::adopt(buffer);
}

CString TextCodecUTF16::encode(const UChar* characters, size_t length, UnencodableHandling)
{
    // We need to be sure we can double the length without overflowing.
    // Since the passed-in length is the length of an actual existing
    // character buffer, each character is two bytes, and we know
    // the buffer doesn't occupy the entire address space, we can
    // assert here that doubling the length does not overflow size_t
    // and there's no need for a runtime check.
    ASSERT(length <= numeric_limits<size_t>::max() / 2);

    char* bytes;
    CString string = CString::newUninitialized(length * 2, bytes);

    // FIXME: CString is not a reasonable data structure for encoded UTF-16, which will have
    // null characters inside it. Perhaps the result of encode should not be a CString.
    if (m_littleEndian) {
        for (size_t i = 0; i < length; ++i) {
            UChar character = characters[i];
            bytes[i * 2] = character;
            bytes[i * 2 + 1] = character >> 8;
        }
    } else {
        for (size_t i = 0; i < length; ++i) {
            UChar character = characters[i];
            bytes[i * 2] = character >> 8;
            bytes[i * 2 + 1] = character;
        }
    }

    return string;
}

} // namespace WebCore
