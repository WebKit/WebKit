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

#include "stdafx.h"
#include "DebuggerObjectCallbacks.h"

#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefCF.h>
#include <JavaScriptCore/RetainPtr.h>

// When you hit a breakpoint you can expand it somehow (double click?) and it shows this html
static JSValueRef breakpointEditorHTMLCallback(JSContextRef context, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
    RetainPtr<CFURLRef> htmlURLRef(AdoptCF, ::CFBundleCopyResourceURL(::CFBundleGetBundleWithIdentifier(CFSTR("org.webkit.drosera")), CFSTR("breakpointEditor"), CFSTR("html"), CFSTR("Drosera")));
    if (!htmlURLRef)
        return 0;

    // FIXME: I'm open to a better way to do this.  We convert from UInt8 to CFString to JSString (3 string types!)
    RetainPtr<CFReadStreamRef> readStreamRef(AdoptCF, CFReadStreamCreateWithFile(0, htmlURLRef.get()));
    if (!CFReadStreamOpen(readStreamRef.get()))
        return 0;

    CFIndex bufferLength = 0;
    const UInt8* buf = CFReadStreamGetBuffer(readStreamRef.get(), 0, &bufferLength);

    CFReadStreamClose(readStreamRef.get());

    if (!buf)
        return 0;

    // FIXME: Is there a way to determine the encoding?
    RetainPtr<CFStringRef> fileContents(AdoptCF, CFStringCreateWithBytes(0, buf, bufferLength, kCFStringEncodingUTF8, true));
    JSStringRef fileContentsJS = JSStringCreateWithCFString(fileContents.get());
    JSValueRef ret = JSValueMakeString(context, fileContentsJS);

    JSStringRelease(fileContentsJS);

    return ret;
}

// ------------------------------------------------------------------------------------------------------------
//FIXME All functions below will change to use the JS API instead of WebScriptObject.  For now plese ignore them.
// ------------------------------------------------------------------------------------------------------------
// however I want to leave these stubs in to help me know what I need to do.
static JSValueRef currentFunctionStackCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (NSArray *)currentFunctionStack
//{
//    NSMutableArray *result = [[NSMutableArray alloc] init];
//    WebScriptCallFrame *frame = currentFrame;
//    while (frame) {
//        if ([frame functionName])
//            [result addObject:[frame functionName]];
//        else if ([frame caller])
//            [result addObject:@"(anonymous function)"];
//        else
//            [result addObject:@"(global scope)"];
//        frame = [frame caller];
//    }
//    return [result autorelease];
//}
    return 0;
}

static JSValueRef doubleClickMillisecondsCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (int)doubleClickMilliseconds  // Event.detail to get click count instead of timing a double click.  This is the time a double click takes.
//{
//    // GetDblTime() returns values in 1/60ths of a second
//    return ((double)GetDblTime() / 60.0) * 1000;
//}
    return 0;
}

static JSValueRef evaluateScript_inCallFrame_Callback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (id)evaluateScript:(NSString *)script inCallFrame:(int)frame  // evaluate the JavaScript
//{
//    WebScriptCallFrame *cframe = currentFrame;
//    for (unsigned count = 0; count < frame; count++)
//        cframe = [cframe caller];
//    if (!cframe)
//        return nil;
//
//    id result = [cframe evaluateWebScript:script];
//    if ([result isKindOfClass:NSClassFromString(@"WebScriptObject")])
//        return [result callWebScriptMethod:@"toString" withArguments:nil];
//    return result;
//}
    return 0;
}

static JSValueRef isPausedCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (BOOL)isPaused
//{
//    return paused;
//}
    return 0;
}

static JSValueRef localScopeVariableNamesForCallFrame_Callback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (NSArray *)localScopeVariableNamesForCallFrame:(int)frame // return local vars names for the given frame
//{
//    WebScriptCallFrame *cframe = currentFrame;
//    for (unsigned count = 0; count < frame; count++)
//        cframe = [cframe caller];
//
//    if (![[cframe scopeChain] count])
//        return nil;
//
//    WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:0]; // local is always first
//    return [self webScriptAttributeKeysForScriptObject:scope];
//}
    return 0;
}

static JSValueRef pauseCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (void)pause
//{
//    paused = YES;
//    if ([[(NSDistantObject *)server connectionForProxy] isValid])
//        [server pause];
//    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
//}
    return 0;
}

static JSValueRef resumeCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (void)resume
//{
//    paused = NO;
//    if ([[(NSDistantObject *)server connectionForProxy] isValid])
//        [server resume];
//}
    return 0;
}

static JSValueRef stepIntoCallback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (void)stepInto
//{
//    if ([[(NSDistantObject *)server connectionForProxy] isValid])
//        [server step];
//}
    return 0;
}

static JSValueRef valueForScopeVariableNamed_inCallFrame_Callback(JSContextRef /*context*/, JSObjectRef /*function*/, JSObjectRef /*thisObject*/, size_t /*argumentCount*/, const JSValueRef /*arguments*/[], JSValueRef* /*exception*/)
{
//- (NSString *)valueForScopeVariableNamed:(NSString *)key inCallFrame:(int)frame // returns the value of a given var
//{
//    WebScriptCallFrame *cframe = currentFrame;
//    for (unsigned count = 0; count < frame; count++)
//        cframe = [cframe caller];
//
//    if (![[cframe scopeChain] count])
//        return nil;
//
//    unsigned scopeCount = [[cframe scopeChain] count];
//    for (unsigned i = 0; i < scopeCount; i++) {
//        WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:i];
//        id value = [scope valueForKey:key];
//        if ([value isKindOfClass:NSClassFromString(@"WebScriptObject")])
//            return [value callWebScriptMethod:@"toString" withArguments:nil];
//        if (value && ![value isKindOfClass:[NSString class]])
//            return [NSString stringWithFormat:@"%@", value];
//        if (value)
//            return value;
//    }
//
//    return nil;
//}
    return 0;
}

JSStaticFunction* staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "breakpointEditorHTML", breakpointEditorHTMLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "currentFunctionStack", currentFunctionStackCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "doubleClickMilliseconds", doubleClickMillisecondsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "evaluateScript_inCallFrame_", evaluateScript_inCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isPaused", isPausedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "localScopeVariableNamesForCallFrame_", localScopeVariableNamesForCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pause", pauseCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resume", resumeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stepInto", stepIntoCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "valueForScopeVariableNamed_inCallFrame_", valueForScopeVariableNamed_inCallFrame_Callback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}
