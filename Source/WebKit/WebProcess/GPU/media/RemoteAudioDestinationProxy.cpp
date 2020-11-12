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
#include "RemoteAudioDestinationProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/AudioBus.h>

#if PLATFORM(COCOA)
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#endif

namespace WebKit {

// Allocate a ring buffer large enough to contain 2 seconds of audio.
constexpr size_t ringBufferSizeInSecond = 2;

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
    , m_ringBuffer(makeUnique<WebCore::CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(this)))
#else
    : m_numberOfOutputChannels(numberOfOutputChannels)
#endif
    , m_inputDeviceId(inputDeviceId)
    , m_numberOfInputChannels(numberOfInputChannels)
{
    auto offThreadRendering = [this, protectedThis = makeRef(*this)]() mutable {
        while (!m_threadTaskQueue.isKilled()) {
            if (auto task = m_threadTaskQueue.waitForMessage())
                task();
        }
    };
    m_renderThread = Thread::create("RemoteAudioDestinationProxy render thread", WTFMove(offThreadRendering), ThreadType::Audio);

    connectToGPUProcess();
}

void RemoteAudioDestinationProxy::connectToGPUProcess()
{
    RemoteAudioDestinationIdentifier destinationID;

    auto& connection = WebProcess::singleton().ensureGPUProcessConnection();
    connection.addClient(*this);
    bool didSucceed = connection.connection().sendSync(
        Messages::RemoteAudioDestinationManager::CreateAudioDestination(m_inputDeviceId, m_numberOfInputChannels, numberOfOutputChannels(), sampleRate(), hardwareSampleRate()),
        Messages::RemoteAudioDestinationManager::CreateAudioDestination::Reply(destinationID), 0);

    if (!didSucceed) {
        // The GPUProcess likely crashed during this synchronous IPC. gpuProcessConnectionDidClose() will get called to reconnect to the GPUProcess.
        RELEASE_LOG_ERROR(Media, "RemoteAudioDestinationProxy::connectToGPUProcess: Failed to send RemoteAudioDestinationManager::CreateAudioDestination() IPC (GPU process likely crashed)");
        return;
    }

    connection.connection().addThreadMessageReceiver(Messages::RemoteAudioDestinationProxy::messageReceiverName(), this, destinationID.toUInt64());
    m_destinationID = destinationID;

#if PLATFORM(COCOA)
    m_currentFrame = 0;
    AudioStreamBasicDescription streamFormat;
    getAudioStreamBasicDescription(streamFormat);
    m_ringBuffer->allocate(streamFormat, m_numberOfFrames);
    m_audioBufferList = makeUnique<WebAudioBufferList>(streamFormat);
#endif
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    auto& connection =  WebProcess::singleton().ensureGPUProcessConnection();
    connection.connection().removeThreadMessageReceiver(Messages::RemoteAudioDestinationProxy::messageReceiverName(), m_destinationID.toUInt64());

    connection.connection().sendWithAsyncReply(
        Messages::RemoteAudioDestinationManager::DeleteAudioDestination(m_destinationID), [] {
        // Can't remove this from proxyMap() here because the object would have been already deleted.
    });

    m_threadTaskQueue.kill();
    m_renderThread->waitForCompletion();
}

void RemoteAudioDestinationProxy::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StartAudioDestination(m_destinationID), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler), dispatchToRenderThread = WTFMove(dispatchToRenderThread)](bool isPlaying) mutable {
        m_dispatchToRenderThread = WTFMove(dispatchToRenderThread);
        setIsPlaying(isPlaying);
        completionHandler(isPlaying);
    });
}

void RemoteAudioDestinationProxy::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioDestinationManager::StopAudioDestination(m_destinationID), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](bool isPlaying) mutable {
        setIsPlaying(isPlaying);
        auto dispatchToRenderThread = std::exchange(m_dispatchToRenderThread, nullptr);
        auto callCompletionHandler = [completionHandler = WTFMove(completionHandler), isPlaying]() mutable {
            completionHandler(!isPlaying);
        };

        if (dispatchToRenderThread) {
            // Do a round-trip to the worklet thread to make sure we call the completion handler after
            // the last rendering quantum has been processed by the worklet thread.
            dispatchToRenderThread([callCompletionHandler = WTFMove(callCompletionHandler)]() mutable {
                callOnMainThread(WTFMove(callCompletionHandler));
            });
        } else
            callCompletionHandler();
    });
}

#if PLATFORM(COCOA)
void RemoteAudioDestinationProxy::requestBuffer(double sampleTime, uint64_t hostTime, uint64_t framesToRender, CompletionHandler<void(uint64_t, uint64_t)>&& completionHandler)
{
    ASSERT(!isMainThread());

    if (!hasEnoughFrames(framesToRender))
        return completionHandler(0, 0);

    auto startFrame = m_currentFrame;
    m_audioBufferList->setSampleCount(framesToRender);

    AudioDestinationCocoa::render(sampleTime, hostTime, framesToRender, m_audioBufferList->list());

    m_currentFrame += framesToRender;

    m_ringBuffer->store(m_audioBufferList->list(), framesToRender, startFrame);

    uint64_t boundsStartFrame;
    uint64_t boundsEndFrame;
    m_ringBuffer->getCurrentFrameBounds(boundsStartFrame, boundsEndFrame);
    completionHandler(boundsStartFrame, boundsEndFrame);
}
#endif

// IPC::Connection::ThreadMessageReceiver
void RemoteAudioDestinationProxy::dispatchToThread(Function<void()>&& task)
{
    if (m_dispatchToRenderThread) {
        m_dispatchToRenderThread(WTFMove(task));
        return;
    }
    m_threadTaskQueue.append(WTFMove(task));
}

#if PLATFORM(COCOA)
void RemoteAudioDestinationProxy::storageChanged(SharedMemory* storage)
{
    SharedMemory::Handle handle;
    if (storage)
        storage->createHandle(handle, SharedMemory::Protection::ReadOnly);

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
