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

// Allocate a ring buffer large enough to contain 2 seconds of audio.
constexpr size_t ringBufferSizeInSecond = 2;

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

void RemoteAudioDestinationProxy::startRenderingThread()
{
    ASSERT(!m_renderThread);

    auto offThreadRendering = [this]() mutable {
        do {
            m_renderSemaphore.wait();
            if (m_shouldStopThread)
                break;

            renderQuantum();
        } while (!m_shouldStopThread);
    };
    m_renderThread = Thread::create("RemoteAudioDestinationProxy render thread", WTFMove(offThreadRendering), ThreadType::Audio, Thread::QOS::UserInteractive);
}

void RemoteAudioDestinationProxy::stopRenderingThread()
{
    if (!m_renderThread)
        return;

    m_shouldStopThread = true;
    m_renderSemaphore.signal();
    m_renderThread->waitForCompletion();
    m_renderThread = nullptr;
}

IPC::Connection* RemoteAudioDestinationProxy::connection()
{
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection->addClient(*this);
        auto sendResult = m_gpuProcessConnection->connection().sendSync(Messages::RemoteAudioDestinationManager::CreateAudioDestination(m_inputDeviceId, m_numberOfInputChannels, m_outputBus->numberOfChannels(), sampleRate(), m_remoteSampleRate, m_renderSemaphore), 0);
        if (!sendResult) {
            // The GPUProcess likely crashed during this synchronous IPC. gpuProcessConnectionDidClose() will get called to reconnect to the GPUProcess.
            RELEASE_LOG_ERROR(Media, "RemoteAudioDestinationProxy::destinationID: IPC to create the audio destination failed, the GPUProcess likely crashed.");
            return nullptr;
        }
        std::tie(m_destinationID) = sendResult.takeReply();

#if PLATFORM(COCOA)
        m_currentFrame = 0;
        auto streamFormat = audioStreamBasicDescriptionForAudioBus(m_outputBus);
        size_t numberOfFrames = m_remoteSampleRate * ringBufferSizeInSecond;
        auto [ringBuffer, handle] = ProducerSharedCARingBuffer::allocate(streamFormat, numberOfFrames);
        m_ringBuffer = WTFMove(ringBuffer);
        m_gpuProcessConnection->connection().send(Messages::RemoteAudioDestinationManager::AudioSamplesStorageChanged { m_destinationID, WTFMove(handle) }, 0);
        m_audioBufferList = makeUnique<WebCore::WebAudioBufferList>(streamFormat);
        m_audioBufferList->setSampleCount(WebCore::AudioUtilities::renderQuantumSize);
#endif

        startRenderingThread();
    }
    return m_destinationID ? &m_gpuProcessConnection->connection() : nullptr;
}

IPC::Connection* RemoteAudioDestinationProxy::existingConnection()
{
    return m_gpuProcessConnection && m_destinationID ? &m_gpuProcessConnection->connection() : nullptr;
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    if (m_gpuProcessConnection && m_destinationID) {
        m_gpuProcessConnection->connection().sendWithAsyncReply(
            Messages::RemoteAudioDestinationManager::DeleteAudioDestination(m_destinationID), [] {
            // Can't remove this from proxyMap() here because the object would have been already deleted.
        });
    }

    stopRenderingThread();
}

void RemoteAudioDestinationProxy::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    auto* connection = this->connection();
    if (!connection) {
        RunLoop::current().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(false);
        });
        return;
    }

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StartAudioDestination(m_destinationID), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        setIsPlaying(isPlaying);
        completionHandler(isPlaying);
    });
}

void RemoteAudioDestinationProxy::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    auto* connection = this->connection();
    if (!connection) {
        RunLoop::current().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(false);
        });
        return;
    }

    connection->sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StopAudioDestination(m_destinationID), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        setIsPlaying(isPlaying);
        completionHandler(!isPlaying);
    });
}

void RemoteAudioDestinationProxy::renderQuantum()
{
    ASSERT(!isMainRunLoop());

#if PLATFORM(COCOA)
    auto sampleTime = m_currentFrame / static_cast<double>(m_remoteSampleRate);
    auto hostTime =  MonotonicTime::fromMachAbsoluteTime(mach_absolute_time());
    size_t numberOfFrames = WebCore::AudioUtilities::renderQuantumSize;
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
    m_ringBuffer->store(m_audioBufferList->list(), WebCore::AudioUtilities::renderQuantumSize, m_currentFrame);
    render(sampleTime, hostTime, framesToRender);
    m_currentFrame += WebCore::AudioUtilities::renderQuantumSize;
#endif
}

void RemoteAudioDestinationProxy::gpuProcessConnectionDidClose(GPUProcessConnection& oldConnection)
{
    oldConnection.removeClient(*this);

    stopRenderingThread();
    m_gpuProcessConnection = nullptr;
    m_destinationID = { };

    if (isPlaying())
        startRendering([](bool) { });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
