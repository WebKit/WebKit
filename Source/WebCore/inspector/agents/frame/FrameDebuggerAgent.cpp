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
#include "FrameDebuggerAgent.h"
#include "DocumentPage.h"

#include "CachedResource.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FrameConsoleClient.h"
#include "InspectorResourceUtilities.h"
#include "InstrumentingAgents.h"
#include "JSDOMWindowCustom.h"
#include "JSExecState.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "UserGestureEmulationScope.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDebuggerAgent);

FrameDebuggerAgent::FrameDebuggerAgent(FrameAgentContext& context)
    : WebDebuggerAgent(context)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameDebuggerAgent::~FrameDebuggerAgent() = default;

bool FrameDebuggerAgent::enabled() const
{
    auto instrumentingAgents = Ref { m_instrumentingAgents.get() };
    return instrumentingAgents->enabledFrameDebuggerAgent() == this && WebDebuggerAgent::enabled();
}

CommandResultOf<Ref<Inspector::Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */> FrameDebuggerAgent::evaluateOnCallFrame(const Inspector::Protocol::Debugger::CallFrameId& callFrameId, const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture)
{
    auto injectedScript = injectedScriptManager().injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given callFrameId"_s);

    RefPtr frame = m_inspectedFrame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    if (!page)
        return makeUnexpected("Frame or page not available"_s);

    UserGestureEmulationScope userGestureScope(*page, emulateUserGesture.value_or(false), dynamicDowncast<Document>(executionContext(injectedScript.globalObject())));
    return WebDebuggerAgent::evaluateOnCallFrame(injectedScript, callFrameId, expression, objectGroup, WTF::move(includeCommandLineAPI), WTF::move(doNotPauseOnExceptionsAndMuteConsole), WTF::move(returnByValue), WTF::move(generatePreview), WTF::move(saveResult), WTF::move(emulateUserGesture));
}

void FrameDebuggerAgent::internalEnable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameDebuggerAgent(this);

    WebDebuggerAgent::internalEnable();
}

void FrameDebuggerAgent::internalDisable(bool isBeingDestroyed)
{
    Ref { m_instrumentingAgents.get() }->setEnabledFrameDebuggerAgent(nullptr);

    WebDebuggerAgent::internalDisable(isBeingDestroyed);
}

String FrameDebuggerAgent::sourceMapURLForScript(const JSC::Debugger::Script& script)
{
    if (script.url.isEmpty())
        return InspectorDebuggerAgent::sourceMapURLForScript(script);

    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return InspectorDebuggerAgent::sourceMapURLForScript(script);

    RefPtr resource = ResourceUtilities::cachedResource(frame.get(), URL({ }, script.url));
    if (!resource)
        return InspectorDebuggerAgent::sourceMapURLForScript(script);

    String sourceMapHeader = resource->response().httpHeaderField("SourceMap"_s);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    sourceMapHeader = resource->response().httpHeaderField("X-SourceMap"_s);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    return InspectorDebuggerAgent::sourceMapURLForScript(script);
}

void FrameDebuggerAgent::muteConsole()
{
    FrameConsoleClient::mute();
}

void FrameDebuggerAgent::unmuteConsole()
{
    FrameConsoleClient::unmute();
}

void FrameDebuggerAgent::debuggerWillEvaluate(JSC::Debugger&, JSC::JSGlobalObject* globalObject, const JSC::Breakpoint::Action& action)
{
    RefPtr frame = m_inspectedFrame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    if (!page)
        return;

    m_breakpointActionUserGestureEmulationScopeStack.append(makeUniqueRef<UserGestureEmulationScope>(*page, action.emulateUserGesture, dynamicDowncast<Document>(executionContext(globalObject))));
}

void FrameDebuggerAgent::debuggerDidEvaluate(JSC::Debugger&, JSC::JSGlobalObject*, const JSC::Breakpoint::Action&)
{
    m_breakpointActionUserGestureEmulationScopeStack.removeLast();
}

void FrameDebuggerAgent::breakpointActionLog(JSC::JSGlobalObject* lexicalGlobalObject, const String& message)
{
    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    frame->console().addMessage(MessageSource::JS, MessageLevel::Log, message, createScriptCallStack(lexicalGlobalObject));
}

InjectedScript FrameDebuggerAgent::injectedScriptForEval(Inspector::Protocol::ErrorString& errorString, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    if (!executionContextId) {
        RefPtr frame = m_inspectedFrame.get();
        if (!frame)
            return InjectedScript();

        Ref protectedFrame = *frame;
        Ref world = mainThreadNormalWorldSingleton();
        CheckedRef script = protectedFrame->script();
        auto* globalObject = script->globalObject(world);
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

void FrameDebuggerAgent::didClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorldSingleton())
        return;

    RefPtr frame = m_inspectedFrame.get();
    if (!frame)
        return;

    // Reattach the debugger to the new globalObject after navigation.
    Ref normalWorld = mainThreadNormalWorldSingleton();
    CheckedRef script = frame->script();
    auto* globalObject = script->globalObject(normalWorld);
    if (!globalObject) {
        didClearGlobalObject();
        return;
    }

    // The globalObject may already have a debugger attached (e.g., by initScriptForWindowProxy
    // in non-site-isolation configurations). Under site isolation, FrameDebugger is the sole
    // debugger, but detach any stale debugger as defense-in-depth.
    if (auto* existingDebugger = globalObject->debugger()) {
        if (existingDebugger == &debugger()) {
            didClearGlobalObject();
            return;
        }
        existingDebugger->detach(globalObject, JSC::Debugger::TerminatingDebuggingSession);
    }
    debugger().attach(globalObject);

    didClearGlobalObject();
}

} // namespace WebCore
