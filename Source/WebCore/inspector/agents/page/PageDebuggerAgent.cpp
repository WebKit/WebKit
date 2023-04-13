/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "PageDebuggerAgent.h"

#include "CachedResource.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "JSLocalDOMWindowCustom.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageDebugger.h"
#include "ScriptExecutionContext.h"
#include "UserGestureEmulationScope.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace Inspector;

PageDebuggerAgent::PageDebuggerAgent(PageAgentContext& context)
    : WebDebuggerAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageDebuggerAgent::~PageDebuggerAgent() = default;

bool PageDebuggerAgent::enabled() const
{
    return m_instrumentingAgents.enabledPageDebuggerAgent() == this && WebDebuggerAgent::enabled();
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */>> PageDebuggerAgent::evaluateOnCallFrame(const Protocol::Debugger::CallFrameId& callFrameId, const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture)
{
    auto injectedScript = injectedScriptManager().injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given callFrameId"_s);

    UserGestureEmulationScope userGestureScope(m_inspectedPage, emulateUserGesture.value_or(false), dynamicDowncast<Document>(executionContext(injectedScript.globalObject())));
    return WebDebuggerAgent::evaluateOnCallFrame(injectedScript, callFrameId, expression, objectGroup, WTFMove(includeCommandLineAPI), WTFMove(doNotPauseOnExceptionsAndMuteConsole), WTFMove(returnByValue), WTFMove(generatePreview), WTFMove(saveResult), WTFMove(emulateUserGesture));
}

void PageDebuggerAgent::internalEnable()
{
    m_instrumentingAgents.setEnabledPageDebuggerAgent(this);

    WebDebuggerAgent::internalEnable();
}

void PageDebuggerAgent::internalDisable(bool isBeingDestroyed)
{
    m_instrumentingAgents.setEnabledPageDebuggerAgent(nullptr);

    WebDebuggerAgent::internalDisable(isBeingDestroyed);
}

String PageDebuggerAgent::sourceMapURLForScript(const JSC::Debugger::Script& script)
{
    static constexpr auto sourceMapHTTPHeader = "SourceMap"_s;
    static constexpr auto sourceMapHTTPHeaderDeprecated = "X-SourceMap"_s;

    if (!script.url.isEmpty()) {
        auto* localMainFrame = dynamicDowncast<LocalFrame>(m_inspectedPage.mainFrame());
        if (!localMainFrame)
            return String();

        CachedResource* resource = InspectorPageAgent::cachedResource(localMainFrame, URL({ }, script.url));
        if (resource) {
            String sourceMapHeader = resource->response().httpHeaderField(StringView { sourceMapHTTPHeader });
            if (!sourceMapHeader.isEmpty())
                return sourceMapHeader;

            sourceMapHeader = resource->response().httpHeaderField(StringView { sourceMapHTTPHeaderDeprecated });
            if (!sourceMapHeader.isEmpty())
                return sourceMapHeader;
        }
    }

    return InspectorDebuggerAgent::sourceMapURLForScript(script);
}

void PageDebuggerAgent::muteConsole()
{
    PageConsoleClient::mute();
}

void PageDebuggerAgent::unmuteConsole()
{
    PageConsoleClient::unmute();
}

void PageDebuggerAgent::debuggerWillEvaluate(JSC::Debugger&, JSC::JSGlobalObject* globalObject, const JSC::Breakpoint::Action& action)
{
    m_breakpointActionUserGestureEmulationScopeStack.append(makeUniqueRef<UserGestureEmulationScope>(m_inspectedPage, action.emulateUserGesture, dynamicDowncast<Document>(executionContext(globalObject))));
}

void PageDebuggerAgent::debuggerDidEvaluate(JSC::Debugger&, JSC::JSGlobalObject*, const JSC::Breakpoint::Action&)
{
    m_breakpointActionUserGestureEmulationScopeStack.removeLast();
}

void PageDebuggerAgent::breakpointActionLog(JSC::JSGlobalObject* lexicalGlobalObject, const String& message)
{
    m_inspectedPage.console().addMessage(MessageSource::JS, MessageLevel::Log, message, createScriptCallStack(lexicalGlobalObject));
}

InjectedScript PageDebuggerAgent::injectedScriptForEval(Protocol::ErrorString& errorString, std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_inspectedPage.mainFrame());
    if (!localMainFrame)
        return InjectedScript();

    if (!executionContextId)
        return injectedScriptManager().injectedScriptFor(&mainWorldGlobalObject(*localMainFrame));

    InjectedScript injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId."_s;

    return injectedScript;
}

void PageDebuggerAgent::didClearWindowObjectInWorld(LocalFrame& frame, DOMWrapperWorld& world)
{
    if (!frame.isMainFrame() || &world != &mainThreadNormalWorld())
        return;

    didClearGlobalObject();
}

void PageDebuggerAgent::mainFrameStartedLoading()
{
    if (isPaused()) {
        setSuppressAllPauses(true);

        resume();
    }
}

void PageDebuggerAgent::mainFrameStoppedLoading()
{
    setSuppressAllPauses(false);
}

void PageDebuggerAgent::mainFrameNavigated()
{
    setSuppressAllPauses(false);
}

void PageDebuggerAgent::didRequestAnimationFrame(int callbackId, Document& document)
{
    if (!breakpointsActive())
        return;

    auto* globalObject = document.globalObject();
    if (!globalObject)
        return;

    didScheduleAsyncCall(globalObject, InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId, true);
}

void PageDebuggerAgent::willFireAnimationFrame(int callbackId)
{
    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId);
}

void PageDebuggerAgent::didCancelAnimationFrame(int callbackId)
{
    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId);
}

void PageDebuggerAgent::didFireAnimationFrame(int callbackId)
{
    didDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId);
}

} // namespace WebCore
