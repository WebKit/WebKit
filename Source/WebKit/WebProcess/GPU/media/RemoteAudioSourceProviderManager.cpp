/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "GPUProcessConnection.h"
#include "Logging.h"
#include "RemoteAudioSourceProvider.h"
#include "RemoteAudioSourceProviderManagerMessages.h"
#include "SharedCARingBuffer.h"
#include "WebProcess.h"
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/SharedMemory.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS)

namespace WebKit {
using namespace WebCore;

RemoteAudioSourceProviderManager::RemoteAudioSourceProviderManager(IPC::Connection& connection)
    : m_connection { connection }
    , m_queue { WorkQueue::create("RemoteAudioSourceProviderManager"_s, WorkQueue::QOS::UserInteractive) }
{
    connection.addWorkQueueMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName(), m_queue, *this);
}

RemoteAudioSourceProviderManager::~RemoteAudioSourceProviderManager()
{
    ASSERT(!m_connection.get());
}

void RemoteAudioSourceProviderManager::stopListeningForIPC()
{
    if (RefPtr connection = m_connection.get()) {
        connection->removeWorkQueueMessageReceiver(Messages::RemoteAudioSourceProviderManager::messageReceiverName());
        m_connection = nullptr;
    }
}

void RemoteAudioSourceProviderManager::addProvider(Ref<RemoteAudioSourceProvider>&& provider)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, provider = WTF::move(provider)]() mutable {
        auto identifier = provider->identifier();

        ASSERT(!m_providers.contains(identifier));
        m_providers.add(identifier, WTF::move(provider));
    });
}

void RemoteAudioSourceProviderManager::removeProvider(MediaPlayerIdentifier identifier)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, identifier] {
        ASSERT(m_providers.contains(identifier));
        m_providers.remove(identifier);
    });
}

RefPtr<RemoteAudioSourceProvider> RemoteAudioSourceProviderManager::providerFor(MediaPlayerIdentifier identifier)
{
    assertIsCurrent(m_queue);

    auto iterator = m_providers.find(identifier);
    if (iterator == m_providers.end())
        return nullptr;
    return iterator->value.ptr();
}

void RemoteAudioSourceProviderManager::audioStorageChanged(MediaPlayerIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle, const WebCore::CAAudioStreamDescription& description)
{
    assertIsCurrent(m_queue);

    RefPtr provider = providerFor(identifier);
    if (!provider) {
        RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for storageChanged", identifier.toUInt64());
        return;
    }
    handle.takeOwnershipOfMemory(MemoryLedger::Media);
    auto ringBuffer = ConsumerSharedCARingBuffer::map(description, WTF::move(handle));
    if (!ringBuffer)
        return;

    provider->audioStorageChanged(WTF::move(ringBuffer), description);
}

void RemoteAudioSourceProviderManager::setNeedsFlush(WebCore::MediaPlayerIdentifier identifier)
{
    assertIsCurrent(m_queue);

    if (RefPtr provider = providerFor(identifier)) {
        provider->setNeedsFlush();
        return;
    }

    RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for setNeedsFlush", identifier.toUInt64());
}

void RemoteAudioSourceProviderManager::setPlaybackRate(WebCore::MediaPlayerIdentifier identifier, double playbackRate)
{
    assertIsCurrent(m_queue);

    if (RefPtr provider = providerFor(identifier)) {
        provider->setPlaybackRate(playbackRate);
        return;
    }

    RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for setPlaybackRate", identifier.toUInt64());
}

void RemoteAudioSourceProviderManager::setPreservesPitch(WebCore::MediaPlayerIdentifier identifier, bool preservesPitch)
{
    assertIsCurrent(m_queue);

    if (RefPtr provider = providerFor(identifier)) {
        provider->setPreservesPitch(preservesPitch);
        return;
    }

    RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for setPreservesPitch", identifier.toUInt64());
}

void RemoteAudioSourceProviderManager::setVolume(WebCore::MediaPlayerIdentifier identifier, double volume)
{
    assertIsCurrent(m_queue);

    if (RefPtr provider = providerFor(identifier)) {
        provider->setVolume(volume);
        return;
    }

    RELEASE_LOG_ERROR(Media, "Unable to find provider %llu for setVolume", identifier.toUInt64());
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS)
