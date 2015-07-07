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
#include "WorkQueue.h"

void WorkQueue::dispatch(std::function<void ()> function)
{
    ref();
    dispatch_async(m_dispatchQueue, ^{
        function();
        deref();
    });
}

void WorkQueue::dispatchAfter(std::chrono::nanoseconds duration, std::function<void ()> function)
{
    ref();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, duration.count()), m_dispatchQueue, ^{
        function();
        deref();
    });
}

#if HAVE(QOS_CLASSES)
static dispatch_qos_class_t dispatchQOSClass(WorkQueue::QOS qos)
{
    switch (qos) {
    case WorkQueue::QOS::UserInteractive:
        return QOS_CLASS_USER_INTERACTIVE;
    case WorkQueue::QOS::UserInitiated:
        return QOS_CLASS_USER_INITIATED;
    case WorkQueue::QOS::Default:
        return QOS_CLASS_DEFAULT;
    case WorkQueue::QOS::Utility:
        return QOS_CLASS_UTILITY;
    case WorkQueue::QOS::Background:
        return QOS_CLASS_BACKGROUND;
    }
}
#endif

void WorkQueue::platformInitialize(const char* name, QOS qos)
{
    dispatch_queue_attr_t attr = 0;
#if HAVE(QOS_CLASSES)
    attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, dispatchQOSClass(qos), 0);
#endif
    m_dispatchQueue = dispatch_queue_create(name, attr);
    dispatch_set_context(m_dispatchQueue, this);
}

void WorkQueue::platformInvalidate()
{
    dispatch_release(m_dispatchQueue);
}
