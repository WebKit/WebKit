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
#include "RemoteAudioDestinationProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteAudioDestinationManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/AudioBus.h>
#include <WebCore/AudioUtilities.h>
#include <algorithm>

#if PLATFORM(COCOA)
#include <WebCore/AudioUtilitiesCocoa.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <mach/mach_time.h>
#endif

namespace WebKit {

#if PLATFORM(COCOA)
// Allocate a ring buffer large enough to contain 2 seconds of audio.
constexpr size_t ringBufferSizeInSecond = 2;
constexpr unsigned maxAudioBufferListSampleCount = 4096;
#endif

uint8_t RemoteAudioDestinationProxy::s_realtimeThreadCount { 0 };

using AudioIOCallback = WebCore::AudioIOCallback;

Ref<RemoteAudioDestinationProxy> RemoteAudioDestinationProxy::create(AudioIOCallback& callback,
    const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptRef(*new RemoteAudioDestinationProxy(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
}

RemoteAudioDestinationProxy::RemoteAudioDestinationProxy(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
    : WebCore::AudioDestinationResampler(callback, numberOfOutputChannels, sampleRate, hardwareSampleRate())
    , m_inputDeviceId(inputDeviceId)
    , m_numberOfInputChannels(numberOfInputChannels)
    , m_remoteSampleRate(hardwareSampleRate())
{
}

uint32_t RemoteAudioDestinationProxy::totalFrameCount() const
{
    RELEASE_ASSERT(m_frameCount->size() == sizeof(std::atomic<uint32_t>));
    return WTF::atomicLoad(static_cast<uint32_t*>(m_frameCount->data()));
}

void RemoteAudioDestinationProxy::startRenderingThread()
{
    ASSERT(!m_renderThread);

    auto offThreadRendering = [this]() mutable {
        do {
            m_renderSemaphore.wait();
            if (m_shouldStopThread || !m_frameCount)
                break;

            uint32_t totalFrameCount = this->totalFrameCount();
            uint32_t frameCount = (totalFrameCount < m_lastFrameCount) ? (totalFrameCount + (std::numeric_limits<uint32_t>::max() - m_lastFrameCount)) : (totalFrameCount - m_lastFrameCount);

            m_lastFrameCount = totalFrameCount;
            renderAudio(frameCount);

        } while (!m_shouldStopThread);
    };

    // FIXME(263073): Coalesce compatible realtime threads together to render sequentially
    // rather than have separate realtime threads for each RemoteAudioDestinationProxy.
    bool shouldCreateRealtimeThread = s_realtimeThreadCount < s_maximumConcurrentRealtimeThreads;
    if (shouldCreateRealtimeThread) {
        m_isRealtimeThread = true;
        ++s_realtimeThreadCount;
    }
    auto schedulingPolicy = shouldCreateRealtimeThread ? Thread::SchedulingPolicy::Realtime : Thread::SchedulingPolicy::Other;

    m_renderThread = Thread::create("RemoteAudioDestinationProxy render thread", WTFMove(offThreadRendering), ThreadType::Audio, Thread::QOS::UserInteractive, schedulingPolicy);

#if HAVE(THREAD_TIME_CONSTRAINTS)
    if (shouldCreateRealtimeThread) {
        ASSERT(m_remoteSampleRate > 0);
        auto rawRenderingQuantumDuration = 128 / m_remoteSampleRate;
        auto renderingQuantumDuration = MonotonicTime::fromRawSeconds(rawRenderingQuantumDuration);
        auto renderingTimeConstraint = MonotonicTime::fromRawSeconds(rawRenderingQuantumDuration * 2);
        m_renderThread->setThreadTimeConstraints(renderingQuantumDuration, renderingQuantumDuration, renderingTimeConstraint, true);
    }
#endif

#if PLATFORM(COCOA)
    // Roughly match the priority of the Audio IO thread in the GPU process
    m_renderThread->changePriority(60);
#endif
}

void RemoteAudioDestinationProxy::stopRenderingThread()
{
    if (!m_renderThread)
        return;

    m_shouldStopThread = true;
    m_renderSemaphore.signal();
    m_renderThread->waitForCompletion();
    m_renderThread = nullptr;

    if (m_isRealtimeThread) {
        ASSERT(s_realtimeThreadCount);
        s_realtimeThreadCount--;
        m_isRealtimeThread = false;
    }
}

IPC::Connection* RemoteAudioDestinationProxy::connection()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    if (!gpuProcessConnection) {
        gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection = gpuProcessConnection;
        gpuProcessConnection->addClient(*this);
        m_destinationID = RemoteAudioDestinationIdentifier::generate();

        m_lastFrameCount = 0;
        std::optional<SharedMemory::Handle> frameCountHandle;
        if ((m_frameCount = SharedMemory::allocate(sizeof(std::atomic<uint32_t>)))) {
            frameCountHandle = m_frameCount->createHandle(SharedMemory::Protection::ReadWrite);
        }
        RELEASE_ASSERT(frameCountHandle.has_value());
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::CreateAudioDestination(m_destinationID, m_inputDeviceId, m_numberOfInputChannels, m_outputBus->numberOfChannels(), sampleRate(), m_remoteSampleRate, m_renderSemaphore, WTFMove(*frameCountHandle)), 0);

#if PLATFORM(COCOA)
        m_currentFrame = 0;
        auto streamFormat = audioStreamBasicDescriptionForAudioBus(m_outputBus);
        size_t numberOfFrames = m_remoteSampleRate * ringBufferSizeInSecond;
        auto result = ProducerSharedCARingBuffer::allocate(streamFormat, numberOfFrames);
        RELEASE_ASSERT(result); // FIXME(https://bugs.webkit.org/show_bug.cgi?id=262690): Handle allocation failure.
        auto [ringBuffer, handle] = WTFMove(*result);
        m_ringBuffer = WTFMove(ringBuffer);
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::AudioSamplesStorageChanged { m_destinationID, WTFMove(handle) }, 0);
        m_audioBufferList = makeUnique<WebCore::WebAudioBufferList>(streamFormat);
        m_audioBufferList->setSampleCount(maxAudioBufferListSampleCount);
#endif

        startRenderingThread();
    }
    return m_destinationID ? &gpuProcessConnection->connection() : nullptr;
}

IPC::Connection* RemoteAudioDestinationProxy::existingConnection()
{
    auto gpuProcessConnection = m_gpuProcessConnection.get();
    return gpuProcessConnection && m_destinationID ? &gpuProcessConnection->connection() : nullptr;
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    if (auto gpuProcessConnection = m_gpuProcessConnection.get(); gpuProcessConnection && m_destinationID)
        gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::DeleteAudioDestination(m_destinationID), 0);
    stopRenderingThread();
}

void RemoteAudioDestinationProxy::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr connection = this->connection();
    if (!connection) {
        RunLoop::current().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            protectedThis->setIsPlaying(false);
            completionHandler(false);
        });
        return;
    }

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StartAudioDestination(m_destinationID), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        protectedThis->setIsPlaying(isPlaying);
        completionHandler(isPlaying);
    });
}

void RemoteAudioDestinationProxy::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
#if PLATFORM(MAC)
    // FIXME: rdar://104617724
    // On macOS, page load testing reports a regression on a particular page if we do not
    // start the GPU process connection and create the audio objects redundantly on a particular earlier
    // page. This should be fixed once it is understood why.
    auto* connection = this->connection();
    if (!connection) {
        RunLoop::current().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
            // Not calling setIsPlaying(false) intentionally to match the pre-regression state.
            completionHandler(false);
        });
        return;
    }
#else
    auto* connection = existingConnection();
    if (!connection) {
        RunLoop::current().dispatch([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            protectedThis->setIsPlaying(false);
            completionHandler(true);
        });
        return;
    }
#endif

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StopAudioDestination(m_destinationID), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        protectedThis->setIsPlaying(isPlaying);
        completionHandler(!isPlaying);
    });
}

void RemoteAudioDestinationProxy::renderAudio(unsigned frameCount)
{
    ASSERT(!isMainRunLoop());

#if PLATFORM(COCOA)
    while (frameCount) {
        auto sampleTime = m_currentFrame / static_cast<double>(m_remoteSampleRate);
        auto hostTime =  MonotonicTime::fromMachAbsoluteTime(mach_absolute_time());
        size_t numberOfFrames = std::min(frameCount, maxAudioBufferListSampleCount);
        frameCount -= numberOfFrames;
        auto* ioData = m_audioBufferList->list();

        auto* buffers = ioData->mBuffers;
        auto numberOfBuffers = std::min<UInt32>(ioData->mNumberBuffers, m_outputBus->numberOfChannels());

        // Associate the destination data array with the output bus then fill the FIFO.
        for (UInt32 i = 0; i < numberOfBuffers; ++i) {
            auto* memory = reinterpret_cast<float*>(buffers[i].mData);
            size_t channelNumberOfFrames = std::min<size_t>(numberOfFrames, buffers[i].mDataByteSize / sizeof(float));
            m_outputBus->setChannelMemory(i, memory, channelNumberOfFrames);
        }
        size_t framesToRender = pullRendered(numberOfFrames);
        m_ringBuffer->store(m_audioBufferList->list(), numberOfFrames, m_currentFrame);
        render(sampleTime, hostTime, framesToRender);
        m_currentFrame += numberOfFrames;
    }
#endif
}

void RemoteAudioDestinationProxy::gpuProcessConnectionDidClose(GPUProcessConnection& oldConnection)
{
    stopRenderingThread();
    m_gpuProcessConnection = nullptr;
    m_destinationID = { };

    if (isPlaying())
        startRendering([](bool) { });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
