/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
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
#include "config.h"
#include "DebuggerDocument.h"

#include "ServerConnection.h"

#include <JavaScriptCore/JSStringRef.h>

// DebuggerDocument platform specific implementations

void DebuggerDocument::platformPause()
{
    m_server->pause();
}

void DebuggerDocument::platformResume()
{
    m_server->resume();
}

void DebuggerDocument::platformStepInto()
{
    m_server->stepInto();
}

// FIXME: Some of the below functionality cannot be implemented until the WebScriptCallFrame Server works on windows.

JSValueRef DebuggerDocument::platformEvaluateScript(JSContextRef context, JSStringRef /*script*/, int /*callFrame*/)
{
    return JSValueMakeUndefined(context);
}

void DebuggerDocument::getPlatformCurrentFunctionStack(JSContextRef /*context*/, Vector<JSValueRef>& /*currentStack*/)
{
}

void DebuggerDocument::getPlatformLocalScopeVariableNamesForCallFrame(JSContextRef /*context*/, int /*callFrame*/, Vector<JSValueRef>& /*variableNames*/)
{
}

JSValueRef DebuggerDocument::platformValueForScopeVariableNamed(JSContextRef context, JSStringRef /*key*/, int /*callFrame*/)
{
    return JSValueMakeUndefined(context);
}

void DebuggerDocument::platformLog(JSStringRef msg)
{
    printf("%S\n", JSStringGetCharactersPtr(msg));
}
