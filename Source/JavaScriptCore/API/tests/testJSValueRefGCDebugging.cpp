/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#define JSC_DEBUG_CHECK_FOR_UNPROTECTED_JSVALUEREF_STORES_TO_HEAP 1
#include <wtf/DataLog.h>
#include <wtf/Deque.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/SharedTask.h>
#include <wtf/NumberOfCores.h>
// We probably don't want to include any private JSC headers here since they probably won't play nicely with our redefinition of JSValueRef/JSObjectRef.
#include <JavaScriptCore/JavaScript.h>


extern "C" unsigned testJSValueRefGCDebugging(const char* filter);

static thread_local const struct OpaqueJSValue* lastValue;
static void reportCallbackInvoked(const JSValueRef* value, const char*)
{
    lastValue = value->m_value;
}

static std::atomic<unsigned> failed = 0;
static bool check(bool condition, auto... messages)
{
    if (!condition) {
        dataLogLn(messages..., ": FAILED");
        failed++;
    } else
        dataLogLn(messages..., ": PASSED");

    return condition;
}

static void testStoreToStatic()
{
    static JSValueRef value;

    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    value = JSObjectMake(context, nullptr, nullptr);
    check(value.m_value == lastValue, "store to static was noted");
    JSGlobalContextRelease(context);
}

static void testStoreToHeap()
{
    auto ptr = std::make_unique<JSValueRef>();

    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    *ptr = JSObjectMake(context, nullptr, nullptr);
    check(ptr->m_value == lastValue, "store to heap was noted");
    JSGlobalContextRelease(context);
}

static void testStoreToStack()
{
    JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
    JSValueRef value = JSObjectMake(context, nullptr, nullptr);
    check(value.m_value != lastValue, "store to stack was not noted");
    JSGlobalContextRelease(context);
}

#define RUN(test) do {                                 \
        if (!shouldRun(#test))                         \
            break;                                     \
        tasks.append(                                  \
            createSharedTask<void()>(                  \
                [&] () {                               \
                    test;                              \
                    dataLog(#test ": Finished!\n");          \
                }));                                   \
    } while (false)

unsigned testJSValueRefGCDebugging(const char* filter)
{
    dataLogLn("Starting JSValueRef GC debugging tests");

    Deque<RefPtr<SharedTask<void()>>> tasks;

    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || WTF::findIgnoringASCIICaseWithoutLength(testName, filter) != WTF::notFound;
    };

    SetPotentiallyInvalidJSValueRefCallback(reportCallbackInvoked);

    RUN(testStoreToStatic());
    RUN(testStoreToHeap());
    RUN(testStoreToStack());

    if (tasks.isEmpty())
        return 0;

    Lock lock;
    Vector<Ref<Thread>> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(Thread::create(
            "TestJSValueRefGCDebugging"_s,
            [&] () {
                for (;;) {
                    RefPtr<SharedTask<void()>> task;
                    {
                        Locker locker { lock };
                        if (tasks.isEmpty())
                            break;
                        task = tasks.takeFirst();
                    }

                    task->run();
                }
            }));
    }

    for (auto& thread : threads)
        thread->waitForCompletion();

    return failed;
}


