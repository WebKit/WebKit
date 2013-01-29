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

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <wtf/PassOwnPtr.h>

struct WorkQueueAndFunction {
    WorkQueueAndFunction(WorkQueue* workQueue, const Function<void()>& function)
        : workQueue(workQueue)
        , function(function)
    {
    }

    WorkQueue* workQueue;
    Function<void()> function;
};

void WorkQueue::executeFunction(void* context)
{
    OwnPtr<WorkQueueAndFunction> workQueueAndFunction = adoptPtr(static_cast<WorkQueueAndFunction*>(context));
    
    {
        MutexLocker locker(workQueueAndFunction->workQueue->m_isValidMutex);
        if (!workQueueAndFunction->workQueue->m_isValid)
            return;
    }

    (workQueueAndFunction->function)();
}

void WorkQueue::dispatch(const Function<void()>& function)
{
    dispatch_async_f(m_dispatchQueue, new WorkQueueAndFunction(this, function), executeFunction);
}

void WorkQueue::dispatchAfterDelay(const Function<void()>& function, double delay)
{
    dispatch_time_t delayTime = dispatch_time(DISPATCH_TIME_NOW, delay * NSEC_PER_SEC);
    dispatch_after_f(delayTime, m_dispatchQueue, new WorkQueueAndFunction(this, function), executeFunction);
}

void WorkQueue::platformInitialize(const char* name)
{
    m_dispatchQueue = dispatch_queue_create(name, 0);
    dispatch_set_context(m_dispatchQueue, this);
}

void WorkQueue::platformInvalidate()
{
    dispatch_release(m_dispatchQueue);
}
