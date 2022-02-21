/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "AudioMediaStreamTrackRendererInternalUnitManager.h"

#if ENABLE(MEDIA_STREAM) && ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "AudioMediaStreamTrackRendererInternalUnitIdentifier.h"
#include "IPCSemaphore.h"
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManagerMessages.h"
#include "SharedMemory.h"
#include <WebCore/AudioMediaStreamTrackRendererInternalUnit.h>
#include <WebCore/AudioMediaStreamTrackRendererUnit.h>
#include <WebCore/AudioSampleBufferList.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <mach/mach_time.h>
#include <wtf/Deque.h>

namespace WebKit {

class AudioMediaStreamTrackRendererInternalUnitManager::Proxy final : public WebCore::AudioMediaStreamTrackRendererInternalUnit, public CanMakeWeakPtr<Proxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Proxy(WebCore::AudioMediaStreamTrackRendererInternalUnit::RenderCallback&&, WebCore::AudioMediaStreamTrackRendererInternalUnit::ResetCallback&&);
    ~Proxy();

    AudioMediaStreamTrackRendererInternalUnitIdentifier identifier() const { return m_identifier; }

    enum class IsClosed { No, Yes };
    void reset(IsClosed);

private:
    // AudioMediaStreamTrackRendererUnit::InternalUnit API.
    void start() final;
    void stop() final;
    void retrieveFormatDescription(CompletionHandler<void(const WebCore::CAAudioStreamDescription*)>&&) final;
    void setAudioOutputDevice(const String&) final;

    void initialize(const WebCore::CAAudioStreamDescription&, size_t frameChunkSize);
    void storageChanged(SharedMemory*, const CAAudioStreamDescription&, size_t);

    void stopThread();
    void startThread();
    void createRemoteUnit();

    WebCore::AudioMediaStreamTrackRendererInternalUnit::RenderCallback m_renderCallback;
    WebCore::AudioMediaStreamTrackRendererInternalUnit::ResetCallback m_resetCallback;
    AudioMediaStreamTrackRendererInternalUnitIdentifier m_identifier;

    Deque<CompletionHandler<void(const WebCore::CAAudioStreamDescription*)>> m_descriptionCallbacks;

    String m_deviceId;
    bool m_isPlaying { false };

    std::optional<WebCore::CAAudioStreamDescription> m_description;
    std::unique_ptr<WebCore::WebAudioBufferList> m_buffer;
    std::unique_ptr<WebCore::CARingBuffer> m_ringBuffer;
    int64_t m_writeOffset { 0 };
    size_t m_frameChunkSize { 0 };
    size_t m_numberOfFrames { 0 };

    std::unique_ptr<IPC::Semaphore> m_semaphore;
    RefPtr<Thread> m_thread;
    std::atomic<bool> m_shouldStopThread { false };
    bool m_didClose { false };
};

void AudioMediaStreamTrackRendererInternalUnitManager::add(Proxy& proxy)
{
    m_proxies.add(proxy.identifier(), proxy);
}

void AudioMediaStreamTrackRendererInternalUnitManager::remove(Proxy& proxy)
{
    m_proxies.remove(proxy.identifier());
}

UniqueRef<WebCore::AudioMediaStreamTrackRendererInternalUnit> AudioMediaStreamTrackRendererInternalUnitManager::createRemoteInternalUnit(WebCore::AudioMediaStreamTrackRendererInternalUnit::RenderCallback&& renderCallback, WebCore::AudioMediaStreamTrackRendererInternalUnit::ResetCallback&& resetCallback)
{
    return makeUniqueRef<AudioMediaStreamTrackRendererInternalUnitManager::Proxy>(WTFMove(renderCallback), WTFMove(resetCallback));
}

void AudioMediaStreamTrackRendererInternalUnitManager::reset(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    if (auto proxy = m_proxies.get(identifier))
        proxy->reset(Proxy::IsClosed::No);
}

void AudioMediaStreamTrackRendererInternalUnitManager::restartAllUnits()
{
    auto proxies = std::exchange(m_proxies, { });
    for (auto proxy : proxies.values())
        proxy->reset(Proxy::IsClosed::Yes);
}

AudioMediaStreamTrackRendererInternalUnitManager::Proxy::Proxy(WebCore::AudioMediaStreamTrackRendererInternalUnit::RenderCallback&& renderCallback, WebCore::AudioMediaStreamTrackRendererInternalUnit::ResetCallback&& resetCallback)
    : m_renderCallback(WTFMove(renderCallback))
    , m_resetCallback(WTFMove(resetCallback))
    , m_identifier(AudioMediaStreamTrackRendererInternalUnitIdentifier::generate())
{
    WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().add(*this);
    createRemoteUnit();
}

AudioMediaStreamTrackRendererInternalUnitManager::Proxy::~Proxy()
{
    WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().remove(*this);
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::DeleteUnit { m_identifier }, 0);

    while (!m_descriptionCallbacks.isEmpty())
        m_descriptionCallbacks.takeFirst()(nullptr);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::createRemoteUnit()
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::CreateUnit { m_identifier }, [weakThis = WeakPtr { *this }](auto&& description, auto frameChunkSize) {
        if (weakThis && frameChunkSize)
            weakThis->initialize(description, frameChunkSize);
    }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::initialize(const WebCore::CAAudioStreamDescription& description, size_t frameChunkSize)
{
    if (m_semaphore)
        stopThread();

    m_semaphore = makeUnique<IPC::Semaphore>();
    m_description = description;
    m_frameChunkSize = frameChunkSize;

    while (!m_descriptionCallbacks.isEmpty())
        m_descriptionCallbacks.takeFirst()(&description);

    if (m_isPlaying)
        start();
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::start()
{
    if (!m_description) {
        m_isPlaying = true;
        return;
    }

    if (m_didClose) {
        m_didClose = false;
        createRemoteUnit();
        if (!m_deviceId.isEmpty())
            setAudioOutputDevice(m_deviceId);
        m_isPlaying = true;
        return;
    }

    stopThread();

    m_isPlaying = true;

    m_numberOfFrames = m_description->sampleRate() * 2;
    m_ringBuffer.reset();
    m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(std::bind(&AudioMediaStreamTrackRendererInternalUnitManager::Proxy::storageChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));
    m_ringBuffer->allocate(m_description->streamDescription(), m_numberOfFrames);

    m_buffer = makeUnique<WebAudioBufferList>(*m_description, m_numberOfFrames);
    m_buffer->setSampleCount(m_frameChunkSize);

    startThread();
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::storageChanged(SharedMemory* memory, const CAAudioStreamDescription& format, size_t frameCount)
{
    if (!frameCount)
        return;

    SharedMemory::Handle handle;
    if (memory)
        memory->createHandle(handle, SharedMemory::Protection::ReadOnly);

    // FIXME: Send the actual data size with IPCHandle.
#if OS(DARWIN) || OS(WINDOWS)
    uint64_t dataSize = handle.size();
#else
    uint64_t dataSize = 0;
#endif
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::StartUnit { m_identifier, SharedMemory::IPCHandle { WTFMove(handle),  dataSize }, format, frameCount, *m_semaphore }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::stop()
{
    m_isPlaying = false;
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::StopUnit { m_identifier }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::setAudioOutputDevice(const String& deviceId)
{
    m_deviceId = deviceId;
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::SetAudioOutputDevice { m_identifier, deviceId }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::retrieveFormatDescription(CompletionHandler<void(const WebCore::CAAudioStreamDescription*)>&& callback)
{
    if (!m_description || !m_descriptionCallbacks.isEmpty()) {
        m_descriptionCallbacks.append(WTFMove(callback));
        return;
    }
    callback(&m_description.value());
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::stopThread()
{
    if (!m_thread)
        return;

    m_shouldStopThread = true;
    m_semaphore->signal();
    m_thread->waitForCompletion();
    m_thread = nullptr;
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::startThread()
{
    ASSERT(!m_thread);
    m_shouldStopThread = false;
    auto threadLoop = [this]() mutable {
        m_writeOffset = 0;
        do {
            // If wait fails, the semaphore on the other side was probably destroyed and we should just exit here and wait to launch a new thread.
            if (!m_semaphore->wait())
                break;
            if (m_shouldStopThread)
                break;

            auto& bufferList = *m_buffer->list();

            AudioUnitRenderActionFlags flags = 0;
            m_renderCallback(m_frameChunkSize, bufferList, m_writeOffset, mach_absolute_time(), flags);

            if (flags == kAudioUnitRenderAction_OutputIsSilence)
                AudioSampleBufferList::zeroABL(bufferList, static_cast<size_t>(m_frameChunkSize * m_description->bytesPerFrame()));

            m_ringBuffer->store(&bufferList, m_frameChunkSize, m_writeOffset);
            m_writeOffset += m_frameChunkSize;
        } while (!m_shouldStopThread);
    };
    m_thread = Thread::create("AudioMediaStreamTrackRendererInternalUnit thread", WTFMove(threadLoop), ThreadType::Audio, Thread::QOS::UserInteractive);
}

void AudioMediaStreamTrackRendererInternalUnitManager::Proxy::reset(IsClosed isClosed)
{
    stopThread();
    m_didClose = isClosed == IsClosed::Yes;
    m_resetCallback();
    if (m_isPlaying)
        start();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM) && ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
