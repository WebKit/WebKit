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
    static SharedJSContext& singleton()
    {
        static NeverDestroyed<SharedJSContext> sharedContext;
        return sharedContext.get();
    }

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

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
    friend class NeverDestroyed<SharedJSContext>;

    SharedJSContext()
        : m_timer(RunLoop::mainSingleton(), this, &SharedJSContext::releaseContextIfNecessary)
    {
        m_timer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
    }

    GRefPtr<JSCContext> m_context;
    RunLoop::Timer m_timer;
    MonotonicTime m_lastUseTime;
};

JSCContext* SerializedScriptValue::sharedJSCContext()
{
    return SharedJSContext::singleton().ensureContext();
}

GRefPtr<JSCValue> SerializedScriptValue::deserialize(WebCore::SerializedScriptValue& serializedScriptValue)
{
    ASSERT(RunLoop::isMain());

    auto* context = sharedJSCContext();
    return jscContextGetOrCreateValue(context, serializedScriptValue.deserialize(jscContextGetJSContext(context), nullptr));
}

RefPtr<SerializedScriptValue> SerializedScriptValue::createFromJSCValue(JSCValue* value)
{
    ASSERT(jsc_value_get_context(value) == sharedJSCContext());
    return create(jscContextGetJSContext(jsc_value_get_context(value)), jscValueGetJSValue(value), nullptr);
}

}; // namespace API
