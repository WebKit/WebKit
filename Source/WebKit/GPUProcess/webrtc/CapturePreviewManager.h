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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include <WebCore/CaptureDevice.h>
#include <WebCore/HostingContext.h>
#include <WebCore/IntSize.h>
#include <WebCore/OrientationNotifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/SampleBufferDisplayLayer.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class LayerHostingContext;

class CapturePreviewManager
    : public ThreadSafeRefCounted<CapturePreviewManager, WTF::DestructionThread::MainRunLoop>
    , public WebCore::RealtimeMediaSource::VideoFrameObserver
    , public WebCore::RealtimeMediaSource::AudioSampleObserver
    , public WebCore::SampleBufferDisplayLayerClient
    , public CanMakeCheckedPtr<CapturePreviewManager> {
    WTF_MAKE_TZONE_ALLOCATED(CapturePreviewManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CapturePreviewManager);
    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);
public:
    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    static Ref<CapturePreviewManager> create(WebCore::PageIdentifier pageIdentifier, Function<void(float)>&& audioLevelHandler) { return adoptRef(*new CapturePreviewManager(pageIdentifier, WTF::move(audioLevelHandler))); }
    ~CapturePreviewManager();

    USING_CAN_MAKE_WEAKPTR(WebCore::SampleBufferDisplayLayerClient);

    using StartCallback = CompletionHandler<void(WebCore::HostingContext)>;
    void start(std::optional<WebCore::CaptureDevice>&& videoDevice, std::optional<WebCore::CaptureDevice>&& audioDevice, WebCore::IntSize previewSize, StartCallback&&);
    void stop();
    bool isRunning() const;

    void setOrientation(WebCore::IntDegrees);
    void rotationAngleForCaptureDeviceChanged(const String&, WebCore::VideoFrameRotation);

private:
    CapturePreviewManager(WebCore::PageIdentifier, Function<void(float)>&& audioLevelHandler);

    RefPtr<WebCore::RealtimeMediaSource> createSource(const WebCore::CaptureDevice&);
    void startVideo(const WebCore::CaptureDevice&, WebCore::IntSize, StartCallback&&) WTF_REQUIRES_CAPABILITY(mainThread);
    void startAudio(const WebCore::CaptureDevice&) WTF_REQUIRES_CAPABILITY(mainThread);
    void startAudioSource(const WebCore::CaptureDevice&) WTF_REQUIRES_CAPABILITY(mainThread);
    void stopVideo() WTF_REQUIRES_CAPABILITY(mainThread);
    void stopAudio() WTF_REQUIRES_CAPABILITY(mainThread);
    void detachAudioSource() WTF_REQUIRES_CAPABILITY(mainThread);
    RefPtr<WebCore::SampleBufferDisplayLayer> displayLayer() const;
    void setDisplayLayer(RefPtr<WebCore::SampleBufferDisplayLayer>&&);
#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
    void prepareAudioSession(CompletionHandler<void()>&&) WTF_REQUIRES_CAPABILITY(mainThread);
    void restoreAudioSession() WTF_REQUIRES_CAPABILITY(mainThread);
#endif

    // WebCore::RealtimeMediaSource::VideoFrameObserver
    void videoFrameAvailable(WebCore::VideoFrame&, WebCore::VideoFrameTimeMetadata) final;

    // WebCore::RealtimeMediaSource::AudioSampleObserver
    void audioSamplesAvailable(const WTF::MediaTime&, const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t) final;

    // WebCore::SampleBufferDisplayLayerClient
    void sampleBufferDisplayLayerStatusDidFail() final;
    void updateVideoFrameCounters(uint64_t, uint64_t) final { }
#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const final { return false; }
#endif

    const WebCore::PageIdentifier m_pageIdentifier;

    RefPtr<WebCore::RealtimeMediaSource> m_videoSource WTF_GUARDED_BY_CAPABILITY(mainThread);
    RefPtr<WebCore::RealtimeMediaSource> m_audioSource WTF_GUARDED_BY_CAPABILITY(mainThread);
    mutable Lock m_displayLayerLock;
    RefPtr<WebCore::SampleBufferDisplayLayer> m_displayLayer WTF_GUARDED_BY_LOCK(m_displayLayerLock);
    std::unique_ptr<LayerHostingContext> m_layerHostingContext WTF_GUARDED_BY_CAPABILITY(mainThread);
    WebCore::HostingContext m_hostingContext WTF_GUARDED_BY_CAPABILITY(mainThread);

    String m_videoDeviceID WTF_GUARDED_BY_CAPABILITY(mainThread);
    String m_audioDeviceID WTF_GUARDED_BY_CAPABILITY(mainThread);
    WebCore::IntSize m_previewSize WTF_GUARDED_BY_CAPABILITY(mainThread);

    WebCore::OrientationNotifier m_orientationNotifier WTF_GUARDED_BY_CAPABILITY(mainThread) { 0 };
    Function<void(float)> m_audioLevelHandler WTF_GUARDED_BY_CAPABILITY(mainThread);
    Lock m_audioLevelLock;
    float m_peakSinceLastLevel WTF_GUARDED_BY_LOCK(m_audioLevelLock) { 0 };
    size_t m_framesSinceLastLevel WTF_GUARDED_BY_LOCK(m_audioLevelLock) { 0 };
#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
    std::optional<String> m_savedPreferredMicrophoneID WTF_GUARDED_BY_CAPABILITY(mainThread);
#endif
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
