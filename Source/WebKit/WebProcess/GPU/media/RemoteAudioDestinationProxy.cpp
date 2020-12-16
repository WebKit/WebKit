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
#include "WebProcess.h"
#include <WebCore/AudioBus.h>
#include <WebCore/AudioUtilities.h>
#include <wtf/RunLoop.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <mach/mach_time.h>
#endif

namespace WebKit {

// Allocate a ring buffer large enough to contain 2 seconds of audio.
constexpr size_t ringBufferSizeInSecond = 2;
constexpr uint64_t framesAheadOfReader = 2 * WebCore::AudioUtilities::renderQuantumSize;

using AudioDestination = WebCore::AudioDestination;
using AudioIOCallback = WebCore::AudioIOCallback;

Ref<AudioDestination> RemoteAudioDestinationProxy::create(AudioIOCallback& callback,
    const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptRef(*new RemoteAudioDestinationProxy(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
}

RemoteAudioDestinationProxy::RemoteAudioDestinationProxy(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
#if PLATFORM(COCOA)
    : WebCore::AudioDestinationCocoa(callback, numberOfOutputChannels, sampleRate, false)
    , m_numberOfFrames(hardwareSampleRate() * ringBufferSizeInSecond)
    , m_ringBuffer(makeUnique<WebCore::CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(std::bind(&RemoteAudioDestinationProxy::storageChanged, this, std::placeholders::_1))))
#else
    : WebCore::AudioDestination(callback)
    , m_numberOfOutputChannels(numberOfOutputChannels)
#endif
    , m_inputDeviceId(inputDeviceId)
    , m_numberOfInputChannels(numberOfInputChannels)
{
    connectToGPUProcess();
}

void RemoteAudioDestinationProxy::startRenderingThread()
{
    ASSERT(!m_renderThread);

    BinarySemaphore semaphore;
    auto offThreadRendering = [this, protectedThis = makeRef(*this), &semaphore]() mutable {
        m_dispatchToRenderThread = [runLoop = makeRef(RunLoop::current())](Function<void()>&& task) {
            runLoop->dispatch(WTFMove(task));
        };
        semaphore.signal();
        RunLoop::run();
    };
    m_renderThread = Thread::create("RemoteAudioDestinationProxy render thread", WTFMove(offThreadRendering), ThreadType::Audio, Thread::QOS::UserInteractive);
    m_renderThread->detach();
    semaphore.wait();
}

void RemoteAudioDestinationProxy::stopRenderingThreadIfNecessary()
{
    if (!m_renderThread)
        return;

    m_dispatchToRenderThread([] {
        RunLoop::current().stop();
    });
    m_renderThread = nullptr;
}

void RemoteAudioDestinationProxy::connectToGPUProcess()
{
    RemoteAudioDestinationIdentifier destinationID;

    auto& connection = WebProcess::singleton().ensureGPUProcessConnection();
    connection.addClient(*this);
    auto didSucceed = connection.connection().sendSync(
        Messages::RemoteAudioDestinationManager::CreateAudioDestination(m_inputDeviceId, m_numberOfInputChannels, numberOfOutputChannels(), sampleRate(), hardwareSampleRate()),
        Messages::RemoteAudioDestinationManager::CreateAudioDestination::Reply(destinationID), 0);

    if (!didSucceed) {
        // The GPUProcess likely crashed during this synchronous IPC. gpuProcessConnectionDidClose() will get called to reconnect to the GPUProcess.
        RELEASE_LOG_ERROR(Media, "RemoteAudioDestinationProxy::connectToGPUProcess: Failed to send RemoteAudioDestinationManager::CreateAudioDestination() IPC (GPU process likely crashed)");
        return;
    }

    m_destinationID = destinationID;

#if PLATFORM(COCOA)
    m_currentFrame = 0;
    AudioStreamBasicDescription streamFormat;
    getAudioStreamBasicDescription(streamFormat);
    m_ringBuffer->allocate(streamFormat, m_numberOfFrames);
    m_audioBufferList = makeUnique<WebCore::WebAudioBufferList>(streamFormat);
#endif
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    auto& connection =  WebProcess::singleton().ensureGPUProcessConnection();

    connection.connection().sendWithAsyncReply(
        Messages::RemoteAudioDestinationManager::DeleteAudioDestination(m_destinationID), [] {
        // Can't remove this from proxyMap() here because the object would have been already deleted.
    });

    RELEASE_ASSERT(!m_renderThread);
    RELEASE_ASSERT(!m_dispatchToRenderThread);
}

void RemoteAudioDestinationProxy::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StartAudioDestination(m_destinationID), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), dispatchToRenderThread = WTFMove(dispatchToRenderThread)](bool isPlaying) mutable {
        if (m_dispatchToRenderThread)
            return completionHandler(true); // Already rendering. Can happen when start() gets called multiple times without stopping in between.

        m_dispatchToRenderThread = WTFMove(dispatchToRenderThread);
        setIsPlaying(isPlaying);
        if (!isPlaying)
            return completionHandler(false);
        startRendering([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(true);
        });
    });
}

void RemoteAudioDestinationProxy::startRendering(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());

    if (!m_dispatchToRenderThread)
        startRenderingThread();

    m_dispatchToRenderThread([this, protectedThis = makeRef(*this), sampleRate = hardwareSampleRate(), completionHandler = WTFMove(completionHandler)]() mutable {
        m_sampleRate = sampleRate;
        renderQuantum();
        // To avoid the need for an IPC message from the GPUProcess for each rendering quantum, we use a timer to trigger the rendering of each quantum.
        m_renderingTimer = makeUnique<RunLoop::Timer<RemoteAudioDestinationProxy>>(RunLoop::current(), this, &RemoteAudioDestinationProxy::renderQuantum);
        m_renderingTimer->startRepeating(Seconds { WebCore::AudioUtilities::renderQuantumSize / static_cast<double>(sampleRate) });

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void RemoteAudioDestinationProxy::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StopAudioDestination(m_destinationID), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        setIsPlaying(isPlaying);
        stopRenderingIfNecessary([completionHandler = WTFMove(completionHandler), isPlaying]() mutable {
            completionHandler(!isPlaying);
        });
    });
}

void RemoteAudioDestinationProxy::stopRenderingIfNecessary(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    if (!m_dispatchToRenderThread)
        return completionHandler();

    m_dispatchToRenderThread([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        m_renderingTimer = nullptr;
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
    stopRenderingThreadIfNecessary();
    m_dispatchToRenderThread = nullptr;
}

void RemoteAudioDestinationProxy::renderQuantum()
{
    ASSERT(!isMainThread());

#if PLATFORM(COCOA)
    m_audioBufferList->setSampleCount(WebCore::AudioUtilities::renderQuantumSize);

    // Try to stay a few rendering quantums ahead of the reader.
    auto lastReadFrame = m_ringBuffer->lastReadFrame();
    while (lastReadFrame >= m_currentFrame || (m_currentFrame - lastReadFrame) < framesAheadOfReader) {
        AudioDestinationCocoa::render(m_currentFrame / static_cast<double>(m_sampleRate), mach_absolute_time(), WebCore::AudioUtilities::renderQuantumSize, m_audioBufferList->list());
        m_ringBuffer->store(m_audioBufferList->list(), WebCore::AudioUtilities::renderQuantumSize, m_currentFrame);
        m_currentFrame += WebCore::AudioUtilities::renderQuantumSize;
    }
#endif
}

#if PLATFORM(COCOA)
void RemoteAudioDestinationProxy::storageChanged(SharedMemory* storage)
{
    SharedMemory::Handle handle;
    if (storage)
        storage->createHandle(handle, SharedMemory::Protection::ReadWrite);

    // FIXME: Send the actual data size with IPCHandle.
#if OS(DARWIN) || OS(WINDOWS)
    uint64_t dataSize = handle.size();
#else
    uint64_t dataSize = 0;
#endif

    AudioStreamBasicDescription streamFormat;
    getAudioStreamBasicDescription(streamFormat);
    WebCore::CAAudioStreamDescription description(streamFormat);

    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioDestinationManager::AudioSamplesStorageChanged { m_destinationID, SharedMemory::IPCHandle { WTFMove(handle), dataSize }, streamFormat, m_numberOfFrames }, 0);
}
#endif

void RemoteAudioDestinationProxy::gpuProcessConnectionDidClose(GPUProcessConnection& oldConnection)
{
    oldConnection.removeClient(*this);

    connectToGPUProcess();

    if (isPlaying())
        start(std::exchange(m_dispatchToRenderThread, nullptr), [](bool) { });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
