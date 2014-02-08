/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingThread_h
#define ScrollingThread_h

#if ENABLE(ASYNC_SCROLLING)

#include <condition_variable>
#include <functional>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class ScrollingThread {
    WTF_MAKE_NONCOPYABLE(ScrollingThread);

public:
    static bool isCurrentThread();
    static void dispatch(std::function<void ()>);

    // Will dispatch the given function on the main thread once all pending functions
    // on the scrolling thread have finished executing. Used for synchronization purposes.
    static void dispatchBarrier(std::function<void ()>);

private:
    friend NeverDestroyed<ScrollingThread>;

    ScrollingThread();

    static ScrollingThread& shared();

    void createThreadIfNeeded();
    static void threadCallback(void* scrollingThread);
    void threadBody();
    void dispatchFunctionsFromScrollingThread();

    void initializeRunLoop();
    void wakeUpRunLoop();

#if PLATFORM(COCOA)
    static void threadRunLoopSourceCallback(void* scrollingThread);
    void threadRunLoopSourceCallback();
#endif

    ThreadIdentifier m_threadIdentifier;

    std::condition_variable m_initializeRunLoopConditionVariable;
    std::mutex m_initializeRunLoopMutex;

    std::mutex m_functionsMutex;
    Vector<std::function<void ()>> m_functions;

#if PLATFORM(COCOA)
    // FIXME: We should use WebCore::RunLoop here.
    RetainPtr<CFRunLoopRef> m_threadRunLoop;
    RetainPtr<CFRunLoopSourceRef> m_threadRunLoopSource;
#endif
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)

#endif // ScrollingThread_h
