/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManager.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "IPCSemaphore.h"
#include "Logging.h"
#include "SharedCARingBuffer.h"
#include <WebCore/AudioMediaStreamTrackRenderer.h>
#include <WebCore/AudioMediaStreamTrackRendererInternalUnit.h>
#include <WebCore/AudioSampleBufferList.h>
#include <WebCore/AudioSession.h>
#include <WebCore/AudioUtilities.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/CoreAudioCaptureSource.h>
#include <WebCore/CoreAudioSharedUnit.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit
    : public WebCore::CoreAudioSpeakerSamplesProducer
    , public WebCore::AudioSessionInterruptionObserver
    , public WebCore::AudioMediaStreamTrackRendererInternalUnit::Client {
    WTF_MAKE_TZONE_ALLOCATED(RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit);
public:
    static Ref<RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit> create(
        AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, const String& deviceID, GPUConnectionToWebProcess& connection, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback) {
        return adoptRef(*new RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit(identifier, deviceID, connection, WTFMove(callback)));
    }

    ~RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit();

    void start(ConsumerSharedCARingBuffer::Handle&&, IPC::Semaphore&&);
    void stop();

    void updateShouldRegisterAsSpeakerSamplesProducer();

    USING_CAN_MAKE_WEAKPTR(WebCore::AudioSessionInterruptionObserver);

private:
    RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier, const String&, GPUConnectionToWebProcess&, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&&);

    // CoreAudioSpeakerSamplesProducer
    const WebCore::CAAudioStreamDescription& format() final { return *m_description; }
    void captureUnitIsStarting() final;
    void captureUnitHasStopped() final;

    // Background thread.
    OSStatus produceSpeakerSamples(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&) final;

    // WebCore::AudioSessionInterruptionObserver
    void beginAudioSessionInterruption() final;
    void endAudioSessionInterruption(WebCore::AudioSession::MayResume) final;

    // WebCore::AudioMediaStreamTrackRendererInternal::Client
    OSStatus render(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&) final;
    void reset() final;

    Ref<WebCore::AudioMediaStreamTrackRendererInternalUnit> protectedLocalUnit() { return m_localUnit; }
    void setShouldRegisterAsSpeakerSamplesProducer(bool);

    AudioMediaStreamTrackRendererInternalUnitIdentifier m_identifier;
    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_connection;
    Ref<WebCore::AudioMediaStreamTrackRendererInternalUnit> m_localUnit;
    uint64_t m_readOffset { 0 };
    uint64_t m_generateOffset { 0 };
    uint64_t m_frameChunkSize { 0 };
    IPC::Semaphore m_renderSemaphore;
    std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
    bool m_isPlaying { false };
    std::optional<WebCore::CAAudioStreamDescription> m_description;
    bool m_canUseCaptureUnit { false };
    bool m_shouldRegisterAsSpeakerSamplesProducer { false };
    bool m_canReset { true };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioMediaStreamTrackRendererInternalUnitManager);

RemoteAudioMediaStreamTrackRendererInternalUnitManager::RemoteAudioMediaStreamTrackRendererInternalUnitManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
{
}

RemoteAudioMediaStreamTrackRendererInternalUnitManager::~RemoteAudioMediaStreamTrackRendererInternalUnitManager()
{
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::createUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, const String& deviceID,  CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback)
{
    ASSERT(!m_units.contains(identifier));
    if (auto connection = m_gpuConnectionToWebProcess.get())
        m_units.add(identifier, RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::create(identifier, deviceID, *connection, WTFMove(callback)));
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::deleteUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    if (!m_units.remove(identifier))
        return;

    if (m_units.isEmpty()) {
        if (auto connection = m_gpuConnectionToWebProcess.get())
            connection->gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
    }
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::startUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle, IPC::Semaphore&& semaphore)
{
    if (RefPtr unit = m_units.get(identifier))
        unit->start(WTFMove(handle), WTFMove(semaphore));
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::stopUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    if (RefPtr unit = m_units.get(identifier))
        unit->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::notifyLastToCaptureAudioChanged()
{
    for (Ref unit : m_units.values())
        unit->updateShouldRegisterAsSpeakerSamplesProducer();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit);

RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, const String& deviceID, GPUConnectionToWebProcess& connection, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback)
    : m_identifier(identifier)
    , m_connection(connection)
    , m_localUnit(WebCore::AudioMediaStreamTrackRendererInternalUnit::create(deviceID, *this))
    , m_canUseCaptureUnit(deviceID == AudioMediaStreamTrackRenderer::defaultDeviceID())
    , m_shouldRegisterAsSpeakerSamplesProducer(m_canUseCaptureUnit && connection.isLastToCaptureAudio())
{
    WebCore::AudioSession::protectedSharedSession()->addInterruptionObserver(*this);
    protectedLocalUnit()->retrieveFormatDescription([weakThis = WeakPtr { *this }, this, callback = WTFMove(callback)](auto&& description) mutable {
        if (!weakThis || !description) {
            RELEASE_LOG_IF(!description, WebRTC, "RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit unable to get format description");
            callback(std::nullopt, 0);
            return;
        }
        size_t tenMsSampleSize = description->sampleRate() * 10 / 1000;
        m_description = *description;
        m_frameChunkSize = std::max(WebCore::AudioUtilities::renderQuantumSize, tenMsSampleSize);
        callback(*description, m_frameChunkSize);
    });
}

RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::~RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit()
{
    WebCore::AudioSession::protectedSharedSession()->removeInterruptionObserver(*this);
    stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::reset()
{
    if (!m_canReset)
        return;

    m_canReset = false;
    if (RefPtr connection = m_connection.get())
        connection->protectedConnection()->send(Messages::GPUProcessConnection::ResetAudioMediaStreamTrackRendererInternalUnit { m_identifier }, 0);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::setShouldRegisterAsSpeakerSamplesProducer(bool value)
{
    if (!m_canUseCaptureUnit)
        return;

    if (m_shouldRegisterAsSpeakerSamplesProducer == value)
        return;

    m_shouldRegisterAsSpeakerSamplesProducer = value;
    if (!m_isPlaying)
        return;

    if (m_shouldRegisterAsSpeakerSamplesProducer) {
        protectedLocalUnit()->stop();
        WebCore::CoreAudioCaptureSourceFactory::singleton().registerSpeakerSamplesProducer(*this);
    } else {
        WebCore::CoreAudioCaptureSourceFactory::singleton().unregisterSpeakerSamplesProducer(*this);
        protectedLocalUnit()->start();
    }
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::start(ConsumerSharedCARingBuffer::Handle&& handle, IPC::Semaphore&& semaphore)
{
    if (m_isPlaying)
        stop();
    m_ringBuffer = ConsumerSharedCARingBuffer::map(*m_description, WTFMove(handle));
    if (!m_ringBuffer)
        return;
    m_readOffset = 0;
    m_generateOffset = 0;
    m_isPlaying = true;
    m_canReset = true;
    m_renderSemaphore = WTFMove(semaphore);

    if (m_shouldRegisterAsSpeakerSamplesProducer) {
        WebCore::CoreAudioCaptureSourceFactory::singleton().registerSpeakerSamplesProducer(*this);
        bool shouldNotStartLocalUnit = WebCore::CoreAudioCaptureSourceFactory::singleton().shouldAudioCaptureUnitRenderAudio() || WebCore::AudioSession::sharedSession().isInterrupted();
        if (shouldNotStartLocalUnit)
            return;
    }

    protectedLocalUnit()->start();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::stop()
{
    m_isPlaying = false;
    WebCore::CoreAudioCaptureSourceFactory::singleton().unregisterSpeakerSamplesProducer(*this);
    protectedLocalUnit()->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::updateShouldRegisterAsSpeakerSamplesProducer()
{
    setShouldRegisterAsSpeakerSamplesProducer(m_connection.get()->isLastToCaptureAudio());
}

OSStatus RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::render(size_t sampleCount, AudioBufferList& list, uint64_t, double, AudioUnitRenderActionFlags& flags)
{
    ASSERT(!isMainRunLoop());

    OSStatus status = -1;

    if (m_ringBuffer->fetchIfHasEnoughData(&list, sampleCount, m_readOffset)) {
        m_readOffset += sampleCount;
        status = noErr;
    } else {
        WebCore::AudioSampleBufferList::zeroABL(list, static_cast<size_t>(sampleCount * m_description->bytesPerFrame()));
        flags = kAudioUnitRenderAction_OutputIsSilence;
    }

    auto requestedSamplesCount = m_generateOffset;
    for (; requestedSamplesCount < sampleCount; requestedSamplesCount += m_frameChunkSize)
        m_renderSemaphore.signal();
    m_generateOffset = requestedSamplesCount - sampleCount;

    return status;
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::captureUnitIsStarting()
{
    // Capture unit is starting and audio will be rendered through it and not by our local unit so stop the local unit.
    if (m_isPlaying && WebCore::CoreAudioCaptureSourceFactory::singleton().shouldAudioCaptureUnitRenderAudio())
        protectedLocalUnit()->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::captureUnitHasStopped()
{
    // Capture unit has stopped and audio will no longer be rendered through it so start the local unit.
    if (m_isPlaying && !WebCore::CoreAudioSharedUnit::singleton().isSuspended())
        protectedLocalUnit()->start();
}

// Background thread.
OSStatus RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::produceSpeakerSamples(size_t sampleCount, AudioBufferList& list, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags& flags)
{
    return render(sampleCount, list, sampleTime, hostTime, flags);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::beginAudioSessionInterruption()
{
    if (m_isPlaying)
        protectedLocalUnit()->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManagerUnit::endAudioSessionInterruption(WebCore::AudioSession::MayResume)
{
    if (!m_isPlaying)
        return;

    if (m_shouldRegisterAsSpeakerSamplesProducer && (WebCore::CoreAudioSharedUnit::singleton().isRunning() || WebCore::CoreAudioSharedUnit::singleton().isSuspended()))
        return;

    protectedLocalUnit()->start();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
