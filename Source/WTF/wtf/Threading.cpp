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

#include "dtoa.h"
#include "dtoa/cached-powers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <wtf/DateMath.h>
#include <wtf/RandomNumberSeed.h>
#include <wtf/ThreadHolder.h>
#include <wtf/ThreadMessage.h>
#include <wtf/WTFThreadData.h>
#include <wtf/text/StringView.h>

#if HAVE(QOS_CLASSES)
#include <bmalloc/bmalloc.h>
#endif

namespace WTF {

struct NewThreadContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    const char* name;
    std::function<void()> entryPoint;
    Mutex creationMutex;
};

const char* Thread::normalizeThreadName(const char* threadName)
{
#if HAVE(PTHREAD_SETNAME_NP)
    return threadName;
#else
    // This name can be com.apple.WebKit.ProcessLauncher or com.apple.CoreIPC.ReceiveQueue.
    // We are using those names for the thread name, but both are longer than the limit of
    // the platform thread name length, 32 for Windows and 16 for Linux.
    StringView result(threadName);
    size_t size = result.reverseFind('.');
    if (size != notFound)
        result = result.substring(size + 1);

#if OS(WINDOWS)
    constexpr const size_t kVisualStudioThreadNameLimit = 32 - 1;
    if (result.length() > kVisualStudioThreadNameLimit)
        result = result.right(kVisualStudioThreadNameLimit);
#elif OS(LINUX)
    constexpr const size_t kLinuxThreadNameLimit = 16 - 1;
    if (result.length() > kLinuxThreadNameLimit)
        result = result.right(kLinuxThreadNameLimit);
#endif
    ASSERT(result.characters8()[result.length()] == '\0');
    return reinterpret_cast<const char*>(result.characters8());
#endif
}

static void threadEntryPoint(void* contextData)
{
    NewThreadContext* context = static_cast<NewThreadContext*>(contextData);

    // Block until our creating thread has completed any extra setup work, including
    // establishing ThreadIdentifier.
    {
        MutexLocker locker(context->creationMutex);
    }

    Thread::initializeCurrentThreadInternal(context->name);

    auto entryPoint = WTFMove(context->entryPoint);

    // Delete the context before starting the thread.
    delete context;

    entryPoint();
}

RefPtr<Thread> Thread::create(const char* name, std::function<void()> entryPoint)
{
    NewThreadContext* context = new NewThreadContext { name, WTFMove(entryPoint), { } };

    // Prevent the thread body from executing until we've established the thread identifier.
    MutexLocker locker(context->creationMutex);

    return Thread::createInternal(threadEntryPoint, context, name);
}

RefPtr<Thread> Thread::create(ThreadFunction entryPoint, void* data, const char* name)
{
    return Thread::create(name, [entryPoint, data] {
        entryPoint(data);
    });
}

void Thread::didExit()
{
    std::unique_lock<std::mutex> locker(m_mutex);
    m_didExit = true;
}

void Thread::setCurrentThreadIsUserInteractive(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(adjustedQOSClass(QOS_CLASS_USER_INTERACTIVE), relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

void Thread::setCurrentThreadIsUserInitiated(int relativePriority)
{
#if HAVE(QOS_CLASSES)
    ASSERT(relativePriority <= 0);
    ASSERT(relativePriority >= QOS_MIN_RELATIVE_PRIORITY);
    pthread_set_qos_class_self_np(adjustedQOSClass(QOS_CLASS_USER_INITIATED), relativePriority);
#else
    UNUSED_PARAM(relativePriority);
#endif
}

#if HAVE(QOS_CLASSES)
static qos_class_t globalMaxQOSclass { QOS_CLASS_UNSPECIFIED };

void Thread::setGlobalMaxQOSClass(qos_class_t maxClass)
{
    bmalloc::api::setScavengerThreadQOSClass(maxClass);
    globalMaxQOSclass = maxClass;
}

qos_class_t Thread::adjustedQOSClass(qos_class_t originalClass)
{
    if (globalMaxQOSclass != QOS_CLASS_UNSPECIFIED)
        return std::min(originalClass, globalMaxQOSclass);
    return originalClass;
}
#endif

void Thread::dump(PrintStream& out) const
{
    out.print(m_id);
}

void initializeThreading()
{
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [] {
        WTF::double_conversion::initialize();
        ThreadHolder::initializeOnce();
        // StringImpl::empty() does not construct its static string in a threadsafe fashion,
        // so ensure it has been initialized from here.
        StringImpl::empty();
        initializeRandomNumberGenerator();
        wtfThreadData();
        initializeDates();
        Thread::initializePlatformThreading();
#if USE(PTHREADS)
        initializeThreadMessages();
#endif
    });
}

} // namespace WTF
