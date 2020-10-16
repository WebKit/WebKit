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

#include "Logging.h"
#include "RemoteAudioSourceProviderManagerMessages.h"
#include "SharedRingBufferStorage.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace PAL;
using namespace WebCore;

RemoteAudioSourceProviderManager::RemoteAudioSourceProviderManager()
    : m_queue(WorkQueue::create("RemoteAudioSourceProviderManager", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive))
{
}

RemoteAudioSourceProviderManager::~RemoteAudioSourceProviderManager()
{
    setConnection(nullptr);
}

void RemoteAudioSourceProviderManager::setConnection(IPC::Connection* connection)
{
    if (m_connection == connection)
        return;

    if (m_connection)
        m_connection->removeThreadMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName());

    m_connection = WTFMove(connection);

    if (m_connection)
        m_connection->addThreadMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName(), this);
}

void RemoteAudioSourceProviderManager::addProvider(Ref<RemoteAudioSourceProvider>&& provider)
{
    ASSERT(WTF::isMainRunLoop());
    setConnection(&WebProcess::singleton().ensureGPUProcessConnection().connection());

    dispatchToThread([this, provider = WTFMove(provider)]() mutable {
        auto identifier = provider->identifier();

        ASSERT(!m_providers.contains(identifier));
        m_providers.add(identifier, makeUnique<RemoteAudio>(WTFMove(provider)));
    });
}

void RemoteAudioSourceProviderManager::removeProvider(MediaPlayerIdentifier identifier)
{
    ASSERT(WTF::isMainRunLoop());

    dispatchToThread([this, identifier] {
        ASSERT(m_providers.contains(identifier));
        m_providers.remove(identifier);
    });
}

void RemoteAudioSourceProviderManager::dispatchToThread(Function<void()>&& callback)
{
    m_queue->dispatch(WTFMove(callback));
}

void RemoteAudioSourceProviderManager::audioStorageChanged(MediaPlayerIdentifier identifier, const SharedMemory::IPCHandle& ipcHandle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    ASSERT(!WTF::isMainRunLoop());

    auto iterator = m_providers.find(identifier);
    if (iterator == m_providers.end()) {
        RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for storageChanged", identifier.toUInt64());
        return;
    }
    iterator->value->setStorage(ipcHandle.handle, description, numberOfFrames);
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
    , m_ringBuffer(makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(nullptr)))
{
}

void RemoteAudioSourceProviderManager::RemoteAudio::setStorage(const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    m_description = description;

    RefPtr<SharedMemory> memory;
    if (!handle.isNull()) {
        memory = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
        RELEASE_LOG_ERROR_IF(!memory, Media, "Unable to create shared memory for audio provider %llu", m_provider->identifier().toUInt64());
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

void RemoteAudioSourceProviderManager::RemoteAudio::audioSamplesAvailable(uint64_t startFrame, uint64_t numberOfFrames)
{
    if (!m_buffer) {
        RELEASE_LOG_ERROR(Media, "buffer for audio provider %llu is null", m_provider->identifier().toUInt64());
        return;
    }

    if (!WebAudioBufferList::isSupportedDescription(m_description, numberOfFrames)) {
        RELEASE_LOG_ERROR(Media, "Unable to support description with given number of frames for audio provider %llu", m_provider->identifier().toUInt64());
        return;
    }

    auto endFrame = startFrame + numberOfFrames;
    m_buffer->setSampleCount(numberOfFrames);

    m_ringBuffer->setCurrentFrameBounds(startFrame, endFrame);
    m_ringBuffer->fetch(m_buffer->list(), numberOfFrames, startFrame);

    m_provider->audioSamplesAvailable(*m_buffer, m_description, numberOfFrames);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
