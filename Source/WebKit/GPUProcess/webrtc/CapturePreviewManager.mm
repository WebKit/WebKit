/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CapturePreviewManager.h"

#if ENABLE(MEDIA_STREAM)

#include "GPUProcess.h"
#include "GPUProcessProxyMessages.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "RotationMonitoringReason.h"
#include <WebCore/AudioStreamDescription.h>
#include <WebCore/MediaDeviceHashSalts.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/VideoFrame.h>
#include <WebCore/WebAudioBufferList.h>
#include <cmath>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
#include "RemoteAudioSessionProxyManager.h"
#include <WebCore/AVAudioSessionCaptureDeviceManager.h>
#endif

namespace WebKit {
WTF_MAKE_TZONE_ALLOCATED_IMPL(CapturePreviewManager);

CapturePreviewManager::CapturePreviewManager(WebCore::PageIdentifier pageIdentifier, Function<void(float)>&& audioLevelHandler)
    : m_pageIdentifier(pageIdentifier)
    , m_audioLevelHandler(WTF::move(audioLevelHandler))
{
}

CapturePreviewManager::~CapturePreviewManager()
{
    assertIsMainThread();
    stop();
}

RefPtr<WebCore::RealtimeMediaSource> CapturePreviewManager::createSource(const WebCore::CaptureDevice& device)
{
    WebCore::MediaDeviceHashSalts hashSalts;
    auto& center = WebCore::RealtimeMediaSourceCenter::singleton();
    auto sourceOrError = device.type() == WebCore::CaptureDevice::DeviceType::Microphone
        ? center.audioCaptureFactory().createAudioCaptureSource(device, WTF::move(hashSalts), nullptr, m_pageIdentifier)
        : center.videoCaptureFactory().createVideoCaptureSource(device, WTF::move(hashSalts), nullptr, m_pageIdentifier);

    if (!sourceOrError) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreviewManager unable to create %s preview source for '%s' (%s), denialReason=%d, %s",
            device.type() == WebCore::CaptureDevice::DeviceType::Microphone ? "microphone" : "camera",
            device.label().utf8(),
            device.persistentId().utf8(),
            static_cast<int>(sourceOrError.error.denialReason),
            sourceOrError.error.errorMessage.utf8());
        return nullptr;
    }

    return RefPtr { sourceOrError.source().ptr() };
}

void CapturePreviewManager::start(std::optional<WebCore::CaptureDevice>&& videoDevice, std::optional<WebCore::CaptureDevice>&& audioDevice, WebCore::IntSize previewSize, StartCallback&& callback)
{
    assertIsMainThread();

    String videoDeviceID = videoDevice ? videoDevice->persistentId() : String { };
    String audioDeviceID = audioDevice ? audioDevice->persistentId() : String { };

    if (audioDeviceID != m_audioDeviceID) {
        stopAudio();
        if (audioDevice)
            startAudio(*audioDevice);
#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
        else
            restoreAudioSession();
#endif
    }

    if (videoDeviceID == m_videoDeviceID && previewSize == m_previewSize) {
        callback(m_hostingContext);
        return;
    }

    stopVideo();

    if (!videoDevice) {
        callback({ });
        return;
    }

    startVideo(*videoDevice, previewSize, WTF::move(callback));
}

void CapturePreviewManager::startAudio(const WebCore::CaptureDevice& device)
{
    m_audioDeviceID = device.persistentId();

#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
    prepareAudioSession([protectedThis = Ref { *this }, device] {
        assertIsMainThread();
        if (protectedThis->m_audioDeviceID != device.persistentId())
            return;
        protectedThis->startAudioSource(device);
    });
#else
    startAudioSource(device);
#endif
}

void CapturePreviewManager::startAudioSource(const WebCore::CaptureDevice& device)
{
    detachAudioSource();

    m_audioSource = createSource(device);
    if (!m_audioSource) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreviewManager no audio source for '%s'", device.label().utf8());
        return;
    }

    protect(m_audioSource)->addAudioSampleObserver(*this);
    protect(m_audioSource)->start();
}

#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)

void CapturePreviewManager::prepareAudioSession(CompletionHandler<void()>&& completionHandler)
{
    if (!m_savedPreferredMicrophoneID)
        m_savedPreferredMicrophoneID = WebCore::AVAudioSessionCaptureDeviceManager::singleton().preferredMicrophoneID();

    protect(GPUProcess::singleton().audioSessionManager())->beginCapturePreview(WTF::move(completionHandler));
}

void CapturePreviewManager::restoreAudioSession()
{
    auto savedPreferredMicrophoneID = std::exchange(m_savedPreferredMicrophoneID, std::nullopt);
    if (!savedPreferredMicrophoneID)
        return;

    WebCore::AVAudioSessionCaptureDeviceManager::singleton().setPreferredMicrophoneID(*savedPreferredMicrophoneID);
    protect(GPUProcess::singleton().audioSessionManager())->endCapturePreview();
}

#endif // PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)

void CapturePreviewManager::startVideo(const WebCore::CaptureDevice& device, WebCore::IntSize previewSize, StartCallback&& callback)
{
    m_videoSource = createSource(device);
    if (!m_videoSource) {
        callback({ });
        return;
    }

    protect(m_videoSource)->monitorOrientation(m_orientationNotifier);

    RefPtr displayLayer = WebCore::SampleBufferDisplayLayer::create(*this);
    if (!displayLayer) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreviewManager unable to create a display layer");
        stopVideo();
        callback({ });
        return;
    }
    setDisplayLayer(displayLayer.copyRef());

    constexpr bool hideRootLayer = false;
    constexpr bool shouldMaintainAspectRatio = true;
    std::optional<bool> didInitialize;

    // LocalSampleBufferDisplayLayer::initialize() is synchronous, so capturing didInitialize by reference safe.
    displayLayer->initialize(hideRootLayer, previewSize, shouldMaintainAspectRatio, [&didInitialize](bool didSucceed) {
        didInitialize = didSucceed;
    });

    ASSERT(didInitialize.has_value());
    if (!didInitialize.value_or(false)) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreviewManager display layer failed to initialize");
        stopVideo();
        callback({ });
        return;
    }

    LayerHostingContextOptions contextOptions;
#if PLATFORM(IOS_FAMILY) && USE(EXTENSIONKIT)
    contextOptions.useHostable = true;
#endif
    m_layerHostingContext = LayerHostingContext::create(contextOptions);
    RetainPtr rootLayer = displayLayer->rootLayer();
    m_layerHostingContext->setRootLayer(rootLayer);
    m_hostingContext = m_layerHostingContext->hostingContext();

    m_videoDeviceID = protect(m_videoSource)->persistentID();
    m_previewSize = previewSize;

    if (RefPtr connection = GPUProcess::singleton().parentProcessConnection())
        connection->send(Messages::GPUProcessProxy::StartMonitoringCaptureDeviceRotation(m_pageIdentifier, m_videoDeviceID, RotationMonitoringReason::CapturePreview), 0);

    // Add the frame observer only once m_displayLayer is setup, so a frame never arrives before we can draw it.
    protect(m_videoSource)->addVideoFrameObserver(*this);
    protect(m_videoSource)->start();

    callback(m_hostingContext);
}

void CapturePreviewManager::setOrientation(WebCore::IntDegrees orientation)
{
    assertIsMainThread();
    m_orientationNotifier.orientationChanged(orientation);
}

void CapturePreviewManager::rotationAngleForCaptureDeviceChanged(const String& persistentId, WebCore::VideoFrameRotation rotation)
{
    assertIsMainThread();
    m_orientationNotifier.rotationAngleForCaptureDeviceChanged(persistentId, rotation);
}

void CapturePreviewManager::stopVideo()
{
    // Remove the observer without holding m_displayLayerLock, frame delivery holds the source's observer
    // lock and then takes ours, so the reverse order could deadlock.
    if (RefPtr videoSource = WTF::move(m_videoSource)) {
        videoSource->removeVideoFrameObserver(*this);
        videoSource->endImmediatly();
    }

    if (!m_videoDeviceID.isEmpty()) {
        if (RefPtr connection = GPUProcess::singleton().parentProcessConnection())
            connection->send(Messages::GPUProcessProxy::StopMonitoringCaptureDeviceRotation(m_pageIdentifier, m_videoDeviceID, RotationMonitoringReason::CapturePreview), 0);
    }
    m_videoDeviceID = { };
    m_previewSize = { };
    m_layerHostingContext = nullptr;
    setDisplayLayer(nullptr);
    m_hostingContext = { };
}

void CapturePreviewManager::detachAudioSource()
{
    if (RefPtr audioSource = WTF::move(m_audioSource)) {
        audioSource->removeAudioSampleObserver(*this);
        audioSource->endImmediatly();
    }
}

void CapturePreviewManager::stopAudio()
{
    detachAudioSource();

    m_audioDeviceID = { };

    Locker locker { m_audioLevelLock };
    m_peakSinceLastLevel = 0;
    m_framesSinceLastLevel = 0;
}

bool CapturePreviewManager::isRunning() const
{
    assertIsMainThread();
    return !m_videoDeviceID.isEmpty() || !m_audioDeviceID.isEmpty();
}

void CapturePreviewManager::stop()
{
    assertIsMainThread();

    m_audioLevelHandler = nullptr;
    stopVideo();
    stopAudio();

#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
    restoreAudioSession();
#endif
}

void CapturePreviewManager::videoFrameAvailable(WebCore::VideoFrame& frame, WebCore::VideoFrameTimeMetadata)
{
    if (RefPtr layer = displayLayer())
        layer->enqueueVideoFrame(frame);
}

RefPtr<WebCore::SampleBufferDisplayLayer> CapturePreviewManager::displayLayer() const
{
    Locker locker { m_displayLayerLock };
    return m_displayLayer;
}

void CapturePreviewManager::setDisplayLayer(RefPtr<WebCore::SampleBufferDisplayLayer>&& displayLayer)
{
    Locker locker { m_displayLayerLock };
    m_displayLayer = WTF::move(displayLayer);
}

void CapturePreviewManager::sampleBufferDisplayLayerStatusDidFail()
{
    RELEASE_LOG_ERROR(WebRTC, "CapturePreviewManager display layer status failed");
}

constexpr float audioLevelFloorInDBFS = -60;
constexpr unsigned audioLevelUpdatesPerSecond = 15;

static float normalizedLevelFromPeak(float peak)
{
    if (peak <= 0)
        return 0;

    float dbfs = 20 * std::log10(peak);
    if (dbfs <= audioLevelFloorInDBFS)
        return 0;

    return std::clamp((dbfs - audioLevelFloorInDBFS) / -audioLevelFloorInDBFS, 0.f, 1.f);
}

void CapturePreviewManager::audioSamplesAvailable(const WTF::MediaTime&, const WebCore::PlatformAudioData& data, const WebCore::AudioStreamDescription& description, size_t numberOfFrames)
{
    if (!description.isFloat())
        return;

    auto& bufferList = downcast<WebCore::WebAudioBufferList>(data);
    auto* list = bufferList.list();
    if (!list)
        return;

    float peak = 0;
    for (uint32_t bufferIndex = 0; bufferIndex < list->mNumberBuffers; ++bufferIndex) {
        for (auto sample : bufferList.bufferAsSpan<const float>(bufferIndex))
            peak = std::max(peak, std::abs(sample));
    }

    float level;
    {
        Locker locker { m_audioLevelLock };
        m_peakSinceLastLevel = std::max(m_peakSinceLastLevel, peak);
        m_framesSinceLastLevel += numberOfFrames;

        size_t framesPerUpdate = static_cast<size_t>(description.sampleRate() / audioLevelUpdatesPerSecond);
        if (!framesPerUpdate || m_framesSinceLastLevel < framesPerUpdate)
            return;

        m_framesSinceLastLevel = 0;
        level = normalizedLevelFromPeak(std::exchange(m_peakSinceLastLevel, 0));
    }

    callOnMainRunLoop([protectedThis = Ref { *this }, level] {
        assertIsMainThread();
        if (protectedThis->m_audioLevelHandler)
            protectedThis->m_audioLevelHandler(level);
    });
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
