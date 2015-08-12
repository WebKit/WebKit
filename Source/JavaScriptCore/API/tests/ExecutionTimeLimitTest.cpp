/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ExecutionTimeLimitTest.h"

#include "JSContextRefPrivate.h"
#include "JavaScriptCore.h"
#include <chrono>
#include <wtf/CurrentTime.h>

using namespace std::chrono;

static JSGlobalContextRef context = nullptr;

static JSValueRef currentCPUTimeAsJSFunctionCallback(JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);
    
    ASSERT(JSContextGetGlobalContext(ctx) == context);
    return JSValueMakeNumber(ctx, currentCPUTime().count() / 1000000.);
}

bool shouldTerminateCallbackWasCalled = false;
static bool shouldTerminateCallback(JSContextRef ctx, void* context)
{
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(context);
    shouldTerminateCallbackWasCalled = true;
    return true;
}

bool cancelTerminateCallbackWasCalled = false;
static bool cancelTerminateCallback(JSContextRef ctx, void* context)
{
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(context);
    cancelTerminateCallbackWasCalled = true;
    return false;
}

int extendTerminateCallbackCalled = 0;
static bool extendTerminateCallback(JSContextRef ctx, void* context)
{
    UNUSED_PARAM(context);
    extendTerminateCallbackCalled++;
    if (extendTerminateCallbackCalled == 1) {
        JSContextGroupRef contextGroup = JSContextGetGroup(ctx);
        JSContextGroupSetExecutionTimeLimit(contextGroup, .200f, extendTerminateCallback, 0);
        return false;
    }
    return true;
}


int testExecutionTimeLimit()
{
    context = JSGlobalContextCreateInGroup(nullptr, nullptr);

    JSContextGroupRef contextGroup = JSContextGetGroup(context);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    ASSERT(JSValueIsObject(context, globalObject));

    JSValueRef v = nullptr;
    JSValueRef exception = nullptr;
    bool failed = false;

    JSStringRef currentCPUTimeStr = JSStringCreateWithUTF8CString("currentCPUTime");
    JSObjectRef currentCPUTimeFunction = JSObjectMakeFunctionWithCallback(context, currentCPUTimeStr, currentCPUTimeAsJSFunctionCallback);
    JSObjectSetProperty(context, globalObject, currentCPUTimeStr, currentCPUTimeFunction, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(currentCPUTimeStr);
    
    /* Test script timeout: */
    JSContextGroupSetExecutionTimeLimit(contextGroup, .10f, shouldTerminateCallback, 0);
    {
        const char* loopForeverScript = "var startTime = currentCPUTime(); while (true) { if (currentCPUTime() - startTime > .150) break; } ";
        JSStringRef script = JSStringCreateWithUTF8CString(loopForeverScript);
        exception = nullptr;
        shouldTerminateCallbackWasCalled = false;
        auto startTime = currentCPUTime();
        v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        auto endTime = currentCPUTime();
        
        if (((endTime - startTime) < milliseconds(150)) && shouldTerminateCallbackWasCalled)
            printf("PASS: script timed out as expected.\n");
        else {
            if (!((endTime - startTime) < milliseconds(150)))
                printf("FAIL: script did not time out as expected.\n");
            if (!shouldTerminateCallbackWasCalled)
                printf("FAIL: script timeout callback was not called.\n");
            failed = true;
        }
        
        if (!exception) {
            printf("FAIL: TerminatedExecutionException was not thrown.\n");
            failed = true;
        }
    }
    
    /* Test the script timeout's TerminatedExecutionException should NOT be catchable: */
    JSContextGroupSetExecutionTimeLimit(contextGroup, 0.10f, shouldTerminateCallback, 0);
    {
        const char* loopForeverScript = "var startTime = currentCPUTime(); try { while (true) { if (currentCPUTime() - startTime > .150) break; } } catch(e) { }";
        JSStringRef script = JSStringCreateWithUTF8CString(loopForeverScript);
        exception = nullptr;
        shouldTerminateCallbackWasCalled = false;
        auto startTime = currentCPUTime();
        v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        auto endTime = currentCPUTime();
        
        if (((endTime - startTime) >= milliseconds(150)) || !shouldTerminateCallbackWasCalled) {
            if (!((endTime - startTime) < milliseconds(150)))
                printf("FAIL: script did not time out as expected.\n");
            if (!shouldTerminateCallbackWasCalled)
                printf("FAIL: script timeout callback was not called.\n");
            failed = true;
        }
        
        if (exception)
            printf("PASS: TerminatedExecutionException was not catchable as expected.\n");
        else {
            printf("FAIL: TerminatedExecutionException was caught.\n");
            failed = true;
        }
    }
    
    /* Test script timeout with no callback: */
    JSContextGroupSetExecutionTimeLimit(contextGroup, .10f, 0, 0);
    {
        const char* loopForeverScript = "var startTime = currentCPUTime(); while (true) { if (currentCPUTime() - startTime > .150) break; } ";
        JSStringRef script = JSStringCreateWithUTF8CString(loopForeverScript);
        exception = nullptr;
        auto startTime = currentCPUTime();
        v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        auto endTime = currentCPUTime();
        
        if (((endTime - startTime) < milliseconds(150)) && shouldTerminateCallbackWasCalled)
            printf("PASS: script timed out as expected when no callback is specified.\n");
        else {
            if (!((endTime - startTime) < milliseconds(150)))
                printf("FAIL: script did not time out as expected when no callback is specified.\n");
            failed = true;
        }
        
        if (!exception) {
            printf("FAIL: TerminatedExecutionException was not thrown.\n");
            failed = true;
        }
    }
    
    /* Test script timeout cancellation: */
    JSContextGroupSetExecutionTimeLimit(contextGroup, 0.10f, cancelTerminateCallback, 0);
    {
        const char* loopForeverScript = "var startTime = currentCPUTime(); while (true) { if (currentCPUTime() - startTime > .150) break; } ";
        JSStringRef script = JSStringCreateWithUTF8CString(loopForeverScript);
        exception = nullptr;
        auto startTime = currentCPUTime();
        v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        auto endTime = currentCPUTime();
        
        if (((endTime - startTime) >= milliseconds(150)) && cancelTerminateCallbackWasCalled && !exception)
            printf("PASS: script timeout was cancelled as expected.\n");
        else {
            if (((endTime - startTime) < milliseconds(150)) || exception)
                printf("FAIL: script timeout was not cancelled.\n");
            if (!cancelTerminateCallbackWasCalled)
                printf("FAIL: script timeout callback was not called.\n");
            failed = true;
        }
        
        if (exception) {
            printf("FAIL: Unexpected TerminatedExecutionException thrown.\n");
            failed = true;
        }
    }
    
    /* Test script timeout extension: */
    JSContextGroupSetExecutionTimeLimit(contextGroup, 0.100f, extendTerminateCallback, 0);
    {
        const char* loopForeverScript = "var startTime = currentCPUTime(); while (true) { if (currentCPUTime() - startTime > .500) break; } ";
        JSStringRef script = JSStringCreateWithUTF8CString(loopForeverScript);
        exception = nullptr;
        auto startTime = currentCPUTime();
        v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        auto endTime = currentCPUTime();
        auto deltaTime = endTime - startTime;
        
        if ((deltaTime >= milliseconds(300)) && (deltaTime < milliseconds(500)) && (extendTerminateCallbackCalled == 2) && exception)
            printf("PASS: script timeout was extended as expected.\n");
        else {
            if (deltaTime < milliseconds(200))
                printf("FAIL: script timeout was not extended as expected.\n");
            else if (deltaTime >= milliseconds(500))
                printf("FAIL: script did not timeout.\n");
            
            if (extendTerminateCallbackCalled < 1)
                printf("FAIL: script timeout callback was not called.\n");
            if (extendTerminateCallbackCalled < 2)
                printf("FAIL: script timeout callback was not called after timeout extension.\n");
            
            if (!exception)
                printf("FAIL: TerminatedExecutionException was not thrown during timeout extension test.\n");
            
            failed = true;
        }
    }

    JSGlobalContextRelease(context);
    return failed;
}
