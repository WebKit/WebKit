/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ArgumentCoders.h"

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {

void ArgumentCoder<AtomicString>::encode(ArgumentEncoder* encoder, const AtomicString& atomicString)
{
    encoder->encode(atomicString.string());
}

bool ArgumentCoder<AtomicString>::decode(ArgumentDecoder* decoder, AtomicString& atomicString)
{
    String string;
    if (!decoder->decode(string))
        return false;

    atomicString = string;
    return true;
}

void ArgumentCoder<CString>::encode(ArgumentEncoder* encoder, const CString& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder->encodeUInt32(std::numeric_limits<uint32_t>::max());
        return;
    }

    uint32_t length = string.length();
    encoder->encode(length);
    encoder->encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.data()), length, 1);
}

bool ArgumentCoder<CString>::decode(ArgumentDecoder* decoder, CString& result)
{
    uint32_t length;
    if (!decoder->decode(length))
        return false;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        result = CString();
        return true;
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder->bufferIsLargeEnoughToContain<char>(length)) {
        decoder->markInvalid();
        return false;
    }

    char* buffer;
    CString string = CString::newUninitialized(length, buffer);
    if (!decoder->decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length, 1))
        return false;

    result = string;
    return true;
}


void ArgumentCoder<String>::encode(ArgumentEncoder* encoder, const String& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder->encodeUInt32(std::numeric_limits<uint32_t>::max());
        return;
    }

    uint32_t length = string.length();
    encoder->encode(length);
    encoder->encodeFixedLengthData(reinterpret_cast<const uint8_t*>(string.characters()), length * sizeof(UChar), __alignof(UChar)); 
}

bool ArgumentCoder<String>::decode(ArgumentDecoder* decoder, String& result)
{
    uint32_t length;
    if (!decoder->decode(length))
        return false;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        result = String();
        return true;
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder->bufferIsLargeEnoughToContain<UChar>(length)) {
        decoder->markInvalid();
        return false;
    }
    
    UChar* buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder->decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(UChar), __alignof(UChar)))
        return false;
    
    result = string;
    return true;
}

} // namespace CoreIPC
