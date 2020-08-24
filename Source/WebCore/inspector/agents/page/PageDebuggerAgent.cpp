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
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageScriptDebugServer.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "UserGestureEmulationScope.h"
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>

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

void PageDebuggerAgent::evaluateOnCallFrame(ErrorString& errorString, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* emulateUserGesture, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    auto shouldEmulateUserGesture = emulateUserGesture && *emulateUserGesture;
    UserGestureEmulationScope userGestureScope(m_inspectedPage, shouldEmulateUserGesture);

    WebDebuggerAgent::evaluateOnCallFrame(errorString, callFrameId, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, saveResult, emulateUserGesture, result, wasThrown, savedResultIndex);
}

void PageDebuggerAgent::enable()
{
    m_instrumentingAgents.setEnabledPageDebuggerAgent(this);

    WebDebuggerAgent::enable();
}

void PageDebuggerAgent::disable(bool isBeingDestroyed)
{
    m_instrumentingAgents.setEnabledPageDebuggerAgent(nullptr);

    WebDebuggerAgent::disable(isBeingDestroyed);
}

String PageDebuggerAgent::sourceMapURLForScript(const JSC::Debugger::Script& script)
{
    static NeverDestroyed<String> sourceMapHTTPHeader(MAKE_STATIC_STRING_IMPL("SourceMap"));
    static NeverDestroyed<String> sourceMapHTTPHeaderDeprecated(MAKE_STATIC_STRING_IMPL("X-SourceMap"));

    if (!script.url.isEmpty()) {
        CachedResource* resource = InspectorPageAgent::cachedResource(&m_inspectedPage.mainFrame(), URL({ }, script.url));
        if (resource) {
            String sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeader);
            if (!sourceMapHeader.isEmpty())
                return sourceMapHeader;

            sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeaderDeprecated);
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

void PageDebuggerAgent::breakpointActionLog(JSC::JSGlobalObject* lexicalGlobalObject, const String& message)
{
    m_inspectedPage.console().addMessage(MessageSource::JS, MessageLevel::Log, message, createScriptCallStack(lexicalGlobalObject));
}

InjectedScript PageDebuggerAgent::injectedScriptForEval(ErrorString& errorString, const int* executionContextId)
{
    if (!executionContextId) {
        JSC::JSGlobalObject* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
        return injectedScriptManager().injectedScriptFor(scriptState);
    }

    InjectedScript injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId."_s;

    return injectedScript;
}

void PageDebuggerAgent::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (!frame.isMainFrame() || &world != &mainThreadNormalWorld())
        return;

    didClearGlobalObject();
}

void PageDebuggerAgent::mainFrameStartedLoading()
{
    if (isPaused()) {
        setSuppressAllPauses(true);

        ErrorString ignored;
        resume(ignored);
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

    JSC::JSGlobalObject* scriptState = document.execState();
    if (!scriptState)
        return;

    didScheduleAsyncCall(scriptState, InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId, true);
}

void PageDebuggerAgent::willFireAnimationFrame(int callbackId)
{
    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId);
}

void PageDebuggerAgent::didCancelAnimationFrame(int callbackId)
{
    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::RequestAnimationFrame, callbackId);
}

} // namespace WebCore
