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

#include <JavaScriptCore/JSRemoteInspector.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/RuntimeApplicationChecks.h>

namespace API {

static constexpr auto SharedJSContextWKMaxIdleTime = 10_s;

class SharedJSContextWK {
public:
    static SharedJSContextWK& singleton()
    {
        static MainRunLoopNeverDestroyed<SharedJSContextWK> sharedContext;
        return sharedContext.get();
    }

    // Do nothing since this is a singleton.
    void ref() const { }
    void deref() const { }

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
    friend class NeverDestroyed<SharedJSContextWK, MainRunLoopAccessTraits>;

    SharedJSContextWK()
        : m_timer(RunLoop::mainSingleton(), "SharedJSContextWK::Timer"_s, this, &SharedJSContextWK::releaseContextIfNecessary)
    {
    }

    JSRetainPtr<JSGlobalContextRef> m_context;
    RunLoop::Timer m_timer;
    MonotonicTime m_lastUseTime;
};

JSRetainPtr<JSGlobalContextRef> SerializedScriptValue::deserializationContext()
{
    return SharedJSContextWK::singleton().ensureContext();
}

} // API
