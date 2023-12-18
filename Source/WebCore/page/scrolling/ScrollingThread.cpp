/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
#include "ScrollingThread.h"

#if ENABLE(SCROLLING_THREAD) || ENABLE(THREADED_ANIMATION_RESOLUTION)

#include <mutex>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ScrollingThread& ScrollingThread::singleton()
{
    static LazyNeverDestroyed<ScrollingThread> scrollingThread;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        scrollingThread.construct();
    });

    return scrollingThread;
}

ScrollingThread::ScrollingThread()
    : m_runLoop(RunLoop::create("WebCore: Scrolling"_s, ThreadType::Graphics, Thread::QOS::UserInteractive))
{
}

bool ScrollingThread::isCurrentThread()
{
    return ScrollingThread::singleton().m_runLoop->isCurrent();
}

void ScrollingThread::dispatch(Function<void ()>&& function)
{
    ScrollingThread::singleton().runLoop().dispatch(WTFMove(function));
}

void ScrollingThread::dispatchBarrier(Function<void ()>&& function)
{
    dispatch([function = WTFMove(function)]() mutable {
        callOnMainThread(WTFMove(function));
    });
}

} // namespace WebCore

#endif // ENABLE(SCROLLING_THREAD) || ENABLE(THREADED_ANIMATION_RESOLUTION)
