/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#include "InitializeThreading.h"
#include "JSContextRefPrivate.h"
#include "JavaScript.h"
#include "Options.h"
#include <wtf/CPUTime.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/text/StringBuilder.h>

#if HAVE(MACH_EXCEPTIONS)
#include <dispatch/dispatch.h>
#endif

using JSC::Options;

static JSGlobalContextRef context = nullptr;

static JSValueRef currentCPUTimeAsJSFunctionCallback(JSContextRef ctx, JSObjectRef functionObject, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UNUSED_PARAM(functionObject);
    UNUSED_PARAM(thisObject);
    UNUSED_PARAM(argumentCount);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(exception);
    
    ASSERT(JSContextGetGlobalContext(ctx) == context);
    return JSValueMakeNumber(ctx, CPUTime::forCurrentThread().seconds());
}

bool shouldTerminateCallbackWasCalled = false;
static bool shouldTerminateCallback(JSContextRef, void*)
{
    shouldTerminateCallbackWasCalled = true;
    return true;
}

bool cancelTerminateCallbackWasCalled = false;
static bool cancelTerminateCallback(JSContextRef, void*)
{
    cancelTerminateCallbackWasCalled = true;
    return false;
}

int extendTerminateCallbackCalled = 0;
static bool extendTerminateCallback(JSContextRef ctx, void*)
{
    extendTerminateCallbackCalled++;
    if (extendTerminateCallbackCalled == 1) {
        JSContextGroupRef contextGroup = JSContextGetGroup(ctx);
        JSContextGroupSetExecutionTimeLimit(contextGroup, .200f, extendTerminateCallback, nullptr);
        return false;
    }
    return true;
}

#if HAVE(MACH_EXCEPTIONS)
bool dispatchTerminateCallbackCalled = false;
static bool dispatchTermitateCallback(JSContextRef, void*)
{
    dispatchTerminateCallbackCalled = true;
    return true;
}
#endif

struct TierOptions {
    const char* tier;
    Seconds timeLimitAdjustment;
    const char* optionsStr;
};

static void testResetAfterTimeout(bool& failed)
{
    JSValueRef v = nullptr;
    JSValueRef exception = nullptr;
    const char* reentryScript = "100";
    JSStringRef script = JSStringCreateWithUTF8CString(reentryScript);
    v = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
    JSStringRelease(script);
    if (exception) {
        printf("FAIL: Watchdog timeout was not reset.\n");
        failed = true;
    } else if (!JSValueIsNumber(context, v) || JSValueToNumber(context, v, nullptr) != 100) {
        printf("FAIL: Script result is not as expected.\n");
        failed = true;
    }
}

int testExecutionTimeLimit()
{
    static const TierOptions tierOptionsList[] = {
        { "LLINT",    0_ms,   "--useConcurrentJIT=false --useLLInt=true --useJIT=false" },
        { "Baseline", 0_ms,   "--useConcurrentJIT=false --useLLInt=true --useJIT=true --useDFGJIT=false" },
        { "DFG",      200_ms,   "--useConcurrentJIT=false --useLLInt=true --useJIT=true --useDFGJIT=true --useFTLJIT=false" },
#if ENABLE(FTL_JIT)
        { "FTL",      500_ms, "--useConcurrentJIT=false --useLLInt=true --useJIT=true --useDFGJIT=true --useFTLJIT=true" },
#endif
    };
    
    bool failed = false;

    JSC::initializeThreading();

    for (auto tierOptions : tierOptionsList) {
        StringBuilder savedOptionsBuilder;
        Options::dumpAllOptionsInALine(savedOptionsBuilder);

        Options::setOptions(tierOptions.optionsStr);
        
        Seconds tierAdjustment = tierOptions.timeLimitAdjustment;
        Seconds timeLimit;

        context = JSGlobalContextCreateInGroup(nullptr, nullptr);

        JSContextGroupRef contextGroup = JSContextGetGroup(context);
        JSObjectRef globalObject = JSContextGetGlobalObject(context);
        ASSERT(JSValueIsObject(context, globalObject));

        JSValueRef exception = nullptr;

        JSStringRef currentCPUTimeStr = JSStringCreateWithUTF8CString("currentCPUTime");
        JSObjectRef currentCPUTimeFunction = JSObjectMakeFunctionWithCallback(context, currentCPUTimeStr, currentCPUTimeAsJSFunctionCallback);
        JSObjectSetProperty(context, globalObject, currentCPUTimeStr, currentCPUTimeFunction, kJSPropertyAttributeNone, nullptr);
        JSStringRelease(currentCPUTimeStr);

        /* Test script on another thread: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), shouldTerminateCallback, nullptr);
        {
#if OS(LINUX) && (CPU(MIPS) || CPU(ARM_THUMB2))
            Seconds timeAfterWatchdogShouldHaveFired = 500_ms + tierAdjustment;
#else
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;
#endif

            JSStringRef script = JSStringCreateWithUTF8CString("function foo() { while (true) { } } foo();");
            exception = nullptr;
            JSValueRef* exn = &exception;
            shouldTerminateCallbackWasCalled = false;
            auto thread = Thread::create("Rogue thread", [=] {
                JSEvaluateScript(context, script, nullptr, nullptr, 1, exn);
            });

            sleep(timeAfterWatchdogShouldHaveFired);

            if (shouldTerminateCallbackWasCalled)
                printf("PASS: %s script timed out as expected.\n", tierOptions.tier);
            else {
                printf("FAIL: %s script timeout callback was not called.\n", tierOptions.tier);
                exit(1);
            }

            if (!exception) {
                printf("FAIL: %s TerminatedExecutionException was not thrown.\n", tierOptions.tier);
                exit(1);
            }

            thread->waitForCompletion();
            testResetAfterTimeout(failed);

            JSStringRelease(script);
        }

        /* Test script timeout: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), shouldTerminateCallback, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;

            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "while (true) {"
                        "for (var i = 0; i < 1000; i++);"
                        "if (currentCPUTime() - startTime > ", timeAfterWatchdogShouldHaveFired.seconds(), ") break;"
                    "}"
                "}"
                "foo();"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            shouldTerminateCallbackWasCalled = false;
            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            JSStringRelease(script);

            if (((endTime - startTime) < timeAfterWatchdogShouldHaveFired) && shouldTerminateCallbackWasCalled)
                printf("PASS: %s script timed out as expected.\n", tierOptions.tier);
            else {
                if ((endTime - startTime) >= timeAfterWatchdogShouldHaveFired)
                    printf("FAIL: %s script did not time out as expected.\n", tierOptions.tier);
                if (!shouldTerminateCallbackWasCalled)
                    printf("FAIL: %s script timeout callback was not called.\n", tierOptions.tier);
                failed = true;
            }
            
            if (!exception) {
                printf("FAIL: %s TerminatedExecutionException was not thrown.\n", tierOptions.tier);
                failed = true;
            }

            testResetAfterTimeout(failed);
        }

        /* Test script timeout with tail calls: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), shouldTerminateCallback, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;

            CString scriptText = makeString(
                "var startTime = currentCPUTime();"
                "function recurse(i) {"
                    "'use strict';"
                    "if (i % 1000 === 0) {"
                        "if (currentCPUTime() - startTime >", timeAfterWatchdogShouldHaveFired.seconds(), ") { return; }"
                    "}"
                "return recurse(i + 1); }"
                "recurse(0);"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            shouldTerminateCallbackWasCalled = false;
            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            JSStringRelease(script);

            if (((endTime - startTime) < timeAfterWatchdogShouldHaveFired) && shouldTerminateCallbackWasCalled)
                printf("PASS: %s script with infinite tail calls timed out as expected .\n", tierOptions.tier);
            else {
                if ((endTime - startTime) >= timeAfterWatchdogShouldHaveFired)
                    printf("FAIL: %s script with infinite tail calls did not time out as expected.\n", tierOptions.tier);
                if (!shouldTerminateCallbackWasCalled)
                    printf("FAIL: %s script with infinite tail calls' timeout callback was not called.\n", tierOptions.tier);
                failed = true;
            }
            
            if (!exception) {
                printf("FAIL: %s TerminatedExecutionException was not thrown.\n", tierOptions.tier);
                failed = true;
            }

            testResetAfterTimeout(failed);
        }

        /* Test the script timeout's TerminatedExecutionException should NOT be catchable: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), shouldTerminateCallback, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;
            
            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "try {"
                        "while (true) {"
                            "for (var i = 0; i < 1000; i++);"
                                "if (currentCPUTime() - startTime > ", timeAfterWatchdogShouldHaveFired.seconds(), ") break;"
                        "}"
                    "} catch(e) { }"
                "}"
                "foo();"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            shouldTerminateCallbackWasCalled = false;

            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            
            JSStringRelease(script);

            if (((endTime - startTime) >= timeAfterWatchdogShouldHaveFired) || !shouldTerminateCallbackWasCalled) {
                if (!((endTime - startTime) < timeAfterWatchdogShouldHaveFired))
                    printf("FAIL: %s script did not time out as expected.\n", tierOptions.tier);
                if (!shouldTerminateCallbackWasCalled)
                    printf("FAIL: %s script timeout callback was not called.\n", tierOptions.tier);
                failed = true;
            }
            
            if (exception)
                printf("PASS: %s TerminatedExecutionException was not catchable as expected.\n", tierOptions.tier);
            else {
                printf("FAIL: %s TerminatedExecutionException was caught.\n", tierOptions.tier);
                failed = true;
            }

            testResetAfterTimeout(failed);
        }
        
        /* Test script timeout with no callback: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), nullptr, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;
            
            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "while (true) {"
                        "for (var i = 0; i < 1000; i++);"
                            "if (currentCPUTime() - startTime > ", timeAfterWatchdogShouldHaveFired.seconds(), ") break;"
                    "}"
                "}"
                "foo();"
            ).utf8();
            
            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            shouldTerminateCallbackWasCalled = false;

            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            
            JSStringRelease(script);

            if (((endTime - startTime) < timeAfterWatchdogShouldHaveFired) && !shouldTerminateCallbackWasCalled)
                printf("PASS: %s script timed out as expected when no callback is specified.\n", tierOptions.tier);
            else {
                if ((endTime - startTime) >= timeAfterWatchdogShouldHaveFired)
                    printf("FAIL: %s script did not time out as expected when no callback is specified.\n", tierOptions.tier);
                else
                    printf("FAIL: %s script called stale callback function.\n", tierOptions.tier);
                failed = true;
            }
            
            if (!exception) {
                printf("FAIL: %s TerminatedExecutionException was not thrown.\n", tierOptions.tier);
                failed = true;
            }

            testResetAfterTimeout(failed);
        }
        
        /* Test script timeout cancellation: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), cancelTerminateCallback, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;
            
            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "while (true) {"
                        "for (var i = 0; i < 1000; i++);"
                            "if (currentCPUTime() - startTime > ", timeAfterWatchdogShouldHaveFired.seconds(), ") break;"
                    "}"
                "}"
                "foo();"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            cancelTerminateCallbackWasCalled = false;

            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            
            JSStringRelease(script);

            if (((endTime - startTime) >= timeAfterWatchdogShouldHaveFired) && cancelTerminateCallbackWasCalled && !exception)
                printf("PASS: %s script timeout was cancelled as expected.\n", tierOptions.tier);
            else {
                if (((endTime - startTime) < timeAfterWatchdogShouldHaveFired) || exception)
                    printf("FAIL: %s script timeout was not cancelled.\n", tierOptions.tier);
                if (!cancelTerminateCallbackWasCalled)
                    printf("FAIL: %s script timeout callback was not called.\n", tierOptions.tier);
                failed = true;
            }
            
            if (exception) {
                printf("FAIL: %s Unexpected TerminatedExecutionException thrown.\n", tierOptions.tier);
                failed = true;
            }
        }
        
        /* Test script timeout extension: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), extendTerminateCallback, nullptr);
        {
            Seconds timeBeforeExtendedDeadline = 250_ms + tierAdjustment;
            Seconds timeAfterExtendedDeadline = 600_ms + tierAdjustment;
            Seconds maxBusyLoopTime = 750_ms + tierAdjustment;

            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "while (true) {"
                        "for (var i = 0; i < 1000; i++);"
                            "if (currentCPUTime() - startTime > ", maxBusyLoopTime.seconds(), ") break;"
                    "}"
                "}"
                "foo();"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            extendTerminateCallbackCalled = 0;

            auto startTime = CPUTime::forCurrentThread();
            JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
            auto endTime = CPUTime::forCurrentThread();
            auto deltaTime = endTime - startTime;
            
            JSStringRelease(script);

            if ((deltaTime >= timeBeforeExtendedDeadline) && (deltaTime < timeAfterExtendedDeadline) && (extendTerminateCallbackCalled == 2) && exception)
                printf("PASS: %s script timeout was extended as expected.\n", tierOptions.tier);
            else {
                if (deltaTime < timeBeforeExtendedDeadline)
                    printf("FAIL: %s script timeout was not extended as expected.\n", tierOptions.tier);
                else if (deltaTime >= timeAfterExtendedDeadline)
                    printf("FAIL: %s script did not timeout.\n", tierOptions.tier);
                
                if (extendTerminateCallbackCalled < 1)
                    printf("FAIL: %s script timeout callback was not called.\n", tierOptions.tier);
                if (extendTerminateCallbackCalled < 2)
                    printf("FAIL: %s script timeout callback was not called after timeout extension.\n", tierOptions.tier);
                
                if (!exception)
                    printf("FAIL: %s TerminatedExecutionException was not thrown during timeout extension test.\n", tierOptions.tier);
                
                failed = true;
            }
        }

#if HAVE(MACH_EXCEPTIONS)
        /* Test script timeout from dispatch queue: */
        timeLimit = 100_ms + tierAdjustment;
        JSContextGroupSetExecutionTimeLimit(contextGroup, timeLimit.seconds(), dispatchTermitateCallback, nullptr);
        {
            Seconds timeAfterWatchdogShouldHaveFired = 300_ms + tierAdjustment;

            CString scriptText = makeString(
                "function foo() {"
                    "var startTime = currentCPUTime();"
                    "while (true) {"
                        "for (var i = 0; i < 1000; i++);"
                            "if (currentCPUTime() - startTime > ", timeAfterWatchdogShouldHaveFired.seconds(), ") break;"
                    "}"
                "}"
                "foo();"
            ).utf8();

            JSStringRef script = JSStringCreateWithUTF8CString(scriptText.data());
            exception = nullptr;
            dispatchTerminateCallbackCalled = false;

            // We have to do this since blocks can only capture things as const.
            JSGlobalContextRef& contextRef = context;
            JSStringRef& scriptRef = script;
            JSValueRef& exceptionRef = exception;

            Lock syncLock;
            Lock& syncLockRef = syncLock;
            Condition synchronize;
            Condition& synchronizeRef = synchronize;
            bool didSynchronize = false;
            bool& didSynchronizeRef = didSynchronize;

            Seconds startTime;
            Seconds endTime;

            Seconds& startTimeRef = startTime;
            Seconds& endTimeRef = endTime;

            dispatch_group_t group = dispatch_group_create();
            dispatch_group_async(group, dispatch_get_global_queue(0, 0), ^{
                startTimeRef = CPUTime::forCurrentThread();
                JSEvaluateScript(contextRef, scriptRef, nullptr, nullptr, 1, &exceptionRef);
                endTimeRef = CPUTime::forCurrentThread();
                auto locker = WTF::holdLock(syncLockRef);
                didSynchronizeRef = true;
                synchronizeRef.notifyAll();
            });

            auto locker = holdLock(syncLock);
            synchronize.wait(syncLock, [&] { return didSynchronize; });

            if (((endTime - startTime) < timeAfterWatchdogShouldHaveFired) && dispatchTerminateCallbackCalled)
                printf("PASS: %s script on dispatch queue timed out as expected.\n", tierOptions.tier);
            else {
                if ((endTime - startTime) >= timeAfterWatchdogShouldHaveFired)
                    printf("FAIL: %s script on dispatch queue did not time out as expected.\n", tierOptions.tier);
                if (!shouldTerminateCallbackWasCalled)
                    printf("FAIL: %s script on dispatch queue timeout callback was not called.\n", tierOptions.tier);
                failed = true;
            }

            JSStringRelease(script);
        }
#endif

        JSGlobalContextRelease(context);

        Options::setOptions(savedOptionsBuilder.toString().ascii().data());
    }
    
    return failed;
}
