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
#include "RemoteAudioDestinationManager.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "GPUConnectionToWebProcess.h"
#include "RemoteAudioBusData.h"
#include "RemoteAudioDestinationProxyMessages.h"
#include "SharedMemory.h"
#include <WebCore/AudioBus.h>
#include <WebCore/AudioDestination.h>
#include <WebCore/AudioIOCallback.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebKit {

class RemoteAudioDestination : public ThreadSafeRefCounted<RemoteAudioDestination>, public WebCore::AudioIOCallback {
public:
    using AudioBus = WebCore::AudioBus;
    using AudioDestination = WebCore::AudioDestination;
    using SharedBuffer = WebCore::SharedBuffer;

    static Ref<RemoteAudioDestination> create(GPUConnectionToWebProcess& connection, RemoteAudioDestinationIdentifier id,
        const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate)
    {
        return adoptRef(*new RemoteAudioDestination(connection, id, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
    }

    virtual ~RemoteAudioDestination()
    {
        RELEASE_ASSERT(!m_destination->isPlaying());
    }

    void scheduleGracefulShutdownIfNeeded()
    {
        if (!isPlaying())
            return;
        m_protectThisDuringGracefulShutdown = this;
        stop();
    }

    void start() { m_destination->start(); }
    void stop() { m_destination->stop(); }
    bool isPlaying() { return m_destination->isPlaying(); }
    unsigned framesPerBuffer() const { return m_destination->framesPerBuffer() ; }

private:
    RemoteAudioDestination(GPUConnectionToWebProcess& connection, RemoteAudioDestinationIdentifier id, const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate)
        : m_connection(connection)
        , m_id(id)
        , m_destination(AudioDestination::create(*this, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate))
    {
    }

    void render(AudioBus* sourceBus, AudioBus* destinationBus, size_t framesToProcess, const WebCore::AudioIOPosition& outputPosition) override
    {
        if (m_protectThisDuringGracefulShutdown)
            return;

        auto protectedThis = makeRef(*this);
        BinarySemaphore renderSemaphore;

        Vector<Ref<SharedMemory>> buffers;
        for (unsigned i = 0; i < destinationBus->numberOfChannels(); ++i) {
            auto memory = SharedMemory::allocate(sizeof(float) * framesToProcess);
            buffers.append(*memory);
        }

        // FIXME: Replace this code with a ring buffer. At least this happens in audio thread.
        ASSERT(!isMainThread());
        callOnMainThread([this, framesToProcess, outputPosition, &buffers, &renderSemaphore] {
            RemoteAudioBusData busData { framesToProcess, outputPosition, buffers.map([](auto& memory) { return memory.copyRef(); }) };
            ASSERT(framesToProcess);
            m_connection.connection().sendWithAsyncReply(Messages::RemoteAudioDestinationProxy::RenderBuffer(busData), [&]() {
                renderSemaphore.signal();
            }, m_id.toUInt64());
        });
        renderSemaphore.wait();

        auto audioBus = AudioBus::create(buffers.size(), framesToProcess, false);
        for (unsigned i = 0; i < buffers.size(); ++i)
            audioBus->setChannelMemory(i, (float*)buffers[i]->data(), framesToProcess);
        destinationBus->copyFrom(*audioBus);
    }

    void isPlayingDidChange() override
    {
        if (m_protectThisDuringGracefulShutdown) {
            RELEASE_ASSERT(!m_destination->isPlaying());
            RELEASE_ASSERT(refCount() == 1);
            m_protectThisDuringGracefulShutdown = nullptr; // Deletes "this".
            return;
        }
        callOnMainThread([this, protectedThis = makeRef(*this), isPlaying = m_destination->isPlaying(), id = m_id.toUInt64()] {
            m_connection.connection().send(Messages::RemoteAudioDestinationProxy::DidChangeIsPlaying(isPlaying), id);
        });
    }

    GPUConnectionToWebProcess& m_connection;
    RemoteAudioDestinationIdentifier m_id;
    std::unique_ptr<AudioDestination> m_destination;
    RefPtr<RemoteAudioDestination> m_protectThisDuringGracefulShutdown;
};

RemoteAudioDestinationManager::RemoteAudioDestinationManager(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteAudioDestinationManager::~RemoteAudioDestinationManager() = default;

void RemoteAudioDestinationManager::createAudioDestination(const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate,
    CompletionHandler<void(RemoteAudioDestinationIdentifier, unsigned framesPerBuffer)>&& completionHandler)
{
    auto newID = RemoteAudioDestinationIdentifier::generateThreadSafe();
    auto destination = RemoteAudioDestination::create(m_gpuConnectionToWebProcess, newID, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate);
    auto framesPerBuffer = destination->framesPerBuffer();
    m_audioDestinations.add(newID, WTFMove(destination));
    completionHandler(newID, framesPerBuffer);
}

void RemoteAudioDestinationManager::deleteAudioDestination(RemoteAudioDestinationIdentifier id, CompletionHandler<void()>&& completionHandler)
{
    auto destination = m_audioDestinations.take(id);
    if (destination)
        destination.value()->scheduleGracefulShutdownIfNeeded();
    completionHandler();
}

void RemoteAudioDestinationManager::startAudioDestination(RemoteAudioDestinationIdentifier id, CompletionHandler<void(bool)>&& completionHandler)
{
    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(id)) {
        item->start();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

void RemoteAudioDestinationManager::stopAudioDestination(RemoteAudioDestinationIdentifier id, CompletionHandler<void(bool)>&& completionHandler)
{
    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(id)) {
        item->stop();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
