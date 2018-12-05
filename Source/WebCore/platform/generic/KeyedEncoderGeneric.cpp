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
#include "KeyedEncoderGeneric.h"

#include "SharedBuffer.h"

namespace WebCore {

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=186410
std::unique_ptr<KeyedEncoder> KeyedEncoder::encoder()
{
    return std::make_unique<KeyedEncoderGeneric>();
}

KeyedEncoderGeneric::KeyedEncoderGeneric()
{
}

KeyedEncoderGeneric::~KeyedEncoderGeneric()
{
}

void KeyedEncoderGeneric::encodeBytes(const String&, const uint8_t*, size_t)
{
}

void KeyedEncoderGeneric::encodeBool(const String&, bool)
{
}

void KeyedEncoderGeneric::encodeUInt32(const String&, uint32_t)
{
}

void KeyedEncoderGeneric::encodeUInt64(const String&, uint64_t)
{
}

void KeyedEncoderGeneric::encodeInt32(const String&, int32_t)
{
}

void KeyedEncoderGeneric::encodeInt64(const String&, int64_t)
{
}

void KeyedEncoderGeneric::encodeFloat(const String&, float)
{
}

void KeyedEncoderGeneric::encodeDouble(const String&, double)
{
}

void KeyedEncoderGeneric::encodeString(const String&, const String&)
{
}

void KeyedEncoderGeneric::beginObject(const String&)
{
}

void KeyedEncoderGeneric::endObject()
{
}

void KeyedEncoderGeneric::beginArray(const String&)
{
}

void KeyedEncoderGeneric::beginArrayElement()
{
}

void KeyedEncoderGeneric::endArrayElement()
{
}

void KeyedEncoderGeneric::endArray()
{
}

RefPtr<SharedBuffer> KeyedEncoderGeneric::finishEncoding()
{
    return nullptr;
}

} // namespace WebCore
