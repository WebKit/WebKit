/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameRuntimeAgent.h"

#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "FrameConsoleClient.h"
#include "FrameLoader.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindowCustom.h"
#include "JSExecState.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "UserGestureEmulationScope.h"
#include "WindowProxy.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameRuntimeAgent);

FrameRuntimeAgent::FrameRuntimeAgent(FrameAgentContext& context)
    : InspectorRuntimeAgent(context)
    , m_frontendDispatcher(makeUniqueRef<RuntimeFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(RuntimeBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedFrame(context.inspectedFrame)
    , m_frameIdentifier(context.inspectedFrame->frameID())
{
}

FrameRuntimeAgent::~FrameRuntimeAgent() = default;

CommandResult<void> FrameRuntimeAgent::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameRuntimeAgent() == this)
        return { };

    auto result = InspectorRuntimeAgent::enable();
    if (!result)
        return result;

    reportExecutionContextCreation();
    agents->setEnabledFrameRuntimeAgent(this);

    return result;
}

CommandResult<void> FrameRuntimeAgent::disable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameRuntimeAgent(nullptr);

    return InspectorRuntimeAgent::disable();
}

CommandResultOf<Ref<Inspector::Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */> FrameRuntimeAgent::evaluate(const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&& executionContextId, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture)
{
    Inspector::Protocol::ErrorString errorString;

    auto injectedScript = injectedScriptForEval(errorString, WTF::move(executionContextId));
    if (injectedScript.hasNoValue())
        return makeUnexpected(errorString);

    RefPtr frame = m_inspectedFrame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    if (!page)
        return makeUnexpected("Frame or page not available"_s);

    UserGestureEmulationScope userGestureScope(*page, emulateUserGesture.value_or(false), dynamicDowncast<Document>(executionContext(injectedScript.globalObject())));
    return InspectorRuntimeAgent::evaluate(injectedScript, expression, objectGroup, WTF::move(includeCommandLineAPI), WTF::move(doNotPauseOnExceptionsAndMuteConsole), WTF::move(returnByValue), WTF::move(generatePreview), WTF::move(saveResult), WTF::move(emulateUserGesture));
}

void FrameRuntimeAgent::callFunctionOn(const Inspector::Protocol::Runtime::RemoteObjectId& objectId, const String& expression, RefPtr<JSON::Array>&& optionalArguments, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& emulateUserGesture, std::optional<bool>&& awaitPromise, Ref<CallFunctionOnCallback>&& callback)
{
    auto injectedScript = injectedScriptManager().injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        callback->sendFailure("Missing injected script for given objectId"_s);
        return;
    }

    RefPtr frame = m_inspectedFrame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    if (!page) {
        callback->sendFailure("Frame or page not available"_s);
        return;
    }

    UserGestureEmulationScope userGestureScope(*page, emulateUserGesture.value_or(false), dynamicDowncast<Document>(executionContext(injectedScript.globalObject())));
    return InspectorRuntimeAgent::callFunctionOn(objectId, expression, WTF::move(optionalArguments), WTF::move(doNotPauseOnExceptionsAndMuteConsole), WTF::move(returnByValue), WTF::move(generatePreview), WTF::move(emulateUserGesture), WTF::move(awaitPromise), WTF::move(callback));
}

InjectedScript FrameRuntimeAgent::injectedScriptForEval(Inspector::Protocol::ErrorString& errorString, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    if (!executionContextId) {
        RefPtr frame = m_inspectedFrame.get();
        if (!frame)
            return InjectedScript();

        Ref protectedFrame = *frame;
        CheckedRef script = protectedFrame->script();
        auto* globalObject = script->globalObject(mainThreadNormalWorldSingleton());
        if (!globalObject)
            return InjectedScript();

        auto result = injectedScriptManager().injectedScriptFor(globalObject);
        if (result.hasNoValue())
            errorString = "Internal error: main world execution context not found"_s;
        return result;
    }

    auto injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId"_s;
    return injectedScript;
}

void FrameRuntimeAgent::muteConsole()
{
    FrameConsoleClient::mute();
}

void FrameRuntimeAgent::unmuteConsole()
{
    FrameConsoleClient::unmute();
}

String FrameRuntimeAgent::frameIdForProtocol() const
{
    return makeString("frame-"_s, m_frameIdentifier.toUInt64());
}

static Inspector::Protocol::Runtime::ExecutionContextType NODELETE toProtocol(DOMWrapperWorld::Type type)
{
    switch (type) {
    case DOMWrapperWorld::Type::Normal:
        return Inspector::Protocol::Runtime::ExecutionContextType::Normal;
    case DOMWrapperWorld::Type::User:
        return Inspector::Protocol::Runtime::ExecutionContextType::User;
    case DOMWrapperWorld::Type::Internal:
        return Inspector::Protocol::Runtime::ExecutionContextType::Internal;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Runtime::ExecutionContextType::Internal;
}

void FrameRuntimeAgent::reportExecutionContextCreation()
{
    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    Ref protectedFrame = *frame;
    CheckedRef script = protectedFrame->script();

    if (!script->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return;

    // Don't report contexts if we're still on about:blank - wait for didClearWindowObjectInWorld after navigation
    RefPtr document = protectedFrame->document();
    if (!document || document->url().isAboutBlank())
        return;

    // Always send the main world first
    auto* mainGlobalObjectPtr = script->globalObject(mainThreadNormalWorldSingleton());
    if (!mainGlobalObjectPtr)
        return;

    auto& mainGlobalObject = *mainGlobalObjectPtr;
    notifyContextCreated(&mainGlobalObject, mainThreadNormalWorldSingleton());

    // Then iterate isolated worlds
    Ref windowProxy = protectedFrame->windowProxy();
    for (auto& jsWindowProxy : windowProxy->jsWindowProxiesAsVector()) {
        auto* globalObject = jsWindowProxy->window();
        if (globalObject == &mainGlobalObject)
            continue;

        RefPtr domDocument = downcast<LocalDOMWindow>(jsWindowProxy->wrapped()).document();
        if (!domDocument)
            continue;

        Ref securityOrigin = domDocument->securityOrigin();
        notifyContextCreated(globalObject, jsWindowProxy->world(), securityOrigin.ptr());
    }
}

void FrameRuntimeAgent::notifyContextCreated(JSC::JSGlobalObject* globalObject, const DOMWrapperWorld& world, SecurityOrigin* securityOrigin)
{
    auto injectedScript = injectedScriptManager().injectedScriptFor(globalObject);
    if (injectedScript.hasNoValue())
        return;

    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    auto name = world.name();
    if (name.isEmpty()) {
        if (securityOrigin)
            name = securityOrigin->toRawString();
        else if (RefPtr document = frame->document())
            name = document->url().string();
    }

    auto frameId = frameIdForProtocol();
    auto contextId = injectedScriptManager().injectedScriptIdFor(globalObject);

    m_frontendDispatcher->executionContextCreated(Inspector::Protocol::Runtime::ExecutionContextDescription::create()
        .setId(contextId)
        .setType(toProtocol(world.type()))
        .setName(name)
        .setFrameId(frameId)
        .release());
}

void FrameRuntimeAgent::didClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    Ref protectedFrame = *frame;
    CheckedRef script = protectedFrame->script();
    auto* globalObject = script->globalObject(world);
    if (!globalObject)
        return;

    notifyContextCreated(globalObject, world);
}

} // namespace WebCore
