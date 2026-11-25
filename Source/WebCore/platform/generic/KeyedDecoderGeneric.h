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
    KeyedDecoderGeneric(std::span<const uint8_t> data);

    class Dictionary;
    using Array = Vector<std::unique_ptr<Dictionary>>;

private:
    [[nodiscard]] bool decodeBytes(StringImpl* key, std::span<const uint8_t>&) override;
    [[nodiscard]] bool decodeBool(StringImpl* key, bool&) override;
    [[nodiscard]] bool decodeUInt32(StringImpl* key, uint32_t&) override;
    [[nodiscard]] bool decodeUInt64(StringImpl* key, uint64_t&) override;
    [[nodiscard]] bool decodeInt32(StringImpl* key, int32_t&) override;
    [[nodiscard]] bool decodeInt64(StringImpl* key, int64_t&) override;
    [[nodiscard]] bool decodeFloat(StringImpl* key, float&) override;
    [[nodiscard]] bool decodeDouble(StringImpl* key, double&) override;
    [[nodiscard]] bool decodeString(StringImpl* key, RefPtr<StringImpl>&) override;

    bool beginObject(StringImpl* key) override;
    void endObject() override;

    bool beginArray(StringImpl* key) override;
    bool beginArrayElement() override;
    void endArrayElement() override;
    void endArray() override;

    template<typename T>
    const T* getPointerFromDictionaryStack(StringImpl* key);

    template<typename T> [[nodiscard]]
    bool decodeSimpleValue(StringImpl* key, T& result);

    const std::unique_ptr<Dictionary> m_rootDictionary;
    Vector<Dictionary*, 16> m_dictionaryStack;
    Vector<Array*, 16> m_arrayStack;
    Vector<size_t, 16> m_arrayIndexStack;
};

} // namespace WebCore
