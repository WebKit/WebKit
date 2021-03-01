/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteCaptureSampleManager.h"

#include "Logging.h"
#include "RemoteCaptureSampleManagerMessages.h"
#include "SharedRingBufferStorage.h"
#include <WebCore/WebAudioBufferList.h>

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

namespace WebKit {
using namespace WebCore;

RemoteCaptureSampleManager::RemoteCaptureSampleManager()
    : m_queue(WorkQueue::create("RemoteCaptureSampleManager", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive))
{
}

RemoteCaptureSampleManager::~RemoteCaptureSampleManager()
{
    setConnection(nullptr);
}

void RemoteCaptureSampleManager::setConnection(IPC::Connection* connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        m_connection->removeThreadMessageReceiver(Messages::RemoteCaptureSampleManager::messageReceiverName());

    m_connection = WTFMove(connection);

    if (m_connection)
        m_connection->addThreadMessageReceiver(Messages::RemoteCaptureSampleManager::messageReceiverName(), this);
}

void RemoteCaptureSampleManager::addSource(Ref<RemoteRealtimeMediaSource>&& source)
{
    ASSERT(WTF::isMainRunLoop());
    setConnection(source->connection());

    dispatchToThread([this, source = WTFMove(source)]() mutable {
        auto identifier = source->identifier();

        ASSERT(!m_sources.contains(identifier));
        m_sources.add(identifier, makeUnique<RemoteAudio>(WTFMove(source)));
    });
}

void RemoteCaptureSampleManager::removeSource(WebCore::RealtimeMediaSourceIdentifier identifier)
{
    ASSERT(WTF::isMainRunLoop());
    dispatchToThread([this, identifier] {
        ASSERT(m_sources.contains(identifier));
        m_sources.remove(identifier);
    });
}

void RemoteCaptureSampleManager::didUpdateSourceConnection(RemoteRealtimeMediaSource& source)
{
    ASSERT(WTF::isMainRunLoop());
    setConnection(source.connection());
}

void RemoteCaptureSampleManager::dispatchToThread(Function<void()>&& callback)
{
    m_queue->dispatch(WTFMove(callback));
}

void RemoteCaptureSampleManager::audioStorageChanged(WebCore::RealtimeMediaSourceIdentifier identifier, const SharedMemory::IPCHandle& ipcHandle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to find source %llu for storageChanged", identifier.toUInt64());
        return;
    }
    iterator->value->setStorage(ipcHandle.handle, description, numberOfFrames);
}

void RemoteCaptureSampleManager::audioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier identifier, MediaTime time, uint64_t numberOfFrames)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to find source %llu for audioSamplesAvailable", identifier.toUInt64());
        return;
    }
    iterator->value->audioSamplesAvailable(time, numberOfFrames);
}

RemoteCaptureSampleManager::RemoteAudio::RemoteAudio(Ref<RemoteRealtimeMediaSource>&& source)
    : m_source(WTFMove(source))
    , m_ringBuffer(makeUnique<CARingBuffer>())
{
}

void RemoteCaptureSampleManager::RemoteAudio::setStorage(const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    m_description = description;
    m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<ReadOnlySharedRingBufferStorage>(handle), description, numberOfFrames);
    m_buffer = makeUnique<WebAudioBufferList>(description, numberOfFrames);
}

void RemoteCaptureSampleManager::RemoteAudio::audioSamplesAvailable(MediaTime time, uint64_t numberOfFrames)
{
    if (!m_buffer) {
        RELEASE_LOG_ERROR(WebRTC, "buffer for audio source %llu is null", m_source->identifier().toUInt64());
        return;
    }

    if (!WebAudioBufferList::isSupportedDescription(m_description, numberOfFrames)) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to support description with given number of frames for audio source %llu", m_source->identifier().toUInt64());
        return;
    }

    m_buffer->setSampleCount(numberOfFrames);

    m_ringBuffer->fetch(m_buffer->list(), numberOfFrames, time.timeValue());

    m_source->remoteAudioSamplesAvailable(time, *m_buffer, m_description, numberOfFrames);
}

}

#endif
