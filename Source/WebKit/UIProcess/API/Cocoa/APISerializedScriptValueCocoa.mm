/*
 * Copyright (C) 2015, 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "APISerializedScriptValue.h"

#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSContextPrivate.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSRemoteInspector.h>
#import <JavaScriptCore/JSValue.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>

namespace API {

static constexpr auto sharedJSContextMaxIdleTime = 10_s;

class SharedJSContext {
public:
    SharedJSContext()
        : m_timer(RunLoop::main(), this, &SharedJSContext::releaseContextIfNecessary)
    {
    }

    JSContext* ensureContext()
    {
        m_lastUseTime = MonotonicTime::now();
        if (!m_context) {
            // FIXME: rdar://100738357 Remote Web Inspector: Remove use of JSRemoteInspectorGetInspectionEnabledByDefault
            // and JSRemoteInspectorSetInspectionEnabledByDefault once the default state is always false.
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            bool previous = JSRemoteInspectorGetInspectionEnabledByDefault();
            JSRemoteInspectorSetInspectionEnabledByDefault(false);
            m_context = adoptNS([[JSContext alloc] init]);
            JSRemoteInspectorSetInspectionEnabledByDefault(previous);
            ALLOW_DEPRECATED_DECLARATIONS_END

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
    RetainPtr<JSContext> m_context;
    RunLoop::Timer<SharedJSContext> m_timer;
    MonotonicTime m_lastUseTime;
};

static SharedJSContext& sharedContext()
{
    static NeverDestroyed<SharedJSContext> sharedContext;
    return sharedContext.get();
}

id SerializedScriptValue::deserialize(WebCore::SerializedScriptValue& serializedScriptValue, JSValueRef* exception)
{
    ASSERT(RunLoop::isMain());
    JSContext* context = sharedContext().ensureContext();

    JSValueRef valueRef = serializedScriptValue.deserialize([context JSGlobalContextRef], exception);
    if (!valueRef)
        return nil;

    JSValue *value = [JSValue valueWithJSValueRef:valueRef inContext:context];
    return value.toObject;
}

static bool validateObject(id argument)
{
    if ([argument isKindOfClass:[NSString class]] || [argument isKindOfClass:[NSNumber class]] || [argument isKindOfClass:[NSDate class]] || [argument isKindOfClass:[NSNull class]])
        return true;

    if ([argument isKindOfClass:[NSArray class]]) {
        __block BOOL valid = true;

        [argument enumerateObjectsUsingBlock:^(id object, NSUInteger, BOOL *stop) {
            if (!validateObject(object)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    if ([argument isKindOfClass:[NSDictionary class]]) {
        __block bool valid = true;

        [argument enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
            if (!validateObject(key) || !validateObject(value)) {
                valid = false;
                *stop = YES;
            }
        }];

        return valid;
    }

    return false;
}

static RefPtr<WebCore::SerializedScriptValue> coreValueFromNSObject(id object)
{
    if (object && !validateObject(object))
        return nullptr;

    ASSERT(RunLoop::isMain());
    JSContext* context = sharedContext().ensureContext();
    JSValue *value = [JSValue valueWithObject:object inContext:context];
    if (!value)
        return nullptr;

    auto globalObject = toJS([context JSGlobalContextRef]);
    ASSERT(globalObject);
    JSC::JSLockHolder lock(globalObject);

    return WebCore::SerializedScriptValue::create(*globalObject, toJS(globalObject, [value JSValueRef]));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::createFromNSObject(id object)
{
    auto coreValue = coreValueFromNSObject(object);
    if (!coreValue)
        return nullptr;
    return create(coreValue.releaseNonNull());
}

} // API
