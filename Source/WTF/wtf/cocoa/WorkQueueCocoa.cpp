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
#include <wtf/WorkQueue.h>

#include <wtf/BlockPtr.h>
#include <wtf/Ref.h>

namespace WTF {

namespace {

struct DispatchWorkItem {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Ref<WorkQueueBase> m_workQueue;
    Function<void()> m_function;
    void operator()() { m_function(); }
};

}

template<typename T> static void dispatchWorkItem(void* dispatchContext)
{
    T* item = reinterpret_cast<T*>(dispatchContext);
    (*item)();
    delete item;
}

void WorkQueueBase::dispatch(Function<void()>&& function)
{
    dispatch_async_f(m_dispatchQueue.get(), new DispatchWorkItem { Ref { *this }, WTFMove(function) }, dispatchWorkItem<DispatchWorkItem>);
}

void WorkQueueBase::dispatchWithQOS(Function<void()>&& function, QOS qos)
{
    dispatch_block_t blockWithQOS = dispatch_block_create_with_qos_class(DISPATCH_BLOCK_ENFORCE_QOS_CLASS, Thread::dispatchQOSClass(qos), 0, makeBlockPtr([function = WTFMove(function)] {
        function();
    }).get());
    dispatch_async(m_dispatchQueue.get(), blockWithQOS);
#if !__has_feature(objc_arc)
    Block_release(blockWithQOS);
#endif
}

void WorkQueueBase::dispatchAfter(Seconds duration, Function<void()>&& function)
{
    dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, duration.nanosecondsAs<int64_t>()), m_dispatchQueue.get(), new DispatchWorkItem { Ref { *this },  WTFMove(function) }, dispatchWorkItem<DispatchWorkItem>);
}

void WorkQueueBase::dispatchSync(Function<void()>&& function)
{
    dispatch_sync_f(m_dispatchQueue.get(), new Function<void()> { WTFMove(function) }, dispatchWorkItem<Function<void()>>);
}

WorkQueueBase::WorkQueueBase(OSObjectPtr<dispatch_queue_t>&& dispatchQueue)
    : m_dispatchQueue(WTFMove(dispatchQueue))
{
}

void WorkQueueBase::platformInitialize(const char* name, Type type, QOS qos)
{
    dispatch_queue_attr_t attr = type == Type::Concurrent ? DISPATCH_QUEUE_CONCURRENT : DISPATCH_QUEUE_SERIAL;
    attr = dispatch_queue_attr_make_with_qos_class(attr, Thread::dispatchQOSClass(qos), 0);
    m_dispatchQueue = adoptOSObject(dispatch_queue_create(name, attr));
    dispatch_set_context(m_dispatchQueue.get(), this);
#if ASSERT_ENABLED
    // We use &s_uid for the key, since it's convenient. Dispatch does not dereference it.
    // We use s_uid to generate the id so that WorkQueues and Threads share the id namespace.
    // This makes it possible to assert that code runs in the expected sequence, regardless of if it is
    // in a thread or a work queue.
    dispatch_queue_set_specific(m_dispatchQueue.get(), &s_uid, reinterpret_cast<void*>(static_cast<uintptr_t>(++s_uid)), nullptr);
#endif
}

void WorkQueueBase::platformInvalidate()
{
}

WorkQueue::WorkQueue(OSObjectPtr<dispatch_queue_t>&& queue)
    : WorkQueueBase(WTFMove(queue))
{
    // Note: for main work queue we do not create a sequence id, the main thread id will be used.
}

Ref<WorkQueue> WorkQueue::constructMainWorkQueue()
{
    return adoptRef(*new WorkQueue(dispatch_get_main_queue()));
}

#if ASSERT_ENABLED
ThreadLikeAssertion WorkQueue::threadLikeAssertion() const
{
    auto sequenceID = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_queue_get_specific(m_dispatchQueue.get(), &s_uid)));
    if (!sequenceID)
        sequenceID = Thread::current().uid(); // Main thread sequence id.
    return createThreadLikeAssertion(sequenceID);
}
#endif

void ConcurrentWorkQueue::apply(size_t iterations, WTF::Function<void(size_t index)>&& function)
{
    dispatch_apply(iterations, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([function = WTFMove(function)](size_t index) {
        function(index);
    }).get());
}

}
