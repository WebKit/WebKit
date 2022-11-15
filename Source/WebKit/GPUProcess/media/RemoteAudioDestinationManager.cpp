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
#include "GPUProcess.h"
#include "Logging.h"
#include <WebCore/AudioUtilities.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"
#include <WebCore/AudioOutputUnitAdaptor.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#endif

#if ENABLE(IPC_TESTING_API)
#define WEB_PROCESS_TERMINATE_CONDITION !m_gpuConnectionToWebProcess.connection().ignoreInvalidMessageForTesting()
#else
#define WEB_PROCESS_TERMINATE_CONDITION true
#endif

#define TERMINATE_WEB_PROCESS_WITH_MESSAGE(message) \
    if (WEB_PROCESS_TERMINATE_CONDITION) { \
        RELEASE_LOG_FAULT(IPC, "Requesting termination of web process %" PRIu64 " for reason: %" PUBLIC_LOG_STRING, m_gpuConnectionToWebProcess.webProcessIdentifier().toUInt64(), #message); \
        m_gpuConnectionToWebProcess.terminateWebProcess(); \
    }

#define MESSAGE_CHECK(assertion, message) do { \
    if (UNLIKELY(!(assertion))) { \
        TERMINATE_WEB_PROCESS_WITH_MESSAGE(message); \
        return; \
    } \
} while (0)

namespace WebKit {

class RemoteAudioDestination final
#if PLATFORM(COCOA)
    : public WebCore::AudioUnitRenderer
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteAudioDestination(GPUConnectionToWebProcess&, RemoteAudioDestinationIdentifier identifier, const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate, IPC::Semaphore&& renderSemaphore)
        : m_id(identifier)
#if PLATFORM(COCOA)
        , m_audioOutputUnitAdaptor(*this)
        , m_numOutputChannels(numberOfOutputChannels)
#endif
        , m_renderSemaphore(WTFMove(renderSemaphore))
    {
        ASSERT(isMainRunLoop());
#if PLATFORM(COCOA)
        m_audioOutputUnitAdaptor.configure(hardwareSampleRate, numberOfOutputChannels);
#endif
    }

    ~RemoteAudioDestination()
    {
        ASSERT(isMainRunLoop());
        // Make sure we stop audio rendering and wait for it to finish before destruction.
        if (m_isPlaying)
            stop();
    }

#if PLATFORM(COCOA)
    void audioSamplesStorageChanged(ConsumerSharedCARingBuffer::Handle&& handle)
    {
        if (m_isPlaying) {
            stop();
            ASSERT(!m_isPlaying);
            if (m_isPlaying)
                return;
        }
        m_ringBuffer = ConsumerSharedCARingBuffer::map(sizeof(Float32), m_numOutputChannels, WTFMove(handle));
        if (!m_ringBuffer)
            return;
        start();
        ASSERT(m_isPlaying);
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
#endif
    }

    bool isPlaying() const { return m_isPlaying; }

private:
#if PLATFORM(COCOA)
    OSStatus render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData)
    {
        ASSERT(!isMainRunLoop());

        OSStatus status = -1;
        if (m_ringBuffer->fetchIfHasEnoughData(ioData, numberOfFrames, m_startFrame)) {
            m_startFrame += numberOfFrames;
            status = noErr;
        }

        unsigned requestedSamplesCount = m_extraRequestedFrames;
        for (; requestedSamplesCount < numberOfFrames; requestedSamplesCount += WebCore::AudioUtilities::renderQuantumSize) {
            // Ask the audio thread in the WebContent process to render a quantum.
            m_renderSemaphore.signal();
        }
        m_extraRequestedFrames = requestedSamplesCount - numberOfFrames;

        return status;
    }
#endif

    RemoteAudioDestinationIdentifier m_id;

#if PLATFORM(COCOA)
    WebCore::AudioOutputUnitAdaptor m_audioOutputUnitAdaptor;
    const uint32_t m_numOutputChannels;
    std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
    uint64_t m_startFrame { 0 };
    unsigned m_extraRequestedFrames { 0 };
#endif
    IPC::Semaphore m_renderSemaphore;

    bool m_isPlaying { false };
};

RemoteAudioDestinationManager::RemoteAudioDestinationManager(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteAudioDestinationManager::~RemoteAudioDestinationManager() = default;

void RemoteAudioDestinationManager::createAudioDestination(const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate, IPC::Semaphore&& renderSemaphore, CompletionHandler<void(const WebKit::RemoteAudioDestinationIdentifier)>&& completionHandler)
{
    MESSAGE_CHECK(!m_gpuConnectionToWebProcess.isLockdownModeEnabled(), "Received a createAudioDestination() message from a webpage in Lockdown mode.");

    auto newID = RemoteAudioDestinationIdentifier::generateThreadSafe();
    auto destination = makeUniqueRef<RemoteAudioDestination>(m_gpuConnectionToWebProcess, newID, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate, hardwareSampleRate, WTFMove(renderSemaphore));
    m_audioDestinations.add(newID, WTFMove(destination));
    completionHandler(newID);
}

void RemoteAudioDestinationManager::deleteAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK(!m_gpuConnectionToWebProcess.isLockdownModeEnabled(), "Received a deleteAudioDestination() message from a webpage in Lockdown mode.");

    m_audioDestinations.remove(identifier);
    completionHandler();

    if (allowsExitUnderMemoryPressure())
        m_gpuConnectionToWebProcess.gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void RemoteAudioDestinationManager::startAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(!m_gpuConnectionToWebProcess.isLockdownModeEnabled(), "Received a startAudioDestination() message from a webpage in Lockdown mode.");

    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(identifier)) {
        item->start();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

void RemoteAudioDestinationManager::stopAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(!m_gpuConnectionToWebProcess.isLockdownModeEnabled(), "Received a stopAudioDestination() message from a webpage in Lockdown mode.");

    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(identifier)) {
        item->stop();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

#if PLATFORM(COCOA)
void RemoteAudioDestinationManager::audioSamplesStorageChanged(RemoteAudioDestinationIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle)
{
    if (auto* item = m_audioDestinations.get(identifier))
        item->audioSamplesStorageChanged(WTFMove(handle));
}
#endif

bool RemoteAudioDestinationManager::allowsExitUnderMemoryPressure() const
{
    for (auto& audioDestination : m_audioDestinations.values()) {
        if (audioDestination->isPlaying())
            return false;
    }
    return true;
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef TERMINATE_WEB_PROCESS_WITH_MESSAGE
#undef WEB_PROCESS_TERMINATE_CONDITION

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
