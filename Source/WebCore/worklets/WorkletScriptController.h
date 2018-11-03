/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/JSRunLoopTimer.h>
#include <JavaScriptCore/Strong.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/NakedPtr.h>

namespace JSC {
class VM;
}

namespace WebCore {

class JSWorkletGlobalScope;
class ScriptSourceCode;
class WorkletConsoleClient;
class WorkletGlobalScope;
class WorkletConsoleClient;

class WorkletScriptController {
    WTF_MAKE_NONCOPYABLE(WorkletScriptController); WTF_MAKE_FAST_ALLOCATED;
public:
    WorkletScriptController(WorkletGlobalScope*);
    ~WorkletScriptController();

    JSWorkletGlobalScope* workletGlobalScopeWrapper()
    {
        initScriptIfNeeded();
        return m_workletGlobalScopeWrapper.get();
    }

    void forbidExecution();
    bool isExecutionForbidden() const;

    void disableEval(const String& errorMessage);
    void disableWebAssembly(const String& errorMessage);

    void evaluate(const ScriptSourceCode&, String* returnedExceptionMessage = nullptr);
    void evaluate(const ScriptSourceCode&, NakedPtr<JSC::Exception>& returnedException, String* returnedExceptionMessage = nullptr);

    void setException(JSC::Exception*);

    JSC::VM& vm() { return *m_vm; }

private:
    void initScriptIfNeeded()
    {
        if (!m_workletGlobalScopeWrapper)
            initScript();
    }
    WEBCORE_EXPORT void initScript();

    template<typename JSGlobalScopePrototype, typename JSGlobalScope, typename GlobalScope>
    void initScriptWithSubclass();

    bool m_executionForbidden { false };
    RefPtr<JSC::VM> m_vm;
    WorkletGlobalScope* m_workletGlobalScope;
    std::unique_ptr<WorkletConsoleClient> m_consoleClient;
    JSC::Strong<JSWorkletGlobalScope> m_workletGlobalScopeWrapper;
};

} // namespace WebCore
#endif
