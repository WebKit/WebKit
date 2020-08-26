/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "SharedRingBufferStorage.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessProxy.h"
#include <WebCore/AudioSession.h>
#include <WebCore/CARingBuffer.h>
#include <WebCore/ImageRotationSessionVT.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, &m_connectionProxy->connection())

namespace WebKit {
using namespace WebCore;

class UserMediaCaptureManagerProxy::SourceProxy
    : private RealtimeMediaSource::Observer
    , private RealtimeMediaSource::AudioSampleObserver
    , private RealtimeMediaSource::VideoSampleObserver
    , public SharedRingBufferStorage::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SourceProxy(RealtimeMediaSourceIdentifier id, Ref<IPC::Connection>&& connection, Ref<RealtimeMediaSource>&& source)
        : m_id(id)
        , m_connection(WTFMove(connection))
        , m_source(WTFMove(source))
        , m_ringBuffer(makeUniqueRef<SharedRingBufferStorage>(makeUniqueRef<SharedRingBufferStorage>(this)))
    {
        m_source->addObserver(*this);
        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->addAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            m_source->addVideoSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::None:
            ASSERT_NOT_REACHED();
        }
    }

    ~SourceProxy()
    {
        storage().invalidate();

        switch (m_source->type()) {
        case RealtimeMediaSource::Type::Audio:
            m_source->removeAudioSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::Video:
            m_source->removeVideoSampleObserver(*this);
            break;
        case RealtimeMediaSource::Type::None:
            ASSERT_NOT_REACHED();
        }
        m_source->removeObserver(*this);
    }

    RealtimeMediaSource& source() { return m_source; }
    SharedRingBufferStorage& storage() { return static_cast<SharedRingBufferStorage&>(m_ringBuffer.storage()); }
    CAAudioStreamDescription& description() { return m_description; }
    int64_t numberOfFrames() { return m_numberOfFrames; }

    void audioUnitWillStart() final
    {
        // FIXME: WebProcess might want to set the category/bufferSize itself, in which case we should remove that code.
        auto bufferSize = AudioSession::sharedSession().sampleRate() / 50;
        if (AudioSession::sharedSession().preferredBufferSize() > bufferSize)
            AudioSession::sharedSession().setPreferredBufferSize(bufferSize);
        AudioSession::sharedSession().setCategory(AudioSession::PlayAndRecord, RouteSharingPolicy::Default);
    }

    void start()
    {
        m_isEnded = false;
        m_source->start();
    }

    void stop()
    {
        m_isEnded = true;
        m_source->stop();
    }

    void requestToEnd()
    {
        m_isEnded = true;
        m_source->requestToEnd(*this);
    }

    void setShouldApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = true; }

private:
    void sourceStopped() final {
        if (m_source->captureDidFail()) {
            m_connection->send(Messages::UserMediaCaptureManager::CaptureFailed(m_id), 0);
            return;
        }
        m_connection->send(Messages::UserMediaCaptureManager::SourceStopped(m_id), 0);
    }

    void sourceMutedChanged() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceMutedChanged(m_id, m_source->muted()), 0);
    }

    void sourceSettingsChanged() final {
        m_connection->send(Messages::UserMediaCaptureManager::SourceSettingsChanged(m_id, m_source->settings()), 0);
    }

    // May get called on a background thread.
    void audioSamplesAvailable(const MediaTime& time, const PlatformAudioData& audioData, const AudioStreamDescription& description, size_t numberOfFrames) final {
        if (m_description != description) {
            ASSERT(description.platformDescription().type == PlatformDescription::CAAudioStreamBasicType);
            m_description = *WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);

            // Allocate a ring buffer large enough to contain 2 seconds of audio.
            m_numberOfFrames = m_description.sampleRate() * 2;
            m_ringBuffer.allocate(m_description.streamDescription(), m_numberOfFrames);
        }

        ASSERT(is<WebAudioBufferList>(audioData));
        m_ringBuffer.store(downcast<WebAudioBufferList>(audioData).list(), numberOfFrames, time.timeValue());
        uint64_t startFrame;
        uint64_t endFrame;
        m_ringBuffer.getCurrentFrameBounds(startFrame, endFrame);
        m_connection->send(Messages::RemoteCaptureSampleManager::AudioSamplesAvailable(m_id, time, numberOfFrames, startFrame, endFrame), 0);
    }

    void videoSampleAvailable(MediaSample& sample) final
    {
        std::unique_ptr<RemoteVideoSample> remoteSample;
        if (m_shouldApplyRotation && sample.videoRotation() != MediaSample::VideoRotation::None) {
            auto pixelBuffer = rotatePixelBuffer(sample);
            remoteSample = RemoteVideoSample::create(pixelBuffer.get(), sample.presentationTime());
        } else
            remoteSample = RemoteVideoSample::create(sample);
        if (remoteSample)
            m_connection->send(Messages::UserMediaCaptureManager::RemoteVideoSampleAvailable(m_id, WTFMove(*remoteSample)), 0);
    }

    RetainPtr<CVPixelBufferRef> rotatePixelBuffer(MediaSample& sample)
    {
        if (!m_rotationSession)
            m_rotationSession = makeUnique<ImageRotationSessionVT>();

        ImageRotationSessionVT::RotationProperties rotation;
        switch (sample.videoRotation()) {
        case MediaSample::VideoRotation::None:
            ASSERT_NOT_REACHED();
            rotation.angle = 0;
            break;
        case MediaSample::VideoRotation::UpsideDown:
            rotation.angle = 180;
            break;
        case MediaSample::VideoRotation::Right:
            rotation.angle = 90;
            break;
        case MediaSample::VideoRotation::Left:
            rotation.angle = 270;
            break;
        }
        return m_rotationSession->rotate(sample, rotation, ImageRotationSessionVT::IsCGImageCompatible::No);
    }

    void storageChanged(SharedMemory* storage) final {
        SharedMemory::Handle handle;
        if (storage)
            storage->createHandle(handle, SharedMemory::Protection::ReadOnly);

        // FIXME: Send the actual data size with IPCHandle.
#if OS(DARWIN) || OS(WINDOWS)
        uint64_t dataSize = handle.size();
#else
        uint64_t dataSize = 0;
#endif
        m_connection->send(Messages::RemoteCaptureSampleManager::AudioStorageChanged(m_id, SharedMemory::IPCHandle { WTFMove(handle),  dataSize }, m_description, m_numberOfFrames), 0);
    }

    bool preventSourceFromStopping()
    {
        // Do not allow the source to stop if we are still using it.
        return !m_isEnded;
    }

    RealtimeMediaSourceIdentifier m_id;
    WeakPtr<PlatformMediaSessionManager> m_sessionManager;
    Ref<IPC::Connection> m_connection;
    Ref<RealtimeMediaSource> m_source;
    CARingBuffer m_ringBuffer;
    CAAudioStreamDescription m_description { };
    int64_t m_numberOfFrames { 0 };
    bool m_isEnded { false };
    std::unique_ptr<ImageRotationSessionVT> m_rotationSession;
    bool m_shouldApplyRotation { false };
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

void UserMediaCaptureManagerProxy::createMediaSourceForCaptureDeviceWithConstraints(RealtimeMediaSourceIdentifier id, const CaptureDevice& device, String&& hashSalt, const MediaConstraints& constraints, CompletionHandler<void(bool succeeded, String invalidConstraints, WebCore::RealtimeMediaSourceSettings&&, WebCore::RealtimeMediaSourceCapabilities&&)>&& completionHandler)
{
    if (!m_connectionProxy->willStartCapture(device.type()))
        return completionHandler(false, "Request is not allowed"_s, RealtimeMediaSourceSettings { }, { });

    CaptureSourceOrError sourceOrError;
    switch (device.type()) {
    case WebCore::CaptureDevice::DeviceType::Microphone:
        sourceOrError = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().createAudioCaptureSource(device, WTFMove(hashSalt), &constraints);
        break;
    case WebCore::CaptureDevice::DeviceType::Camera:
        sourceOrError = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().createVideoCaptureSource(device, WTFMove(hashSalt), &constraints);
        if (sourceOrError)
            sourceOrError.captureSource->monitorOrientation(m_orientationNotifier);
        break;
    case WebCore::CaptureDevice::DeviceType::Screen:
    case WebCore::CaptureDevice::DeviceType::Window:
        sourceOrError = RealtimeMediaSourceCenter::singleton().displayCaptureFactory().createDisplayCaptureSource(device, &constraints);
        break;
    case WebCore::CaptureDevice::DeviceType::Speaker:
    case WebCore::CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    bool succeeded = !!sourceOrError;
    String invalidConstraints;
    WebCore::RealtimeMediaSourceSettings settings;
    WebCore::RealtimeMediaSourceCapabilities capabilities;
    if (sourceOrError) {
        auto source = sourceOrError.source();
#if !RELEASE_LOG_DISABLED
        source->setLogger(m_connectionProxy->logger(), LoggerHelper::uniqueLogIdentifier());
#endif
        settings = source->settings();
        capabilities = source->capabilities();
        ASSERT(!m_proxies.contains(id));
        m_proxies.add(id, makeUnique<SourceProxy>(id, m_connectionProxy->connection(), WTFMove(source)));
    } else
        invalidConstraints = WTFMove(sourceOrError.errorMessage);
    completionHandler(succeeded, invalidConstraints, WTFMove(settings), WTFMove(capabilities));
}

void UserMediaCaptureManagerProxy::startProducingData(RealtimeMediaSourceIdentifier id)
{
    if (auto* proxy = m_proxies.get(id))
        proxy->start();
}

void UserMediaCaptureManagerProxy::stopProducingData(RealtimeMediaSourceIdentifier id)
{
    if (auto* proxy = m_proxies.get(id))
        proxy->stop();
}

void UserMediaCaptureManagerProxy::end(RealtimeMediaSourceIdentifier id)
{
    m_proxies.remove(id);
}

void UserMediaCaptureManagerProxy::capabilities(RealtimeMediaSourceIdentifier id, CompletionHandler<void(WebCore::RealtimeMediaSourceCapabilities&&)>&& completionHandler)
{
    WebCore::RealtimeMediaSourceCapabilities capabilities;
    if (auto* proxy = m_proxies.get(id))
        capabilities = proxy->source().capabilities();
    completionHandler(WTFMove(capabilities));
}

void UserMediaCaptureManagerProxy::applyConstraints(RealtimeMediaSourceIdentifier id, const WebCore::MediaConstraints& constraints)
{
    auto* proxy = m_proxies.get(id);
    if (!proxy)
        return;

    auto& source = proxy->source();
    auto result = source.applyConstraints(constraints);
    if (!result)
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsSucceeded(id, source.settings()), 0);
    else
        m_connectionProxy->connection().send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, result->badConstraint, result->message), 0);
}

void UserMediaCaptureManagerProxy::clone(RealtimeMediaSourceIdentifier clonedID, RealtimeMediaSourceIdentifier newSourceID)
{
    MESSAGE_CHECK(m_proxies.contains(clonedID));
    MESSAGE_CHECK(!m_proxies.contains(newSourceID));
    if (auto* proxy = m_proxies.get(clonedID))
        m_proxies.add(newSourceID, makeUnique<SourceProxy>(newSourceID, m_connectionProxy->connection(), proxy->source().clone()));
}

void UserMediaCaptureManagerProxy::requestToEnd(RealtimeMediaSourceIdentifier sourceID)
{
    if (auto* proxy = m_proxies.get(sourceID))
        proxy->requestToEnd();
}

void UserMediaCaptureManagerProxy::setShouldApplyRotation(RealtimeMediaSourceIdentifier sourceID, bool shouldApplyRotation)
{
    if (auto* proxy = m_proxies.get(sourceID))
        proxy->setShouldApplyRotation(shouldApplyRotation);
}

void UserMediaCaptureManagerProxy::clear()
{
    m_proxies.clear();
}

void UserMediaCaptureManagerProxy::setOrientation(uint64_t orientation)
{
    m_orientationNotifier.orientationChanged(orientation);
}

}

#undef MESSAGE_CHECK

#endif
