/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "JavaScriptEvaluationResult.h"

#if USE(GLIB)
#include "APIArray.h"
#include "APIDictionary.h"
#include "APISerializedScriptValue.h"
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>

namespace WebKit {

class JavaScriptEvaluationResult::GLibExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    JSObjectID addObjectToMap(GVariant*);
private:
    Value toValue(GVariant*);

    Map m_map;
};

GRefPtr<JSCValue> JavaScriptEvaluationResult::toJSC()
{
    auto context = API::SerializedScriptValue::deserializationContext();
    auto js = this->toJS(context.get());
    return jscContextGetOrCreateValue(jscContextGetOrCreate(context.get()).get(), js.get());
}

JSObjectID JavaScriptEvaluationResult::GLibExtractor::addObjectToMap(GVariant* variant)
{
    auto identifier = JSObjectID::generate();
    m_map.add(identifier, toValue(variant));
    return identifier;
}

auto JavaScriptEvaluationResult::GLibExtractor::toValue(GVariant* variant) -> Value
{
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE("a{sv}"))) {
        ObjectMap objectMap;
        GVariantIter iter;
        g_variant_iter_init(&iter, variant);
        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!key || !value)
                continue;
            auto keyID = JSObjectID::generate();
            m_map.add(keyID, String::fromUTF8(key));
            auto valueID = JSObjectID::generate();
            m_map.add(valueID, toValue(value));
            objectMap.add(keyID, valueID);
        }
        return { WTFMove(objectMap) };
    }

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32))
        return static_cast<double>(g_variant_get_uint32(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32))
        return static_cast<double>(g_variant_get_int32(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64))
        return static_cast<double>(g_variant_get_uint64(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64))
        return static_cast<double>(g_variant_get_int64(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT16))
        return static_cast<double>(g_variant_get_int16(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT16))
        return static_cast<double>(g_variant_get_uint16(variant));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE))
        return static_cast<double>(g_variant_get_double(variant));

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING))
        return String::fromUTF8(g_variant_get_string(variant, nullptr));

    return EmptyType::Null;
}

static bool isSerializable(GVariant* variant)
{
    if (!variant)
        return false;

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_INT16)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT16)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE)
        || g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING))
        return true;

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE("a{sv}"))) {
        GVariantIter iter;
        g_variant_iter_init(&iter, variant);
        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!key || !isSerializable(value))
                return false;
        }
        return true;
    }

    return false;
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(GVariant* variant)
{
    if (!isSerializable(variant))
        return std::nullopt;

    GLibExtractor extractor;
    auto root = extractor.addObjectToMap(variant);
    return JavaScriptEvaluationResult { root, extractor.takeMap() };
}

} // namespace WebKit

#endif // USE(GLIB)
