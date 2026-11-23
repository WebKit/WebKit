/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(GPU_PROCESS)
#include "RemoteSharedResourceCacheProxy.h"

#include "RemoteNativeImageProxy.h"
#include "RemoteSharedResourceCacheMessages.h"
#include "WebProcess.h"
#include <WebCore/ShareableBitmap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {
using namespace WebCore;

namespace {

// Outlives a waiter that times out, so that a late reply has somewhere to go.
class NativeImagePixelsReply : public ThreadSafeRefCounted<NativeImagePixelsReply> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(NativeImagePixelsReply);
public:
    static Ref<NativeImagePixelsReply> create() { return adoptRef(*new NativeImagePixelsReply); }

    void setHandle(std::optional<ShareableBitmap::Handle>&& handle)
    {
        {
            Locker locker { m_lock };
            m_handle = WTF::move(handle);
        }
        m_semaphore.signal();
    }

    std::optional<ShareableBitmap::Handle> waitForHandle(Seconds timeout)
    {
        if (!m_semaphore.waitFor(timeout))
            return std::nullopt;
        Locker locker { m_lock };
        return std::exchange(m_handle, std::nullopt);
    }

private:
    NativeImagePixelsReply() = default;

    Lock m_lock;
    std::optional<ShareableBitmap::Handle> m_handle WTF_GUARDED_BY_LOCK(m_lock);
    BinarySemaphore m_semaphore;
};

}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSharedResourceCacheProxy);

Ref<RemoteSharedResourceCacheProxy> RemoteSharedResourceCacheProxy::create(IPC::Connection& connection)
{
    return adoptRef(*new RemoteSharedResourceCacheProxy(connection));
}

RemoteSharedResourceCacheProxy::RemoteSharedResourceCacheProxy(IPC::Connection& connection)
    : m_connection(connection)
    , m_replyQueue(WorkQueue::create("org.webkit.RemoteSharedResourceCacheProxy"_s, WorkQueue::QOS::UserInteractive))
{
}

RemoteSharedResourceCacheProxy::~RemoteSharedResourceCacheProxy() = default;

void RemoteSharedResourceCacheProxy::releaseNativeImage(const RemoteNativeImageProxy& image)
{
    // The write reference carries the number of reads handed out (pendingReads), so the GPU process
    // removes the entry once that many reads have been retired.
    m_connection->send(Messages::RemoteSharedResourceCache::ReleaseNativeImage(image.writeReference()), 0);
}

RefPtr<ShareableBitmap> RemoteSharedResourceCacheProxy::nativeImagePixels(const RemoteNativeImageProxy& image)
{
    Ref replyQueue = m_replyQueue;
    // The reply is delivered on the reply queue, so waiting there would never end.
    ASSERT(!replyQueue->isCurrent());
    Ref reply = NativeImagePixelsReply::create();
    // A closed connection cancels the reply, which wakes the waiter with no handle.
    m_connection->sendWithAsyncReplyOnDispatcher(Messages::RemoteSharedResourceCache::GetNativeImagePixels(image.newReadReference()), replyQueue, [reply](std::optional<ShareableBitmap::Handle>&& handle) {
        reply->setHandle(WTF::move(handle));
    });
    auto handle = reply->waitForHandle(WebProcess::singleton().gpuProcessTimeoutDuration());
    if (!handle)
        return nullptr;
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    return ShareableBitmap::create(WTF::move(*handle));
}

}

#endif
