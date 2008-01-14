/*
 * Copyright (C) 2007, 2008 Apple, Inc. All rights reserved.
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
#include "TextCodecUserDefined.h"

#include "CString.h"
#include "PlatformString.h"
#include "StringBuffer.h"

using std::auto_ptr;

namespace WebCore {

void TextCodecUserDefined::registerEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("x-user-defined", "x-user-defined");
}

static auto_ptr<TextCodec> newStreamingTextDecoderUserDefined(const TextEncoding&, const void*)
{
    return auto_ptr<TextCodec>(new TextCodecUserDefined);
}

void TextCodecUserDefined::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("x-user-defined", newStreamingTextDecoderUserDefined, 0);
}

String TextCodecUserDefined::decode(const char* bytes, size_t length, bool)
{
    StringBuffer buffer(length);

    for (size_t i = 0; i < length; ++i) {
        signed char c = bytes[i];
        buffer[i] = c & 0xF7FF;
    }

    return String::adopt(buffer);
}

static CString encodeComplexUserDefined(const UChar* characters, size_t length, bool allowEntities)
{
    Vector<char> result(length);
    char* bytes = result.data();

    size_t resultLength = 0;
    for (size_t i = 0; i < length; ) {
        UChar32 c;
        U16_NEXT(characters, i, length, c);
        signed char signedByte = c;
        if ((signedByte & 0xf7ff) == c)
            bytes[resultLength++] = signedByte;
        else {
            // No way to encode this character with x-user-defined.
            if (allowEntities) {
                char entityBuffer[16];
                sprintf(entityBuffer, "&#%u;", c);
                size_t entityLength = strlen(entityBuffer);
                result.grow(resultLength + entityLength + length - i);
                bytes = result.data();
                memcpy(bytes + resultLength, entityBuffer, entityLength);
                resultLength += entityLength;
            } else
                bytes[resultLength++] = '?';
        }
    }

    return CString(bytes, resultLength);
}

CString TextCodecUserDefined::encode(const UChar* characters, size_t length, bool allowEntities)
{
    char* bytes;
    CString string = CString::newUninitialized(length, bytes);

    // Convert the string a fast way and simultaneously do an efficient check to see if it's all ASCII.
    UChar ored = 0;
    for (size_t i = 0; i < length; ++i) {
        UChar c = characters[i];
        bytes[i] = c;
        ored |= c;
    }

    if (!(ored & 0xFF80))
        return string;

    // If it wasn't all ASCII, call the function that handles more-complex cases.
    return encodeComplexUserDefined(characters, length, allowEntities);
}

} // namespace WebCore
