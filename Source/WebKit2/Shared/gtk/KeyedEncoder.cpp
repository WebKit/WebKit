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
#include "KeyedEncoder.h"

#include <WebCore/SharedBuffer.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

KeyedEncoder::KeyedEncoder()
{
    g_variant_builder_init(&m_variantBuilder, G_VARIANT_TYPE("a{sv}"));
    m_variantBuilderStack.append(&m_variantBuilder);
}

KeyedEncoder::~KeyedEncoder()
{
    ASSERT(m_variantBuilderStack.size() == 1);
    ASSERT(m_variantBuilderStack.last() == &m_variantBuilder);
    ASSERT(m_arrayStack.isEmpty());
    ASSERT(m_objectStack.isEmpty());
}

void KeyedEncoder::encodeBytes(const String& key, const uint8_t* bytes, size_t size)
{
    GRefPtr<GBytes> gBytes = adoptGRef(g_bytes_new_static(bytes, size));
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_from_bytes(G_VARIANT_TYPE("ay"), gBytes.get(), TRUE));
}

void KeyedEncoder::encodeBool(const String& key, bool value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_boolean(value));
}

void KeyedEncoder::encodeUInt32(const String& key, uint32_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_uint32(value));
}

void KeyedEncoder::encodeInt32(const String& key, int32_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_int32(value));
}

void KeyedEncoder::encodeInt64(const String& key, int64_t value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_int64(value));
}

void KeyedEncoder::encodeFloat(const String& key, float value)
{
    encodeDouble(key, value);
}

void KeyedEncoder::encodeDouble(const String& key, double value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_double(value));
}

void KeyedEncoder::encodeString(const String& key, const String& value)
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", key.utf8().data(), g_variant_new_string(value.utf8().data()));
}

void KeyedEncoder::beginObject(const String& key)
{
    GRefPtr<GVariantBuilder> builder = adoptGRef(g_variant_builder_new(G_VARIANT_TYPE("aa{sv}")));
    m_objectStack.append(std::make_pair(key, builder));
    m_variantBuilderStack.append(builder.get());
}

void KeyedEncoder::endObject()
{
    GVariantBuilder* builder = m_variantBuilderStack.takeLast();
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", m_objectStack.last().first.utf8().data(), g_variant_builder_end(builder));
    m_objectStack.removeLast();
}

void KeyedEncoder::beginArray(const String& key)
{
    m_arrayStack.append(std::make_pair(key, adoptGRef(g_variant_builder_new(G_VARIANT_TYPE("aa{sv}")))));
}

void KeyedEncoder::beginArrayElement()
{
    m_variantBuilderStack.append(g_variant_builder_new(G_VARIANT_TYPE("a{sv}")));
}

void KeyedEncoder::endArrayElement()
{
    GRefPtr<GVariantBuilder> variantBuilder = adoptGRef(m_variantBuilderStack.takeLast());
    g_variant_builder_add_value(m_arrayStack.last().second.get(), g_variant_builder_end(variantBuilder.get()));
}

void KeyedEncoder::endArray()
{
    g_variant_builder_add(m_variantBuilderStack.last(), "{sv}", m_arrayStack.last().first.utf8().data(), g_variant_builder_end(m_arrayStack.last().second.get()));
    m_arrayStack.removeLast();
}

PassRefPtr<SharedBuffer> KeyedEncoder::finishEncoding()
{
    g_assert(m_variantBuilderStack.last() == &m_variantBuilder);
    GRefPtr<GVariant> variant = g_variant_builder_end(&m_variantBuilder);
    GRefPtr<GBytes> data = g_variant_get_data_as_bytes(variant.get());
    return SharedBuffer::create(static_cast<const unsigned char*>(g_bytes_get_data(data.get(), nullptr)), static_cast<unsigned>(g_bytes_get_size(data.get())));
}

} // namespace WebKit
