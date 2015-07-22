/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "KeyedDecoder.h"

#include <wtf/text/CString.h>

namespace WebKit {

KeyedDecoder::KeyedDecoder(const uint8_t* data, size_t size)
{
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data, size));
    GRefPtr<GVariant> variant = g_variant_new_from_bytes(G_VARIANT_TYPE("a{sv}"), bytes.get(), TRUE);
    m_dictionaryStack.append(dictionaryFromGVariant(variant.get()));
}

KeyedDecoder::~KeyedDecoder()
{
    ASSERT(m_dictionaryStack.size() == 1);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_arrayIndexStack.isEmpty());
}

HashMap<String, GRefPtr<GVariant>> KeyedDecoder::dictionaryFromGVariant(GVariant* variant)
{
    HashMap<String, GRefPtr<GVariant>> dictionary;
    GVariantIter iter;
    g_variant_iter_init(&iter, variant);
    const char* key;
    GVariant* value;
    while (g_variant_iter_loop(&iter, "{&sv}", &key, &value))
        dictionary.set(String::fromUTF8(key), value);
    return WTF::move(dictionary);
}

bool KeyedDecoder::decodeBytes(const String& key, const uint8_t*& bytes, size_t& size)
{
    GRefPtr<GVariant> value = m_dictionaryStack.last().get(key);
    if (!value)
        return false;

    size = g_variant_get_size(value.get());
    bytes = static_cast<const uint8_t*>(g_variant_get_data(value.get()));
    return true;
}

template<typename T, typename F>
bool KeyedDecoder::decodeSimpleValue(const String& key, T& result, F getFunction)
{
    GRefPtr<GVariant> value = m_dictionaryStack.last().get(key);
    if (!value)
        return false;

    result = getFunction(value.get());
    return true;
}

bool KeyedDecoder::decodeBool(const String& key, bool& result)
{
    return decodeSimpleValue(key, result, g_variant_get_boolean);
}

bool KeyedDecoder::decodeUInt32(const String& key, uint32_t& result)
{
    return decodeSimpleValue(key, result, g_variant_get_uint32);
}

bool KeyedDecoder::decodeInt32(const String& key, int32_t& result)
{
    return decodeSimpleValue(key, result, g_variant_get_int32);
}

bool KeyedDecoder::decodeInt64(const String& key, int64_t& result)
{
    return decodeSimpleValue(key, result, g_variant_get_int64);
}

bool KeyedDecoder::decodeFloat(const String& key, float& result)
{
    return decodeSimpleValue(key, result, g_variant_get_double);
}

bool KeyedDecoder::decodeDouble(const String& key, double& result)
{
    return decodeSimpleValue(key, result, g_variant_get_double);
}

bool KeyedDecoder::decodeString(const String& key, String& result)
{
    GRefPtr<GVariant> value = m_dictionaryStack.last().get(key);
    if (!value)
        return false;

    result = String::fromUTF8(g_variant_get_string(value.get(), nullptr));
    return true;
}

bool KeyedDecoder::beginObject(const String& key)
{
    GRefPtr<GVariant> value = m_dictionaryStack.last().get(key);
    if (!value)
        return false;

    m_dictionaryStack.append(dictionaryFromGVariant(value.get()));
    return true;
}

void KeyedDecoder::endObject()
{
    m_dictionaryStack.removeLast();
}

bool KeyedDecoder::beginArray(const String& key)
{
    GRefPtr<GVariant> value = m_dictionaryStack.last().get(key);
    if (!value)
        return false;

    m_arrayStack.append(value.get());
    m_arrayIndexStack.append(0);
    return true;
}

bool KeyedDecoder::beginArrayElement()
{
    if (m_arrayIndexStack.last() >= g_variant_n_children(m_arrayStack.last()))
        return false;

    GRefPtr<GVariant> variant = adoptGRef(g_variant_get_child_value(m_arrayStack.last(), m_arrayIndexStack.last()++));
    m_dictionaryStack.append(dictionaryFromGVariant(variant.get()));
    return true;
}

void KeyedDecoder::endArrayElement()
{
    m_dictionaryStack.removeLast();
}

void KeyedDecoder::endArray()
{
    m_arrayStack.removeLast();
    m_arrayIndexStack.removeLast();
}

} // namespace WebKit
