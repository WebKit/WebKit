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

#include "RemoteCaptureSampleManagerMessages.h"
#include "SharedRingBufferStorage.h"
#include <WebCore/WebAudioBufferList.h>

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

namespace WebKit {
using namespace PAL;
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

void RemoteCaptureSampleManager::dispatchToThread(Function<void()>&& callback)
{
    m_queue->dispatch(WTFMove(callback));
}

void RemoteCaptureSampleManager::audioStorageChanged(WebCore::RealtimeMediaSourceIdentifier identifier, const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to find source %llu for storageChanged", identifier.toUInt64());
        return;
    }
    iterator->value->setStorage(handle, description, numberOfFrames);
}

void RemoteCaptureSampleManager::audioSamplesAvailable(WebCore::RealtimeMediaSourceIdentifier identifier, MediaTime time, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_sources.find(identifier);
    if (iterator == m_sources.end()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to find source %llu for audioSamplesAvailable", identifier.toUInt64());
        return;
    }
    iterator->value->audioSamplesAvailable(time, numberOfFrames, startFrame, endFrame);
}

RemoteCaptureSampleManager::RemoteAudio::RemoteAudio(Ref<RemoteRealtimeMediaSource>&& source)
    : m_source(WTFMove(source))
    , m_ringBuffer(makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(nullptr)))
{
}

void RemoteCaptureSampleManager::RemoteAudio::setStorage(const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    m_description = description;

    RefPtr<SharedMemory> memory;
    if (!handle.isNull()) {
        memory = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
        RELEASE_LOG_ERROR_IF(!memory, WebRTC, "Unable to create shared memory for audio source %llu", m_source->identifier().toUInt64());
    }

    auto& storage = static_cast<SharedRingBufferStorage&>(m_ringBuffer->storage());
    if (!memory) {
        m_ringBuffer->deallocate();
        storage.setReadOnly(false);
        storage.setStorage(nullptr);
        return;
    }

    storage.setStorage(memory.releaseNonNull());
    storage.setReadOnly(true);
    m_ringBuffer->allocate(description, numberOfFrames);
    m_buffer = makeUnique<WebAudioBufferList>(description, numberOfFrames);
}

void RemoteCaptureSampleManager::RemoteAudio::audioSamplesAvailable(MediaTime time, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame)
{
    if (!m_buffer)
        return;

    m_buffer->setSampleCount(numberOfFrames);

    m_ringBuffer->setCurrentFrameBounds(startFrame, endFrame);
    m_ringBuffer->fetch(m_buffer->list(), numberOfFrames, time.timeValue());

    m_source->remoteAudioSamplesAvailable(time, *m_buffer, m_description, numberOfFrames);
}

}

#endif
