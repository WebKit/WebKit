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

#include "config.h"
#include "JSGlobalObjectInspectorController.h"

#include "Completion.h"
#include "ConsoleMessage.h"
#include "ErrorHandlingScope.h"
#include "Exception.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorAgent.h"
#include "InspectorBackendDispatcher.h"
#include "InspectorFrontendChannel.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectConsoleAgent.h"
#include "JSGlobalObjectConsoleClient.h"
#include "JSGlobalObjectDebuggerAgent.h"
#include "JSGlobalObjectRuntimeAgent.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include <wtf/Stopwatch.h>

#include <cxxabi.h>
#if OS(DARWIN) || (OS(LINUX) && !PLATFORM(GTK))
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if ENABLE(REMOTE_INSPECTOR)
#include "JSGlobalObjectDebuggable.h"
#include "RemoteInspector.h"
#endif

using namespace JSC;

namespace Inspector {

JSGlobalObjectInspectorController::JSGlobalObjectInspectorController(JSGlobalObject& globalObject)
    : m_globalObject(globalObject)
    , m_injectedScriptManager(std::make_unique<InjectedScriptManager>(*this, InjectedScriptHost::create()))
    , m_frontendChannel(nullptr)
    , m_executionStopwatch(Stopwatch::create())
    , m_includeNativeCallStackWithExceptions(true)
    , m_isAutomaticInspection(false)
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    , m_augmentingClient(nullptr)
#endif
{
    auto inspectorAgent = std::make_unique<InspectorAgent>(*this);
    auto runtimeAgent = std::make_unique<JSGlobalObjectRuntimeAgent>(m_injectedScriptManager.get(), m_globalObject);
    auto consoleAgent = std::make_unique<JSGlobalObjectConsoleAgent>(m_injectedScriptManager.get());
    auto debuggerAgent = std::make_unique<JSGlobalObjectDebuggerAgent>(m_injectedScriptManager.get(), m_globalObject, consoleAgent.get());

    m_inspectorAgent = inspectorAgent.get();
    m_debuggerAgent = debuggerAgent.get();
    m_consoleAgent = consoleAgent.get();
    m_consoleClient = std::make_unique<JSGlobalObjectConsoleClient>(m_consoleAgent);

    runtimeAgent->setScriptDebugServer(&debuggerAgent->scriptDebugServer());

    m_agents.append(WTF::move(inspectorAgent));
    m_agents.append(WTF::move(runtimeAgent));
    m_agents.append(WTF::move(consoleAgent));
    m_agents.append(WTF::move(debuggerAgent));

    m_executionStopwatch->start();
}

JSGlobalObjectInspectorController::~JSGlobalObjectInspectorController()
{
    m_agents.discardAgents();
}

void JSGlobalObjectInspectorController::globalObjectDestroyed()
{
    disconnectFrontend(DisconnectReason::InspectedTargetDestroyed);

    m_injectedScriptManager->disconnect();
}

void JSGlobalObjectInspectorController::connectFrontend(FrontendChannel* frontendChannel, bool isAutomaticInspection)
{
    ASSERT(!m_frontendChannel);
    ASSERT(!m_backendDispatcher);

    m_isAutomaticInspection = isAutomaticInspection;

    m_frontendChannel = frontendChannel;
    m_backendDispatcher = BackendDispatcher::create(frontendChannel);

    m_agents.didCreateFrontendAndBackend(frontendChannel, m_backendDispatcher.get());

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    m_inspectorAgent->activateExtraDomains(m_agents.extraDomains());

    if (m_augmentingClient)
        m_augmentingClient->inspectorConnected();
#endif
}

void JSGlobalObjectInspectorController::disconnectFrontend(DisconnectReason reason)
{
    if (!m_frontendChannel)
        return;

    m_agents.willDestroyFrontendAndBackend(reason);

    m_backendDispatcher->clearFrontend();
    m_backendDispatcher = nullptr;
    m_frontendChannel = nullptr;

    m_isAutomaticInspection = false;

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    if (m_augmentingClient)
        m_augmentingClient->inspectorDisconnected();
#endif
}

void JSGlobalObjectInspectorController::dispatchMessageFromFrontend(const String& message)
{
    if (m_backendDispatcher)
        m_backendDispatcher->dispatch(message);
}

void JSGlobalObjectInspectorController::pause()
{
    if (!m_frontendChannel)
        return;

    ErrorString dummyError;
    m_debuggerAgent->enable(dummyError);
    m_debuggerAgent->pause(dummyError);
}

void JSGlobalObjectInspectorController::appendAPIBacktrace(ScriptCallStack* callStack)
{
#if OS(DARWIN) || (OS(LINUX) && !PLATFORM(GTK))
    static const int framesToShow = 31;
    static const int framesToSkip = 3; // WTFGetBacktrace, appendAPIBacktrace, reportAPIException.

    void* samples[framesToShow + framesToSkip];
    int frames = framesToShow + framesToSkip;
    WTFGetBacktrace(samples, &frames);

    void** stack = samples + framesToSkip;
    int size = frames - framesToSkip;
    for (int i = 0; i < size; ++i) {
        const char* mangledName = nullptr;
        char* cxaDemangled = nullptr;
        Dl_info info;
        if (dladdr(stack[i], &info) && info.dli_sname)
            mangledName = info.dli_sname;
        if (mangledName)
            cxaDemangled = abi::__cxa_demangle(mangledName, nullptr, nullptr, nullptr);
        if (mangledName || cxaDemangled)
            callStack->append(ScriptCallFrame(cxaDemangled ? cxaDemangled : mangledName, ASCIILiteral("[native code]"), 0, 0));
        else
            callStack->append(ScriptCallFrame(ASCIILiteral("?"), ASCIILiteral("[native code]"), 0, 0));
        free(cxaDemangled);
    }
#else
    UNUSED_PARAM(callStack);
#endif
}

void JSGlobalObjectInspectorController::reportAPIException(ExecState* exec, Exception* exception)
{
    if (isTerminatedExecutionException(exception))
        return;

    ErrorHandlingScope errorScope(exec->vm());

    RefPtr<ScriptCallStack> callStack = createScriptCallStackFromException(exec, exception, ScriptCallStack::maxCallStackSizeToCapture);
    if (includesNativeCallStackWhenReportingExceptions())
        appendAPIBacktrace(callStack.get());

    // FIXME: <http://webkit.org/b/115087> Web Inspector: Should not evaluate JavaScript handling exceptions
    // If this is a custom exception object, call toString on it to try and get a nice string representation for the exception.
    String errorMessage = exception->value().toString(exec)->value(exec);
    exec->clearException();

    if (JSGlobalObjectConsoleClient::logToSystemConsole()) {
        if (callStack->size()) {
            const ScriptCallFrame& callFrame = callStack->at(0);
            ConsoleClient::printConsoleMessage(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, callFrame.sourceURL(), callFrame.lineNumber(), callFrame.columnNumber());
        } else
            ConsoleClient::printConsoleMessage(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, String(), 0, 0);
    }

    m_consoleAgent->addMessageToConsole(std::make_unique<ConsoleMessage>(MessageSource::JS, MessageType::Log, MessageLevel::Error, errorMessage, callStack));
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
#if ENABLE(REMOTE_INSPECTOR)
    if (m_isAutomaticInspection)
        m_globalObject.inspectorDebuggable().unpauseForInitializedInspector();
#endif
}

Ref<Stopwatch> JSGlobalObjectInspectorController::executionStopwatch()
{
    return m_executionStopwatch.copyRef();
}

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
void JSGlobalObjectInspectorController::appendExtraAgent(std::unique_ptr<InspectorAgentBase> agent)
{
    String domainName = agent->domainName();

    if (m_frontendChannel)
        agent->didCreateFrontendAndBackend(m_frontendChannel, m_backendDispatcher.get());

    m_agents.appendExtraAgent(WTF::move(agent));

    if (m_frontendChannel)
        m_inspectorAgent->activateExtraDomain(domainName);
}
#endif

} // namespace Inspector

