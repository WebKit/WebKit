/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/JSContext.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSValue.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>

namespace API {

class SharedJSContext {
public:
    SharedJSContext()
        : m_timer(RunLoop::main(), this, &SharedJSContext::releaseContext)
    {
    }

    JSContext* ensureContext()
    {
        if (!m_context) {
            m_context = adoptNS([[JSContext alloc] init]);
            m_timer.startOneShot(1_s);
        }
        return m_context.get();
    }

    void releaseContext()
    {
        m_context.clear();
    }

private:
    RetainPtr<JSContext> m_context;
    RunLoop::Timer<SharedJSContext> m_timer;
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

Optional<Vector<uint8_t>> SerializedScriptValue::wireBytesFromNSObject(id object)
{
    ASSERT(RunLoop::isMain());
    JSContext* context = sharedContext().ensureContext();
    JSValue *value = [JSValue valueWithObject:object inContext:context];
    if (!value)
        return WTF::nullopt;

    auto globalObject = toJS([context JSGlobalContextRef]);
    ASSERT(globalObject);
    JSC::JSLockHolder lock(globalObject);

    auto coreValue = WebCore::SerializedScriptValue::create(*globalObject, toJS(globalObject, [value JSValueRef]));
    return coreValue->toWireBytes();
}

} // API
