/*
 * Copyright (C) 2008, 2009, 2014 Apple Inc. All rights reserved.
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
#include "Threading.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace WTF {

struct NewThreadContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const char* name;
    std::function<void()> entryPoint;
    Mutex creationMutex;
};

static void threadEntryPoint(void* contextData)
{
    NewThreadContext* context = static_cast<NewThreadContext*>(contextData);

    // Block until our creating thread has completed any extra setup work, including
    // establishing ThreadIdentifier.
    {
        MutexLocker locker(context->creationMutex);
    }

    initializeCurrentThreadInternal(context->name);

    auto entryPoint = WTFMove(context->entryPoint);

    // Delete the context before starting the thread.
    delete context;

    entryPoint();
}

ThreadIdentifier createThread(const char* name, std::function<void()> entryPoint)
{
    // Visual Studio has a 31-character limit on thread names. Longer names will
    // be truncated silently, but we'd like callers to know about the limit.
#if !LOG_DISABLED && PLATFORM(WIN)
    if (name && strlen(name) > 31)
        LOG_ERROR("Thread name \"%s\" is longer than 31 characters and will be truncated by Visual Studio", name);
#endif

    NewThreadContext* context = new NewThreadContext { name, WTFMove(entryPoint), { } };

    // Prevent the thread body from executing until we've established the thread identifier.
    MutexLocker locker(context->creationMutex);

    return createThreadInternal(threadEntryPoint, context, name);
}

ThreadIdentifier createThread(ThreadFunction entryPoint, void* data, const char* name)
{
    return createThread(name, [entryPoint, data] {
        entryPoint(data);
    });
}

void setCurrentThreadIsUserInteractive(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

void setCurrentThreadIsUserInitiated(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

} // namespace WTF
