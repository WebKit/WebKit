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
#include "GPUProcessConnection.h"
#include "IPCSemaphore.h"
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManagerMessages.h"
#include "SharedCARingBuffer.h"
#include "WebProcess.h"
#include <WebCore/AudioMediaStreamTrackRendererInternalUnit.h>
#include <WebCore/AudioMediaStreamTrackRendererUnit.h>
#include <WebCore/AudioSampleBufferList.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/WebAudioBufferList.h>
#include <mach/mach_time.h>
#include <wtf/Deque.h>
#include <wtf/Identified.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

class AudioMediaStreamTrackRendererInternalUnitManagerProxy final : public WebCore::AudioMediaStreamTrackRendererInternalUnit, public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<AudioMediaStreamTrackRendererInternalUnitManagerProxy>, public Identified<AudioMediaStreamTrackRendererInternalUnitIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(AudioMediaStreamTrackRendererInternalUnitManagerProxy);
public:
    static Ref<AudioMediaStreamTrackRendererInternalUnitManagerProxy> create(const String& deviceID, Client& client)
    {
        return adoptRef(*new AudioMediaStreamTrackRendererInternalUnitManagerProxy(deviceID, client));
    }

    ~AudioMediaStreamTrackRendererInternalUnitManagerProxy();

    void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
    void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }

    enum class IsClosed : bool { No, Yes };
    void reset(IsClosed);

private:
    explicit AudioMediaStreamTrackRendererInternalUnitManagerProxy(const String&, Client&);

    // AudioMediaStreamTrackRendererUnit::InternalUnit API.
    void start() final;
    void stop() final;
    void close() { stopThread(); }

    void retrieveFormatDescription(CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>)>&&) final;

    void initialize(const WebCore::CAAudioStreamDescription&, size_t frameChunkSize);
    void storageChanged(WebCore::SharedMemory*, const WebCore::CAAudioStreamDescription&, size_t);

    void stopThread();
    void startThread();
    void createRemoteUnit();

    String m_deviceID;
    ThreadSafeWeakPtr<Client> m_client;

    Deque<CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>)>> m_descriptionCallbacks;

    bool m_isPlaying { false };

    std::optional<WebCore::CAAudioStreamDescription> m_description;
    std::unique_ptr<WebCore::WebAudioBufferList> m_buffer;
    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    int64_t m_writeOffset { 0 };
    size_t m_frameChunkSize { 0 };
    size_t m_numberOfFrames { 0 };

    std::unique_ptr<IPC::Semaphore> m_semaphore;
    RefPtr<Thread> m_thread;
    std::atomic<bool> m_shouldStopThread { false };
    bool m_didClose { false };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioMediaStreamTrackRendererInternalUnitManager);

void AudioMediaStreamTrackRendererInternalUnitManager::add(AudioMediaStreamTrackRendererInternalUnitManagerProxy& proxy)
{
    m_proxies.add(proxy.identifier(), proxy);
}

void AudioMediaStreamTrackRendererInternalUnitManager::remove(AudioMediaStreamTrackRendererInternalUnitManagerProxy& proxy)
{
    m_proxies.remove(proxy.identifier());
}

Ref<WebCore::AudioMediaStreamTrackRendererInternalUnit> createRemoteAudioMediaStreamTrackRendererInternalUnitProxy(const String& deviceID, WebCore::AudioMediaStreamTrackRendererInternalUnit::Client& client)
{
    return AudioMediaStreamTrackRendererInternalUnitManagerProxy::create(deviceID, client);
}

void AudioMediaStreamTrackRendererInternalUnitManager::reset(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    auto weakProxy = m_proxies.get(identifier);
    if (RefPtr proxy = weakProxy.get())
        proxy->reset(AudioMediaStreamTrackRendererInternalUnitManagerProxy::IsClosed::No);
}

void AudioMediaStreamTrackRendererInternalUnitManager::restartAllUnits()
{
    auto proxies = std::exchange(m_proxies, { });
    for (auto weakProxy : proxies.values()) {
        if (RefPtr proxy = weakProxy.get())
            proxy->reset(AudioMediaStreamTrackRendererInternalUnitManagerProxy::IsClosed::Yes);
    }
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(AudioMediaStreamTrackRendererInternalUnitManagerProxy);

AudioMediaStreamTrackRendererInternalUnitManagerProxy::AudioMediaStreamTrackRendererInternalUnitManagerProxy(const String& deviceID, Client& client)
    : m_deviceID(deviceID)
    , m_client(client)
{
    WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().add(*this);
    createRemoteUnit();
}

AudioMediaStreamTrackRendererInternalUnitManagerProxy::~AudioMediaStreamTrackRendererInternalUnitManagerProxy()
{
    WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().remove(*this);
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::DeleteUnit { identifier() }, 0);

    while (!m_descriptionCallbacks.isEmpty())
        m_descriptionCallbacks.takeFirst()(std::nullopt);
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::createRemoteUnit()
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendWithAsyncReply(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::CreateUnit { identifier(), m_deviceID }, [weakThis = ThreadSafeWeakPtr { *this }](auto&& description, auto frameChunkSize) {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && description && frameChunkSize)
            protectedThis->initialize(*description, frameChunkSize);
    }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::initialize(const WebCore::CAAudioStreamDescription& description, size_t frameChunkSize)
{
    if (m_semaphore)
        stopThread();

    m_semaphore = makeUnique<IPC::Semaphore>();
    m_description = description;
    m_frameChunkSize = frameChunkSize;

    while (!m_descriptionCallbacks.isEmpty())
        m_descriptionCallbacks.takeFirst()(description);

    if (m_isPlaying)
        start();
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::start()
{
    if (!m_description) {
        m_isPlaying = true;
        return;
    }

    if (m_didClose) {
        m_didClose = false;
        createRemoteUnit();
        m_isPlaying = true;
        return;
    }

    stopThread();

    m_isPlaying = true;

    m_numberOfFrames = m_description->sampleRate() * 2;
    auto result = ProducerSharedCARingBuffer::allocate(*m_description, m_numberOfFrames);
    RELEASE_ASSERT(result); // FIXME(https://bugs.webkit.org/show_bug.cgi?id=262690): Handle allocation failure.
    auto [ringBuffer, handle] = WTFMove(*result);
    m_ringBuffer = WTFMove(ringBuffer);
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::StartUnit { identifier(), WTFMove(handle), *m_semaphore }, 0);

    m_buffer = makeUnique<WebCore::WebAudioBufferList>(*m_description, m_numberOfFrames);
    m_buffer->setSampleCount(m_frameChunkSize);

    startThread();
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::stop()
{
    m_isPlaying = false;
    WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::StopUnit { identifier() }, 0);
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::retrieveFormatDescription(CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>)>&& callback)
{
    if (!m_description || !m_descriptionCallbacks.isEmpty()) {
        m_descriptionCallbacks.append(WTFMove(callback));
        return;
    }
    callback(m_description);
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::stopThread()
{
    if (!m_thread)
        return;

    m_shouldStopThread = true;
    m_semaphore->signal();
    m_thread->waitForCompletion();
    m_thread = nullptr;
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::startThread()
{
    ASSERT(!m_thread);
    m_shouldStopThread = false;
    auto threadLoop = [weakThis = ThreadSafeWeakPtr { *this }] mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        RefPtr client = protectedThis->m_client.get();
        if (!client)
            return;

        protectedThis->m_writeOffset = 0;
        do {
            // If wait fails, the semaphore on the other side was probably destroyed and we should just exit here and wait to launch a new thread.
            if (!protectedThis->m_semaphore->wait())
                break;
            if (protectedThis->m_shouldStopThread)
                break;

            auto& bufferList = *protectedThis->m_buffer->list();

            AudioUnitRenderActionFlags flags = 0;
            client->render(protectedThis->m_frameChunkSize, bufferList, protectedThis->m_writeOffset, mach_absolute_time(), flags);

            if (flags == kAudioUnitRenderAction_OutputIsSilence)
                WebCore::AudioSampleBufferList::zeroABL(bufferList, static_cast<size_t>(protectedThis->m_frameChunkSize * protectedThis->m_description->bytesPerFrame()));

            protectedThis->m_ringBuffer->store(&bufferList, protectedThis->m_frameChunkSize, protectedThis->m_writeOffset);
            protectedThis->m_writeOffset += protectedThis->m_frameChunkSize;
        } while (!protectedThis->m_shouldStopThread);
    };
    m_thread = Thread::create("AudioMediaStreamTrackRendererInternalUnit thread"_s, WTFMove(threadLoop), ThreadType::Audio, Thread::QOS::UserInteractive);
}

void AudioMediaStreamTrackRendererInternalUnitManagerProxy::reset(IsClosed isClosed)
{
    stopThread();
    m_didClose = isClosed == IsClosed::Yes;

    if (RefPtr client = m_client.get())
        client->reset();
    if (m_isPlaying)
        start();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM) && ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
