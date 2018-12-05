/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "KeyedDecoderGeneric.h"

namespace WebCore {

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=186410
std::unique_ptr<KeyedDecoder> KeyedDecoder::decoder(const uint8_t* data, size_t size)
{
    return std::make_unique<KeyedDecoderGeneric>(data, size);
}

KeyedDecoderGeneric::KeyedDecoderGeneric(const uint8_t*, size_t)
{
}

KeyedDecoderGeneric::~KeyedDecoderGeneric()
{
}

bool KeyedDecoderGeneric::decodeBytes(const String&, const uint8_t*&, size_t&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeBool(const String&, bool&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeUInt32(const String&, uint32_t&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeUInt64(const String&, uint64_t&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeInt32(const String&, int32_t&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeInt64(const String&, int64_t&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeFloat(const String&, float&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeDouble(const String&, double&)
{
    return false;
}

bool KeyedDecoderGeneric::decodeString(const String&, String&)
{
    return false;
}

bool KeyedDecoderGeneric::beginObject(const String&)
{
    return false;
}

void KeyedDecoderGeneric::endObject()
{
}

bool KeyedDecoderGeneric::beginArray(const String&)
{
    return false;
}

bool KeyedDecoderGeneric::beginArrayElement()
{
    return false;
}

void KeyedDecoderGeneric::endArrayElement()
{
}

void KeyedDecoderGeneric::endArray()
{
}

} // namespace WebCore
