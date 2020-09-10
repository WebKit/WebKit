/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "PageRuntimeAgent.h"

#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindowBase.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "ScriptController.h"
#include "ScriptState.h"
#include "SecurityOrigin.h"
#include "UserGestureEmulationScope.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>

namespace WebCore {

using namespace Inspector;

PageRuntimeAgent::PageRuntimeAgent(PageAgentContext& context)
    : InspectorRuntimeAgent(context)
    , m_frontendDispatcher(makeUnique<Inspector::RuntimeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::RuntimeBackendDispatcher::create(context.backendDispatcher, this))
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedPage(context.inspectedPage)
{
}

PageRuntimeAgent::~PageRuntimeAgent() = default;

Protocol::ErrorStringOr<void> PageRuntimeAgent::enable()
{
    if (m_instrumentingAgents.enabledPageRuntimeAgent() == this)
        return { };

    auto result = InspectorRuntimeAgent::enable();
    if (!result)
        return result;

    // Report initial contexts before enabling instrumentation as the reporting
    // can force creation of script state which could result in duplicate notifications.
    reportExecutionContextCreation();

    m_instrumentingAgents.setEnabledPageRuntimeAgent(this);

    return result;
}

Protocol::ErrorStringOr<void> PageRuntimeAgent::disable()
{
    m_instrumentingAgents.setEnabledPageRuntimeAgent(nullptr);

    return InspectorRuntimeAgent::disable();
}

void PageRuntimeAgent::frameNavigated(Frame& frame)
{
    // Ensure execution context is created for the frame even if it doesn't have scripts.
    mainWorldExecState(&frame);
}

void PageRuntimeAgent::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent)
        return;

    notifyContextCreated(pageAgent->frameId(&frame), frame.script().globalObject(world), world);
}

InjectedScript PageRuntimeAgent::injectedScriptForEval(Protocol::ErrorString& errorString, Optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    if (!executionContextId) {
        JSC::JSGlobalObject* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
        InjectedScript result = injectedScriptManager().injectedScriptFor(scriptState);
        if (result.hasNoValue())
            errorString = "Internal error: main world execution context not found"_s;
        return result;
    }

    InjectedScript injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId"_s;
    return injectedScript;
}

void PageRuntimeAgent::muteConsole()
{
    PageConsoleClient::mute();
}

void PageRuntimeAgent::unmuteConsole()
{
    PageConsoleClient::unmute();
}

void PageRuntimeAgent::reportExecutionContextCreation()
{
    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent)
        return;

    for (auto* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
            continue;

        auto frameId = pageAgent->frameId(frame);

        // Always send the main world first.
        auto* mainGlobalObject = mainWorldExecState(frame);
        notifyContextCreated(frameId, mainGlobalObject, mainThreadNormalWorld());

        for (auto& jsWindowProxy : frame->windowProxy().jsWindowProxiesAsVector()) {
            auto* globalObject = jsWindowProxy->window();
            if (globalObject == mainGlobalObject)
                continue;

            auto& securityOrigin = downcast<DOMWindow>(jsWindowProxy->wrapped()).document()->securityOrigin();
            notifyContextCreated(frameId, globalObject, jsWindowProxy->world(), &securityOrigin);
        }
    }
}

static Protocol::Runtime::ExecutionContextType toProtocol(DOMWrapperWorld::Type type)
{
    switch (type) {
    case DOMWrapperWorld::Type::Normal:
        return Protocol::Runtime::ExecutionContextType::Normal;
    case DOMWrapperWorld::Type::User:
        return Protocol::Runtime::ExecutionContextType::User;
    case DOMWrapperWorld::Type::Internal:
        return Protocol::Runtime::ExecutionContextType::Internal;
    }

    ASSERT_NOT_REACHED();
    return Protocol::Runtime::ExecutionContextType::Internal;
}

void PageRuntimeAgent::notifyContextCreated(const Protocol::Network::FrameId& frameId, JSC::JSGlobalObject* globalObject, const DOMWrapperWorld& world, SecurityOrigin* securityOrigin)
{
    auto injectedScript = injectedScriptManager().injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return;

    auto name = world.name();
    if (name.isEmpty() && securityOrigin)
        name = securityOrigin->toRawString();

    m_frontendDispatcher->executionContextCreated(Protocol::Runtime::ExecutionContextDescription::create()
        .setId(injectedScriptManager().injectedScriptIdFor(globalObject))
        .setType(toProtocol(world.type()))
        .setName(name)
        .setFrameId(frameId)
        .release());
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */, Optional<int> /* savedResultIndex */>> PageRuntimeAgent::evaluate(const String& expression, const String& objectGroup, Optional<bool>&& includeCommandLineAPI, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<Protocol::Runtime::ExecutionContextId>&& executionContextId, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& saveResult, Optional<bool>&& emulateUserGesture)
{
    UserGestureEmulationScope userGestureScope(m_inspectedPage, emulateUserGesture && *emulateUserGesture);
    return InspectorRuntimeAgent::evaluate(expression, objectGroup, WTFMove(includeCommandLineAPI), WTFMove(doNotPauseOnExceptionsAndMuteConsole), WTFMove(executionContextId), WTFMove(returnByValue), WTFMove(generatePreview), WTFMove(saveResult), WTFMove(emulateUserGesture));
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */>> PageRuntimeAgent::callFunctionOn(const Protocol::Runtime::RemoteObjectId& objectId, const String& expression, RefPtr<JSON::Array>&& optionalArguments, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& emulateUserGesture)
{
    UserGestureEmulationScope userGestureScope(m_inspectedPage, emulateUserGesture && *emulateUserGesture);
    return InspectorRuntimeAgent::callFunctionOn(objectId, expression, WTFMove(optionalArguments), WTFMove(doNotPauseOnExceptionsAndMuteConsole), WTFMove(returnByValue), WTFMove(generatePreview), WTFMove(emulateUserGesture));
}

} // namespace WebCore
