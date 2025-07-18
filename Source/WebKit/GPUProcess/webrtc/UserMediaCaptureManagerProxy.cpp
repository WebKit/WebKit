/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#include <WebCore/AudioSession.h>
#include <WebCore/AudioUtilities.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/ImageRotationSessionVT.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/VideoFrameCV.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/NativePromise.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/cocoa/Entitlements.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_connectionProxy->connection())
#define MESSAGE_CHECK_COMPLETION(assertion, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, m_connectionProxy->connection(), completion)

namespace WebKit {
using namespace WebCore;

class UserMediaCaptureManagerProxySourceProxy final
    : public RefCounted<UserMediaCaptureManagerProxySourceProxy>
    , public RealtimeMediaSourceObserver
    , private RealtimeMediaSource::AudioSampleObserver
    , private RealtimeMediaSource::VideoFrameObserver
    , public CanMakeCheckedPtr<UserMediaCaptureManagerProxySourceProxy> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(UserMediaCaptureManagerProxySourceProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(UserMediaCaptureManagerProxySourceProxy);
public:
    static Ref<UserMediaCaptureManagerProxySourceProxy> create(RealtimeMediaSourceIdentifier id, Ref<IPC::Connection>&& connection, ProcessIdentity&& resourceOwner, Ref<RealtimeMediaSource>&& source, RefPtr<RemoteVideoFrameObjectHeap>&& videoFrameObjectHeap) { return adoptRef(*new UserMediaCaptureManagerProxySourceProxy(id, WTFMove(connection), WTFMove(resourceOwner), WTFMove(source), WTFMove(videoFrameObjectHeap))); }
    ~UserMediaCaptureManagerProxySourceProxy()
    {
        unobserveMedia();
        m_source->removeObserver(*this);
    }

    void unobserveMedia()
    {
        if (!m_isObservingMedia)
            return;
        m_isObservingMedia = false;

        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->removeAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            m_source->removeVideoFrameObserver(*this);
            break;
        }
    }

    void observeMedia()
    {
        if (m_isObservingMedia)
            return;
        m_isObservingMedia = true;

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

    bool isObservingMedia() const { return m_isObservingMedia; }

    void whenReady(UserMediaCaptureManagerProxy::CreateSourceCallback&& createCallback)
    {
        m_source->whenReady([weakThis = WeakPtr { *this }, createCallback = WTFMove(createCallback)] (auto&& sourceError) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                createCallback(CaptureSourceError { "Source is no longer needed"_s, MediaAccessDenialReason::InvalidAccess }, { }, { });
                return;
            }
            createCallback(sourceError, protectedThis->settings(), protectedThis->source().capabilities());
        });
    }

    bool isUsingSource(const RealtimeMediaSource& source) const { return m_source.ptr() == &source; }
    RealtimeMediaSource& source() { return m_source; }

    void audioUnitWillStart() final
    {
        Ref session = AudioSession::singleton();
        session->setCategory(AudioSession::CategoryType::PlayAndRecord, AudioSession::Mode::VideoChat, RouteSharingPolicy::Default);
        session->tryToSetActive(true);
    }

    void start()
    {
        m_shouldReset = true;
        m_isStopped = false;
        m_source->start();

        if (m_source->type() == RealtimeMediaSource::Type::Audio) {
            if (auto* description = m_source->audioStreamDescription())
                prepareAudioDescription(*description);
        }

        observeMedia();
    }

    void stop()
    {
        m_isStopped = true;
        m_source->stop();

        unobserveMedia();
    }

    void end()
    {
        m_isStopped = true;
        m_isEnded = true;
        m_source->requestToEnd(*this);
    }

    void setShouldApplyRotation() { m_source->setShouldApplyRotation(); }
    void setIsInBackground(bool value) { m_source->setIsInBackground(value); }

    bool isPowerEfficient()
    {
        return m_source->isPowerEfficient();
    }

    bool updateVideoConstraints(const WebCore::MediaConstraints& constraints)
    {
        m_videoConstraints = constraints;

        auto resultingConstraints = m_source->extractVideoPresetConstraints(constraints);

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

        Ref pendingAction = WTFMove(m_pendingAction);
        m_pendingAction = pendingAction->isResolved() ? action() : pendingAction->whenSettled(RunLoop::mainSingleton(), WTFMove(action));
    }

    void applyConstraints(WebCore::MediaConstraints&& constraints, CompletionHandler<void(std::optional<RealtimeMediaSource::ApplyConstraintsError>&&)> callback)
    {
        queueAndProcessSerialAction([weakThis = WeakPtr { *this }, constraints = WTFMove(constraints), callback = WTFMove(callback)]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                callback(RealtimeMediaSource::ApplyConstraintsError { { }, { } });
                return GenericPromise::createAndResolve();
            }

            Ref source = protectedThis->m_source;
            if (source->type() != RealtimeMediaSource::Type::Video) {
                source->applyConstraints(constraints, WTFMove(callback));
                return GenericPromise::createAndResolve();
            }

            bool isObservingMedia = protectedThis->isObservingMedia();
            protectedThis->unobserveMedia();

            source->applyConstraints(WTFMove(constraints), [weakThis = WTFMove(weakThis), &constraints, isObservingMedia, callback = WTFMove(callback)](auto&& error) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis) {
                    callback(RealtimeMediaSource::ApplyConstraintsError { { }, { } });
                    return;
                }

                if (!error) {
                    protectedThis->updateVideoConstraints(constraints);
                    protectedThis->m_settings = { };
                }

                if (isObservingMedia)
                    protectedThis->observeMedia();

                callback(WTFMove(error));
            });

            return GenericPromise::createAndResolve();
        });
    }

    const WebCore::RealtimeMediaSourceSettings& settings()
    {
        Ref source = m_source;
        if (source->type() != RealtimeMediaSource::Type::Video)
            return source->settings();

        if (!m_settings) {
            m_settings = source->settings();
            if (m_widthConstraint || m_heightConstraint) {
                auto desiredSize = source->computeResizedVideoFrameSize({ m_widthConstraint, m_heightConstraint }, source->intrinsicSize());

                auto videoFrameRotation = source->videoFrameRotation();
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

    void copySettings(UserMediaCaptureManagerProxySourceProxy& proxy)
    {
        m_settings = proxy.m_settings;
        m_widthConstraint = proxy.m_widthConstraint;
        m_heightConstraint = proxy.m_heightConstraint;
        m_frameRateConstraint = proxy.m_frameRateConstraint;
        m_videoConstraints = proxy.m_videoConstraints;
    }

    Ref<RealtimeMediaSource::TakePhotoNativePromise> takePhoto(PhotoSettings&& photoSettings)
    {
        RealtimeMediaSource::TakePhotoNativePromise::Producer takePhotoProducer;
        Ref<RealtimeMediaSource::TakePhotoNativePromise> takePhotoPromise = takePhotoProducer;

        queueAndProcessSerialAction([weakThis = WeakPtr { *this }, photoSettings = WTFMove(photoSettings), takePhotoProducer = WTFMove(takePhotoProducer)]() mutable -> Ref<GenericPromise> {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                takePhotoProducer.reject("Track has ended"_s);
                return GenericPromise::createAndResolve();
            }

            return protectedThis->source().takePhoto(WTFMove(photoSettings))->whenSettled(RunLoop::mainSingleton(), [takePhotoProducer = WTFMove(takePhotoProducer)] (auto&& result) mutable {
                ASSERT(isMainRunLoop());

                takePhotoProducer.settle(WTFMove(result));
                return GenericPromise::createAndResolve();
            });
        });

        return takePhotoPromise;
    }

    Ref<RealtimeMediaSource::PhotoCapabilitiesNativePromise> getPhotoCapabilities()
    {
        return m_source->getPhotoCapabilities();
    }

    Ref<RealtimeMediaSource::PhotoSettingsNativePromise> getPhotoSettings()
    {
        return m_source->getPhotoSettings();
    }

private:
    UserMediaCaptureManagerProxySourceProxy(RealtimeMediaSourceIdentifier id, Ref<IPC::Connection>&& connection, ProcessIdentity&& resourceOwner, Ref<RealtimeMediaSource>&& source, RefPtr<RemoteVideoFrameObjectHeap>&& videoFrameObjectHeap)
        : m_id(id)
        , m_connection(WTFMove(connection))
        , m_resourceOwner(WTFMove(resourceOwner))
        , m_source(WTFMove(source))
        , m_videoFrameObjectHeap(WTFMove(videoFrameObjectHeap))
    {
        m_source->addObserver(*this);
        m_source->setCanUseIOSurface();
    }

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    void sourceStopped() final
    {
        m_connection->send(Messages::UserMediaCaptureManager::SourceStopped(m_id, m_source->captureDidFail()), 0);
    }

    void sourceMutedChanged() final
    {
        m_connection->send(Messages::UserMediaCaptureManager::SourceMutedChanged(m_id, m_source->muted(), m_source->interrupted()), 0);
    }

    void sourceSettingsChanged() final
    {
        m_settings = { };
        m_connection->send(Messages::UserMediaCaptureManager::SourceSettingsChanged(m_id, settings()), 0);
    }

    void sourceConfigurationChanged() final
    {
        Ref source = m_source;
        auto deviceType = source->deviceType();

        if ((deviceType == CaptureDevice::DeviceType::Screen || deviceType == CaptureDevice::DeviceType::Window) && m_videoConstraints && updateVideoConstraints(*m_videoConstraints)) {
            source->removeVideoFrameObserver(*this);
            source->addVideoFrameObserver(*this, { m_widthConstraint, m_heightConstraint }, m_frameRateConstraint);
        }

        m_settings = { };
        m_connection->send(Messages::UserMediaCaptureManager::SourceConfigurationChanged(m_id, source->persistentID(), settings(), source->capabilities()), 0);
    }

    void prepareAudioDescription(const AudioStreamDescription& description)
    {
        m_captureSemaphore = makeUnique<IPC::Semaphore>();
        ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
        m_description = *std::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

        // Allocate a ring buffer large enough to contain 2 seconds of audio.
        auto result = ProducerSharedCARingBuffer::allocate(*m_description, m_description->sampleRate() * 2);
        RELEASE_ASSERT(result); // FIXME(https://bugs.webkit.org/show_bug.cgi?id=262690): Handle allocation failure.
        auto [ringBuffer, audioHandle] = WTFMove(*result);
        m_ringBuffer = WTFMove(ringBuffer);
        m_audioHandle = WTFMove(audioHandle);
    }

    // May get called on a background thread.
    void audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames) final
    {
        bool descriptionChanged = m_description != description;
        if (descriptionChanged || m_shouldReset) {
            m_shouldReset = false;
            m_writeOffset = 0;
            m_remainingFrameCount = 0;
            m_startTime = time;
            m_frameChunkSize = std::max(WebCore::AudioUtilities::renderQuantumSize, AudioSession::singleton().preferredBufferSize());

            ASSERT(descriptionChanged || m_audioHandle);

            DisableMallocRestrictionsForCurrentThreadScope scope;
            if (descriptionChanged)
                prepareAudioDescription(description);

            m_connection->send(Messages::RemoteCaptureSampleManager::AudioStorageChanged(m_id, WTFMove(*m_audioHandle), *m_description, *m_captureSemaphore, m_startTime, m_frameChunkSize), 0);
            m_audioHandle = { };
        }

        m_ringBuffer->store(downcast<WebAudioBufferList>(audioData).list(), numberOfFrames, m_writeOffset);
        m_writeOffset += numberOfFrames;

        size_t framesToSend = numberOfFrames + m_remainingFrameCount;
        size_t signalCount = framesToSend / m_frameChunkSize;
        m_remainingFrameCount = framesToSend - (signalCount * m_frameChunkSize);
        for (unsigned i = 0; i < signalCount; ++i)
            m_captureSemaphore->signal();
    }

    void videoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata metadata) final
    {
        if (m_resourceOwner)
            frame.setOwnershipIdentity(m_resourceOwner);

        RefPtr videoFrameObjectHeap = m_videoFrameObjectHeap;
        if (!videoFrameObjectHeap) {
            m_connection->send(Messages::RemoteCaptureSampleManager::VideoFrameAvailableCV(m_id, frame.pixelBuffer(), frame.rotation(), frame.isMirrored(), frame.presentationTime(), metadata), 0);
            return;
        }

        auto properties = videoFrameObjectHeap->add(frame);
        m_connection->send(Messages::RemoteCaptureSampleManager::VideoFrameAvailable(m_id, properties, metadata), 0);
    }

    bool preventSourceFromEnding()
    {
        // Do not allow the source to end if we are still using it.
        return !m_isEnded;
    }

    Ref<IPC::Connection> protectedConnection() const { return m_connection; }

    bool m_isObservingMedia { false };
    bool m_isStopped { false };
    bool m_isEnded { false };
    bool m_shouldApplyRotation { false };
    std::atomic<bool> m_shouldReset { false };

    RealtimeMediaSourceIdentifier m_id;
    const Ref<IPC::Connection> m_connection;
    ProcessIdentity m_resourceOwner;
    const Ref<RealtimeMediaSource> m_source;
    std::unique_ptr<ProducerSharedCARingBuffer> m_ringBuffer;
    std::optional<CAAudioStreamDescription> m_description;
    std::unique_ptr<ImageRotationSessionVT> m_rotationSession;
    std::unique_ptr<IPC::Semaphore> m_captureSemaphore;
    std::optional<ConsumerSharedCARingBufferHandle> m_audioHandle;
    int64_t m_writeOffset { 0 };
    int64_t m_remainingFrameCount { 0 };
    size_t m_frameChunkSize { 0 };
    MediaTime m_startTime;
    RefPtr<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;

    std::optional<WebCore::RealtimeMediaSourceSettings> m_settings;
    int m_widthConstraint { 0 };
    int m_heightConstraint { 0 };
    double m_frameRateConstraint { 0 };

    std::optional<MediaConstraints> m_videoConstraints;
    Ref<GenericPromise> m_pendingAction { GenericPromise::createAndResolve() };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(UserMediaCaptureManagerProxy);

Ref<UserMediaCaptureManagerProxy> UserMediaCaptureManagerProxy::create(UniqueRef<ConnectionProxy>&& connectionProxy)
{
    return adoptRef(*new UserMediaCaptureManagerProxy(WTFMove(connectionProxy)));
}

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

    auto& perPageSources = m_pageSources.ensure(pageIdentifier, [] {
        return PageSources { };
    }).iterator->value;

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
    auto& perPageSources = m_pageSources.ensure(pageIdentifier, [] {
        return PageSources { };
    }).iterator->value;
    for (Ref cameraSource : perPageSources.cameraSources) {
        // FIXME: Optimize multiple concurrent cameras.
        if (cameraSource->persistentID() == device.persistentId() && !cameraSource->isEnded()) {
            // We can reuse the source, let's do it.
            auto source = cameraSource->clone();
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
    m_connectionProxy->startMonitoringCaptureDeviceRotation(pageIdentifier, device.persistentId());
    perPageSources.cameraSources.add(source.get());
    return source;
}

void UserMediaCaptureManagerProxy::createMediaSourceForCaptureDeviceWithConstraints(RealtimeMediaSourceIdentifier id, const CaptureDevice& device, WebCore::MediaDeviceHashSalts&& hashSalts, MediaConstraints&& mediaConstraints, bool shouldUseGPUProcessRemoteFrames, PageIdentifier pageIdentifier, CreateSourceCallback&& completionHandler)
{
    if (!m_connectionProxy->willStartCapture(device.type(), pageIdentifier)) {
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
    source->setLogger(m_connectionProxy->protectedLogger(), LoggerHelper::uniqueLogIdentifier());
#endif

    ASSERT(!m_proxies.contains(id));
    Ref connection = m_connectionProxy->connection();
    RefPtr remoteVideoFrameObjectHeap = shouldUseGPUProcessRemoteFrames ? m_connectionProxy->remoteVideoFrameObjectHeap() : nullptr;
    auto proxy = UserMediaCaptureManagerProxySourceProxy::create(id, WTFMove(connection), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(source), WTFMove(remoteVideoFrameObjectHeap));

    auto completeSetup = [](UserMediaCaptureManagerProxySourceProxy& proxy, CreateSourceCallback&& completionHandler) mutable {
        proxy.whenReady(WTFMove(completionHandler));
    };

    if (!constraints || proxy->source().type() != RealtimeMediaSource::Type::Video) {
        completeSetup(proxy, WTFMove(completionHandler));
        m_proxies.add(id, WTFMove(proxy));
        return;
    }

    MESSAGE_CHECK_COMPLETION(constraints->mandatoryConstraints.isValid(), completionHandler({ "Invalid mandatoryConstraints"_s, WebCore::MediaAccessDenialReason::InvalidConstraint }, { }, { }));
    for (const auto& advancedConstraint : constraints->advancedConstraints)
        MESSAGE_CHECK_COMPLETION(advancedConstraint.isValid(), completionHandler({ "Invalid advancedConstraints"_s, WebCore::MediaAccessDenialReason::InvalidConstraint }, { }, { }));

    proxy->applyConstraints(WTFMove(mediaConstraints), [weakProxy = WeakPtr { proxy }, completionHandler = WTFMove(completionHandler), completeSetup = WTFMove(completeSetup)](auto&& error) mutable {
        RefPtr protectedProxy = weakProxy.get();
        if (error || !protectedProxy) {
            completionHandler(CaptureSourceError { error->invalidConstraint }, { }, { });
            return;
        }

        completeSetup(*protectedProxy, WTFMove(completionHandler));
    });

    m_proxies.add(id, WTFMove(proxy));
}

void UserMediaCaptureManagerProxy::startProducingData(RealtimeMediaSourceIdentifier id, WebCore::PageIdentifier pageIdentifier)
{
    RefPtr proxy = m_proxies.get(id);
    if (!proxy)
        return;

    if (!m_connectionProxy->setCaptureAttributionString()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to set capture attribution, failing capture.");
        return;
    }

    Ref source = proxy->source();

#if ENABLE(APP_PRIVACY_REPORT)
    m_connectionProxy->setTCCIdentity();
#endif
#if ENABLE(EXTENSION_CAPABILITIES) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    bool hasValidMediaEnvironmentOrIdentity = m_connectionProxy->setCurrentMediaEnvironment(pageIdentifier) || RealtimeMediaSourceCenter::singleton().hasIdentity();
    if (!hasValidMediaEnvironmentOrIdentity && source->deviceType() == CaptureDevice::DeviceType::Camera
        && WTF::processHasEntitlement("com.apple.developer.web-browser-engine.rendering"_s)) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to set media environment, failing capture.");
        source->captureFailed();
        return;
    }
#endif // ENABLE(EXTENSION_CAPABILITIES) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    m_connectionProxy->startProducingData(source->deviceType());
    proxy->start();
}

void UserMediaCaptureManagerProxy::stopProducingData(RealtimeMediaSourceIdentifier id)
{
    if (RefPtr proxy = m_proxies.get(id))
        proxy->stop();
}

void UserMediaCaptureManagerProxy::removeSource(RealtimeMediaSourceIdentifier id)
{
    auto iterator = m_proxies.find(id);
    if (iterator == m_proxies.end())
        return;

    Ref source = iterator->value->source();
    m_proxies.remove(iterator);

    for (Ref proxy : m_proxies.values()) {
        if (proxy->isUsingSource(source))
            return;
    }

    if (auto pageIdentifier = source->pageIdentifier()) {
        auto iterator = m_pageSources.find(*pageIdentifier);
        if (iterator != m_pageSources.end()) {
            auto& pageSources = iterator->value;
            if (source->deviceType() == WebCore::CaptureDevice::DeviceType::Camera) {
                if (pageSources.cameraSources.remove(source.get())) {
                    bool shouldContinueMonitoring = false;
                    pageSources.cameraSources.forEach([&] (auto& cameraSource) {
                        shouldContinueMonitoring |= cameraSource.persistentID() == source->persistentID() && !cameraSource.isEnded();
                    });
                    if (!shouldContinueMonitoring)
                        m_connectionProxy->stopMonitoringCaptureDeviceRotation(*pageIdentifier, source->persistentID());
                }
            } else if (source->deviceType() == WebCore::CaptureDevice::DeviceType::Microphone) {
                if (source.ptr() == pageSources.microphoneSource.get().get())
                    iterator->value.microphoneSource = nullptr;
            }
        }
    }
}

void UserMediaCaptureManagerProxy::capabilities(RealtimeMediaSourceIdentifier id, CompletionHandler<void(RealtimeMediaSourceCapabilities&&)>&& completionHandler)
{
    RealtimeMediaSourceCapabilities capabilities;
    if (RefPtr proxy = m_proxies.get(id))
        capabilities = proxy->source().capabilities();
    completionHandler(WTFMove(capabilities));
}

void UserMediaCaptureManagerProxy::applyConstraints(RealtimeMediaSourceIdentifier id, WebCore::MediaConstraints&& constraints)
{
    RefPtr proxy = m_proxies.get(id);
    if (!proxy) {
        m_connectionProxy->protectedConnection()->send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, { }, "Unknown source"_s), 0);
        return;
    }

    MESSAGE_CHECK(constraints.mandatoryConstraints.isValid());
    for (const auto& advancedConstraint : constraints.advancedConstraints)
        MESSAGE_CHECK(advancedConstraint.isValid());

    proxy->applyConstraints(WTFMove(constraints), [id, proxy, connection = m_connectionProxy->protectedConnection()](auto&& result) {
        if (result) {
            connection->send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, result->invalidConstraint, result->message), 0);
            return;
        }

        connection->send(Messages::UserMediaCaptureManager::ApplyConstraintsSucceeded(id, proxy->settings()), 0);
    });
}

void UserMediaCaptureManagerProxy::clone(RealtimeMediaSourceIdentifier clonedID, RealtimeMediaSourceIdentifier newSourceID, WebCore::PageIdentifier pageIdentifier)
{
    MESSAGE_CHECK(m_proxies.contains(clonedID));
    MESSAGE_CHECK(!m_proxies.contains(newSourceID));
    if (RefPtr proxy = m_proxies.get(clonedID)) {
        auto sourceClone = proxy->source().clone();
        if (sourceClone->deviceType() == WebCore::CaptureDevice::DeviceType::Camera) {
            m_pageSources.ensure(pageIdentifier, [] {
                return PageSources { };
            }).iterator->value.cameraSources.add(sourceClone.get());
        }

        Ref connection = m_connectionProxy->connection();
        RefPtr remoteVideoFrameObjectHeap = m_connectionProxy->remoteVideoFrameObjectHeap();
        auto cloneProxy = UserMediaCaptureManagerProxySourceProxy::create(newSourceID, WTFMove(connection), ProcessIdentity { m_connectionProxy->resourceOwner() }, WTFMove(sourceClone), WTFMove(remoteVideoFrameObjectHeap));
        cloneProxy->copySettings(*proxy);
        if (proxy->isObservingMedia())
            cloneProxy->observeMedia();
        m_proxies.add(newSourceID, WTFMove(cloneProxy));
    }
}

void UserMediaCaptureManagerProxy::takePhoto(RealtimeMediaSourceIdentifier sourceID, WebCore::PhotoSettings&& settings, TakePhotoCallback&& handler)
{
    RefPtr proxy = m_proxies.get(sourceID);
    if (!proxy) {
        handler(Unexpected<String>("Device not available"_s));
        return;
    }

    proxy->takePhoto(WTFMove(settings))->whenSettled(RunLoop::mainSingleton(), WTFMove(handler));
}



void UserMediaCaptureManagerProxy::getPhotoCapabilities(RealtimeMediaSourceIdentifier sourceID, GetPhotoCapabilitiesCallback&& handler)
{
    RefPtr proxy = m_proxies.get(sourceID);
    if (!proxy) {
        handler(Unexpected<String>("Device not available"_s));
        return;
    }

    proxy->getPhotoCapabilities()->whenSettled(RunLoop::mainSingleton(), WTFMove(handler));
}

void UserMediaCaptureManagerProxy::getPhotoSettings(RealtimeMediaSourceIdentifier sourceID, GetPhotoSettingsCallback&& handler)
{
    RefPtr proxy = m_proxies.get(sourceID);
    if (!proxy) {
        handler(Unexpected<String>("Device not available"_s));
        return;
    }

    proxy->getPhotoSettings()->whenSettled(RunLoop::mainSingleton(), [handler = WTFMove(handler)] (auto&& result) mutable {
        handler(WTFMove(result));
    });
}

void UserMediaCaptureManagerProxy::endProducingData(RealtimeMediaSourceIdentifier sourceID)
{
    if (RefPtr proxy = m_proxies.get(sourceID))
        proxy->end();
}

void UserMediaCaptureManagerProxy::setShouldApplyRotation(RealtimeMediaSourceIdentifier sourceID)
{
    if (RefPtr proxy = m_proxies.get(sourceID))
        proxy->setShouldApplyRotation();
}

void UserMediaCaptureManagerProxy::setIsInBackground(RealtimeMediaSourceIdentifier sourceID, bool isInBackground)
{
    if (RefPtr proxy = m_proxies.get(sourceID))
        proxy->setIsInBackground(isInBackground);
}

void UserMediaCaptureManagerProxy::isPowerEfficient(WebCore::RealtimeMediaSourceIdentifier sourceID, CompletionHandler<void(bool)>&& callback)
{
    RefPtr proxy = m_proxies.get(sourceID);
    callback(proxy ? proxy->isPowerEfficient() : false);
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

void UserMediaCaptureManagerProxy::rotationAngleForCaptureDeviceChanged(const String& persistentId, WebCore::VideoFrameRotation rotation)
{
    m_orientationNotifier.rotationAngleForCaptureDeviceChanged(persistentId, rotation);
}

bool UserMediaCaptureManagerProxy::hasSourceProxies() const
{
    return !m_proxies.isEmpty();
}

std::optional<SharedPreferencesForWebProcess> UserMediaCaptureManagerProxy::sharedPreferencesForWebProcess() const
{
    return m_connectionProxy->sharedPreferencesForWebProcess();
}

}

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK

#endif
