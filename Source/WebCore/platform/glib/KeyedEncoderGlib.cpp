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
#include "KeyedEncoderGlib.h"

#include "SharedBuffer.h"
#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<KeyedEncoder> KeyedEncoder::encoder()
{
    return makeUnique<KeyedEncoderGlib>();
}

KeyedEncoderGlib::KeyedEncoderGlib()
{
    g_variant_builder_init(&m_variantBuilder, G_VARIANT_TYPE("a{sv}"));
    m_variantBuilderStack.append(&m_variantBuilder);
}

KeyedEncoderGlib::~KeyedEncoderGlib()
{
    ASSERT(m_variantBuilderStack.size() == 1);
    ASSERT(m_variantBuilderStack.last() == &m_variantBuilder);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_objectStack.isEmpty());
}

void KeyedEncoderGlib::encodeBytes(const String& key, const uint8_t* bytes, size_t size)
{
    GRefPtr<GBytes> gBytes = adoptGRef(g_bytes_new_static(bytes, size));
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_from_bytes(G_VARIANT_TYPE("ay"), gBytes.get(), TRUE));
}

void KeyedEncoderGlib::encodeBool(const String& key, bool value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_boolean(value));
}

void KeyedEncoderGlib::encodeUInt32(const String& key, uint32_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_uint32(value));
}
    
void KeyedEncoderGlib::encodeUInt64(const String& key, uint64_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_uint64(value));
}

void KeyedEncoderGlib::encodeInt32(const String& key, int32_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_int32(value));
}

void KeyedEncoderGlib::encodeInt64(const String& key, int64_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_int64(value));
}

void KeyedEncoderGlib::encodeFloat(const String& key, float value)
{
    encodeDouble(key, value);
}

void KeyedEncoderGlib::encodeDouble(const String& key, double value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_double(value));
}

void KeyedEncoderGlib::encodeString(const String& key, const String& value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_string(value.utf8().data()));
}

void KeyedEncoderGlib::beginObject(const String& key)
{
    GRefPtr<GVariantBuilder> builder = adoptGRef(g_variant_builder_new(G_VARIANT_TYPE("a{sv}")));
    m_objectStack.append(std::make_pair(key, builder));
    m_variantBuilderStack.append(builder.get());
}

void KeyedEncoderGlib::endObject()
{
    GVariantBuilder* builder = m_variantBuilderStack.takeLast();
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", m_objectStack.last().first.utf8().data(), g_variant_builder_end(builder));
    m_objectStack.removeLast();
}

void KeyedEncoderGlib::beginArray(const String& key)
{
    m_arrayStack.append(std::make_pair(key, adoptGRef(g_variant_builder_new(G_VARIANT_TYPE("aa{sv}")))));
}

void KeyedEncoderGlib::beginArrayElement()
{
    m_variantBuilderStack.append(g_variant_builder_new(G_VARIANT_TYPE("a{sv}")));
}

void KeyedEncoderGlib::endArrayElement()
{
    GRefPtr<GVariantBuilder> variantBuilder = adoptGRef(m_variantBuilderStack.takeLast());
    g_variant_builder_add_value(m_arrayStack.last().second.get(), g_variant_builder_end(variantBuilder.get()));
}

void KeyedEncoderGlib::endArray()
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", m_arrayStack.last().first.utf8().data(), g_variant_builder_end(m_arrayStack.last().second.get()));
    m_arrayStack.removeLast();
}

RefPtr<SharedBuffer> KeyedEncoderGlib::finishEncoding()
{
    g_assert(m_variantBuilderStack.last() == &m_variantBuilder);
    GRefPtr<GVariant> variant = g_variant_builder_end(&m_variantBuilder);
    GRefPtr<GBytes> data = g_variant_get_data_as_bytes(variant.get());
    return SharedBuffer::create(static_cast<const unsigned char*>(g_bytes_get_data(data.get(), nullptr)), static_cast<unsigned>(g_bytes_get_size(data.get())));
}

} // namespace WebCore
