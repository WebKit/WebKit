/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "APISerializedScriptValue.h"

#include "WKMutableArray.h"
#include "WKMutableDictionary.h"
#include "WKNumber.h"
#include "WKString.h"
#include <JavaScriptCore/JSRemoteInspector.h>
#include <JavaScriptCore/JSRetainPtr.h>

namespace API {

static constexpr auto SharedJSContextWKMaxIdleTime = 10_s;

class SharedJSContextWK {
public:
    SharedJSContextWK()
        : m_timer(RunLoop::main(), this, &SharedJSContextWK::releaseContextIfNecessary)
    {
    }

    JSRetainPtr<JSGlobalContextRef> ensureContext()
    {
        m_lastUseTime = MonotonicTime::now();
        if (!m_context) {
            bool inspectionPreviouslyFollowedInternalPolicies = JSRemoteInspectorGetInspectionFollowsInternalPolicies();
            JSRemoteInspectorSetInspectionFollowsInternalPolicies(false);

            // FIXME: rdar://100738357 Remote Web Inspector: Remove use of JSRemoteInspectorGetInspectionEnabledByDefault
            // and JSRemoteInspectorSetInspectionEnabledByDefault once the default state is always false.
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            bool previous = JSRemoteInspectorGetInspectionEnabledByDefault();
            JSRemoteInspectorSetInspectionEnabledByDefault(false);
            m_context = adopt(JSGlobalContextCreate(nullptr));
            JSRemoteInspectorSetInspectionEnabledByDefault(previous);
            ALLOW_DEPRECATED_DECLARATIONS_END

            JSRemoteInspectorSetInspectionFollowsInternalPolicies(inspectionPreviouslyFollowedInternalPolicies);

            m_timer.startOneShot(SharedJSContextWKMaxIdleTime);
        }
        return m_context;
    }

    void releaseContextIfNecessary()
    {
        auto idleTime = MonotonicTime::now() - m_lastUseTime;
        if (idleTime < SharedJSContextWKMaxIdleTime) {
            // We lazily restart the timer if needed every 10 seconds instead of doing so every time ensureContext()
            // is called, for performance reasons.
            m_timer.startOneShot(SharedJSContextWKMaxIdleTime - idleTime);
            return;
        }
        m_context.clear();
    }

private:
    JSRetainPtr<JSGlobalContextRef> m_context;
    RunLoop::Timer m_timer;
    MonotonicTime m_lastUseTime;
};

static SharedJSContextWK& sharedContext()
{
    static MainThreadNeverDestroyed<SharedJSContextWK> sharedContext;
    return sharedContext.get();
}

static WKRetainPtr<WKTypeRef> valueToWKObject(JSContextRef context, JSValueRef value)
{
    auto jsToWKString = [] (JSStringRef input) {
        size_t bufferSize = JSStringGetMaximumUTF8CStringSize(input);
        Vector<char> buffer(bufferSize);
        size_t utf8Length = JSStringGetUTF8CString(input, buffer.data(), bufferSize);
        ASSERT(buffer[utf8Length - 1] == '\0');
        return adoptWK(WKStringCreateWithUTF8CStringWithLength(buffer.data(), utf8Length - 1));
    };

    if (!JSValueIsObject(context, value)) {
        if (JSValueIsBoolean(context, value))
            return adoptWK(WKBooleanCreate(JSValueToBoolean(context, value)));
        if (JSValueIsNumber(context, value))
            return adoptWK(WKDoubleCreate(JSValueToNumber(context, value, nullptr)));
        if (JSValueIsString(context, value)) {
            JSStringRef jsString = JSValueToStringCopy(context, value, nullptr);
            WKRetainPtr result = jsToWKString(jsString);
            JSStringRelease(jsString);
            return result;
        }
        return nullptr;
    }

    JSObjectRef object = JSValueToObject(context, value, nullptr);

    if (JSValueIsArray(context, value)) {
        JSStringRef jsString = JSStringCreateWithUTF8CString("length");
        JSValueRef lengthPropertyName = JSValueMakeString(context, jsString);
        JSStringRelease(jsString);
        JSValueRef lengthValue = JSObjectGetPropertyForKey(context, object, lengthPropertyName, nullptr);
        double lengthDouble = JSValueToNumber(context, lengthValue, nullptr);
        if (lengthDouble < 0 || lengthDouble > static_cast<double>(std::numeric_limits<size_t>::max()))
            return nullptr;
        size_t length = lengthDouble;
        WKRetainPtr result = adoptWK(WKMutableArrayCreateWithCapacity(length));
        for (size_t i = 0; i < length; ++i)
            WKArrayAppendItem(result.get(), valueToWKObject(context, JSObjectGetPropertyAtIndex(context, object, i, nullptr)).get());
        return result;
    }

    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, object);
    size_t length = JSPropertyNameArrayGetCount(names);
    auto result = adoptWK(WKMutableDictionaryCreateWithCapacity(length));
    for (size_t i = 0; i < length; i++) {
        JSStringRef jsKey = JSPropertyNameArrayGetNameAtIndex(names, i);
        WKRetainPtr key = jsToWKString(jsKey);
        WKRetainPtr value = valueToWKObject(context, JSObjectGetPropertyForKey(context, object, JSValueMakeString(context, jsKey), nullptr));
        WKDictionarySetItem(result.get(), key.get(), value.get());
    }
    JSPropertyNameArrayRelease(names);

    return result;
}

WKRetainPtr<WKTypeRef> SerializedScriptValue::deserializeWK(WebCore::SerializedScriptValue& serializedScriptValue)
{
    ASSERT(RunLoop::isMain());
    JSRetainPtr context = sharedContext().ensureContext();
    ASSERT(context);

    JSValueRef value = serializedScriptValue.deserialize(context.get(), nullptr);
    if (!value)
        return nullptr;

    return valueToWKObject(context.get(), value);
}

} // API
