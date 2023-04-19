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
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManager.h"

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "IPCSemaphore.h"
#include "Logging.h"
#include "SharedCARingBuffer.h"
#include <WebCore/AudioMediaStreamTrackRendererInternalUnit.h>
#include <WebCore/AudioSampleBufferList.h>
#include <WebCore/AudioSession.h>
#include <WebCore/AudioUtilities.h>
#include <WebCore/CAAudioStreamDescription.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/CoreAudioCaptureSource.h>
#include <WebCore/CoreAudioSharedUnit.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit
    : public CanMakeWeakPtr<RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit>
    , public WebCore::CoreAudioSpeakerSamplesProducer
    , public WebCore::AudioSession::InterruptionObserver
    , private WebCore::AudioMediaStreamTrackRendererInternalUnit::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Unit(AudioMediaStreamTrackRendererInternalUnitIdentifier, Ref<IPC::Connection>&&, bool shouldRegisterAsSpeakerSamplesProducer, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&&);
    ~Unit();

    void start(ConsumerSharedCARingBuffer::Handle&&, IPC::Semaphore&&);
    void stop();
    void setAudioOutputDevice(const String&);

    void setShouldRegisterAsSpeakerSamplesProducer(bool);

    using WebCore::AudioSession::InterruptionObserver::weakPtrFactory;
    using WebCore::AudioSession::InterruptionObserver::WeakValueType;
    using WebCore::AudioSession::InterruptionObserver::WeakPtrImplType;

private:
    // CoreAudioSpeakerSamplesProducer
    const WebCore::CAAudioStreamDescription& format() final { return *m_description; }
    void captureUnitIsStarting() final;
    void captureUnitHasStopped() final;

    // Background thread.
    OSStatus produceSpeakerSamples(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&) final;

    // WebCore::AudioSession::InterruptionObserver
    void beginAudioSessionInterruption() final;
    void endAudioSessionInterruption(WebCore::AudioSession::MayResume) final;

    // WebCore::AudioMediaStreamTrackRendererInternal::Client
    OSStatus render(size_t sampleCount, AudioBufferList&, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags&) final;
    void reset() final;

    AudioMediaStreamTrackRendererInternalUnitIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
    UniqueRef<WebCore::AudioMediaStreamTrackRendererInternalUnit> m_localUnit;
    uint64_t m_readOffset { 0 };
    uint64_t m_generateOffset { 0 };
    uint64_t m_frameChunkSize { 0 };
    IPC::Semaphore m_renderSemaphore;
    std::unique_ptr<ConsumerSharedCARingBuffer> m_ringBuffer;
    bool m_isPlaying { false };
    std::optional<WebCore::CAAudioStreamDescription> m_description;
    bool m_shouldRegisterAsSpeakerSamplesProducer { false };
    bool m_canReset { true };
};

RemoteAudioMediaStreamTrackRendererInternalUnitManager::RemoteAudioMediaStreamTrackRendererInternalUnitManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
{
}

RemoteAudioMediaStreamTrackRendererInternalUnitManager::~RemoteAudioMediaStreamTrackRendererInternalUnitManager()
{
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::createUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback)
{
    ASSERT(!m_units.contains(identifier));
    m_units.add(identifier, makeUniqueRef<RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit>(identifier, m_gpuConnectionToWebProcess.connection(), m_gpuConnectionToWebProcess.isLastToCaptureAudio(), WTFMove(callback)));
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::deleteUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    if (!m_units.remove(identifier))
        return;

    if (m_units.isEmpty())
        m_gpuConnectionToWebProcess.gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::startUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, ConsumerSharedCARingBuffer::Handle&& handle, IPC::Semaphore&& semaphore)
{
    if (auto* unit = m_units.get(identifier))
        unit->start(WTFMove(handle), WTFMove(semaphore));
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::stopUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    if (auto* unit = m_units.get(identifier))
        unit->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::setAudioOutputDevice(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, const String& deviceId)
{
    if (auto* unit = m_units.get(identifier))
        unit->setAudioOutputDevice(deviceId);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::notifyLastToCaptureAudioChanged()
{
    // FIXME: When supporting multiple units to different speakers, we should only select the unit matching the VPIO output device.
    for (auto& unit : m_units.values())
        unit->setShouldRegisterAsSpeakerSamplesProducer(m_gpuConnectionToWebProcess.isLastToCaptureAudio());
}

RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::Unit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier, Ref<IPC::Connection>&& connection, bool shouldRegisterAsSpeakerSamplesProducer, CompletionHandler<void(std::optional<WebCore::CAAudioStreamDescription>, size_t)>&& callback)
    : m_identifier(identifier)
    , m_connection(WTFMove(connection))
    , m_localUnit(WebCore::AudioMediaStreamTrackRendererInternalUnit::create(*this))
    , m_shouldRegisterAsSpeakerSamplesProducer(shouldRegisterAsSpeakerSamplesProducer)
{
    WebCore::AudioSession::sharedSession().addInterruptionObserver(*this);
    m_localUnit->retrieveFormatDescription([weakThis = WeakPtr { *this }, this, callback = WTFMove(callback)](auto&& description) mutable {
        if (!weakThis || !description) {
            RELEASE_LOG_IF(!description, WebRTC, "RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit unable to get format description");
            callback(std::nullopt, 0);
            return;
        }
        size_t tenMsSampleSize = description->sampleRate() * 10 / 1000;
        m_description = *description;
        m_frameChunkSize = std::max(WebCore::AudioUtilities::renderQuantumSize, tenMsSampleSize);
        callback(*description, m_frameChunkSize);
    });
}

RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::~Unit()
{
    WebCore::AudioSession::sharedSession().removeInterruptionObserver(*this);
    stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::reset()
{
    if (!m_canReset)
        return;

    m_canReset = false;
    m_connection->send(Messages::GPUProcessConnection::ResetAudioMediaStreamTrackRendererInternalUnit { m_identifier }, 0);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::setShouldRegisterAsSpeakerSamplesProducer(bool value)
{
    if (m_shouldRegisterAsSpeakerSamplesProducer == value)
        return;

    m_shouldRegisterAsSpeakerSamplesProducer = value;
    if (!m_isPlaying)
        return;

    if (m_shouldRegisterAsSpeakerSamplesProducer)
        WebCore::CoreAudioCaptureSourceFactory::singleton().registerSpeakerSamplesProducer(*this);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::start(ConsumerSharedCARingBuffer::Handle&& handle, IPC::Semaphore&& semaphore)
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

    m_localUnit->start();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::stop()
{
    m_isPlaying = false;
    WebCore::CoreAudioCaptureSourceFactory::singleton().unregisterSpeakerSamplesProducer(*this);
    m_localUnit->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::setAudioOutputDevice(const String& deviceId)
{
    m_localUnit->setAudioOutputDevice(deviceId);
}

OSStatus RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::render(size_t sampleCount, AudioBufferList& list, uint64_t, double, AudioUnitRenderActionFlags& flags)
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

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::captureUnitIsStarting()
{
    // Capture unit is starting and audio will be rendered through it and not by our local unit so stop the local unit.
    if (m_isPlaying && WebCore::CoreAudioCaptureSourceFactory::singleton().shouldAudioCaptureUnitRenderAudio())
        m_localUnit->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::captureUnitHasStopped()
{
    // Capture unit has stopped and audio will no longer be rendered through it so start the local unit.
    if (m_isPlaying && !CoreAudioSharedUnit::unit().isSuspended())
        m_localUnit->start();
}

// Background thread.
OSStatus RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::produceSpeakerSamples(size_t sampleCount, AudioBufferList& list, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags& flags)
{
    return render(sampleCount, list, sampleTime, hostTime, flags);
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::beginAudioSessionInterruption()
{
    if (m_isPlaying)
        m_localUnit->stop();
}

void RemoteAudioMediaStreamTrackRendererInternalUnitManager::Unit::endAudioSessionInterruption(WebCore::AudioSession::MayResume)
{
    if (!m_isPlaying)
        return;

    if (m_shouldRegisterAsSpeakerSamplesProducer && (WebCore::CoreAudioSharedUnit::unit().isRunning() || WebCore::CoreAudioSharedUnit::unit().isSuspended()))
        return;

    m_localUnit->start();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
