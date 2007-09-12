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
#include "DebuggerClient.h"

#include "DebuggerApplication.h"
#include "DebuggerDocument.h"

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>
#include <JavaScriptCore/RetainPtr.h>

void DebuggerClient::pause()
{
    //if ([[(NSDistantObject *)server connectionForProxy] isValid])
    //    [server pause];
    //[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

void DebuggerClient::resume()
{
    //if ([[(NSDistantObject *)server connectionForProxy] isValid])
    //    [server resume];
}

void DebuggerClient::stepInto()
{
    //if ([[(NSDistantObject *)server connectionForProxy] isValid])
    //    [server step];
}


// DebuggerDocument platform specific implementations

void DebuggerDocument::platformPause(JSContextRef /*context*/)
{
    m_debuggerClient->pause();
}

void DebuggerDocument::platformResume(JSContextRef /*context*/)
{
    m_debuggerClient->resume();
}

void DebuggerDocument::platformStepInto(JSContextRef /*context*/)
{
    m_debuggerClient->stepInto();
}

JSValueRef DebuggerDocument::platformEvaluateScript(JSContextRef context, JSStringRef /*script*/, int /*callFrame*/)
{
//    WebScriptCallFrame *cframe = [m_debuggerClient currentFrame];
//    for (unsigned count = 0; count < callFrame; count++)
//        cframe = [cframe caller];
//
//    if (!cframe)
        return JSValueMakeUndefined(context);
//
//    RetainPtr<CFStringRef> scriptCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, script));
//    id value = [cframe evaluateWebScript:(NSString *)scriptCF.get()];
//
//    NSString *resultString = [NSString stringOrNilFromWebScriptResult:value];
//    JSRetainPtr<JSStringRef> resultJS(KJS::Adopt, JSStringCreateWithCFString((CFStringRef)resultString));
//    JSValueRef returnValue = JSValueMakeString(context, resultJS.get());
//
//    return returnValue;
}

void DebuggerDocument::getPlatformCurrentFunctionStack(JSContextRef /*context*/, Vector<JSValueRef>& /*currentStack*/)
{
//    for (WebScriptCallFrame *frame = [m_debuggerClient currentFrame]; frame;) {
//        CFStringRef function;
//        if ([frame functionName])
//            function = (CFStringRef)[frame functionName];
//        else if ([frame caller])
//            function = CFSTR("(anonymous function)");
//        else
//            function = CFSTR("(global scope)");
//        frame = [frame caller];
//
//        JSRetainPtr<JSStringRef> stackString(KJS::Adopt, JSStringCreateWithCFString(function));
//        JSValueRef stackValue = JSValueMakeString(context, stackString.get());
//        currentStack.append(stackValue);
//    }
}

void DebuggerDocument::getPlatformLocalScopeVariableNamesForCallFrame(JSContextRef /*context*/, int /*callFrame*/, Vector<JSValueRef>& /*variableNames*/)
{
//    WebScriptCallFrame *cframe = [m_debuggerClient currentFrame];
//    for (unsigned count = 0; count < callFrame; count++)
//        cframe = [cframe caller];
//
//    if (!cframe)
//        return;
//    if (![[cframe scopeChain] count])
//        return;
//
//    WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:0]; // local is always first
//    NSArray *localScopeVariableNames = [m_debuggerClient webScriptAttributeKeysForScriptObject:scope];
//
//    for (int i = 0; i < [localScopeVariableNames count]; ++i) {
//        JSRetainPtr<JSStringRef> variableName(KJS::Adopt, JSStringCreateWithCFString((CFStringRef)[localScopeVariableNames objectAtIndex:i]));
//        JSValueRef variableNameValue = JSValueMakeString(context, variableName.get());
//        variableNames.append(variableNameValue);
//    }
}

JSValueRef DebuggerDocument::platformValueForScopeVariableNamed(JSContextRef context, JSStringRef /*key*/, int /*callFrame*/)
{
//    WebScriptCallFrame *cframe = [m_debuggerClient currentFrame];
//    for (unsigned count = 0; count < callFrame; count++)
//        cframe = [cframe caller];
//
//    if (!cframe)
//        return JSValueMakeUndefined(context);
//
//    unsigned scopeCount = [[cframe scopeChain] count];
//    
//    if (!scopeCount)
//        return JSValueMakeUndefined(context);
//
//    NSString *resultString = nil;
//    
//    for (unsigned i = 0; i < scopeCount && resultString == nil; i++) {
//        WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:i];
//
//        RetainPtr<CFStringRef> keyCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, key));
//        
//        id value = nil;
//        @try {
//            value = [scope valueForKey:(NSString *)keyCF.get()];
//        } @catch(NSException* localException) { // The value wasn't found.
//        }
//
//        resultString = [NSString stringOrNilFromWebScriptResult:value];
//    }
//
//    if (!resultString)
        return JSValueMakeUndefined(context);
//
//    JSRetainPtr<JSStringRef> resultJS(KJS::Adopt, JSStringCreateWithCFString((CFStringRef)resultString));
//    JSValueRef retVal = JSValueMakeString(context, resultJS.get());
//    return retVal;
}

void DebuggerDocument::platformLog(JSContextRef /*context*/, JSStringRef /*msg*/)
{
//    RetainPtr<CFStringRef> msgCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, msg));
//    [DebuggerClientMac log:(NSString *)msgCF.get()];
}