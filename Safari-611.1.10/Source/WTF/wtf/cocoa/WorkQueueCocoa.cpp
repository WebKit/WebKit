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

void WorkQueue::dispatch(Function<void()>&& function)
{
    dispatch_async(m_dispatchQueue, makeBlockPtr([protectedThis = makeRef(*this), function = WTFMove(function)] {
        function();
    }).get());
}

void WorkQueue::dispatchAfter(Seconds duration, Function<void()>&& function)
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, duration.nanosecondsAs<int64_t>()), m_dispatchQueue, makeBlockPtr([protectedThis = makeRef(*this), function = WTFMove(function)] {
        function();
    }).get());
}

void WorkQueue::platformInitialize(const char* name, Type type, QOS qos)
{
    dispatch_queue_attr_t attr = type == Type::Concurrent ? DISPATCH_QUEUE_CONCURRENT : DISPATCH_QUEUE_SERIAL;
    attr = dispatch_queue_attr_make_with_qos_class(attr, Thread::dispatchQOSClass(qos), 0);
    m_dispatchQueue = dispatch_queue_create(name, attr);
    dispatch_set_context(m_dispatchQueue, this);
}

void WorkQueue::platformInvalidate()
{
    dispatch_release(m_dispatchQueue);
}

void WorkQueue::concurrentApply(size_t iterations, WTF::Function<void(size_t index)>&& function)
{
    dispatch_apply(iterations, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([function = WTFMove(function)](size_t index) {
        function(index);
    }).get());
}

}
