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

// FIXME: This file and class should be renamed WebScriptDebugger

#include "WebCoreScriptDebuggerImp.h"

#include "WebCoreScriptDebugger.h"
#include <JavaScriptCore/JSGlobalObject.h>

using namespace KJS;

WebCoreScriptDebuggerImp::WebCoreScriptDebuggerImp(WebCoreScriptDebugger *debugger, JSGlobalObject* globalObject)
    : m_debugger(debugger)
{
    m_callingDelegate = true;
    m_topCallFrame = [[m_debugger delegate] enterFrame:globalObject->globalExec()];
    attach(globalObject);
    [[m_debugger delegate] enteredFrame:m_topCallFrame sourceId:-1 line:-1];
    m_callingDelegate = false;
}

// callbacks - relay to delegate
bool WebCoreScriptDebuggerImp::sourceParsed(ExecState* state, int sourceID, const UString& url, const UString& source, int lineNumber, int errorLine, const UString& errorMsg)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;
    [[m_debugger delegate] parsedSource:toNSString(source) fromURL:toNSURL(url) sourceId:sourceID startLine:lineNumber errorLine:errorLine errorMessage:toNSString(errorMsg)];
    m_callingDelegate = false;

    return true;
}

bool WebCoreScriptDebuggerImp::callEvent(ExecState* state, int sourceID, int lineNumber, JSObject*, const List&)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;
    m_topCallFrame = [[m_debugger delegate] enterFrame:state];
    [[m_debugger delegate] enteredFrame:m_topCallFrame sourceId:sourceID line:lineNumber];
    m_callingDelegate = false;

    return true;
}

bool WebCoreScriptDebuggerImp::atStatement(ExecState* state, int sourceID, int lineNumber, int)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;
    [[m_debugger delegate] hitStatement:m_topCallFrame sourceId:sourceID line:lineNumber];
    m_callingDelegate = false;

    return true;
}

bool WebCoreScriptDebuggerImp::returnEvent(ExecState* state, int sourceID, int lineNumber, JSObject*)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;
    [[m_debugger delegate] leavingFrame:m_topCallFrame sourceId:sourceID line:lineNumber];
    m_topCallFrame = [[m_debugger delegate] leaveFrame];
    m_callingDelegate = false;

    return true;
}

bool WebCoreScriptDebuggerImp::exception(ExecState* state, int sourceID, int lineNumber, JSValue*)
{
    if (m_callingDelegate)
        return true;

    m_callingDelegate = true;
    [[m_debugger delegate] exceptionRaised:m_topCallFrame sourceId:sourceID line:lineNumber];
    m_callingDelegate = false;

    return true;
}
