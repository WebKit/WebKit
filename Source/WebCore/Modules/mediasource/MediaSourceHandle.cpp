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
#include "MediaSourceHandle.h"

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)

#include "MediaSource.h"
#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaSourceHandle);

class MediaSourceHandle::SharedPrivate final : public ThreadSafeRefCounted<SharedPrivate> {
public:
    static Ref<SharedPrivate> create(MediaSource& mediaSource, MediaSourceHandle::DispatcherType&& dispatcher) { return adoptRef(*new SharedPrivate(mediaSource, WTFMove(dispatcher))); }

    void dispatch(MediaSourceHandle::TaskType&& task, bool forceRunInWorker = false) const
    {
        m_dispatcher(WTFMove(task), forceRunInWorker);
    }

    void setMediaSourcePrivate(MediaSourcePrivate& mediaSourcePrivate)
    {
        Locker locker { m_lock };
        m_private = mediaSourcePrivate;
    }

    RefPtr<MediaSourcePrivate> mediaSourcePrivate() const
    {
        Locker locker { m_lock };
        return m_private.get();
    }

    friend class MediaSourceHandle;
    SharedPrivate(MediaSource& mediaSource, MediaSourceHandle::DispatcherType&& dispatcher)
        : m_client(mediaSource.client().get())
        , m_isManaged(mediaSource.isManaged())
        , m_dispatcher(WTFMove(dispatcher))
    {
    }
    mutable Lock m_lock;
    ThreadSafeWeakPtr<MediaSourcePrivate> m_private WTF_GUARDED_BY_LOCK(m_lock);
    std::atomic<bool> m_hasEverBeenAssignedAsSrcObject { false };
    ThreadSafeWeakPtr<MediaSourcePrivateClient> m_client;
    const bool m_isManaged;
    const MediaSourceHandle::DispatcherType m_dispatcher;
};

Ref<MediaSourceHandle> MediaSourceHandle::create(MediaSource& mediaSource, MediaSourceHandle::DispatcherType&& dispatcher)
{
    return adoptRef(*new MediaSourceHandle(mediaSource, WTFMove(dispatcher)));
}

Ref<MediaSourceHandle> MediaSourceHandle::create(Ref<MediaSourceHandle>&& other)
{
    other->setDetached(false);
    return other;
}

MediaSourceHandle::MediaSourceHandle(MediaSource& mediaSource, MediaSourceHandle::DispatcherType&& dispatcher)
    : m_private(MediaSourceHandle::SharedPrivate::create(mediaSource, WTFMove(dispatcher)))
{
}

MediaSourceHandle::MediaSourceHandle(MediaSourceHandle& other)
    : m_private(other.m_private)
{
    ASSERT(!other.m_detached);
    other.m_detached = true;
}

MediaSourceHandle::~MediaSourceHandle() = default;

bool MediaSourceHandle::canDetach() const
{
    return !isDetached() && !m_private->m_hasEverBeenAssignedAsSrcObject;
}

void MediaSourceHandle::setHasEverBeenAssignedAsSrcObject()
{
    m_private->m_hasEverBeenAssignedAsSrcObject = true;
}

bool MediaSourceHandle::hasEverBeenAssignedAsSrcObject() const
{
    return m_private->m_hasEverBeenAssignedAsSrcObject;
}

bool MediaSourceHandle::isManaged() const
{
    return m_private->m_isManaged;
}

void MediaSourceHandle::ensureOnDispatcher(MediaSourceHandle::TaskType&& task, bool forceRun) const
{
    m_private->dispatch(WTFMove(task), forceRun);
}

Ref<DetachedMediaSourceHandle> MediaSourceHandle::detach()
{
    return adoptRef(*new MediaSourceHandle(*this));
}

RefPtr<MediaSourcePrivateClient> MediaSourceHandle::mediaSourcePrivateClient() const
{
    return m_private->m_client.get();
}

void MediaSourceHandle::mediaSourceDidOpen(MediaSourcePrivate& privateMediaSource)
{
    m_private->setMediaSourcePrivate(privateMediaSource);
}

RefPtr<MediaSourcePrivate> MediaSourceHandle::mediaSourcePrivate() const
{
    return m_private->mediaSourcePrivate();
}

} // namespace WebCore

#endif // MEDIA_SOURCE_IN_WORKERS
