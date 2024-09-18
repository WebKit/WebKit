/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
#include <wtf/MainThread.h>

#include <mutex>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WTF {

void initializeMainThread()
{
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [] {
        initialize();
        initializeMainThreadPlatform();
        RunLoop::initializeMain();
    });
}

#if !USE(WEB_THREAD)
bool canCurrentThreadAccessThreadLocalData(Thread& thread)
{
    return &thread == &Thread::current();
}
#endif

bool isMainRunLoop()
{
    return RunLoop::isMain();
}

void callOnMainRunLoop(Function<void()>&& function)
{
    RunLoop::main().dispatch(WTFMove(function));
}

void ensureOnMainRunLoop(Function<void()>&& function)
{
    if (RunLoop::isMain())
        function();
    else
        RunLoop::main().dispatch(WTFMove(function));
}

void callOnMainThread(Function<void()>&& function)
{
#if USE(WEB_THREAD)
    if (auto* webRunLoop = RunLoop::webIfExists()) {
        webRunLoop->dispatch(WTFMove(function));
        return;
    }
#endif

    RunLoop::main().dispatch(WTFMove(function));
}

void ensureOnMainThread(Function<void()>&& function)
{
    if (isMainThread())
        function();
    else
        callOnMainThread(WTFMove(function));
}

bool isMainThreadOrGCThread()
{
    if (Thread::mayBeGCThread())
        return true;

    return isMainThread();
}

enum class MainStyle : bool {
    Thread,
    RunLoop
};

template <MainStyle mainStyle>
static void callOnMainAndWait(Function<void()>&& function)
{

    if (mainStyle == MainStyle::Thread ? isMainThread() : isMainRunLoop()) {
        function();
        return;
    }

    BinarySemaphore semaphore;
    auto functionImpl = [&semaphore, function = WTFMove(function)] {
        function();
        semaphore.signal();
    };

    switch (mainStyle) {
    case MainStyle::Thread:
        callOnMainThread(WTFMove(functionImpl));
        break;
    case MainStyle::RunLoop:
        callOnMainRunLoop(WTFMove(functionImpl));
    };
    semaphore.wait();
}

void callOnMainRunLoopAndWait(Function<void()>&& function)
{
    callOnMainAndWait<MainStyle::RunLoop>(WTFMove(function));
}

void callOnMainThreadAndWait(Function<void()>&& function)
{
    callOnMainAndWait<MainStyle::Thread>(WTFMove(function));
}

} // namespace WTF
