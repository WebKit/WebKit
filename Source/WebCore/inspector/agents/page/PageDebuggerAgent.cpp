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
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "Frame.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageScriptDebugServer.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "Timer.h"
#include "UserGestureIndicator.h"
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

void PageDebuggerAgent::evaluateOnCallFrame(ErrorString& errorString, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* emulateUserGesture, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    auto& pageChromeClient = m_inspectedPage.chrome().client();

    auto shouldEmulateUserGesture = emulateUserGesture && *emulateUserGesture;

    Optional<ProcessingUserGestureState> userGestureState = shouldEmulateUserGesture ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt;
    UserGestureIndicator gestureIndicator(userGestureState);

    bool userWasInteracting = false;
    if (shouldEmulateUserGesture) {
        userWasInteracting = pageChromeClient.userIsInteracting();
        if (!userWasInteracting)
            pageChromeClient.setUserIsInteracting(true);
    }

    WebDebuggerAgent::evaluateOnCallFrame(errorString, callFrameId, expression, objectGroup, includeCommandLineAPI, doNotPauseOnExceptionsAndMuteConsole, returnByValue, generatePreview, saveResult, emulateUserGesture, result, wasThrown, savedResultIndex);

    if (shouldEmulateUserGesture && !userWasInteracting && pageChromeClient.userIsInteracting())
        pageChromeClient.setUserIsInteracting(false);
}

void PageDebuggerAgent::enable()
{
    WebDebuggerAgent::enable();
    m_instrumentingAgents.setPageDebuggerAgent(this);
}

void PageDebuggerAgent::disable(bool isBeingDestroyed)
{
    WebDebuggerAgent::disable(isBeingDestroyed);
    m_instrumentingAgents.setPageDebuggerAgent(nullptr);
}

String PageDebuggerAgent::sourceMapURLForScript(const Script& script)
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

void PageDebuggerAgent::didClearAsyncStackTraceData()
{
    m_registeredEventListeners.clear();
    m_postMessageTimers.clear();
    m_nextEventListenerIdentifier = 1;
    m_nextPostMessageIdentifier = 1;
}

void PageDebuggerAgent::muteConsole()
{
    PageConsoleClient::mute();
}

void PageDebuggerAgent::unmuteConsole()
{
    PageConsoleClient::unmute();
}

void PageDebuggerAgent::breakpointActionLog(JSC::ExecState& state, const String& message)
{
    m_inspectedPage.console().addMessage(MessageSource::JS, MessageLevel::Log, message, createScriptCallStack(&state));
}

InjectedScript PageDebuggerAgent::injectedScriptForEval(ErrorString& errorString, const int* executionContextId)
{
    if (!executionContextId) {
        JSC::ExecState* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
        return injectedScriptManager().injectedScriptFor(scriptState);
    }

    InjectedScript injectedScript = injectedScriptManager().injectedScriptForId(*executionContextId);
    if (injectedScript.hasNoValue())
        errorString = "Missing injected script for given executionContextId."_s;

    return injectedScript;
}

void PageDebuggerAgent::didClearMainFrameWindowObject()
{
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

void PageDebuggerAgent::didAddEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (!breakpointsActive())
        return;

    auto& eventListeners = target.eventListeners(eventType);
    auto position = eventListeners.findMatching([&](auto& registeredListener) {
        return &registeredListener->callback() == &listener && registeredListener->useCapture() == capture;
    });
    if (position == notFound)
        return;

    auto& registeredListener = eventListeners.at(position);
    if (m_registeredEventListeners.contains(registeredListener.get()))
        return;

    JSC::ExecState* scriptState = target.scriptExecutionContext()->execState();
    if (!scriptState)
        return;

    int identifier = m_nextEventListenerIdentifier++;
    m_registeredEventListeners.set(registeredListener.get(), identifier);

    didScheduleAsyncCall(scriptState, InspectorDebuggerAgent::AsyncCallType::EventListener, identifier, registeredListener->isOnce());
}

void PageDebuggerAgent::willRemoveEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    auto& eventListeners = target.eventListeners(eventType);
    size_t listenerIndex = eventListeners.findMatching([&](auto& registeredListener) {
        return &registeredListener->callback() == &listener && registeredListener->useCapture() == capture;
    });

    if (listenerIndex == notFound)
        return;

    int identifier = m_registeredEventListeners.take(eventListeners[listenerIndex].get());
    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::EventListener, identifier);
}

void PageDebuggerAgent::willHandleEvent(const RegisteredEventListener& listener)
{
    auto it = m_registeredEventListeners.find(&listener);
    if (it == m_registeredEventListeners.end())
        return;

    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::EventListener, it->value);
}

void PageDebuggerAgent::didRequestAnimationFrame(int callbackId, Document& document)
{
    if (!breakpointsActive())
        return;

    JSC::ExecState* scriptState = document.execState();
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

void PageDebuggerAgent::didPostMessage(const TimerBase& timer, JSC::ExecState& state)
{
    if (!breakpointsActive())
        return;

    if (m_postMessageTimers.contains(&timer))
        return;

    int postMessageIdentifier = m_nextPostMessageIdentifier++;
    m_postMessageTimers.set(&timer, postMessageIdentifier);

    didScheduleAsyncCall(&state, InspectorDebuggerAgent::AsyncCallType::PostMessage, postMessageIdentifier, true);
}

void PageDebuggerAgent::didFailPostMessage(const TimerBase& timer)
{
    auto it = m_postMessageTimers.find(&timer);
    if (it == m_postMessageTimers.end())
        return;

    didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::PostMessage, it->value);

    m_postMessageTimers.remove(it);
}

void PageDebuggerAgent::willDispatchPostMessage(const TimerBase& timer)
{
    auto it = m_postMessageTimers.find(&timer);
    if (it == m_postMessageTimers.end())
        return;

    willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::PostMessage, it->value);
}

void PageDebuggerAgent::didDispatchPostMessage(const TimerBase& timer)
{
    auto it = m_postMessageTimers.find(&timer);
    if (it == m_postMessageTimers.end())
        return;

    didDispatchAsyncCall();

    m_postMessageTimers.remove(it);
}

} // namespace WebCore
