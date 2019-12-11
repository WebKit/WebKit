/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *           (C) 2012 Patrick Ganstere <paroga@paroga.com>
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
 
#include "config.h"
#include "JavaScriptThreading.h"

#include <JavaScriptCore/JavaScriptCore.h>
#include <stdlib.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Threading.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

static const size_t javaScriptThreadsCount = 4;
static bool javaScriptThreadsShouldTerminate;
static JSContextGroupRef javaScriptThreadsGroup;
static Lock javaScriptThreadsLock;

typedef HashSet<RefPtr<Thread>> ThreadSet;
static ThreadSet& javaScriptThreads()
{
    static NeverDestroyed<ThreadSet> staticJavaScriptThreads;
    ASSERT(!javaScriptThreadsLock.tryLock());
    return staticJavaScriptThreads;
}

// This function exercises JSC in a loop until javaScriptThreadsShouldTerminate
// becomes true or it probabilistically decides to spawn a replacement thread and exit.
void runJavaScriptThread()
{
    static const char* const script =
        "var array = [];"
        "for (var i = 0; i < 1024; i++) {"
        "    array.push(String(i));"
        "}";

    JSGlobalContextRef ctx;
    {
        auto locker = holdLock(javaScriptThreadsLock);
        ctx = JSGlobalContextCreateInGroup(javaScriptThreadsGroup, 0);
    }

    JSStringRef scriptRef;
    {
        auto locker = holdLock(javaScriptThreadsLock);
        scriptRef = JSStringCreateWithUTF8CString(script);
    }

    while (true) {
        {
            auto locker = holdLock(javaScriptThreadsLock);
            JSValueRef exception = 0;
            JSEvaluateScript(ctx, scriptRef, 0, 0, 1, &exception);
            ASSERT(!exception);
        }

        {
            auto locker = holdLock(javaScriptThreadsLock);
            const size_t valuesCount = 1024;
            JSValueRef values[valuesCount];
            for (size_t i = 0; i < valuesCount; ++i)
                values[i] = JSObjectMake(ctx, 0, 0);
        }

        {
            auto locker = holdLock(javaScriptThreadsLock);
            if (javaScriptThreadsShouldTerminate)
                break;
        }

        // Respawn probabilistically.
        if (rand() % 5)
            continue;

        auto locker = holdLock(javaScriptThreadsLock);
        Thread& thread = Thread::current();
        thread.detach();
        javaScriptThreads().remove(&thread);
        javaScriptThreads().add(Thread::create("JavaScript Thread", &runJavaScriptThread));
        break;
    }

    auto locker = holdLock(javaScriptThreadsLock);
    JSStringRelease(scriptRef);
    JSGarbageCollect(ctx);
    JSGlobalContextRelease(ctx);
}

void startJavaScriptThreads()
{
    javaScriptThreadsGroup = JSContextGroupCreate();

    auto locker = holdLock(javaScriptThreadsLock);

    for (size_t i = 0; i < javaScriptThreadsCount; ++i)
        javaScriptThreads().add(Thread::create("JavaScript Thread", &runJavaScriptThread));
}

void stopJavaScriptThreads()
{
    {
        auto locker = holdLock(javaScriptThreadsLock);
        javaScriptThreadsShouldTerminate = true;
    }

    Vector<RefPtr<Thread>, javaScriptThreadsCount> threads;
    {
        auto locker = holdLock(javaScriptThreadsLock);
        threads = copyToVector(javaScriptThreads());
        ASSERT(threads.size() == javaScriptThreadsCount);
    }

    for (size_t i = 0; i < javaScriptThreadsCount; ++i)
        threads[i]->waitForCompletion();

    {
        auto locker = holdLock(javaScriptThreadsLock);
        javaScriptThreads().clear();
    }

    JSContextGroupRelease(javaScriptThreadsGroup);
}
