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
#include <WebCore/VideoFrameCV.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/NativePromise.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, &m_connectionProxy->connection())

namespace WebKit {
using namespace WebCore;

class UserMediaCaptureManagerProxy::SourceProxy
    : public RealtimeMediaSource::Observer
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

    void observeMedia()
    {
        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->addAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            if (m_widthConstraint || m_heightConstraint || m_frameRateConstraint)
                m_source->addVideoFrameObserver(*this, { m_widthConstraint, m_heightConstraint }, m_frameRateConstraint);
            else
                m_source->addVideoFrameObserver(*this);
            break;
        }
    }

    RealtimeMediaSource& source() { return m_source; }

    void audioUnitWillStart() final
    {
        AudioSession::sharedSession().setCategory(AudioSession::CategoryType::PlayAndRecord, AudioSession::Mode::VideoChat, RouteSharingPolicy::Default);
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
        m_isEnded = true;
        m_source->requestToEnd(*this);
    }

    void setShouldApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = true; }
    void setIsInBackground(bool value) { m_source->setIsInBackground(value); }

    bool updateVideoConstraints(const WebCore::MediaConstraints& constraints)
    {
        m_videoConstraints = constraints;

        auto resultingConstraints = m_source->extractVideoFrameSizeConstraints(constraints);

        bool didChange = false;
        if (resultingConstraints.width) {
            didChange |= m_widthConstraint != *resultingConstraints.width;
            m_widthConstraint = *resultingConstraints.width;
        } else if (resultingConstraints.height) {
            didChange |= !!m_widthConstraint;
            m_widthConstraint = 0;
        }
        if (resultingConstraints.height) {
            didChange |= m_heightConstraint != *resultingConstraints.height;
            m_heightConstraint = *resultingConstraints.height;
        } else if (resultingConstraints.width) {
            didChange |= !!m_heightConstraint;
            m_heightConstraint = 0;
        }

        if (resultingConstraints.frameRate) {
            didChange |= m_frameRateConstraint != *resultingConstraints.frameRate;
            m_frameRateConstraint = *resultingConstraints.frameRate;
        } else {
            didChange |= !!m_frameRateConstraint;
            m_frameRateConstraint = 0;
        }

        return didChange;
    }

    void queueAndProcessSerialAction(Function<Ref<GenericPromise>()>&& action)
    {
        ASSERT(isMainRunLoop());

        m_pendingAction = m_pendingAction->isResolved() ? action() : m_pendingAction->whenSettled(RunLoop::main(), WTFMove(action));
    }

    void applyConstraints(WebCore::MediaConstraints&& constraints, CompletionHandler<void(std::optional<RealtimeMediaSource::ApplyConstraintsError>&&)> callback)
    {
        queueAndProcessSerialAction([this, weakThis = WeakPtr { this }, constraints = WTFMove(constraints), callback = WTFMove(callback)]() mutable -> Ref<GenericPromise> {

            if (!weakThis) {
                callback(RealtimeMediaSource::ApplyConstraintsError { { }, { } });
                return GenericPromise::createAndResolve();
            }

            if (m_source->type() != RealtimeMediaSource::Type::Video) {
                m_source->applyConstraints(constraints, WTFMove(callback));
                return GenericPromise::createAndResolve();
            }

            m_source->removeVideoFrameObserver(*this);

            m_source->applyConstraints(WTFMove(constraints), [this, weakThis = WTFMove(weakThis), &constraints, callback = WTFMove(callback)](auto&& result) mutable {
                if (!weakThis) {
                    callback(RealtimeMediaSource::ApplyConstraintsError { { }, { } });
                    return;
                }

                if (!result && updateVideoConstraints(constraints))
                    m_settings = { };

                m_source->addVideoFrameObserver(*this, { m_widthConstraint, m_heightConstraint }, m_frameRateConstraint);

                callback(WTFMove(result));
            });

            return GenericPromise::createAndResolve();
        });
    }

    const WebCore::RealtimeMediaSourceSettings& settings()
    {
        if (m_source->type() != RealtimeMediaSource::Type::Video)
            return m_source->settings();

        if (!m_settings) {
            m_settings = m_source->settings();
            if (m_widthConstraint || m_heightConstraint) {
                auto desiredSize = m_source->computeResizedVideoFrameSize({ m_widthConstraint, m_heightConstraint }, m_source->intrinsicSize());

                auto videoFrameRotation = m_source->videoFrameRotation();
                if (videoFrameRotation == VideoFrameRotation::Left || videoFrameRotation == VideoFrameRotation::Right)
                    desiredSize = desiredSize.transposedSize();

                m_settings->setWidth(desiredSize.width());
                m_settings->setHeight(desiredSize.height());
            }

            if (m_frameRateConstraint && m_frameRateConstraint < m_settings->frameRate())
                m_settings->setFrameRate(m_frameRateConstraint);
        }

        return *m_settings;
    }

    void copySettings(SourceProxy& proxy)
    {
        m_settings = proxy.m_settings;
        m_widthConstraint = proxy.m_widthConstraint;
        m_heightConstraint = proxy.m_heightConstraint;
        m_frameRateConstraint = proxy.m_frameRateConstraint;
    }

    Ref<RealtimeMediaSource::TakePhotoNativePromise> takePhoto(PhotoSettings&& photoSettings)
    {
        RealtimeMediaSource::TakePhotoNativePromise::Producer takePhotoProducer;
        Ref<RealtimeMediaSource::TakePhotoNativePromise> takePhotoPromise = takePhotoProducer;

        queueAndProcessSerialAction([this, weakThis = WeakPtr { this }, photoSettings = WTFMove(photoSettings), takePhotoProducer = WTFMove(takePhotoProducer)]() mutable -> Ref<GenericPromise> {

            if (!weakThis) {
                takePhotoProducer.reject("Track has ended"_s);
                return GenericPromise::createAndResolve();
            }

            GenericPromise::Producer serialActionProducer;
            Ref<GenericPromise> serialActionPromise = serialActionProducer;

            m_source->takePhoto(WTFMove(photoSettings))->whenSettled(RunLoop::main(), [takePhotoProducer = WTFMove(takePhotoProducer), serialActionProducer = WTFMove(serialActionProducer)] (auto&& result) mutable {
                ASSERT(isMainRunLoop());

                takePhotoProducer.settle(WTFMove(result));
                serialActionProducer.resolve();
            });

            return serialActionPromise;
        });

        return takePhotoPromise;
    }

    void getPhotoCapabilities(GetPhotoCapabilitiesCallback&& handler)
    {
        m_source->getPhotoCapabilities(WTFMove(handler));
    }

    Ref<RealtimeMediaSource::PhotoSettingsNativePromise> getPhotoSettings()
    {
        return m_source->getPhotoSettings();
    }

private:
    void sourceStopped() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceStopped(m_id, m_source->captureDidFail()), 0);
    }

    void sourceMutedChanged() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceMutedChanged(m_id, m_source->muted(), m_source->interrupted()), 0);
    }

    void sourceSettingsChanged() final {
        m_settings = { };
        m_connection->send(Messages::UserMediaCaptureManager::SourceSettingsChanged(m_id, settings()), 0);
    }

    void sourceConfigurationChanged() final
    {
        auto deviceType = m_source->deviceType();
        if ((deviceType == CaptureDevice::DeviceType::Screen || deviceType == CaptureDevice::DeviceType::Window) && m_videoConstraints && updateVideoConstraints(*m_videoConstraints)) {
            m_source->removeVideoFrameObserver(*this);
            m_source->addVideoFrameObserver(*this, { m_widthConstraint, m_heightConstraint }, m_frameRateConstraint);
        }

        m_connection->send(Messages::UserMediaCaptureManager::SourceConfigurationChanged(m_id, m_source->persistentID(), settings(), m_source->capabilities()), 0);
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
            auto result = ProducerSharedCARingBuffer::allocate(*m_description, m_description->sampleRate() * 2);
            RELEASE_ASSERT(result); // FIXME(https://bugs.webkit.org/show_bug.cgi?id=262690): Handle allocation failure.
            auto [ringBuffer, handle] = WTFMove(*result);
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

    bool preventSourceFromEnding()
    {
        // Do not allow the source to end if we are still using it.
        return !m_isEnded;
    }

    RealtimeMediaSourceIdentifier m_id;
    Ref<IPC::Connection> m_connection;
    ProcessIdentity m_resourceOwner;
    Ref<RealtimeMediaSource> m_source;
    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    std::optional<CAAudioStreamDescription> m_description;
    bool m_isStopped { false };
    bool m_isEnded { false };
    std::unique_ptr<ImageRotationSessionVT> m_rotationSession;
    bool m_shouldApplyRotation { false };
    std::unique_ptr<IPC::Semaphore> m_captureSemaphore;
    int64_t m_writeOffset { 0 };
    int64_t m_remainingFrameCount { 0 };
    size_t m_frameChunkSize { 0 };
    MediaTime m_startTime;
    bool m_shouldReset { false };
    RefPtr<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;

    std::optional<WebCore::RealtimeMediaSourceSettings> m_settings;
    int m_widthConstraint { 0 };
    int m_heightConstraint { 0 };
    double m_frameRateConstraint { 0 };

    std::optional<MediaConstraints> m_videoConstraints;
    Ref<GenericPromise> m_pendingAction { GenericPromise::createAndResolve() };
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
    if (auto microphoneSource = perPageSources.microphoneSource.get()) {
        if (microphoneSource->persistentID() != device.persistentId() && !microphoneSource->isEnded()) {
            RELEASE_LOG_ERROR(WebRTC, "Ending microphone source as new source is using a different device.");
            // FIXME: We should probably fail the capture in a way that shows a specific console log message.
            microphoneSource->endImmediatly();
        }
    }

    auto source = sourceOrError.source();
    perPageSources.microphoneSource = ThreadSafeWeakPtr { source.get() };
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

CaptureSourceOrError UserMediaCaptureManagerProxy::createCameraSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, PageIdentifier pageIdentifier)
{
    auto& perPageSources = m_pageSources.ensure(pageIdentifier, [] { return PageSources { }; }).iterator->value;
    for (auto& cameraSource : perPageSources.cameraSources) {
        // FIXME: Optimize multiple concurrent cameras.
        if (cameraSource.persistentID() == device.persistentId() && !cameraSource.isEnded()) {
            // We can reuse the source, let's do it.
            auto source = cameraSource.clone();
            perPageSources.cameraSources.add(source.get());
            return source;
        }
    }

    auto sourceOrError = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().createVideoCaptureSource(device, WTFMove(hashSalts), nullptr, pageIdentifier);
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
    if (!m_connectionProxy->willStartCapture(device.type())) {
        completionHandler({ "Request is not allowed"_s, WebCore::MediaAccessDenialReason::PermissionDenied }, { }, { });
        return;
    }

    auto* constraints = mediaConstraints.isValid ? &mediaConstraints : nullptr;

    CaptureSourceOrError sourceOrError;
    switch (device.type()) {
    case WebCore::CaptureDevice::DeviceType::Microphone:
        sourceOrError = createMicrophoneSource(device, WTFMove(hashSalts), constraints, pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::Camera:
        sourceOrError = createCameraSource(device, WTFMove(hashSalts), pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::Screen:
    case WebCore::CaptureDevice::DeviceType::Window:
        sourceOrError = RealtimeMediaSourceCenter::singleton().displayCaptureFactory().createDisplayCaptureSource(device, WTFMove(hashSalts), nullptr, pageIdentifier);
        break;
    case WebCore::CaptureDevice::DeviceType::SystemAudio:
    case WebCore::CaptureDevice::DeviceType::Speaker:
    case WebCore::CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    if (!sourceOrError) {
        completionHandler(WTFMove(sourceOrError.error), { }, { });
        return;
    }

    auto source = sourceOrError.source();
#if !RELEASE_LOG_DISABLED
    source->setLogger(m_connectionProxy->logger(), LoggerHelper::uniqueLogIdentifier());
#endif

    ASSERT(!m_proxies.contains(id));
    Ref connection = m_connectionProxy->connection();
    RefPtr remoteVideoFrameObjectHeap = shouldUseGPUProcessRemoteFrames ? m_connectionProxy->remoteVideoFrameObjectHeap() : nullptr;
    auto proxy = makeUnique<SourceProxy>(id, WTFMove(connection), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(source), WTFMove(remoteVideoFrameObjectHeap));
    proxy->observeMedia();

    auto completeSetup = [this](std::unique_ptr<SourceProxy>&& proxy, RealtimeMediaSourceIdentifier id, CreateSourceCallback&& completionHandler) mutable {
        auto settings = proxy->settings();
        auto capabilities = proxy->source().capabilities();
        m_proxies.add(id, WTFMove(proxy));

        completionHandler({ }, WTFMove(settings), WTFMove(capabilities));
    };

    if (!constraints || proxy->source().type() != RealtimeMediaSource::Type::Video) {
        completeSetup(WTFMove(proxy), id, WTFMove(completionHandler));
        return;
    }

    proxy->applyConstraints(WTFMove(const_cast<WebCore::MediaConstraints&>(*constraints)), [proxy = WTFMove(proxy), id, completionHandler = WTFMove(completionHandler), completeSetup = WTFMove(completeSetup)](auto&& result) mutable {
        if (result) {
            completionHandler({ WTFMove(result->badConstraint), WebCore::MediaAccessDenialReason::InvalidConstraint }, { }, { });
            return;
        }

        completeSetup(WTFMove(proxy), id, WTFMove(completionHandler));
    });
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
    m_connectionProxy->startProducingData(proxy->source().deviceType());
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

void UserMediaCaptureManagerProxy::applyConstraints(RealtimeMediaSourceIdentifier id, WebCore::MediaConstraints&& constraints)
{
    auto* proxy = m_proxies.get(id);
    if (!proxy) {
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, { }, "Unknown source"_s), 0);
        return;
    }

    proxy->applyConstraints(WTFMove(constraints), [this, weakThis = WeakPtr { *this }, id, proxy](auto&& result) {

        if (!weakThis)
            return;

        if (result) {
            m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, result->badConstraint, result->message), 0);
            return;
        }

        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsSucceeded(id, proxy->settings()), 0);
    });
}

void UserMediaCaptureManagerProxy::clone(RealtimeMediaSourceIdentifier clonedID, RealtimeMediaSourceIdentifier newSourceID, WebCore::PageIdentifier pageIdentifier)
{
    MESSAGE_CHECK(m_proxies.contains(clonedID));
    MESSAGE_CHECK(!m_proxies.contains(newSourceID));
    if (auto* proxy = m_proxies.get(clonedID)) {
        auto sourceClone = proxy->source().clone();
        if (sourceClone->deviceType() == WebCore::CaptureDevice::DeviceType::Camera)
            m_pageSources.ensure(pageIdentifier, [] { return PageSources { }; }).iterator->value.cameraSources.add(sourceClone.get());

        Ref connection = m_connectionProxy->connection();
        RefPtr remoteVideoFrameObjectHeap = m_connectionProxy->remoteVideoFrameObjectHeap();
        auto cloneProxy = makeUnique<SourceProxy>(newSourceID, WTFMove(connection), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(sourceClone), WTFMove(remoteVideoFrameObjectHeap));
        cloneProxy->copySettings(*proxy);
        cloneProxy->observeMedia();
        m_proxies.add(newSourceID, WTFMove(cloneProxy));
    }
}

void UserMediaCaptureManagerProxy::takePhoto(RealtimeMediaSourceIdentifier sourceID, WebCore::PhotoSettings&& settings, TakePhotoCallback&& handler)
{
    auto* proxy = m_proxies.get(sourceID);
    if (!proxy) {
        handler(Unexpected<String>("Device not available"_s));
        return;
    }

    proxy->takePhoto(WTFMove(settings))->whenSettled(RunLoop::main(), [handler = WTFMove(handler)] (auto&& result) mutable {
        handler(WTFMove(result));
    });
}



void UserMediaCaptureManagerProxy::getPhotoCapabilities(RealtimeMediaSourceIdentifier sourceID, GetPhotoCapabilitiesCallback&& handler)
{
    if (auto* proxy = m_proxies.get(sourceID)) {
        proxy->getPhotoCapabilities(WTFMove(handler));
        return;
    }

    handler(PhotoCapabilitiesOrError("Device not available"_s));
}

void UserMediaCaptureManagerProxy::getPhotoSettings(RealtimeMediaSourceIdentifier sourceID, GetPhotoSettingsCallback&& handler)
{
    auto* proxy = m_proxies.get(sourceID);
    if (!proxy) {
        handler(Unexpected<String>("Device not available"_s));
        return;
    }

    proxy->getPhotoSettings()->whenSettled(RunLoop::main(), [handler = WTFMove(handler)] (auto&& result) mutable {
        handler(WTFMove(result));
    });
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

void UserMediaCaptureManagerProxy::close()
{
    auto proxies = std::exchange(m_proxies, { });
    for (auto& proxy : proxies.values())
        proxy->stop();
}

void UserMediaCaptureManagerProxy::setOrientation(WebCore::IntDegrees orientation)
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
