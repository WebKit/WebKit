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

#pragma once

#include "KeyedCoding.h"

namespace WebCore {

class KeyedDecoderGeneric final : public KeyedDecoder {
public:
    KeyedDecoderGeneric(const uint8_t* data, size_t);

    class Dictionary;
    using Array = Vector<std::unique_ptr<Dictionary>>;

private:
    bool decodeBytes(const String& key, const uint8_t*&, size_t&) override;
    bool decodeBool(const String& key, bool&) override;
    bool decodeUInt32(const String& key, uint32_t&) override;
    bool decodeUInt64(const String& key, uint64_t&) override;
    bool decodeInt32(const String& key, int32_t&) override;
    bool decodeInt64(const String& key, int64_t&) override;
    bool decodeFloat(const String& key, float&) override;
    bool decodeDouble(const String& key, double&) override;
    bool decodeString(const String& key, String&) override;

    bool beginObject(const String& key) override;
    void endObject() override;

    bool beginArray(const String& key) override;
    bool beginArrayElement() override;
    void endArrayElement() override;
    void endArray() override;

    std::unique_ptr<Dictionary> m_rootDictionary;
    Vector<Dictionary*, 16> m_dictionaryStack;
    Vector<Array*, 16> m_arrayStack;
    Vector<size_t, 16> m_arrayIndexStack;
};

} // namespace WebCore
