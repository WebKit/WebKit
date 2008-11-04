/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "MainThread.h"

#include "Threading.h"
#include "Vector.h"

namespace WTF {

struct FunctionWithContext {
    MainThreadFunction* function;
    void* context;
    ThreadCondition* syncFlag;

    FunctionWithContext(MainThreadFunction* function = 0, void* context = 0, ThreadCondition* syncFlag = 0)
        : function(function)
        , context(context)
        , syncFlag(syncFlag)
    { 
    }
};

typedef Vector<FunctionWithContext> FunctionQueue;

static bool callbacksPaused; // This global variable is only accessed from main thread.

Mutex& mainThreadFunctionQueueMutex()
{
    static Mutex& staticMutex = *new Mutex;
    return staticMutex;
}

static FunctionQueue& functionQueue()
{
    static FunctionQueue& staticFunctionQueue = *new FunctionQueue;
    return staticFunctionQueue;
}

#if !PLATFORM(WIN)
void initializeMainThread()
{
    mainThreadFunctionQueueMutex();
}
#endif

void dispatchFunctionsFromMainThread()
{
    ASSERT(isMainThread());

    if (callbacksPaused)
        return;

    FunctionQueue queueCopy;
    {
        MutexLocker locker(mainThreadFunctionQueueMutex());
        queueCopy.swap(functionQueue());
    }

    for (unsigned i = 0; i < queueCopy.size(); ++i) {
        FunctionWithContext& invocation = queueCopy[i];
        invocation.function(invocation.context);
        if (invocation.syncFlag)
            invocation.syncFlag->signal();
    }
}

void callOnMainThread(MainThreadFunction* function, void* context)
{
    ASSERT(function);

    {
        MutexLocker locker(mainThreadFunctionQueueMutex());
        functionQueue().append(FunctionWithContext(function, context));
    }

    scheduleDispatchFunctionsOnMainThread();
}

void callOnMainThreadAndWait(MainThreadFunction* function, void* context)
{
    ASSERT(function);

    if (isMainThread()) {
        function(context);
        return;
    }

    ThreadCondition syncFlag;
    Mutex conditionMutex;

    {
        MutexLocker locker(mainThreadFunctionQueueMutex());
        functionQueue().append(FunctionWithContext(function, context, &syncFlag));
        conditionMutex.lock();
    }

    scheduleDispatchFunctionsOnMainThread();
    syncFlag.wait(conditionMutex);
}

void setMainThreadCallbacksPaused(bool paused)
{
    ASSERT(isMainThread());

    if (callbacksPaused == paused)
        return;

    callbacksPaused = paused;

    if (!callbacksPaused)
        scheduleDispatchFunctionsOnMainThread();
}

} // namespace WTF
