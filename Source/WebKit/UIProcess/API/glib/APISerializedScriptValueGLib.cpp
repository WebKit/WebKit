/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "APISerializedScriptValue.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextPrivate.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSRemoteInspector.h>
#include <jsc/JSCContextPrivate.h>
#include <jsc/JSCValuePrivate.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace API {

static constexpr auto sharedJSContextMaxIdleTime = 10_s;

class SharedJSContext {
public:
    SharedJSContext()
        : m_timer(RunLoop::main(), this, &SharedJSContext::releaseContextIfNecessary)
    {
        m_timer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
    }

    JSCContext* ensureContext()
    {
        m_lastUseTime = MonotonicTime::now();
        if (!m_context) {
            bool previous = JSRemoteInspectorGetInspectionEnabledByDefault();
            JSRemoteInspectorSetInspectionEnabledByDefault(false);
            m_context = adoptGRef(jsc_context_new());
            JSRemoteInspectorSetInspectionEnabledByDefault(previous);

            m_timer.startOneShot(sharedJSContextMaxIdleTime);
        }
        return m_context.get();
    }

    void releaseContextIfNecessary()
    {
        auto idleTime = MonotonicTime::now() - m_lastUseTime;
        if (idleTime < sharedJSContextMaxIdleTime) {
            // We lazily restart the timer if needed every 10 seconds instead of doing so every time ensureContext()
            // is called, for performance reasons.
            m_timer.startOneShot(sharedJSContextMaxIdleTime - idleTime);
            return;
        }
        m_context.clear();
    }

private:
    GRefPtr<JSCContext> m_context;
    RunLoop::Timer<SharedJSContext> m_timer;
    MonotonicTime m_lastUseTime;
};

static SharedJSContext& sharedContext()
{
    static NeverDestroyed<SharedJSContext> sharedContext;
    return sharedContext.get();
}

JSCContext* SerializedScriptValue::sharedJSCContext()
{
    return sharedContext().ensureContext();
}

GRefPtr<JSCValue> SerializedScriptValue::deserialize(WebCore::SerializedScriptValue& serializedScriptValue)
{
    ASSERT(RunLoop::isMain());

    auto* context = sharedJSCContext();
    return jscContextGetOrCreateValue(context, serializedScriptValue.deserialize(jscContextGetJSContext(context), nullptr));
}

static GRefPtr<JSCValue> valueFromGVariant(JSCContext* context, GVariant* variant)
{
    if (g_variant_is_container(variant)) {
        auto result = adoptGRef(jsc_value_new_object(context, nullptr, nullptr));
        GVariantIter iter;
        g_variant_iter_init(&iter, variant);
        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!key)
                continue;
            auto jsValue = valueFromGVariant(context, value);
            if (jsValue)
                jsc_value_object_set_property(result.get(), key, jsValue.get());
        }
        return result;
    }

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_uint32(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_int32(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_uint64(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_int64(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT16))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_int16(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT16))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_uint16(variant)));
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE))
        return adoptGRef(jsc_value_new_number(context, g_variant_get_double(variant)));

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING))
        return adoptGRef(jsc_value_new_string(context, g_variant_get_string(variant, nullptr)));

    g_warning("Unhandled %s GVariant for conversion to JSCValue", g_variant_get_type_string(variant));
    return nullptr;
}

static RefPtr<WebCore::SerializedScriptValue> coreValueFromGVariant(GVariant* variant)
{
    if (!variant)
        return nullptr;

    ASSERT(RunLoop::isMain());
    auto* context = SerializedScriptValue::sharedJSCContext();
    auto value = valueFromGVariant(context, variant);
    if (!value)
        return nullptr;

    auto globalObject = toJS(jscContextGetJSContext(context));
    ASSERT(globalObject);
    JSC::JSLockHolder lock(globalObject);

    return WebCore::SerializedScriptValue::create(*globalObject, toJS(globalObject, jscValueGetJSValue(value.get())));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::createFromGVariant(GVariant* object)
{
    auto coreValue = coreValueFromGVariant(object);
    if (!coreValue)
        return nullptr;
    return create(coreValue.releaseNonNull());
}

}; // namespace API
