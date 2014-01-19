/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebCoreThreadRun.h"

#if PLATFORM(IOS)

#include "WebCoreThread.h"
#include "WebCoreThreadInternal.h"
#include <condition_variable>
#include <wtf/Vector.h>

namespace {

class WebThreadBlockState {
public:
    WebThreadBlockState()
        : m_completed(false)
    {
    }

    void waitForCompletion()
    {
        std::unique_lock<std::mutex> lock(m_stateMutex);

        m_completionConditionVariable.wait(lock, [this] { return m_completed; });
    }

    void setCompleted()
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        ASSERT(!m_completed);
        m_completed = true;
        m_completionConditionVariable.notify_one();
    }

private:
    std::mutex m_stateMutex;
    std::condition_variable m_completionConditionVariable;
    bool m_completed;
};

class WebThreadBlock {
public:
    WebThreadBlock(void (^task)(), WebThreadBlockState* state)
        : m_task(Block_copy(task))
        , m_state(state)
    {
    }

    WebThreadBlock(const WebThreadBlock& other)
        : m_task(Block_copy(other.m_task))
        , m_state(other.m_state)
    {
    }

    WebThreadBlock& operator=(const WebThreadBlock& other)
    {
        void (^oldTask)() = m_task;
        m_task = Block_copy(other.m_task);
        Block_release(oldTask);
        m_state = other.m_state;
        return *this;
    }

    ~WebThreadBlock()
    {
        Block_release(m_task);
    }

    void operator()() const
    {
        m_task();
        if (m_state)
            m_state->setCompleted();
    }

private:
    void (^m_task)();
    WebThreadBlockState* m_state;
};

}

extern "C" {

typedef WTF::Vector<WebThreadBlock> WebThreadRunQueue;

static std::mutex* runQueueMutex;
static CFRunLoopSourceRef runSource;
static WebThreadRunQueue* runQueue;

static void HandleRunSource(void *info)
{
    UNUSED_PARAM(info);
    ASSERT(WebThreadIsCurrent());
    ASSERT(runQueueMutex);
    ASSERT(runSource);
    ASSERT(runQueue);

    WebThreadRunQueue queueCopy;
    {
        std::lock_guard<std::mutex> lock(*runQueueMutex);
        queueCopy = *runQueue;
        runQueue->clear();
    }

    for (const auto& block : queueCopy)
        block();
}

static void _WebThreadRun(void (^task)(), bool synchronous)
{
    if (WebThreadIsCurrent() || !WebThreadIsEnabled()) {
        task();
        return;
    }

    ASSERT(runQueueMutex);
    ASSERT(runSource);
    ASSERT(runQueue);

    WebThreadBlockState* state = 0;
    if (synchronous)
        state = new WebThreadBlockState;

    {
        std::lock_guard<std::mutex> lock(*runQueueMutex);
        runQueue->append(WebThreadBlock(task, state));
    }

    CFRunLoopSourceSignal(runSource);
    CFRunLoopWakeUp(WebThreadRunLoop());

    if (synchronous) {
        state->waitForCompletion();
        delete state;
    }
}

void WebThreadRun(void (^task)())
{
    _WebThreadRun(task, false);
}

void WebThreadRunSync(void (^task)())
{
    _WebThreadRun(task, true);
}
    
void WebThreadInitRunQueue()
{
    ASSERT(!runQueue);
    ASSERT(!runQueueMutex);
    ASSERT(!runSource);

    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        runQueue = new WebThreadRunQueue;

        CFRunLoopSourceContext runSourceContext = {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, HandleRunSource};
        runSource = CFRunLoopSourceCreate(NULL, -1, &runSourceContext);
        CFRunLoopAddSource(WebThreadRunLoop(), runSource, kCFRunLoopDefaultMode);

        runQueueMutex = std::make_unique<std::mutex>().release();
    });
}

}

#endif // PLATFORM(IOS)
