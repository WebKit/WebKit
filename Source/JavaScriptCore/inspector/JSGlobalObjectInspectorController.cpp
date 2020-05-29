/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "JSGlobalObjectInspectorController.h"

#include "CatchScope.h"
#include "Completion.h"
#include "ConsoleMessage.h"
#include "ErrorHandlingScope.h"
#include "Exception.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorAgent.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorConsoleAgent.h"
#include "InspectorFrontendRouter.h"
#include "InspectorHeapAgent.h"
#include "InspectorScriptProfilerAgent.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectAuditAgent.h"
#include "JSGlobalObjectConsoleClient.h"
#include "JSGlobalObjectDebuggerAgent.h"
#include "JSGlobalObjectRuntimeAgent.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include <wtf/StackTrace.h>
#include <wtf/Stopwatch.h>

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectDebuggable.h"
#include "RemoteInspector.h"
#endif

namespace Inspector {

using namespace JSC;

JSGlobalObjectInspectorController::JSGlobalObjectInspectorController(JSGlobalObject& globalObject)
    : m_globalObject(globalObject)
    , m_injectedScriptManager(makeUnique<InjectedScriptManager>(*this, InjectedScriptHost::create()))
    , m_executionStopwatch(Stopwatch::create())
    , m_scriptDebugServer(globalObject)
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
{
    auto context = jsAgentContext();

    auto consoleAgent = makeUnique<InspectorConsoleAgent>(context);
    m_consoleAgent = consoleAgent.get();
    m_agents.append(WTFMove(consoleAgent));

    m_consoleClient = makeUnique<JSGlobalObjectConsoleClient>(m_consoleAgent);

    m_executionStopwatch->start();
}

JSGlobalObjectInspectorController::~JSGlobalObjectInspectorController()
{
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    if (m_augmentingClient)
        m_augmentingClient->inspectorControllerDestroyed();
#endif
}

void JSGlobalObjectInspectorController::globalObjectDestroyed()
{
    ASSERT(!m_frontendRouter->hasFrontends());

    m_injectedScriptManager->disconnect();

    m_agents.discardValues();
}

void JSGlobalObjectInspectorController::connectFrontend(FrontendChannel& frontendChannel, bool isAutomaticInspection, bool immediatelyPause)
{
    m_isAutomaticInspection = isAutomaticInspection;
    m_pauseAfterInitialization = immediatelyPause;

    createLazyAgents();

    bool connectedFirstFrontend = !m_frontendRouter->hasFrontends();
    m_frontendRouter->connectFrontend(frontendChannel);

    if (!connectedFirstFrontend)
        return;

    // Keep the JSGlobalObject and VM alive while we are debugging it.
    m_strongVM = &m_globalObject.vm();
    m_strongGlobalObject.set(m_globalObject.vm(), &m_globalObject);

    // FIXME: change this to notify agents which frontend has connected (by id).
    m_agents.didCreateFrontendAndBackend(nullptr, nullptr);

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    if (m_globalObject.inspectorDebuggable().type() == Inspector::RemoteControllableTarget::Type::JavaScript)
        ensureInspectorAgent().activateExtraDomains(m_agents.extraDomains());

    if (m_augmentingClient)
        m_augmentingClient->inspectorConnected();
#endif
}

void JSGlobalObjectInspectorController::disconnectFrontend(FrontendChannel& frontendChannel)
{
    // FIXME: change this to notify agents which frontend has disconnected (by id).
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);

    m_frontendRouter->disconnectFrontend(frontendChannel);

    m_isAutomaticInspection = false;
    m_pauseAfterInitialization = false;

    bool disconnectedLastFrontend = !m_frontendRouter->hasFrontends();
    if (!disconnectedLastFrontend)
        return;

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    if (m_augmentingClient)
        m_augmentingClient->inspectorDisconnected();
#endif

    // Remove our JSGlobalObject and VM references, we are done debugging it.
    m_strongGlobalObject.clear();
    m_strongVM = nullptr;
}

void JSGlobalObjectInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

void JSGlobalObjectInspectorController::appendAPIBacktrace(ScriptCallStack& callStack)
{
    static constexpr int framesToShow = 31;
    static constexpr int framesToSkip = 3; // WTFGetBacktrace, appendAPIBacktrace, reportAPIException.

    void* samples[framesToShow + framesToSkip];
    int frames = framesToShow + framesToSkip;
    WTFGetBacktrace(samples, &frames);

    void** stack = samples + framesToSkip;
    int size = frames - framesToSkip;
    for (int i = 0; i < size; ++i) {
        auto demangled = StackTrace::demangle(stack[i]);
        if (demangled)
            callStack.append(ScriptCallFrame(demangled->demangledName() ? demangled->demangledName() : demangled->mangledName(), "[native code]"_s, noSourceID, 0, 0));
        else
            callStack.append(ScriptCallFrame("?"_s, "[native code]"_s, noSourceID, 0, 0));
    }
}

void JSGlobalObjectInspectorController::reportAPIException(JSGlobalObject* globalObject, Exception* exception)
{
    VM& vm = globalObject->vm();
    if (isTerminatedExecutionException(vm, exception))
        return;

    auto scope = DECLARE_CATCH_SCOPE(vm);
    ErrorHandlingScope errorScope(vm);

    Ref<ScriptCallStack> callStack = createScriptCallStackFromException(globalObject, exception);
    if (includesNativeCallStackWhenReportingExceptions())
        appendAPIBacktrace(callStack.get());

    // FIXME: <http://webkit.org/b/115087> Web Inspector: Should not evaluate JavaScript handling exceptions
    // If this is a custom exception object, call toString on it to try and get a nice string representation for the exception.
    String errorMessage = exception->value().toWTFString(globalObject);
    scope.clearException();

    if (JSGlobalObjectConsoleClient::logToSystemConsole()) {
        if (callStack->size()) {
            const ScriptCallFrame& callFrame = callStack->at(0);
            ConsoleClient::printConsoleMessage(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, callFrame.sourceURL(), callFrame.lineNumber(), callFrame.columnNumber());
        } else
            ConsoleClient::printConsoleMessage(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, String(), 0, 0);
    }

    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, WTFMove(callStack)));
}

ConsoleClient* JSGlobalObjectInspectorController::consoleClient() const
{
    return m_consoleClient.get();
}

bool JSGlobalObjectInspectorController::developerExtrasEnabled() const
{
#if ENABLE(REMOTE_INSPECTOR)
    if (!RemoteInspector::singleton().enabled())
        return false;

    if (!m_globalObject.inspectorDebuggable().remoteDebuggingAllowed())
        return false;
#endif

    return true;
}

InspectorFunctionCallHandler JSGlobalObjectInspectorController::functionCallHandler() const
{
    return JSC::call;
}

InspectorEvaluateHandler JSGlobalObjectInspectorController::evaluateHandler() const
{
    return JSC::evaluate;
}

void JSGlobalObjectInspectorController::frontendInitialized()
{
    if (m_pauseAfterInitialization) {
        m_pauseAfterInitialization = false;

        ErrorString ignored;
        ensureDebuggerAgent().enable(ignored);
        ensureDebuggerAgent().pause(ignored);
    }

#if ENABLE(REMOTE_INSPECTOR)
    if (m_isAutomaticInspection)
        m_globalObject.inspectorDebuggable().unpauseForInitializedInspector();
#endif
}

Stopwatch& JSGlobalObjectInspectorController::executionStopwatch() const
{
    return m_executionStopwatch;
}

JSGlobalObjectScriptDebugServer& JSGlobalObjectInspectorController::scriptDebugServer()
{
    return m_scriptDebugServer;
}

VM& JSGlobalObjectInspectorController::vm()
{
    return m_globalObject.vm();
}

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
void JSGlobalObjectInspectorController::appendExtraAgent(std::unique_ptr<InspectorAgentBase> agent)
{
    String domainName = agent->domainName();

    // FIXME: change this to notify agents which frontend has connected (by id).
    agent->didCreateFrontendAndBackend(nullptr, nullptr);

    m_agents.appendExtraAgent(WTFMove(agent));

    if (m_globalObject.inspectorDebuggable().type() == Inspector::RemoteControllableTarget::Type::JavaScript)
        ensureInspectorAgent().activateExtraDomain(domainName);
}
#endif

InspectorAgent& JSGlobalObjectInspectorController::ensureInspectorAgent()
{
    if (!m_inspectorAgent) {
        auto context = jsAgentContext();
        auto inspectorAgent = makeUnique<InspectorAgent>(context);
        m_inspectorAgent = inspectorAgent.get();
        m_agents.append(WTFMove(inspectorAgent));
    }
    return *m_inspectorAgent;
}

InspectorDebuggerAgent& JSGlobalObjectInspectorController::ensureDebuggerAgent()
{
    if (!m_debuggerAgent) {
        auto context = jsAgentContext();
        auto debuggerAgent = makeUnique<JSGlobalObjectDebuggerAgent>(context, m_consoleAgent);
        m_debuggerAgent = debuggerAgent.get();
        m_consoleClient->setInspectorDebuggerAgent(m_debuggerAgent);
        m_agents.append(WTFMove(debuggerAgent));
    }
    return *m_debuggerAgent;
}

JSAgentContext JSGlobalObjectInspectorController::jsAgentContext()
{
    AgentContext baseContext = {
        *this,
        *m_injectedScriptManager,
        m_frontendRouter.get(),
        m_backendDispatcher.get()
    };

    JSAgentContext context = {
        baseContext,
        m_globalObject
    };

    return context;
}

void JSGlobalObjectInspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;

    auto context = jsAgentContext();

    ensureInspectorAgent();

    m_agents.append(makeUnique<JSGlobalObjectRuntimeAgent>(context));

    ensureDebuggerAgent();

    auto scriptProfilerAgentPtr = makeUnique<InspectorScriptProfilerAgent>(context);
    m_consoleClient->setInspectorScriptProfilerAgent(scriptProfilerAgentPtr.get());
    m_agents.append(WTFMove(scriptProfilerAgentPtr));

    auto heapAgent = makeUnique<InspectorHeapAgent>(context);
    if (m_consoleAgent)
        m_consoleAgent->setInspectorHeapAgent(heapAgent.get());
    m_agents.append(WTFMove(heapAgent));

    m_agents.append(makeUnique<JSGlobalObjectAuditAgent>(context));
}

} // namespace Inspector
