/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "UserMediaCaptureManager.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include  "GPUProcessConnection.h"
#include "SharedRingBufferStorage.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/WebAudioBufferList.h>
#include <WebCore/WebAudioSourceProviderAVFObjC.h>
#include <wtf/Assertions.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

static uint64_t nextSessionID()
{
    static uint64_t nextID = 0;
    return ++nextID;
}

class UserMediaCaptureManager::Source : public RealtimeMediaSource {
public:
    Source(String&& sourceID, Type type, CaptureDevice::DeviceType deviceType, String&& name, String&& hashSalt, uint64_t id, UserMediaCaptureManager& manager)
        : RealtimeMediaSource(type, WTFMove(name), WTFMove(sourceID), WTFMove(hashSalt))
        , m_id(id)
        , m_manager(manager)
        , m_deviceType(deviceType)
    {
        switch (m_deviceType) {
        case CaptureDevice::DeviceType::Microphone:
            m_ringBuffer = makeUnique<CARingBuffer>(makeUniqueRef<SharedRingBufferStorage>(nullptr));
#if PLATFORM(IOS_FAMILY)
            RealtimeMediaSourceCenter::singleton().audioCaptureFactory().setActiveSource(*this);
#endif
            break;
        case CaptureDevice::DeviceType::Camera:
#if PLATFORM(IOS_FAMILY)
            RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif
            break;
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Window:
            break;
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
        }
    }

    ~Source()
    {
        switch (m_deviceType) {
        case CaptureDevice::DeviceType::Microphone:
            storage().invalidate();
#if PLATFORM(IOS_FAMILY)
            RealtimeMediaSourceCenter::singleton().audioCaptureFactory().unsetActiveSource(*this);
#endif
            break;
        case CaptureDevice::DeviceType::Camera:
#if PLATFORM(IOS_FAMILY)
            RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
            break;
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Window:
            break;
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
        }
    }

    SharedRingBufferStorage& storage()
    {
        ASSERT(type() == Type::Audio);
        return static_cast<SharedRingBufferStorage&>(m_ringBuffer->storage());
    }

    Ref<RealtimeMediaSource> clone() final
    {
        return m_manager.cloneSource(*this);
    }

    uint64_t sourceID() const
    {
        return m_id;
    }

    const RealtimeMediaSourceSettings& settings() const
    {
        return m_settings;
    }

    const RealtimeMediaSourceCapabilities& capabilities() final;

    const RealtimeMediaSourceSettings& settings() final { return m_settings; }
    void setSettings(RealtimeMediaSourceSettings&& settings)
    {
        auto changed = m_settings.difference(settings);
        m_settings = WTFMove(settings);
        notifySettingsDidChangeObservers(changed);
    }

    const CAAudioStreamDescription& description() const { return m_description; }
    void setStorage(const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
    {
        ASSERT(type() == Type::Audio);
        m_description = description;

        if (handle.isNull()) {
            m_ringBuffer->deallocate();
            storage().setReadOnly(false);
            storage().setStorage(nullptr);
            return;
        }

        RefPtr<SharedMemory> memory = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);
        storage().setStorage(WTFMove(memory));
        storage().setReadOnly(true);

        m_ringBuffer->allocate(description, numberOfFrames);
    }

    void setRingBufferFrameBounds(uint64_t startFrame, uint64_t endFrame)
    {
        ASSERT(type() == Type::Audio);
        m_ringBuffer->setCurrentFrameBounds(startFrame, endFrame);
    }

    void audioSamplesAvailable(MediaTime time, uint64_t numberOfFrames)
    {
        ASSERT(type() == Type::Audio);
        WebAudioBufferList audioData(m_description, numberOfFrames);
        m_ringBuffer->fetch(audioData.list(), numberOfFrames, time.timeValue());

        RealtimeMediaSource::audioSamplesAvailable(time, audioData, m_description, numberOfFrames);
    }

#if HAVE(IOSURFACE)
    void remoteVideoSampleAvailable(RemoteVideoSample&& remoteSample)
    {
        ASSERT(type() == Type::Video);

        setIntrinsicSize(remoteSample.size());

        if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != remoteSample.videoFormat())
            m_imageTransferSession = ImageTransferSessionVT::create(remoteSample.videoFormat());

        if (!m_imageTransferSession) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto sampleRef = m_imageTransferSession->createMediaSample(remoteSample.surface(), remoteSample.time(), remoteSample.size());
        if (!sampleRef) {
            ASSERT_NOT_REACHED();
            return;
        }

        RealtimeMediaSource::videoSampleAvailable(*sampleRef);
    }
#endif

    void applyConstraintsSucceeded(const WebCore::RealtimeMediaSourceSettings& settings)
    {
        setSettings(WebCore::RealtimeMediaSourceSettings(settings));

        auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
        callback({ });
    }

    void applyConstraintsFailed(String&& failedConstraint, String&& errorMessage)
    {
        auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
        callback(ApplyConstraintsError { WTFMove(failedConstraint), WTFMove(errorMessage) });
    }

    CaptureDevice::DeviceType deviceType() const final { return m_deviceType; }

    void setShouldCaptureInGPUProcess(bool value) { m_shouldCaptureInGPUProcess = value; }
    bool shouldCaptureInGPUProcess() const { return m_shouldCaptureInGPUProcess; }

    IPC::Connection* connection();

    void hasEnded() final;

private:
    void startProducingData() final;
    void stopProducingData() final;
    bool isCaptureSource() const final { return true; }

    // RealtimeMediaSource
    void beginConfiguration() final { }
    void commitConfiguration() final { }
    void applyConstraints(const WebCore::MediaConstraints&, ApplyConstraintsHandler&&) final;
    void requestToEnd(Observer&) final;
    void stopBeingObserved() final;

    uint64_t m_id;
    UserMediaCaptureManager& m_manager;
    mutable Optional<RealtimeMediaSourceCapabilities> m_capabilities;
    RealtimeMediaSourceSettings m_settings;

    CAAudioStreamDescription m_description;
    std::unique_ptr<CARingBuffer> m_ringBuffer;

    std::unique_ptr<ImageTransferSessionVT> m_imageTransferSession;
    CaptureDevice::DeviceType m_deviceType { CaptureDevice::DeviceType::Unknown };

    Deque<ApplyConstraintsHandler> m_pendingApplyConstraintsCallbacks;
    bool m_shouldCaptureInGPUProcess { false };
};

UserMediaCaptureManager::UserMediaCaptureManager(WebProcess& process)
    : m_process(process)
    , m_audioFactory(*this)
    , m_videoFactory(*this)
    , m_displayFactory(*this)
{
    m_process.addMessageReceiver(Messages::UserMediaCaptureManager::messageReceiverName(), *this);
}

UserMediaCaptureManager::~UserMediaCaptureManager()
{
    RealtimeMediaSourceCenter::singleton().unsetAudioCaptureFactory(m_audioFactory);
    RealtimeMediaSourceCenter::singleton().unsetDisplayCaptureFactory(m_displayFactory);
    RealtimeMediaSourceCenter::singleton().unsetVideoCaptureFactory(m_videoFactory);
    m_process.removeMessageReceiver(Messages::UserMediaCaptureManager::messageReceiverName());
}

const char* UserMediaCaptureManager::supplementName()
{
    return "UserMediaCaptureManager";
}

void UserMediaCaptureManager::initialize(const WebProcessCreationParameters& parameters)
{
    MockRealtimeMediaSourceCenter::singleton().setMockAudioCaptureEnabled(!parameters.shouldCaptureAudioInUIProcess && !parameters.shouldCaptureAudioInGPUProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockVideoCaptureEnabled(!parameters.shouldCaptureVideoInUIProcess);
    MockRealtimeMediaSourceCenter::singleton().setMockDisplayCaptureEnabled(!parameters.shouldCaptureDisplayInUIProcess);

    m_audioFactory.setShouldCaptureInGPUProcess(parameters.shouldCaptureAudioInGPUProcess);
    if (parameters.shouldCaptureAudioInUIProcess || parameters.shouldCaptureAudioInGPUProcess)
        RealtimeMediaSourceCenter::singleton().setAudioCaptureFactory(m_audioFactory);
    if (parameters.shouldCaptureVideoInUIProcess)
        RealtimeMediaSourceCenter::singleton().setVideoCaptureFactory(m_videoFactory);
    if (parameters.shouldCaptureDisplayInUIProcess)
        RealtimeMediaSourceCenter::singleton().setDisplayCaptureFactory(m_displayFactory);
}

WebCore::CaptureSourceOrError UserMediaCaptureManager::createCaptureSource(const CaptureDevice& device, String&& hashSalt, const WebCore::MediaConstraints* constraints, bool shouldCaptureInGPUProcess)
{
    if (!constraints)
        return { };

    uint64_t id = nextSessionID();
    RealtimeMediaSourceSettings settings;
    String errorMessage;
    bool succeeded;
#if ENABLE(GPU_PROCESS)
    auto* connection = shouldCaptureInGPUProcess ? &m_process.ensureGPUProcessConnection().connection() : m_process.parentProcessConnection();
#else
    ASSERT(!shouldCaptureInGPUProcess);
    auto* connection = m_process.parentProcessConnection();
#endif
    if (!connection->sendSync(Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints(id, device, hashSalt, *constraints), Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints::Reply(succeeded, errorMessage, settings), 0))
        return WTFMove(errorMessage);

    if (!succeeded)
        return WTFMove(errorMessage);

    auto type = device.type() == CaptureDevice::DeviceType::Microphone ? WebCore::RealtimeMediaSource::Type::Audio : WebCore::RealtimeMediaSource::Type::Video;
    auto source = adoptRef(*new Source(String::number(id), type, device.type(), String { settings.label().string() }, WTFMove(hashSalt), id, *this));
    if (shouldCaptureInGPUProcess)
        source->setShouldCaptureInGPUProcess(shouldCaptureInGPUProcess);
    source->setSettings(WTFMove(settings));
    m_sources.add(id, source.copyRef());
    return WebCore::CaptureSourceOrError(WTFMove(source));
}

void UserMediaCaptureManager::sourceStopped(uint64_t id)
{
    if (auto source = m_sources.get(id)) {
        source->stop();
        source->hasEnded();
    }
}

void UserMediaCaptureManager::captureFailed(uint64_t id)
{
    if (auto source = m_sources.get(id)) {
        source->captureFailed();
        source->hasEnded();
    }
}

void UserMediaCaptureManager::sourceMutedChanged(uint64_t id, bool muted)
{
    if (auto source = m_sources.get(id))
        source->setMuted(muted);
}

void UserMediaCaptureManager::sourceSettingsChanged(uint64_t id, const RealtimeMediaSourceSettings& settings)
{
    if (auto source = m_sources.get(id))
        source->setSettings(RealtimeMediaSourceSettings(settings));
}

void UserMediaCaptureManager::storageChanged(uint64_t id, const SharedMemory::Handle& handle, const WebCore::CAAudioStreamDescription& description, uint64_t numberOfFrames)
{
    if (auto source = m_sources.get(id))
        source->setStorage(handle, description, numberOfFrames);
}

void UserMediaCaptureManager::ringBufferFrameBoundsChanged(uint64_t id, uint64_t startFrame, uint64_t endFrame)
{
    if (auto source = m_sources.get(id))
        source->setRingBufferFrameBounds(startFrame, endFrame);
}

void UserMediaCaptureManager::audioSamplesAvailable(uint64_t id, MediaTime time, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame)
{
    if (auto source = m_sources.get(id)) {
        source->setRingBufferFrameBounds(startFrame, endFrame);
        source->audioSamplesAvailable(time, numberOfFrames);
    }
}

#if HAVE(IOSURFACE)
void UserMediaCaptureManager::remoteVideoSampleAvailable(uint64_t id, RemoteVideoSample&& sample)
{
    if (auto source = m_sources.get(id))
        source->remoteVideoSampleAvailable(WTFMove(sample));
}
#else
NO_RETURN_DUE_TO_ASSERT void UserMediaCaptureManager::remoteVideoSampleAvailable(uint64_t, RemoteVideoSample&&)
{
    ASSERT_NOT_REACHED();
}
#endif

IPC::Connection* UserMediaCaptureManager::Source::connection()
{
#if ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return &m_manager.m_process.ensureGPUProcessConnection().connection();
#endif
    return m_manager.m_process.parentProcessConnection();
}

void UserMediaCaptureManager::Source::startProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StartProducingData(m_id), 0);
}

void UserMediaCaptureManager::Source::stopProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StopProducingData(m_id), 0);
}

const WebCore::RealtimeMediaSourceCapabilities& UserMediaCaptureManager::Source::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities;
        connection()->sendSync(Messages::UserMediaCaptureManagerProxy::Capabilities { m_id }, Messages::UserMediaCaptureManagerProxy::Capabilities::Reply(capabilities), 0);
        m_capabilities = WTFMove(capabilities);
    }

    return m_capabilities.value();
}

void UserMediaCaptureManager::Source::applyConstraints(const WebCore::MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    m_pendingApplyConstraintsCallbacks.append(WTFMove(completionHandler));
    connection()->send(Messages::UserMediaCaptureManagerProxy::ApplyConstraints(m_id, constraints), 0);
}

void UserMediaCaptureManager::sourceEnded(uint64_t id)
{
    m_sources.remove(id);
}

void UserMediaCaptureManager::Source::hasEnded()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::End { m_id }, 0);
    m_manager.sourceEnded(m_id);
}

void UserMediaCaptureManager::applyConstraintsSucceeded(uint64_t id, const WebCore::RealtimeMediaSourceSettings& settings)
{
    if (auto source = m_sources.get(id))
        source->applyConstraintsSucceeded(settings);
}

void UserMediaCaptureManager::applyConstraintsFailed(uint64_t id, String&& failedConstraint, String&& message)
{
    if (auto source = m_sources.get(id))
        source->applyConstraintsFailed(WTFMove(failedConstraint), WTFMove(message));
}

Ref<RealtimeMediaSource> UserMediaCaptureManager::cloneSource(Source& source)
{
    switch (source.type()) {
    case RealtimeMediaSource::Type::Video:
        return cloneVideoSource(source);
    case RealtimeMediaSource::Type::Audio:
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
    return makeRef(source);
}

Ref<RealtimeMediaSource> UserMediaCaptureManager::cloneVideoSource(Source& source)
{
    uint64_t id = nextSessionID();
    if (!m_process.send(Messages::UserMediaCaptureManagerProxy::Clone { source.sourceID(), id }, 0))
        return makeRef(source);

    auto settings = source.settings();
    auto cloneSource = adoptRef(*new Source(String::number(id), source.type(), source.deviceType(), String { settings.label().string() }, source.deviceIDHashSalt(), id, *this));
    cloneSource->setSettings(WTFMove(settings));
    m_sources.add(id, cloneSource.copyRef());
    return cloneSource;
}

void UserMediaCaptureManager::Source::stopBeingObserved()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::RequestToEnd { m_id }, 0);
}

void UserMediaCaptureManager::Source::requestToEnd(Observer& observer)
{
    switch (type()) {
    case Type::Audio:
        RealtimeMediaSource::requestToEnd(observer);
        break;
    case Type::Video:
        stopBeingObserved();
        break;
    case Type::None:
        ASSERT_NOT_REACHED();
    }
}

CaptureSourceOrError UserMediaCaptureManager::AudioFactory::createAudioCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints)
{
    if (m_shouldCaptureInGPUProcess) {
#if ENABLE(GPU_PROCESS)
        return m_manager.createCaptureSource(device, WTFMove(hashSalt), constraints, m_shouldCaptureInGPUProcess);
#else
        return CaptureSourceOrError { "Audio capture in GPUProcess is not implemented"_s };
#endif
    }
    return m_manager.createCaptureSource(device, WTFMove(hashSalt), constraints);
}

#if PLATFORM(IOS_FAMILY)
void UserMediaCaptureManager::AudioFactory::setAudioCapturePageState(bool interrupted, bool pageMuted)
{
    if (auto* activeSource = this->activeSource())
        activeSource->setInterrupted(interrupted, pageMuted);
}

void UserMediaCaptureManager::VideoFactory::setVideoCapturePageState(bool interrupted, bool pageMuted)
{
    // In case of cloning, we might have more than a single source.
    for (auto& source : m_manager.m_sources.values()) {
        if (source->deviceType() == CaptureDevice::DeviceType::Camera)
            source->setInterrupted(interrupted, pageMuted);
    }
}
#endif

}

#endif
