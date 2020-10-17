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
#include "RemoteAudioDestinationProxyMessages.h"
#include <WebCore/AudioUtilities.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#include "SharedRingBufferStorage.h"
#include <WebCore/AudioOutputUnitAdaptor.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#endif

namespace WebKit {

class RemoteAudioDestination
    : public ThreadSafeRefCounted<RemoteAudioDestination>
#if PLATFORM(COCOA)
    , public WebCore::AudioUnitRenderer
#endif
{
public:
    static Ref<RemoteAudioDestination> create(GPUConnectionToWebProcess& connection, RemoteAudioDestinationIdentifier identifier,
        const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate)
    {
        return adoptRef(*new RemoteAudioDestination(connection, identifier, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate, hardwareSampleRate));
    }

    virtual ~RemoteAudioDestination() = default;

    void scheduleGracefulShutdownIfNeeded()
    {
        if (!m_isPlaying)
            return;
        m_protectThisDuringGracefulShutdown = this;
        stop();
    }

#if PLATFORM(COCOA)
    void audioSamplesStorageChanged(const SharedMemory::IPCHandle& ipcHandle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
    {
        m_description = description;

        if (ipcHandle.handle.isNull()) {
            m_ringBuffer->deallocate();
            storage().setReadOnly(false);
            storage().setStorage(nullptr);
            return;
        }

        auto memory = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
        storage().setStorage(WTFMove(memory));
        storage().setReadOnly(true);

        m_ringBuffer->allocate(description, numberOfFrames);
    }
#endif

    void start()
    {
#if PLATFORM(COCOA)
        if (m_audioOutputUnitAdaptor.start())
            return;

        m_isPlaying = true;
#endif
    }

    void stop()
    {
#if PLATFORM(COCOA)
        if (m_audioOutputUnitAdaptor.stop())
            return;

        m_isPlaying = false;

        if (m_protectThisDuringGracefulShutdown) {
            RELEASE_ASSERT(refCount() == 1);
            m_protectThisDuringGracefulShutdown = nullptr;
        }
#endif
    }

    bool isPlaying() const { return m_isPlaying; }

private:
    RemoteAudioDestination(GPUConnectionToWebProcess& connection, RemoteAudioDestinationIdentifier identifier, const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate)
        : m_connection(connection)
        , m_id(identifier)
#if PLATFORM(COCOA)
        , m_audioOutputUnitAdaptor(*this)
        , m_ringBuffer(makeUniqueRef<WebCore::CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(nullptr)))
#endif
    {
#if PLATFORM(COCOA)
        m_audioOutputUnitAdaptor.configure(hardwareSampleRate, numberOfOutputChannels);
#endif
    }

#if PLATFORM(COCOA)
    SharedRingBufferStorage& storage()
    {
        return static_cast<SharedRingBufferStorage&>(m_ringBuffer->storage());
    }

    OSStatus render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData)
    {
        ASSERT(!isMainThread());

        if (m_protectThisDuringGracefulShutdown)
            return noErr;

        // FIXME: It is unfortunate we have to dispatch to the main thead here. We should be able to IPC straight from the
        // render thread but this is not supported by sendWithAsyncReply().
        callOnMainThread([this, protectedThis = makeRef(*this), sampleTime, hostTime, numberOfFrames, ioData]() mutable {
            m_connection.connection().sendWithAsyncReply(Messages::RemoteAudioDestinationProxy::RequestBuffer(sampleTime, hostTime, numberOfFrames), [this, protectedThis = WTFMove(protectedThis), ioData](auto startFrame, auto numberOfFramesToRender, auto boundsStartFrame, auto boundsEndFrame) mutable {
                m_ringBuffer->setCurrentFrameBounds(boundsStartFrame, boundsEndFrame);
                m_ringBuffer->fetch(ioData, numberOfFramesToRender, startFrame);
            }, m_id.toUInt64());
        });

        return noErr;
    }
#endif

    GPUConnectionToWebProcess& m_connection;
    RemoteAudioDestinationIdentifier m_id;

    RefPtr<RemoteAudioDestination> m_protectThisDuringGracefulShutdown;

#if PLATFORM(COCOA)
    WebCore::AudioOutputUnitAdaptor m_audioOutputUnitAdaptor;

    WebCore::CAAudioStreamDescription m_description;
    UniqueRef<WebCore::CARingBuffer> m_ringBuffer;
#endif

    bool m_isPlaying { false };
};

RemoteAudioDestinationManager::RemoteAudioDestinationManager(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteAudioDestinationManager::~RemoteAudioDestinationManager() = default;

void RemoteAudioDestinationManager::createAudioDestination(const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate,
    CompletionHandler<void(RemoteAudioDestinationIdentifier)>&& completionHandler)
{
    auto newID = RemoteAudioDestinationIdentifier::generateThreadSafe();
    auto destination = RemoteAudioDestination::create(m_gpuConnectionToWebProcess, newID, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate, hardwareSampleRate);
    m_audioDestinations.add(newID, WTFMove(destination));
    completionHandler(newID);
}

void RemoteAudioDestinationManager::deleteAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    auto destination = m_audioDestinations.take(identifier);
    if (destination)
        destination->scheduleGracefulShutdownIfNeeded();
    completionHandler();
}

void RemoteAudioDestinationManager::startAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(identifier)) {
        item->start();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

void RemoteAudioDestinationManager::stopAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(identifier)) {
        item->stop();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

#if PLATFORM(COCOA)
void RemoteAudioDestinationManager::audioSamplesStorageChanged(RemoteAudioDestinationIdentifier identifier, const SharedMemory::IPCHandle& ipcHandle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    if (auto* item = m_audioDestinations.get(identifier))
        item->audioSamplesStorageChanged(ipcHandle, description, numberOfFrames);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
