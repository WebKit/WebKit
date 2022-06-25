/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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

#include "DebuggerCallFrame.h"
#include "JSCJSValueInlines.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextPosition.h>

namespace Inspector {

class JavaScriptCallFrame : public RefCounted<JavaScriptCallFrame> {
public:
    static Ref<JavaScriptCallFrame> create(Ref<JSC::DebuggerCallFrame>&& debuggerCallFrame)
    {
        return adoptRef(*new JavaScriptCallFrame(WTFMove(debuggerCallFrame)));
    }

    JavaScriptCallFrame* caller();
    JSC::SourceID sourceID() const { return m_debuggerCallFrame->sourceID(); }
    const TextPosition position() const { return m_debuggerCallFrame->position(); }
    int line() const { return m_debuggerCallFrame->line(); }
    int column() const { return m_debuggerCallFrame->column(); }

    String functionName(JSC::VM& vm) const { return m_debuggerCallFrame->functionName(vm); }
    JSC::DebuggerCallFrame::Type type(JSC::VM& vm) const { return m_debuggerCallFrame->type(vm); }
    JSC::DebuggerScope* scopeChain(JSC::VM& vm) const { return m_debuggerCallFrame->scope(vm); }
    bool isTailDeleted() const { return m_debuggerCallFrame->isTailDeleted(); }

    JSC::JSValue thisValue(JSC::VM& vm) const { return m_debuggerCallFrame->thisValue(vm); }
    JSC::JSValue evaluateWithScopeExtension(JSC::VM& vm, const String& script, JSC::JSObject* scopeExtension, NakedPtr<JSC::Exception>& exception) const { return m_debuggerCallFrame->evaluateWithScopeExtension(vm, script, scopeExtension, exception); }

private:
    JavaScriptCallFrame(Ref<JSC::DebuggerCallFrame>&&);

    Ref<JSC::DebuggerCallFrame> m_debuggerCallFrame;
    RefPtr<JavaScriptCallFrame> m_caller;
};

} // namespace Inspector
