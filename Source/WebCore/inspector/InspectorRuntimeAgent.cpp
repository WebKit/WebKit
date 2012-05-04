/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR)

#include "InspectorRuntimeAgent.h"

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "WorkerContext.h"
#include "WorkerDebuggerAgent.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"
#include <wtf/PassRefPtr.h>


#if ENABLE(JAVASCRIPT_DEBUGGER)
#include "ScriptDebugServer.h"
#endif

namespace WebCore {

static bool asBool(const bool* const b)
{
    return b ? *b : false;
}

InspectorRuntimeAgent::InspectorRuntimeAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorRuntimeAgent>("Runtime", instrumentingAgents, state)
    , m_injectedScriptManager(injectedScriptManager)
#if ENABLE(JAVASCRIPT_DEBUGGER)
    , m_scriptDebugServer(0)
#endif
    , m_paused(false)
{
    m_instrumentingAgents->setInspectorRuntimeAgent(this);
}

InspectorRuntimeAgent::~InspectorRuntimeAgent()
{
    m_instrumentingAgents->setInspectorRuntimeAgent(0);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
static ScriptDebugServer::PauseOnExceptionsState setPauseOnExceptionsState(ScriptDebugServer* scriptDebugServer, ScriptDebugServer::PauseOnExceptionsState newState)
{
    ASSERT(scriptDebugServer);
    ScriptDebugServer::PauseOnExceptionsState presentState = scriptDebugServer->pauseOnExceptionsState();
    if (presentState != newState)
        scriptDebugServer->setPauseOnExceptionsState(newState);
    return presentState;
}
#endif

void InspectorRuntimeAgent::evaluate(ErrorString* errorString, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const String* const frameId, const bool* const returnByValue, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptState* scriptState = scriptStateForEval(errorString, frameId);
    if (!scriptState)
        return;
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
#if ENABLE(JAVASCRIPT_DEBUGGER)
    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
#endif
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.evaluate(errorString, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
#endif
    }
}

void InspectorRuntimeAgent::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const RefPtr<InspectorArray>* const optionalArguments, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    String arguments;
    if (optionalArguments)
        arguments = (*optionalArguments)->toJSONString();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
#endif
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.callFunctionOn(errorString, objectId, expression, arguments, asBool(returnByValue), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
#if ENABLE(JAVASCRIPT_DEBUGGER)
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
#endif
    }
}

void InspectorRuntimeAgent::getProperties(ErrorString* errorString, const String& objectId, const bool* const ownProperties, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor> >& result)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

#if ENABLE(JAVASCRIPT_DEBUGGER)
    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
#endif
    muteConsole();

    injectedScript.getProperties(errorString, objectId, ownProperties ? *ownProperties : false, &result);

    unmuteConsole();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
#endif
}

void InspectorRuntimeAgent::releaseObject(ErrorString*, const String& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (!injectedScript.hasNoValue())
        injectedScript.releaseObject(objectId);
}

void InspectorRuntimeAgent::releaseObjectGroup(ErrorString*, const String& objectGroup)
{
    m_injectedScriptManager->releaseObjectGroup(objectGroup);
}

void InspectorRuntimeAgent::run(ErrorString*)
{
    m_paused = false;
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorRuntimeAgent::setScriptDebugServer(ScriptDebugServer* scriptDebugServer)
{
    m_scriptDebugServer = scriptDebugServer;
}

#if ENABLE(WORKERS)
void InspectorRuntimeAgent::pauseWorkerContext(WorkerContext* context)
{
    m_paused = true;
    MessageQueueWaitResult result;
    do {
        result = context->thread()->runLoop().runInMode(context, WorkerDebuggerAgent::debuggerTaskMode);
    // Keep waiting until execution is resumed.
    } while (result == MessageQueueMessageReceived && m_paused);
}
#endif // ENABLE(WORKERS)
#endif // ENABLE(JAVASCRIPT_DEBUGGER)

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
