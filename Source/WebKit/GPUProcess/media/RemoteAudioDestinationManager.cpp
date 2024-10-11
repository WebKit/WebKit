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
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeRefCounted.h>

#if PLATFORM(COCOA)
#include "SharedCARingBuffer.h"
#include <WebCore/AudioOutputUnitAdaptor.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#define MESSAGE_CHECK(assertion, message) MESSAGE_CHECK_WITH_MESSAGE_BASE(assertion, &connection->connection(), message)
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection->connection(), completion)

namespace WebKit {

class RemoteAudioDestination final
#if PLATFORM(COCOA)
    : public WebCore::AudioUnitRenderer
#endif
{
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RemoteAudioDestination);
public:
    RemoteAudioDestination(GPUConnectionToWebProcess& connection, const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate, IPC::Semaphore&& renderSemaphore)
        : m_renderSemaphore(WTFMove(renderSemaphore))
#if !RELEASE_LOG_DISABLED
        , m_logger(connection.logger())
        , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
#if PLATFORM(COCOA)
        , m_audioOutputUnitAdaptor(*this)
        , m_numOutputChannels(numberOfOutputChannels)
#endif
    {
        ASSERT(isMainRunLoop());
        ALWAYS_LOG(LOGIDENTIFIER);
#if PLATFORM(COCOA)
        m_audioOutputUnitAdaptor.configure(hardwareSampleRate, numberOfOutputChannels);
#endif
    }

    ~RemoteAudioDestination()
    {
        ASSERT(isMainRunLoop());
        ALWAYS_LOG(LOGIDENTIFIER);
        // Make sure we stop audio rendering and wait for it to finish before destruction.
        if (m_isPlaying)
            stop();
    }

#if PLATFORM(COCOA)
    void setSharedMemory(WebCore::SharedMemory::Handle&& handle)
    {
        m_frameCount = WebCore::SharedMemory::map(WTFMove(handle), WebCore::SharedMemory::Protection::ReadWrite);
    }

    void audioSamplesStorageChanged(ConsumerSharedCARingBuffer::Handle&& handle)
    {
        bool wasPlaying = m_isPlaying;
        if (m_isPlaying) {
            stop();
            ASSERT(!m_isPlaying);
            if (m_isPlaying)
                return;
        }
        m_ringBuffer = ConsumerSharedCARingBuffer::map(sizeof(Float32), m_numOutputChannels, WTFMove(handle));
        if (!m_ringBuffer)
            return;
        if (wasPlaying) {
            start();
            ASSERT(m_isPlaying);
        }
    }
#endif

    void start()
    {
#if PLATFORM(COCOA)
        if (m_audioOutputUnitAdaptor.start()) {
            ERROR_LOG(LOGIDENTIFIER, "Failed to start AudioOutputUnit");
            return;
        }

        ALWAYS_LOG(LOGIDENTIFIER);
        m_isPlaying = true;
#endif
    }

    void stop()
    {
#if PLATFORM(COCOA)
        if (m_audioOutputUnitAdaptor.stop()) {
            ERROR_LOG(LOGIDENTIFIER, "Failed to stop AudioOutputUnit");
            return;
        }

        ALWAYS_LOG(LOGIDENTIFIER);
        m_isPlaying = false;
#endif
    }

    bool isPlaying() const { return m_isPlaying; }

private:
#if PLATFORM(COCOA)
    void incrementTotalFrameCount(UInt32 numberOfFrames)
    {
        static_assert(std::atomic<UInt32>::is_always_lock_free, "Shared memory atomic usage assumes lock free primitives are used");
        if (m_frameCount) {
            RELEASE_ASSERT(m_frameCount->size() == sizeof(std::atomic<uint32_t>));
            WTF::atomicExchangeAdd(reinterpret_cast<uint32_t*>(m_frameCount->mutableSpan().data()), numberOfFrames);
        }
    }

    OSStatus render(double sampleTime, uint64_t hostTime, UInt32 numberOfFrames, AudioBufferList* ioData)
    {
        ASSERT(!isMainRunLoop());

        OSStatus status = -1;
        if (m_ringBuffer->fetchIfHasEnoughData(ioData, numberOfFrames, m_startFrame)) {
            m_startFrame += numberOfFrames;
            status = noErr;
        }

        incrementTotalFrameCount(numberOfFrames);
        m_renderSemaphore.signal();

        return status;
    }
#endif

    IPC::Semaphore m_renderSemaphore;
    bool m_isPlaying { false };

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const { return "RemoteAudioDestination"_s; }
    WTFLogChannel& logChannel() const { return WebKit2LogMedia; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    Logger& logger() const { return m_logger; }

    Ref<Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif

#if PLATFORM(COCOA)
    WebCore::AudioOutputUnitAdaptor m_audioOutputUnitAdaptor;
    RefPtr<WebCore::SharedMemory> m_frameCount;
    const uint32_t m_numOutputChannels;
    std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
    uint64_t m_startFrame { 0 };
#endif
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioDestinationManager);

RemoteAudioDestinationManager::RemoteAudioDestinationManager(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteAudioDestinationManager::~RemoteAudioDestinationManager() = default;

void RemoteAudioDestinationManager::createAudioDestination(RemoteAudioDestinationIdentifier identifier, const String& inputDeviceId, uint32_t numberOfInputChannels, uint32_t numberOfOutputChannels, float sampleRate, float hardwareSampleRate, IPC::Semaphore&& renderSemaphore, WebCore::SharedMemory::Handle&& handle)
{
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;
    MESSAGE_CHECK(!connection->isLockdownModeEnabled(), "Received a createAudioDestination() message from a webpage in Lockdown mode.");

    auto destination = makeUniqueRef<RemoteAudioDestination>(*connection, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate, hardwareSampleRate, WTFMove(renderSemaphore));
#if PLATFORM(COCOA)
    destination->setSharedMemory(WTFMove(handle));
#else
    UNUSED_PARAM(handle);
#endif
    m_audioDestinations.add(identifier, WTFMove(destination));
}

void RemoteAudioDestinationManager::deleteAudioDestination(RemoteAudioDestinationIdentifier identifier)
{
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;
    MESSAGE_CHECK(!connection->isLockdownModeEnabled(), "Received a deleteAudioDestination() message from a webpage in Lockdown mode.");

    m_audioDestinations.remove(identifier);

    if (allowsExitUnderMemoryPressure())
        connection->gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void RemoteAudioDestinationManager::startAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return completionHandler(false);
    MESSAGE_CHECK_COMPLETION(!connection->isLockdownModeEnabled(), completionHandler(false));

    bool isPlaying = false;
    if (auto* item = m_audioDestinations.get(identifier)) {
        item->start();
        isPlaying = item->isPlaying();
    }
    completionHandler(isPlaying);
}

void RemoteAudioDestinationManager::stopAudioDestination(RemoteAudioDestinationIdentifier identifier, CompletionHandler<void(bool)>&& completionHandler)
{
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return completionHandler(false);
    MESSAGE_CHECK_COMPLETION(!connection->isLockdownModeEnabled(), completionHandler(false));

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

std::optional<SharedPreferencesForWebProcess> RemoteAudioDestinationManager::sharedPreferencesForWebProcess() const
{
    if (RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return gpuConnectionToWebProcess->sharedPreferencesForWebProcess();

    return std::nullopt;
}

} // namespace WebKit

#undef MESSAGE_CHECK

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
