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
#include "MediaSourceInterfaceMainThread.h"

#if ENABLE(MEDIA_SOURCE)

#include "ManagedMediaSource.h"
#include "MediaSource.h"
#include "SourceBufferList.h"
#include "TimeRanges.h"

namespace WebCore {

MediaSourceInterfaceMainThread::MediaSourceInterfaceMainThread(Ref<MediaSource>&& mediaSource)
    : m_mediaSource(WTFMove(mediaSource))
{
}

RefPtr<MediaSourcePrivateClient> MediaSourceInterfaceMainThread::client() const
{
    return m_mediaSource->client();
}

void MediaSourceInterfaceMainThread::monitorSourceBuffers()
{
    m_mediaSource->monitorSourceBuffers();
}

bool MediaSourceInterfaceMainThread::isClosed() const
{
    return m_mediaSource->isClosed();
}

MediaTime MediaSourceInterfaceMainThread::duration() const
{
    return m_mediaSource->duration();
}

PlatformTimeRanges MediaSourceInterfaceMainThread::buffered() const
{
    return m_mediaSource->buffered();
}

Ref<TimeRanges> MediaSourceInterfaceMainThread::seekable() const
{
    return m_mediaSource->seekable();
}

bool MediaSourceInterfaceMainThread::isStreamingContent() const
{
    if (RefPtr managedMediasource = dynamicDowncast<ManagedMediaSource>(m_mediaSource))
        return managedMediasource && managedMediasource->streamingAllowed() && managedMediasource->streaming();
    // We can assume that if we have active source buffers, later networking activity (such as stream or XHR requests) will be media related.
    return m_mediaSource->activeSourceBuffers()->length();
}

bool MediaSourceInterfaceMainThread::attachToElement(WeakPtr<HTMLMediaElement>&& element)
{
    return m_mediaSource->attachToElement(WTFMove(element));
}

void MediaSourceInterfaceMainThread::detachFromElement()
{
    m_mediaSource->detachFromElement();
}

void MediaSourceInterfaceMainThread::elementIsShuttingDown()
{
    m_mediaSource->elementIsShuttingDown();
}

void MediaSourceInterfaceMainThread::openIfDeferredOpen()
{
    m_mediaSource->openIfDeferredOpen();
}

bool MediaSourceInterfaceMainThread::isManaged() const
{
    return m_mediaSource->isManaged();
}

void MediaSourceInterfaceMainThread::setAsSrcObject(bool set)
{
    m_mediaSource->setAsSrcObject(set);
}

void MediaSourceInterfaceMainThread::memoryPressure()
{
    m_mediaSource->memoryPressure();
}

bool MediaSourceInterfaceMainThread::detachable() const
{
    return m_mediaSource->detachable();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
