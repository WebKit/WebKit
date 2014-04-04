/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef JSGlobalObjectInspectorController_h
#define JSGlobalObjectInspectorController_h

#if ENABLE(INSPECTOR)

#include "InspectorAgentRegistry.h"
#include "InspectorEnvironment.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ConsoleClient;
class ExecState;
class JSGlobalObject;
class JSValue;
}

namespace Inspector {

class InjectedScriptManager;
class InspectorConsoleAgent;
class InspectorBackendDispatcher;
class InspectorConsoleAgent;
class InspectorFrontendChannel;
class JSConsoleClient;
class ScriptCallStack;

class JSGlobalObjectInspectorController final : public InspectorEnvironment {
    WTF_MAKE_NONCOPYABLE(JSGlobalObjectInspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    JSGlobalObjectInspectorController(JSC::JSGlobalObject&);
    ~JSGlobalObjectInspectorController();

    void connectFrontend(InspectorFrontendChannel*);
    void disconnectFrontend(InspectorDisconnectReason reason);
    void dispatchMessageFromFrontend(const String&);

    void globalObjectDestroyed();

    bool includesNativeCallStackWhenReportingExceptions() const { return m_includeNativeCallStackWithExceptions; }
    void setIncludesNativeCallStackWhenReportingExceptions(bool includesNativeCallStack) { m_includeNativeCallStackWithExceptions = includesNativeCallStack; }

    void reportAPIException(JSC::ExecState*, JSC::JSValue exception);

    JSC::ConsoleClient* consoleClient() const;

    virtual bool developerExtrasEnabled() const override { return true; }
    virtual bool canAccessInspectedScriptState(JSC::ExecState*) const override { return true; }
    virtual InspectorFunctionCallHandler functionCallHandler() const override;
    virtual InspectorEvaluateHandler evaluateHandler() const override;
    virtual void willCallInjectedScriptFunction(JSC::ExecState*, const String&, int) override { }
    virtual void didCallInjectedScriptFunction(JSC::ExecState*) override { }

private:
    void appendAPIBacktrace(ScriptCallStack* callStack);

    JSC::JSGlobalObject& m_globalObject;
    std::unique_ptr<InjectedScriptManager> m_injectedScriptManager;
    std::unique_ptr<JSConsoleClient> m_consoleClient;
    InspectorConsoleAgent* m_consoleAgent;
    InspectorAgentRegistry m_agents;
    InspectorFrontendChannel* m_inspectorFrontendChannel;
    RefPtr<InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    bool m_includeNativeCallStackWithExceptions;
};

} // namespace Inspector

#endif // ENABLE(INSPECTOR)

#endif // !defined(JSGlobalObjectInspectorController_h)
