/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "UserMediaCaptureManagerProxy.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "Connection.h"
#include "RemoteCaptureSampleManagerMessages.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "SharedCARingBuffer.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessProxy.h"
#include <WebCore/AudioSession.h>
#include <WebCore/AudioUtilities.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/ImageRotationSessionVT.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeVideoSource.h>
#include <WebCore/VideoFrameCV.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, &m_connectionProxy->connection())

namespace WebKit {
using namespace WebCore;

class UserMediaCaptureManagerProxy::SourceProxy
    : private RealtimeMediaSource::Observer
    , private RealtimeMediaSource::AudioSampleObserver
    , private RealtimeMediaSource::VideoFrameObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SourceProxy(RealtimeMediaSourceIdentifier id, Ref<IPC::Connection>&& connection, ProcessIdentity&& resourceOwner, Ref<RealtimeMediaSource>&& source, RefPtr<RemoteVideoFrameObjectHeap>&& videoFrameObjectHeap)
        : m_id(id)
        , m_connection(WTFMove(connection))
        , m_resourceOwner(WTFMove(resourceOwner))
        , m_source(WTFMove(source))
        , m_videoFrameObjectHeap(WTFMove(videoFrameObjectHeap))
    {
        m_source->addObserver(*this);
        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->addAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            m_source->addVideoFrameObserver(*this);
            break;
        }
    }

    ~SourceProxy()
    {
        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->removeAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            m_source->removeVideoFrameObserver(*this);
            break;
        }
        m_source->removeObserver(*this);
    }

    RealtimeMediaSource& source() { return m_source; }

    void audioUnitWillStart() final
    {
        AudioSession::sharedSession().setCategory(AudioSession::CategoryType::PlayAndRecord, RouteSharingPolicy::Default);
        AudioSession::sharedSession().tryToSetActive(true);
    }

    void start()
    {
        m_shouldReset = true;
        m_isStopped = false;
        m_source->start();
    }

    void stop()
    {
        m_isStopped = true;
        m_source->stop();
    }

    void end()
    {
        m_isStopped = true;
        m_source->requestToEnd(*this);
    }

    void setShouldApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = true; }
    void setIsInBackground(bool value) { m_source->setIsInBackground(value); }

private:
    void sourceStopped() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceStopped(m_id, m_source->captureDidFail()), 0);
    }

    void sourceMutedChanged() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceMutedChanged(m_id, m_source->muted(), m_source->interrupted()), 0);
    }

    void sourceSettingsChanged() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceSettingsChanged(m_id, m_source->settings()), 0);
    }

    void sourceConfigurationChanged() final
    {
        m_connection->send(Messages::UserMediaCaptureManager::SourceConfigurationChanged(m_id, m_source->persistentID(), m_source->settings(), m_source->capabilities()), 0);
    }

    // May get called on a background thread.
    void audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames) final {
        if (m_description != description || m_shouldReset) {
            DisableMallocRestrictionsForCurrentThreadScope scope;

            m_shouldReset = false;
            m_writeOffset = 0;
            m_remainingFrameCount = 0;
            m_startTime = time;
            m_captureSemaphore = makeUnique<IPC::Semaphore>();
            ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
            m_description = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

            m_frameChunkSize = std::max(WebCore::AudioUtilities::renderQuantumSize, AudioSession::sharedSession().preferredBufferSize());

            // Allocate a ring buffer large enough to contain 2 seconds of audio.
            auto [ringBuffer, handle] = ProducerSharedCARingBuffer::allocate(*m_description, m_description->sampleRate() * 2);
            m_ringBuffer = WTFMove(ringBuffer);
            m_connection->send(Messages::RemoteCaptureSampleManager::AudioStorageChanged(m_id, WTFMove(handle), *m_description, *m_captureSemaphore, m_startTime, m_frameChunkSize), 0);
        }

        ASSERT(is<WebAudioBufferList>(audioData));
        m_ringBuffer->store(downcast<WebAudioBufferList>(audioData).list(), numberOfFrames, m_writeOffset);
        m_writeOffset += numberOfFrames;

        size_t framesToSend = numberOfFrames + m_remainingFrameCount;
        size_t signalCount = framesToSend / m_frameChunkSize;
        m_remainingFrameCount = framesToSend - (signalCount * m_frameChunkSize);
        for (unsigned i = 0; i < signalCount; ++i)
            m_captureSemaphore->signal();
    }

    RefPtr<VideoFrame> rotateVideoFrameIfNeeded(VideoFrame& frame)
    {
        if (m_shouldApplyRotation && frame.rotation() != VideoFrame::Rotation::None) {
            auto pixelBuffer = rotatePixelBuffer(frame);
            return VideoFrameCV::create(frame.presentationTime(), frame.isMirrored(), VideoFrame::Rotation::None, WTFMove(pixelBuffer));
        }
        return &frame;
    }

    void videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata metadata) final
    {
        auto videoFrame = rotateVideoFrameIfNeeded(frame);
        if (!videoFrame)
            return;
        if (m_resourceOwner)
            videoFrame->setOwnershipIdentity(m_resourceOwner);
        if (!m_videoFrameObjectHeap) {
            m_connection->send(Messages::RemoteCaptureSampleManager::VideoFrameAvailableCV(m_id, videoFrame->pixelBuffer(), videoFrame->rotation(), videoFrame->isMirrored(), videoFrame->presentationTime(), metadata), 0);
            return;
        }
        auto properties = m_videoFrameObjectHeap->add(*videoFrame);
        m_connection->send(Messages::RemoteCaptureSampleManager::VideoFrameAvailable(m_id, properties, metadata), 0);
    }

    RetainPtr<CVPixelBufferRef> rotatePixelBuffer(VideoFrame& videoFrame)
    {
        if (!m_rotationSession)
            m_rotationSession = makeUnique<ImageRotationSessionVT>();

        ImageRotationSessionVT::RotationProperties rotation;
        switch (videoFrame.rotation()) {
        case VideoFrame::Rotation::None:
            ASSERT_NOT_REACHED();
            rotation.angle = 0;
            break;
        case VideoFrame::Rotation::UpsideDown:
            rotation.angle = 180;
            break;
        case VideoFrame::Rotation::Right:
            rotation.angle = 90;
            break;
        case VideoFrame::Rotation::Left:
            rotation.angle = 270;
            break;
        }
        return m_rotationSession->rotate(videoFrame, rotation, ImageRotationSessionVT::IsCGImageCompatible::No);
    }

    bool preventSourceFromStopping()
    {
        // Do not allow the source to stop if we are still using it.
        return !m_isStopped;
    }

    RealtimeMediaSourceIdentifier m_id;
    WeakPtr<PlatformMediaSessionManager> m_sessionManager;
    Ref<IPC::Connection> m_connection;
    ProcessIdentity m_resourceOwner;
    Ref<RealtimeMediaSource> m_source;
    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    std::optional<CAAudioStreamDescription> m_description;
    bool m_isStopped { false };
    std::unique_ptr<ImageRotationSessionVT> m_rotationSession;
    bool m_shouldApplyRotation { false };
    std::unique_ptr<IPC::Semaphore> m_captureSemaphore;
    int64_t m_writeOffset { 0 };
    int64_t m_remainingFrameCount { 0 };
    size_t m_frameChunkSize { 0 };
    MediaTime m_startTime;
    bool m_shouldReset { false };
    RefPtr<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
};

UserMediaCaptureManagerProxy::UserMediaCaptureManagerProxy(UniqueRef<ConnectionProxy>&& connectionProxy)
    : m_connectionProxy(WTFMove(connectionProxy))
{
    m_connectionProxy->addMessageReceiver(Messages::UserMediaCaptureManagerProxy::messageReceiverName(), *this);
}

UserMediaCaptureManagerProxy::~UserMediaCaptureManagerProxy()
{
    m_connectionProxy->removeMessageReceiver(Messages::UserMediaCaptureManagerProxy::messageReceiverName());
}

CaptureSourceOrError UserMediaCaptureManagerProxy::createMicrophoneSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* mediaConstraints, PageIdentifier pageIdentifier)
{
    auto sourceOrError = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().createAudioCaptureSource(device, WTFMove(hashSalts), mediaConstraints, pageIdentifier);
    if (!sourceOrError)
        return sourceOrError;

    auto& perPageSources = m_pageSources.ensure(pageIdentifier, [] { return PageSources { }; }).iterator->value;

    // FIXME: Support multiple microphones simultaneously.
    if (auto* microphoneSource = perPageSources.microphoneSource.get()) {
        if (microphoneSource->persistentID() != device.persistentId() && !microphoneSource->isEnded()) {
            RELEASE_LOG_ERROR(WebRTC, "Ending microphone source as new source is using a different device.");
            // FIXME: We should probably fail the capture in a way that shows a specific console log message.
            microphoneSource->endImmediatly();
        }
    }

    auto source = sourceOrError.source();
    perPageSources.microphoneSource = WeakPtr { source.get() };
    return source;
}

static bool canCaptureFromMultipleCameras()
{
#if PLATFORM(IOS_FAMILY)
    return false;
#else
    return true;
#endif
}

CaptureSourceOrError UserMediaCaptureManagerProxy::createCameraSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* mediaConstraints, PageIdentifier pageIdentifier)
{
    auto& perPageSources = m_pageSources.ensure(pageIdentifier, [] { return PageSources { }; }).iterator->value;
    for (auto& cameraSource : perPageSources.cameraSources) {
        // FIXME: Optimize multiple concurrent cameras.
        if (cameraSource.persistentID() == device.persistentId() && !cameraSource.isEnded()) {
            // We can reuse the source, let's do it.
            auto source = cameraSource.clone();
            if (mediaConstraints) {
                auto error = source->applyConstraints(*mediaConstraints);
                if (error)
                    return WTFMove(error->message);
            }
            perPageSources.cameraSources.add(source.get());
            return source;
        }
    }

    auto sourceOrError = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().createVideoCaptureSource(device, WTFMove(hashSalts), mediaConstraints, pageIdentifier);
    if (!sourceOrError)
        return sourceOrError;

    if (!canCaptureFromMultipleCameras()) {
        perPageSources.cameraSources.forEach([](auto& source) {
            RELEASE_LOG_ERROR(WebRTC, "Ending camera source as new source is using a different device.");
            // FIXME: We should probably fail the capture in a way that shows a specific console log message.
            source.endImmediatly();
        });
    }

    auto source = sourceOrError.source();
    source->monitorOrientation(m_orientationNotifier);
    perPageSources.cameraSources.add(source.get());
    return source;
}

void UserMediaCaptureManagerProxy::createMediaSourceForCaptureDeviceWithConstraints(RealtimeMediaSourceIdentifier id, const CaptureDevice& device, WebCore::MediaDeviceHashSalts&& hashSalts, const MediaConstraints& mediaConstraints, bool shouldUseGPUProcessRemoteFrames, PageIdentifier pageIdentifier, CreateSourceCallback&& completionHandler)
{
    if (!m_connectionProxy->willStartCapture(device.type()))
        return completionHandler(false, "Request is not allowed"_s, RealtimeMediaSourceSettings { }, { }, { }, { }, 0);

    auto* constraints = mediaConstraints.isValid ? &mediaConstraints : nullptr;

    CaptureSourceOrError sourceOrError;
    switch (device.type()) {
    case WebCore::CaptureDevice::DeviceType::Microphone:
        sourceOrError = createMicrophoneSource(device, WTFMove(hashSalts), constraints, pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::Camera:
        sourceOrError = createCameraSource(device, WTFMove(hashSalts), constraints, pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::Screen:
    case WebCore::CaptureDevice::DeviceType::Window:
        sourceOrError = RealtimeMediaSourceCenter::singleton().displayCaptureFactory().createDisplayCaptureSource(device, WTFMove(hashSalts), constraints, pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::SystemAudio:
    case WebCore::CaptureDevice::DeviceType::Speaker:
    case WebCore::CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    bool succeeded = !!sourceOrError;
    String invalidConstraints;
    RealtimeMediaSourceSettings settings;
    RealtimeMediaSourceCapabilities capabilities;
    Vector<VideoPresetData> presets;
    IntSize size;
    double frameRate = 0;

    if (sourceOrError) {
        auto source = sourceOrError.source();
#if !RELEASE_LOG_DISABLED
        source->setLogger(m_connectionProxy->logger(), LoggerHelper::uniqueLogIdentifier());
#endif
        settings = source->settings();
        capabilities = source->capabilities();
        if (source->isVideoSource()) {
            auto& videoSource = static_cast<RealtimeVideoSource&>(source.get());
            presets = videoSource.presetsData();
            size = videoSource.size();
            frameRate = videoSource.frameRate();
        }

        ASSERT(!m_proxies.contains(id));
        auto proxy = makeUnique<SourceProxy>(id, m_connectionProxy->connection(), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(source), shouldUseGPUProcessRemoteFrames ? m_connectionProxy->remoteVideoFrameObjectHeap() : nullptr);
        m_proxies.add(id, WTFMove(proxy));
    } else
        invalidConstraints = WTFMove(sourceOrError.errorMessage);

    completionHandler(succeeded, invalidConstraints, WTFMove(settings), WTFMove(capabilities), WTFMove(presets), size, frameRate);
}

void UserMediaCaptureManagerProxy::startProducingData(RealtimeMediaSourceIdentifier id)
{
    auto* proxy = m_proxies.get(id);
    if (!proxy)
        return;

    if (!m_connectionProxy->setCaptureAttributionString()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to set capture attribution, failing capture.");
        return;
    }

#if ENABLE(APP_PRIVACY_REPORT)
    m_connectionProxy->setTCCIdentity();
#endif
    m_connectionProxy->startProducingData(proxy->source().type());
    proxy->start();
}

void UserMediaCaptureManagerProxy::stopProducingData(RealtimeMediaSourceIdentifier id)
{
    if (auto* proxy = m_proxies.get(id))
        proxy->stop();
}

void UserMediaCaptureManagerProxy::removeSource(RealtimeMediaSourceIdentifier id)
{
    m_proxies.remove(id);
}

void UserMediaCaptureManagerProxy::capabilities(RealtimeMediaSourceIdentifier id, CompletionHandler<void(RealtimeMediaSourceCapabilities&&)>&& completionHandler)
{
    RealtimeMediaSourceCapabilities capabilities;
    if (auto* proxy = m_proxies.get(id))
        capabilities = proxy->source().capabilities();
    completionHandler(WTFMove(capabilities));
}

void UserMediaCaptureManagerProxy::applyConstraints(RealtimeMediaSourceIdentifier id, const WebCore::MediaConstraints& constraints)
{
    auto* proxy = m_proxies.get(id);
    if (!proxy) {
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, { }, "Unknown source"_s), 0);
        return;
    }

    auto& source = proxy->source();
    auto result = source.applyConstraints(constraints);
    if (!result)
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsSucceeded(id, source.settings()), 0);
    else
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, result->badConstraint, result->message), 0);
}

void UserMediaCaptureManagerProxy::clone(RealtimeMediaSourceIdentifier clonedID, RealtimeMediaSourceIdentifier newSourceID, WebCore::PageIdentifier pageIdentifier)
{
    MESSAGE_CHECK(m_proxies.contains(clonedID));
    MESSAGE_CHECK(!m_proxies.contains(newSourceID));
    if (auto* proxy = m_proxies.get(clonedID)) {
        auto sourceClone = proxy->source().clone();
        if (sourceClone->deviceType() == WebCore::CaptureDevice::DeviceType::Camera)
            m_pageSources.ensure(pageIdentifier, [] { return PageSources { }; }).iterator->value.cameraSources.add(sourceClone.get());

        m_proxies.add(newSourceID, makeUnique<SourceProxy>(newSourceID, m_connectionProxy->connection(), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(sourceClone), m_connectionProxy->remoteVideoFrameObjectHeap()));
    }
}

void UserMediaCaptureManagerProxy::endProducingData(RealtimeMediaSourceIdentifier sourceID)
{
    if (auto* proxy = m_proxies.get(sourceID))
        proxy->end();
}

void UserMediaCaptureManagerProxy::setShouldApplyRotation(RealtimeMediaSourceIdentifier sourceID, bool shouldApplyRotation)
{
    if (auto* proxy = m_proxies.get(sourceID))
        proxy->setShouldApplyRotation(shouldApplyRotation);
}

void UserMediaCaptureManagerProxy::setIsInBackground(RealtimeMediaSourceIdentifier sourceID, bool isInBackground)
{
    if (auto* proxy = m_proxies.get(sourceID))
        proxy->setIsInBackground(isInBackground);
}

void UserMediaCaptureManagerProxy::clear()
{
    m_proxies.clear();
}

void UserMediaCaptureManagerProxy::setOrientation(uint64_t orientation)
{
    m_orientationNotifier.orientationChanged(orientation);
}

bool UserMediaCaptureManagerProxy::hasSourceProxies() const
{
    return !m_proxies.isEmpty();
}

}

#undef MESSAGE_CHECK

#endif
