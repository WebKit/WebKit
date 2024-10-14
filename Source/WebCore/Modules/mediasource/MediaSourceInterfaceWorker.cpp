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
#include "MediaSourceInterfaceWorker.h"

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)

#include "ManagedMediaSource.h"
#include "MediaSource.h"
#include "MediaSourceHandle.h"
#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include "PlatformTimeRanges.h"
#include "TimeRanges.h"

namespace WebCore {

MediaSourceInterfaceWorker::MediaSourceInterfaceWorker(Ref<MediaSourceHandle>&& handle)
    : m_handle(WTFMove(handle))
    , m_client(m_handle->mediaSourcePrivateClient())
{
}

RefPtr<MediaSourcePrivateClient> MediaSourceInterfaceWorker::client() const
{
    return m_client.get();
}

void MediaSourceInterfaceWorker::monitorSourceBuffers()
{
    ASSERT(m_handle->hasEverBeenAssignedAsSrcObject());
    m_handle->ensureOnDispatcher([](MediaSource& mediaSource) {
        mediaSource.monitorSourceBuffers();
    });
}

bool MediaSourceInterfaceWorker::isClosed() const
{
    if (RefPtr mediaSourcePrivate = m_handle->mediaSourcePrivate())
        return mediaSourcePrivate->readyState() == MediaSource::ReadyState::Closed;
    return true;
}

MediaTime MediaSourceInterfaceWorker::duration() const
{
    if (RefPtr mediaSourcePrivate = m_handle->mediaSourcePrivate(); mediaSourcePrivate && !isClosed())
        return mediaSourcePrivate->duration();
    return MediaTime::invalidTime();
}

PlatformTimeRanges MediaSourceInterfaceWorker::buffered() const
{
    if (RefPtr mediaSourcePrivate = m_handle->mediaSourcePrivate(); mediaSourcePrivate && !isClosed())
        return mediaSourcePrivate->buffered();
    return PlatformTimeRanges::emptyRanges();
}

Ref<TimeRanges> MediaSourceInterfaceWorker::seekable() const
{
    if (RefPtr mediaSourcePrivate = m_handle->mediaSourcePrivate(); mediaSourcePrivate && !isClosed())
        return TimeRanges::create(mediaSourcePrivate->seekable());
    return TimeRanges::create();
}

bool MediaSourceInterfaceWorker::isStreamingContent() const
{
    RefPtr mediaSourcePrivate = m_handle->mediaSourcePrivate();
    return mediaSourcePrivate && m_handle->isManaged() && mediaSourcePrivate->streamingAllowed() && mediaSourcePrivate->streaming();
}

bool MediaSourceInterfaceWorker::attachToElement(WeakPtr<HTMLMediaElement>&& element)
{
    if (m_handle->hasEverBeenAssignedAsSrcObject())
        return false;
    m_handle->setHasEverBeenAssignedAsSrcObject();
    bool forceRun = true;
    m_handle->ensureOnDispatcher([element = WTFMove(element)](MediaSource& mediaSource) mutable {
        mediaSource.attachToElement(WTFMove(element));
    }, forceRun);
    return true;
}

void MediaSourceInterfaceWorker::detachFromElement()
{
    ASSERT(m_handle->hasEverBeenAssignedAsSrcObject());
    m_handle->ensureOnDispatcher([](MediaSource& mediaSource) {
        mediaSource.detachFromElement();
    });
}

void MediaSourceInterfaceWorker::elementIsShuttingDown()
{
    ASSERT(m_handle->hasEverBeenAssignedAsSrcObject());
    m_handle->ensureOnDispatcher([](MediaSource& mediaSource) {
        mediaSource.elementIsShuttingDown();
    });
}

void MediaSourceInterfaceWorker::openIfDeferredOpen()
{
    ASSERT(m_handle->hasEverBeenAssignedAsSrcObject());
    m_handle->ensureOnDispatcher([](MediaSource& mediaSource) {
        mediaSource.openIfDeferredOpen();
    });
}

bool MediaSourceInterfaceWorker::isManaged() const
{
    return m_handle->isManaged();
}

void MediaSourceInterfaceWorker::setAsSrcObject(bool set)
{
    m_handle->ensureOnDispatcher([set](MediaSource& mediaSource) {
        mediaSource.setAsSrcObject(set);
    });
}

void MediaSourceInterfaceWorker::memoryPressure()
{
    m_handle->ensureOnDispatcher([](MediaSource& mediaSource) {
        mediaSource.memoryPressure();
    });
}

bool MediaSourceInterfaceWorker::detachable() const
{
    return m_handle->detachable();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE_IN_WORKERS)
