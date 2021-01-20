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

#pragma once

#include "ConsoleClient.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

class InspectorConsoleAgent;
class InspectorDebuggerAgent;
class InspectorScriptProfilerAgent;

class JSGlobalObjectConsoleClient final : public JSC::ConsoleClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit JSGlobalObjectConsoleClient(InspectorConsoleAgent*);
    ~JSGlobalObjectConsoleClient() final { }

    static bool logToSystemConsole();
    static void setLogToSystemConsole(bool);

    void setDebuggerAgent(InspectorDebuggerAgent* agent) { m_debuggerAgent = agent; }
    void setPersistentScriptProfilerAgent(InspectorScriptProfilerAgent* agent) { m_scriptProfilerAgent = agent; }

private:
    void messageWithTypeAndLevel(MessageType, MessageLevel, JSC::JSGlobalObject*, Ref<ScriptArguments>&&) final;
    void count(JSC::JSGlobalObject*, const String& label) final;
    void countReset(JSC::JSGlobalObject*, const String& label) final;
    void profile(JSC::JSGlobalObject*, const String& title) final;
    void profileEnd(JSC::JSGlobalObject*, const String& title) final;
    void takeHeapSnapshot(JSC::JSGlobalObject*, const String& title) final;
    void time(JSC::JSGlobalObject*, const String& label) final;
    void timeLog(JSC::JSGlobalObject*, const String& label, Ref<ScriptArguments>&&) final;
    void timeEnd(JSC::JSGlobalObject*, const String& label) final;
    void timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) final;
    void record(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) final;
    void recordEnd(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) final;
    void screenshot(JSC::JSGlobalObject*, Ref<ScriptArguments>&&) final;

    void warnUnimplemented(const String& method);
    void internalAddMessage(MessageType, MessageLevel, JSC::JSGlobalObject*, Ref<ScriptArguments>&&);

    void startConsoleProfile();
    void stopConsoleProfile();

    InspectorConsoleAgent* m_consoleAgent;
    InspectorDebuggerAgent* m_debuggerAgent { nullptr };
    InspectorScriptProfilerAgent* m_scriptProfilerAgent { nullptr };
    Vector<String> m_profiles;
    bool m_profileRestoreBreakpointActiveValue { false };
};

}
