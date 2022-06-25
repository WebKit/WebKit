/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScriptDebugger_h
#define WebScriptDebugger_h

#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/Strong.h>

#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>

namespace JSC {
    class CallFrame;
    class DebuggerCallFrame;
    class JSGlobalObject;
    class JSObject;
    class ArgList;
}

@class WebScriptCallFrame;

class WebScriptDebugger final : public JSC::Debugger {
public:
    WebScriptDebugger(JSC::JSGlobalObject*);

    JSC::JSGlobalObject* globalObject() const { return m_globalObject.get(); }

private:
    void sourceParsed(JSC::JSGlobalObject*, JSC::SourceProvider*, int errorLine, const WTF::String& errorMsg) override;
    void handlePause(JSC::JSGlobalObject*, JSC::Debugger::ReasonForPause) override;

    bool m_callingDelegate;

    JSC::Strong<JSC::JSGlobalObject> m_globalObject;
};

#endif // WebScriptDebugger_h
