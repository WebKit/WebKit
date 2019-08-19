/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "SharedRingBufferStorage.h"
#include "UserMediaCaptureManagerMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcessProxy.h"
#include <WebCore/CARingBuffer.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/WebAudioBufferList.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK_CONTEXTID(id) MESSAGE_CHECK_BASE(m_proxies.isValidKey(id), m_process.connection())

namespace WebKit {
using namespace WebCore;

class UserMediaCaptureManagerProxy::SourceProxy : public RealtimeMediaSource::Observer, public SharedRingBufferStorage::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SourceProxy(uint64_t id, Ref<IPC::Connection>&& connection, Ref<RealtimeMediaSource>&& source)
        : m_id(id)
        , m_connection(WTFMove(connection))
        , m_source(WTFMove(source))
        , m_ringBuffer(makeUniqueRef<SharedRingBufferStorage>(makeUniqueRef<SharedRingBufferStorage>(this)))
    {
        m_source->addObserver(*this);
    }

    ~SourceProxy()
    {
        storage().invalidate();
        m_source->removeObserver(*this);
    }

    RealtimeMediaSource& source() { return m_source; }
    SharedRingBufferStorage& storage() { return static_cast<SharedRingBufferStorage&>(m_ringBuffer.storage()); }
    CAAudioStreamDescription& description() { return m_description; }
    int64_t numberOfFrames() { return m_numberOfFrames; }

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
        m_connection->send(Messages::UserMediaCaptureManager::AudioSamplesAvailable(m_id, time, numberOfFrames, startFrame, endFrame), 0);
    }

    void videoSampleAvailable(MediaSample& sample) final
    {
#if HAVE(IOSURFACE)
        auto remoteSample = RemoteVideoSample::create(WTFMove(sample));
        if (remoteSample)
            m_connection->send(Messages::UserMediaCaptureManager::RemoteVideoSampleAvailable(m_id, WTFMove(*remoteSample)), 0);
#else
        ASSERT_NOT_REACHED();
#endif
    }

    void storageChanged(SharedMemory* storage) final {
        SharedMemory::Handle handle;
        if (storage)
            storage->createHandle(handle, SharedMemory::Protection::ReadOnly);
        m_connection->send(Messages::UserMediaCaptureManager::StorageChanged(m_id, handle, m_description, m_numberOfFrames), 0);
    }

protected:
    uint64_t m_id;
    Ref<IPC::Connection> m_connection;
    Ref<RealtimeMediaSource> m_source;
    CARingBuffer m_ringBuffer;
    CAAudioStreamDescription m_description { };
    int64_t m_numberOfFrames { 0 };
};

UserMediaCaptureManagerProxy::UserMediaCaptureManagerProxy(WebProcessProxy& process)
    : m_process(process)
{
    m_process.addMessageReceiver(Messages::UserMediaCaptureManagerProxy::messageReceiverName(), *this);
}

UserMediaCaptureManagerProxy::~UserMediaCaptureManagerProxy()
{
    m_process.removeMessageReceiver(Messages::UserMediaCaptureManagerProxy::messageReceiverName());
}

void UserMediaCaptureManagerProxy::createMediaSourceForCaptureDeviceWithConstraints(uint64_t id, const CaptureDevice& device, String&& hashSalt, const MediaConstraints& constraints, CompletionHandler<void(bool succeeded, String invalidConstraints, WebCore::RealtimeMediaSourceSettings&&)>&& completionHandler)
{
    MESSAGE_CHECK_CONTEXTID(id);

    CaptureSourceOrError sourceOrError;
    switch (device.type()) {
    case WebCore::CaptureDevice::DeviceType::Microphone:
        sourceOrError = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().createAudioCaptureSource(device, WTFMove(hashSalt), &constraints);
        break;
    case WebCore::CaptureDevice::DeviceType::Camera:
        sourceOrError = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().createVideoCaptureSource(device, WTFMove(hashSalt), &constraints);
        break;
    case WebCore::CaptureDevice::DeviceType::Screen:
    case WebCore::CaptureDevice::DeviceType::Window:
        sourceOrError = RealtimeMediaSourceCenter::singleton().displayCaptureFactory().createDisplayCaptureSource(device, &constraints);
        break;
    case WebCore::CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    bool succeeded = !!sourceOrError;
    String invalidConstraints;
    WebCore::RealtimeMediaSourceSettings settings;
    if (sourceOrError) {
        auto source = sourceOrError.source();
        source->setIsRemote(true);
        settings = source->settings();
        ASSERT(!m_proxies.contains(id));
        m_proxies.add(id, makeUnique<SourceProxy>(id, *m_process.connection(), WTFMove(source)));
    } else
        invalidConstraints = WTFMove(sourceOrError.errorMessage);
    completionHandler(succeeded, invalidConstraints, WTFMove(settings));
}

void UserMediaCaptureManagerProxy::startProducingData(uint64_t id)
{
    MESSAGE_CHECK_CONTEXTID(id);
    if (auto* proxy = m_proxies.get(id))
        proxy->source().start();
}

void UserMediaCaptureManagerProxy::stopProducingData(uint64_t id)
{
    MESSAGE_CHECK_CONTEXTID(id);
    if (auto* proxy = m_proxies.get(id))
        proxy->source().stop();
}

void UserMediaCaptureManagerProxy::end(uint64_t id)
{
    MESSAGE_CHECK_CONTEXTID(id);
    m_proxies.remove(id);
}

void UserMediaCaptureManagerProxy::capabilities(uint64_t id, CompletionHandler<void(WebCore::RealtimeMediaSourceCapabilities&&)>&& completionHandler)
{
    MESSAGE_CHECK_CONTEXTID(id);
    WebCore::RealtimeMediaSourceCapabilities capabilities;
    if (auto* proxy = m_proxies.get(id))
        capabilities = proxy->source().capabilities();
    completionHandler(WTFMove(capabilities));
}

void UserMediaCaptureManagerProxy::setMuted(uint64_t id, bool muted)
{
    MESSAGE_CHECK_CONTEXTID(id);
    if (auto* proxy = m_proxies.get(id))
        proxy->source().setMuted(muted);
}

void UserMediaCaptureManagerProxy::applyConstraints(uint64_t id, const WebCore::MediaConstraints& constraints)
{
    MESSAGE_CHECK_CONTEXTID(id);
    auto* proxy = m_proxies.get(id);
    if (!proxy)
        return;

    auto& source = proxy->source();
    auto result = source.applyConstraints(constraints);
    if (!result)
        m_process.send(Messages::UserMediaCaptureManager::ApplyConstraintsSucceeded(id, source.settings()), 0);
    else
        m_process.send(Messages::UserMediaCaptureManager::ApplyConstraintsFailed(id, result->badConstraint, result->message), 0);
}

void UserMediaCaptureManagerProxy::clear()
{
    m_proxies.clear();
}

}

#undef MESSAGE_CHECK_CONTEXTID

#endif
