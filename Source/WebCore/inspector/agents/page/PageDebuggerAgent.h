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

class DOMWrapperWorld;
class Document;
class Frame;
class Page;
class UserGestureEmulationScope;

class PageDebuggerAgent final : public WebDebuggerAgent {
    WTF_MAKE_NONCOPYABLE(PageDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageDebuggerAgent(PageAgentContext&);
    ~PageDebuggerAgent();
    bool enabled() const;

    // DebuggerBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<std::tuple<Ref<Inspector::Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */>> evaluateOnCallFrame(const Inspector::Protocol::Debugger::CallFrameId&, const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture);

    // JSC::Debugger::Client
    void debuggerWillEvaluate(JSC::Debugger&, const JSC::Breakpoint::Action&);
    void debuggerDidEvaluate(JSC::Debugger&, const JSC::Breakpoint::Action&);

    // JSC::Debugger::Observer
    void breakpointActionLog(JSC::JSGlobalObject*, const String& data);

    // InspectorInstrumentation
    void didClearWindowObjectInWorld(Frame&, DOMWrapperWorld&);
    void mainFrameStartedLoading();
    void mainFrameStoppedLoading();
    void mainFrameNavigated();
    void didRequestAnimationFrame(int callbackId, Document&);
    void willFireAnimationFrame(int callbackId);
    void didCancelAnimationFrame(int callbackId);

private:
    void internalEnable();
    void internalDisable(bool isBeingDestroyed);

    String sourceMapURLForScript(const JSC::Debugger::Script&);

    void muteConsole();
    void unmuteConsole();

    Inspector::InjectedScript injectedScriptForEval(Inspector::Protocol::ErrorString&, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&&);

    Page& m_inspectedPage;
    Vector<UniqueRef<UserGestureEmulationScope>> m_breakpointActionUserGestureEmulationScopeStack;
};

} // namespace WebCore
