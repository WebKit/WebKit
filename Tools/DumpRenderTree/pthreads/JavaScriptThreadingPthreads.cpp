/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "JavaScriptThreading.h"

#include <CoreFoundation/CoreFoundation.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <pthread.h>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

static pthread_mutex_t javaScriptThreadsMutex = PTHREAD_MUTEX_INITIALIZER;
static bool javaScriptThreadsShouldTerminate;

static const int javaScriptThreadsCount = 4;

typedef HashSet<pthread_t> ThreadSet;

static ThreadSet* javaScriptThreads()
{
    ASSERT(pthread_mutex_trylock(&javaScriptThreadsMutex) == EBUSY);
    static ThreadSet staticJavaScriptThreads;
    return &staticJavaScriptThreads;
}

// Loops forever, running a script and randomly respawning, until 
// javaScriptThreadsShouldTerminate becomes true.
void* runJavaScriptThread(void* arg)
{
    const char* const script =
        "var array = [];"
        "for (var i = 0; i < 10; i++) {"
        "    array.push(String(i));"
        "}";

    while (1) {
        JSGlobalContextRef ctx = JSGlobalContextCreate(0);
        JSStringRef scriptRef = JSStringCreateWithUTF8CString(script);

        JSValueRef exception = 0;
        JSEvaluateScript(ctx, scriptRef, 0, 0, 1, &exception);
        ASSERT(!exception);

        JSGarbageCollect(ctx);
        JSGlobalContextRelease(ctx);
        JSStringRelease(scriptRef);
        
        JSGarbageCollect(0);

        pthread_mutex_lock(&javaScriptThreadsMutex);

        // Check for cancellation.
        if (javaScriptThreadsShouldTerminate) {
            javaScriptThreads()->remove(pthread_self());
            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        // Respawn probabilistically.
        if (random() % 5 == 0) {
            pthread_t pthread;
            pthread_create(&pthread, 0, &runJavaScriptThread, 0);
            pthread_detach(pthread);

            javaScriptThreads()->remove(pthread_self());
            javaScriptThreads()->add(pthread);

            pthread_mutex_unlock(&javaScriptThreadsMutex);
            return 0;
        }

        pthread_mutex_unlock(&javaScriptThreadsMutex);
    }
}

void startJavaScriptThreads()
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    for (int i = 0; i < javaScriptThreadsCount; i++) {
        pthread_t pthread;
        pthread_create(&pthread, 0, &runJavaScriptThread, 0);
        pthread_detach(pthread);
        javaScriptThreads()->add(pthread);
    }

    pthread_mutex_unlock(&javaScriptThreadsMutex);
}

void stopJavaScriptThreads()
{
    pthread_mutex_lock(&javaScriptThreadsMutex);

    javaScriptThreadsShouldTerminate = true;

    ASSERT(javaScriptThreads()->size() == javaScriptThreadsCount);

    pthread_mutex_unlock(&javaScriptThreadsMutex);

    while (true) {
        pthread_mutex_lock(&javaScriptThreadsMutex);
        int threadCount = javaScriptThreads()->size();
        pthread_mutex_unlock(&javaScriptThreadsMutex);

        if (!threadCount)
            break;

        usleep(1000);
    }
}
