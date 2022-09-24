/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "EventLoop.h"
#include "Microtasks.h"
#include "ReferrerPolicy.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

#include <wtf/IsoMalloc.h>

namespace WebCore {

class EmptyScriptExecutionContext final : public RefCounted<EmptyScriptExecutionContext>, public ScriptExecutionContext {
public:
    static Ref<EmptyScriptExecutionContext> create(JSC::VM& vm)
    {
        return adoptRef(*new EmptyScriptExecutionContext(vm));
    }

    ~EmptyScriptExecutionContext()
    {
        m_eventLoop->removeAssociatedContext(*this);
    }

    bool isSecureContext() const final { return false; }
    bool isJSExecutionForbidden() const final { return false; }
    EventLoopTaskGroup& eventLoop() final
    {
        ASSERT_NOT_REACHED();
        return *m_eventLoopTaskGroup;
    }
    const URL& url() const final { return m_url; }
    URL completeURL(const String&, ForceUTF8 = ForceUTF8::No) const final { return { }; };
    String userAgent(const URL&) const final { return emptyString(); }
    ReferrerPolicy referrerPolicy() const final { return ReferrerPolicy::EmptyString; }

    void disableEval(const String&) final { };
    void disableWebAssembly(const String&) final { };

    IDBClient::IDBConnectionProxy* idbConnectionProxy() final { return nullptr; }
    SocketProvider* socketProvider() final { return nullptr; }

    void addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&&) final { }
    void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long) final { };

    SecurityOrigin& topOrigin() const final { return m_origin.get(); };

    void postTask(Task&&) final { ASSERT_NOT_REACHED(); }
    EventTarget* errorEventTarget() final { return nullptr; };

#if ENABLE(WEB_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { return false; }
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) final { return false; }
#endif

    JSC::VM& vm() final { return m_vm; }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    EmptyScriptExecutionContext(JSC::VM& vm)
        : m_vm(vm)
        , m_origin(SecurityOrigin::createOpaque())
        , m_eventLoop(EmptyEventLoop::create(vm))
        , m_eventLoopTaskGroup(makeUnique<EventLoopTaskGroup>(m_eventLoop))
    {
        m_eventLoop->addAssociatedContext(*this);
    }

    void addMessage(MessageSource, MessageLevel, const String&, const String&, unsigned, unsigned, RefPtr<Inspector::ScriptCallStack>&&, JSC::JSGlobalObject* = nullptr, unsigned long = 0) final { }
    void logExceptionToConsole(const String&, const String&, int, int, RefPtr<Inspector::ScriptCallStack>&&) final { };
    void refScriptExecutionContext() final { ref(); };
    void derefScriptExecutionContext() final { deref(); };

    const Settings::Values& settingsValues() const final { return m_settingsValues; }

#if ENABLE(NOTIFICATIONS)
    NotificationClient* notificationClient() final { return nullptr; }
#endif

    class EmptyEventLoop final : public EventLoop {
    public:
        static Ref<EmptyEventLoop> create(JSC::VM& vm)
        {
            return adoptRef(*new EmptyEventLoop(vm));
        }

        MicrotaskQueue& microtaskQueue() final { return m_queue; };

    private:
        explicit EmptyEventLoop(JSC::VM& vm)
            : m_queue(MicrotaskQueue(vm, *this))
        {
        }

        void scheduleToRun() final { ASSERT_NOT_REACHED(); }
        bool isContextThread() const final { return true; }

        MicrotaskQueue m_queue;
    };

    Ref<JSC::VM> m_vm;
    Ref<SecurityOrigin> m_origin;
    URL m_url;
    Ref<EmptyEventLoop> m_eventLoop;
    std::unique_ptr<EventLoopTaskGroup> m_eventLoopTaskGroup;
    Settings::Values m_settingsValues;
};

} // namespace WebCore
