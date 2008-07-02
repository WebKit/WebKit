/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include <kjs/debugger.h>

#include <wtf/RetainPtr.h>

namespace KJS {
    class DebuggerCallFrame;
    class ExecState;
    class JSGlobalObject;
    class JSObject;
    class JSValue;
    class ArgList;
    class UString;
}

@class WebScriptCallFrame;

NSString *toNSString(const KJS::UString&);

class WebScriptDebugger : public KJS::Debugger {
public:
    WebScriptDebugger(KJS::JSGlobalObject*);

    virtual void sourceParsed(KJS::ExecState*, int sourceID, const KJS::UString& sourceURL, const KJS::SourceProvider& source, int lineNumber, int errorLine, const KJS::UString& errorMsg);
    virtual void callEvent(const KJS::DebuggerCallFrame&, int sourceID, int lineNumber);
    virtual void atStatement(const KJS::DebuggerCallFrame&, int sourceID, int lineNumber);
    virtual void returnEvent(const KJS::DebuggerCallFrame&, int sourceID, int lineNumber);
    virtual void exception(const KJS::DebuggerCallFrame&, int sourceID, int lineNumber);
    virtual void willExecuteProgram(const KJS::DebuggerCallFrame&, int sourceId, int lineno);
    virtual void didExecuteProgram(const KJS::DebuggerCallFrame&, int sourceId, int lineno);
    virtual void didReachBreakpoint(const KJS::DebuggerCallFrame&, int sourceId, int lineno);

private:
    bool m_callingDelegate;
    RetainPtr<WebScriptCallFrame> m_topCallFrame;
};

#endif // WebScriptDebugger_h
