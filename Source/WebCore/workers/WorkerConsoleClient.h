/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ConsoleClient.h>
#include <wtf/Forward.h>

namespace JSC {
class CallFrame;
}

namespace WebCore {

class WorkerConsoleClient final : public JSC::ConsoleClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerConsoleClient(WorkerGlobalScope&);
    virtual ~WorkerConsoleClient();

private:
    void messageWithTypeAndLevel(MessageType, MessageLevel, JSC::JSGlobalObject*, Ref<Inspector::ScriptArguments>&&) override;
    void count(JSC::JSGlobalObject*, const String& label) override;
    void countReset(JSC::JSGlobalObject*, const String& label) override;
    void profile(JSC::JSGlobalObject*, const String& title) override;
    void profileEnd(JSC::JSGlobalObject*, const String& title) override;
    void takeHeapSnapshot(JSC::JSGlobalObject*, const String& title) override;
    void time(JSC::JSGlobalObject*, const String& label) override;
    void timeLog(JSC::JSGlobalObject*, const String& label, Ref<Inspector::ScriptArguments>&&) override;
    void timeEnd(JSC::JSGlobalObject*, const String& label) override;
    void timeStamp(JSC::JSGlobalObject*, Ref<Inspector::ScriptArguments>&&) override;
    void record(JSC::JSGlobalObject*, Ref<Inspector::ScriptArguments>&&) override;
    void recordEnd(JSC::JSGlobalObject*, Ref<Inspector::ScriptArguments>&&) override;
    void screenshot(JSC::JSGlobalObject*, Ref<Inspector::ScriptArguments>&&) override;

    WorkerGlobalScope& m_workerGlobalScope;
};

} // namespace WebCore
