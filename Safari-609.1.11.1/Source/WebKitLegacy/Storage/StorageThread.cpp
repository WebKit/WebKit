/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StorageThread.h"

#include <wtf/AutodrainedPool.h>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashSet<StorageThread*>& activeStorageThreads()
{
    ASSERT(isMainThread());
    static NeverDestroyed<HashSet<StorageThread*>> threads;
    return threads;
}

StorageThread::StorageThread()
{
    ASSERT(isMainThread());
}

StorageThread::~StorageThread()
{
    ASSERT(isMainThread());
    ASSERT(!m_thread);
}

void StorageThread::start()
{
    ASSERT(isMainThread());
    if (!m_thread) {
        m_thread = Thread::create("WebCore: LocalStorage", [this] {
            threadEntryPoint();
        });
    }
    activeStorageThreads().add(this);
}

void StorageThread::threadEntryPoint()
{
    ASSERT(!isMainThread());

    while (auto function = m_queue.waitForMessage()) {
        AutodrainedPool pool;
        (*function)();
    }
}

void StorageThread::dispatch(Function<void ()>&& function)
{
    ASSERT(isMainThread());
    ASSERT(!m_queue.killed() && m_thread);
    m_queue.append(makeUnique<Function<void ()>>(WTFMove(function)));
}

void StorageThread::terminate()
{
    ASSERT(isMainThread());
    ASSERT(!m_queue.killed() && m_thread);
    activeStorageThreads().remove(this);
    // Even in weird, exceptional cases, don't wait on a nonexistent thread to terminate.
    if (!m_thread)
        return;

    m_queue.append(makeUnique<Function<void ()>>([this] {
        performTerminate();
    }));
    m_thread->waitForCompletion();
    ASSERT(m_queue.killed());
    m_thread = nullptr;
}

void StorageThread::performTerminate()
{
    ASSERT(!isMainThread());
    m_queue.kill();
}

void StorageThread::releaseFastMallocFreeMemoryInAllThreads()
{
    for (auto& thread : activeStorageThreads())
        thread->dispatch(&WTF::releaseFastMallocFreeMemory);
}

}
