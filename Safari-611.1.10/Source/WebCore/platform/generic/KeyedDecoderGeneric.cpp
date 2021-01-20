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

#include "KeyedEncoderGeneric.h"
#include <wtf/HashMap.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class KeyedDecoderGeneric::Dictionary {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Node = Variant<Vector<uint8_t>, bool, uint32_t, uint64_t, int32_t, int64_t, float, double, String, std::unique_ptr<Dictionary>, std::unique_ptr<Array>>;

    template <typename T>
    void add(const String& key, T&& value) { m_map.add(key, makeUnique<Node>(std::forward<T>(value))); }
    Node* get(const String& key) { return m_map.get(key); }

private:
    HashMap<String, std::unique_ptr<Node>> m_map;
};

static Optional<String> readString(WTF::Persistence::Decoder& decoder)
{
    Optional<size_t> size;
    decoder >> size;
    if (!size)
        return WTF::nullopt;
    if (!size.value())
        return emptyString();

    Vector<uint8_t> buffer(size.value());
    if (!decoder.decodeFixedLengthData(buffer.data(), size.value()))
        return WTF::nullopt;
    auto result = String::fromUTF8(buffer.data(), size.value());
    if (result.isNull())
        return WTF::nullopt;

    return result;
}

template<typename T>
static bool readSimpleValue(WTF::Persistence::Decoder& decoder, KeyedDecoderGeneric::Dictionary& dictionary)
{
    auto key = readString(decoder);
    if (!key)
        return false;
    Optional<T> value;
    decoder >> value;
    if (!value)
        return false;
    dictionary.add(key.value(), WTFMove(value.value()));
    return true;
}

std::unique_ptr<KeyedDecoder> KeyedDecoder::decoder(const uint8_t* data, size_t size)
{
    return makeUnique<KeyedDecoderGeneric>(data, size);
}

KeyedDecoderGeneric::KeyedDecoderGeneric(const uint8_t* data, size_t size)
{
    WTF::Persistence::Decoder decoder(data, size);

    m_rootDictionary = makeUnique<Dictionary>();
    m_dictionaryStack.append(m_rootDictionary.get());

    bool ok = true;
    while (ok) {
        Optional<KeyedEncoderGeneric::Type> type;
        decoder >> type;
        if (!type)
            break;

        switch (*type) {
        case KeyedEncoderGeneric::Type::Bytes: {
            auto key = readString(decoder);
            if (!key)
                ok = false;
            if (!ok)
                break;
            Optional<size_t> size;
            decoder >> size;
            if (!size)
                ok = false;
            if (!ok)
                break;
            ok = decoder.bufferIsLargeEnoughToContain<uint8_t>(*size);
            if (!ok)
                break;
            Vector<uint8_t> buffer(*size);
            ok = decoder.decodeFixedLengthData(buffer.data(), *size);
            if (!ok)
                break;
            m_dictionaryStack.last()->add(*key, WTFMove(buffer));
            break;
        }
        case KeyedEncoderGeneric::Type::Bool:
            ok = readSimpleValue<bool>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::UInt32:
            ok = readSimpleValue<uint32_t>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::UInt64:
            ok = readSimpleValue<uint64_t>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::Int32:
            ok = readSimpleValue<int32_t>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::Int64:
            ok = readSimpleValue<int64_t>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::Float:
            ok = readSimpleValue<float>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::Double:
            ok = readSimpleValue<double>(decoder, *m_dictionaryStack.last());
            break;
        case KeyedEncoderGeneric::Type::String: {
            auto key = readString(decoder);
            if (!key)
                ok = false;
            if (!ok)
                break;
            auto value = readString(decoder);
            if (!value)
                ok = false;
            if (!ok)
                break;
            m_dictionaryStack.last()->add(*key, WTFMove(*value));
            break;
        }
        case KeyedEncoderGeneric::Type::BeginObject: {
            auto key = readString(decoder);
            if (!key)
                ok = false;
            if (!ok)
                break;
            auto* currentDictinary = m_dictionaryStack.last();
            auto newDictionary = makeUnique<Dictionary>();
            m_dictionaryStack.append(newDictionary.get());
            currentDictinary->add(*key, WTFMove(newDictionary));
            break;
        }
        case KeyedEncoderGeneric::Type::EndObject:
            m_dictionaryStack.removeLast();
            if (m_dictionaryStack.isEmpty())
                ok = false;
            break;
        case KeyedEncoderGeneric::Type::BeginArray: {
            auto key = readString(decoder);
            if (!key)
                ok = false;
            if (!ok)
                break;
            auto newArray = makeUnique<Array>();
            m_arrayStack.append(newArray.get());
            m_dictionaryStack.last()->add(*key, WTFMove(newArray));
            break;
        }
        case KeyedEncoderGeneric::Type::BeginArrayElement: {
            ok = !m_arrayStack.isEmpty();
            if (!ok)
                break;
            auto newDictionary = makeUnique<Dictionary>();
            m_dictionaryStack.append(newDictionary.get());
            m_arrayStack.last()->append(WTFMove(newDictionary));
            break;
        }
        case KeyedEncoderGeneric::Type::EndArrayElement:
            m_dictionaryStack.removeLast();
            if (m_dictionaryStack.isEmpty())
                ok = false;
            break;
        case KeyedEncoderGeneric::Type::EndArray:
            ok = !m_arrayStack.isEmpty();
            if (!ok)
                break;
            m_arrayStack.removeLast();
            break;
        }
    }
    while (m_dictionaryStack.size() > 1)
        m_dictionaryStack.removeLast();
    while (!m_arrayStack.isEmpty())
        m_arrayStack.removeLast();
}

template<typename T>
const T* KeyedDecoderGeneric::getPointerFromDictionaryStack(const String& key)
{
    auto& dictionary = m_dictionaryStack.last();

    auto node = dictionary->get(key);
    if (!node)
        return nullptr;

    return WTF::get_if<T>(*node);
}

template<typename T>
bool KeyedDecoderGeneric::decodeSimpleValue(const String& key, T& result)
{
    auto value = getPointerFromDictionaryStack<T>(key);
    if (!value)
        return false;

    result = *value;
    return true;
}

bool KeyedDecoderGeneric::decodeBytes(const String& key, const uint8_t*& data, size_t& size)
{
    auto value = getPointerFromDictionaryStack<Vector<uint8_t>>(key);
    if (!value)
        return false;

    data = value->data();
    size = value->size();
    return true;
}

bool KeyedDecoderGeneric::decodeBool(const String& key, bool& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeUInt32(const String& key, uint32_t& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeUInt64(const String& key, uint64_t& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeInt32(const String& key, int32_t& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeInt64(const String& key, int64_t& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeFloat(const String& key, float& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeDouble(const String& key, double& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::decodeString(const String& key, String& result)
{
    return decodeSimpleValue(key, result);
}

bool KeyedDecoderGeneric::beginObject(const String& key)
{
    auto value = getPointerFromDictionaryStack<std::unique_ptr<Dictionary>>(key);
    if (!value)
        return false;

    m_dictionaryStack.append(value->get());
    return true;
}

void KeyedDecoderGeneric::endObject()
{
    m_dictionaryStack.removeLast();
}

bool KeyedDecoderGeneric::beginArray(const String& key)
{
    auto value = getPointerFromDictionaryStack<std::unique_ptr<Array>>(key);
    if (!value)
        return false;

    m_arrayStack.append(value->get());
    m_arrayIndexStack.append(0);
    return true;
}

bool KeyedDecoderGeneric::beginArrayElement()
{
    if (m_arrayIndexStack.last() >= m_arrayStack.last()->size())
        return false;

    auto dictionary = m_arrayStack.last()->at(m_arrayIndexStack.last()++).get();
    m_dictionaryStack.append(dictionary);
    return true;
}

void KeyedDecoderGeneric::endArrayElement()
{
    m_dictionaryStack.removeLast();
}

void KeyedDecoderGeneric::endArray()
{
    m_arrayStack.removeLast();
    m_arrayIndexStack.removeLast();
}

} // namespace WebCore
