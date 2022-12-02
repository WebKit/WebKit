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
#include "RemoteAudioSourceProviderManager.h"

#include "GPUProcessConnection.h"
#include "Logging.h"
#include "RemoteAudioSourceProvider.h"
#include "RemoteAudioSourceProviderManagerMessages.h"
#include "SharedCARingBuffer.h"
#include "WebProcess.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

RemoteAudioSourceProviderManager::RemoteAudioSourceProviderManager()
    : m_queue(WorkQueue::create("RemoteAudioSourceProviderManager", WorkQueue::QOS::UserInteractive))
{
}

RemoteAudioSourceProviderManager::~RemoteAudioSourceProviderManager()
{
    ASSERT(!m_connection);
}

void RemoteAudioSourceProviderManager::stopListeningForIPC()
{
    setConnection(nullptr);
}

void RemoteAudioSourceProviderManager::setConnection(IPC::Connection* connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        m_connection->removeWorkQueueMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName());

    m_connection = WTFMove(connection);

    if (m_connection)
        m_connection->addWorkQueueMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName(), m_queue, *this);
}

void RemoteAudioSourceProviderManager::addProvider(Ref<RemoteAudioSourceProvider>&& provider)
{
    ASSERT(WTF::isMainRunLoop());
    setConnection(&WebProcess::singleton().ensureGPUProcessConnection().connection());

    m_queue->dispatch([this, protectedThis = Ref { *this }, provider = WTFMove(provider)]() mutable {
        auto identifier = provider->identifier();

        ASSERT(!m_providers.contains(identifier));
        m_providers.add(identifier, makeUnique<RemoteAudio>(WTFMove(provider)));
    });
}

void RemoteAudioSourceProviderManager::removeProvider(MediaPlayerIdentifier identifier)
{
    ASSERT(WTF::isMainRunLoop());

    m_queue->dispatch([this, protectedThis = Ref { *this }, identifier] {
        ASSERT(m_providers.contains(identifier));
        m_providers.remove(identifier);
    });
}

void RemoteAudioSourceProviderManager::audioStorageChanged(MediaPlayerIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle, const WebCore::CAAudioStreamDescription& description)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_providers.find(identifier);
    if (iterator == m_providers.end()) {
        RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for storageChanged", identifier.toUInt64());
        return;
    }
    iterator->value->setStorage(WTFMove(handle), description);
}

void RemoteAudioSourceProviderManager::audioSamplesAvailable(MediaPlayerIdentifier identifier, uint64_t startFrame, uint64_t numberOfFrames)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_providers.find(identifier);
    if (iterator == m_providers.end()) {
        RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for audioSamplesAvailable", identifier.toUInt64());
        return;
    }
    iterator->value->audioSamplesAvailable(startFrame, numberOfFrames);
}

RemoteAudioSourceProviderManager::RemoteAudio::RemoteAudio(Ref<RemoteAudioSourceProvider>&& provider)
    : m_provider(WTFMove(provider))
{
}

void RemoteAudioSourceProviderManager::RemoteAudio::setStorage(ConsumerSharedCARingBuffer::Handle&& handle, const WebCore::CAAudioStreamDescription& description)
{
    m_buffer = nullptr;
    handle.takeOwnershipOfMemory(MemoryLedger::Media);
    m_ringBuffer = ConsumerSharedCARingBuffer::map(description, WTFMove(handle));
    if (!m_ringBuffer)
        return;
    m_description = description;
    m_buffer = makeUnique<WebAudioBufferList>(description);
}

void RemoteAudioSourceProviderManager::RemoteAudio::audioSamplesAvailable(uint64_t startFrame, uint64_t numberOfFrames)
{
    if (!m_buffer) {
        RELEASE_LOG_ERROR(Media, "buffer for audio provider %llu is null", m_provider->identifier().toUInt64());
        return;
    }

    if (!WebAudioBufferList::isSupportedDescription(*m_description, numberOfFrames)) {
        RELEASE_LOG_ERROR(Media, "Unable to support description with given number of frames for audio provider %llu", m_provider->identifier().toUInt64());
        return;
    }

    m_buffer->setSampleCount(numberOfFrames);

    m_ringBuffer->fetch(m_buffer->list(), numberOfFrames, startFrame);

    m_provider->audioSamplesAvailable(*m_buffer, *m_description, numberOfFrames);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
