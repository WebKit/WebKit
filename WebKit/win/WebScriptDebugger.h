/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebScriptDebugger_H
#define WebScriptDebugger_H

#include "IWebView.h"
#include "IWebScriptCallFrame.h"

#include <JavaScriptCore/debugger.h>
#pragma warning(push, 0)
#include <WebCore/COMPtr.h>
#pragma warning(pop)

class WebFrame;
interface IWebScriptCallFrame;

namespace KJS {
    class ExecState;
    class JSObject;
    class JSValue;
    class List;
}

class WebScriptDebugger : public KJS::Debugger {
public:
    WebScriptDebugger(WebFrame*);

    bool sourceParsed(KJS::ExecState*, int sourceId, const KJS::UString& sourceURL,
        const KJS::UString& source, int startingLineNumber, int errorLine, const KJS::UString& errorMsg);

    bool callEvent(KJS::ExecState*, int sourceId, int lineno, KJS::JSObject* function, const KJS::List& args);
    bool atStatement(KJS::ExecState*, int sourceId, int firstLine, int lastLine);
    bool returnEvent(KJS::ExecState*, int sourceId, int lineno, KJS::JSObject* function);
    bool exception(KJS::ExecState*, int sourceId, int lineno, KJS::JSValue* exception);

private:
    void enterFrame(KJS::ExecState*);
    void leaveFrame();
    bool m_callingServer;

    WebFrame* m_frame;
    COMPtr<IWebView> m_webView;
    COMPtr<IWebScriptCallFrame> m_topStackFrame; 
};

#endif
