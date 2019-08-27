/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "WebDebuggerAgent.h"

namespace WebCore {

class Document;
class EventListener;
class EventTarget;
class Page;
class RegisteredEventListener;
class TimerBase;

class PageDebuggerAgent final : public WebDebuggerAgent {
    WTF_MAKE_NONCOPYABLE(PageDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageDebuggerAgent(PageAgentContext&);
    virtual ~PageDebuggerAgent();

    // DebuggerBackendDispatcherHandler
    void evaluateOnCallFrame(ErrorString&, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* emulateUserGesture, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex);

    // ScriptDebugListener
    void breakpointActionLog(JSC::ExecState&, const String&);

    // InspectorInstrumentation
    void didClearMainFrameWindowObject();
    void mainFrameStartedLoading();
    void mainFrameStoppedLoading();
    void mainFrameNavigated();
    void didRequestAnimationFrame(int callbackId, Document&);
    void willFireAnimationFrame(int callbackId);
    void didCancelAnimationFrame(int callbackId);
    void didAddEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    void willRemoveEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    void willHandleEvent(const RegisteredEventListener&);
    void didPostMessage(const TimerBase&, JSC::ExecState&);
    void didFailPostMessage(const TimerBase&);
    void willDispatchPostMessage(const TimerBase&);
    void didDispatchPostMessage(const TimerBase&);

private:
    void enable();
    void disable(bool isBeingDestroyed);

    String sourceMapURLForScript(const Script&);

    void didClearAsyncStackTraceData();

    void muteConsole();
    void unmuteConsole();

    Inspector::InjectedScript injectedScriptForEval(ErrorString&, const int* executionContextId);

    Page& m_inspectedPage;

    HashMap<const RegisteredEventListener*, int> m_registeredEventListeners;
    HashMap<const TimerBase*, int> m_postMessageTimers;
    int m_nextEventListenerIdentifier { 1 };
    int m_nextPostMessageIdentifier { 1 };
};

} // namespace WebCore
