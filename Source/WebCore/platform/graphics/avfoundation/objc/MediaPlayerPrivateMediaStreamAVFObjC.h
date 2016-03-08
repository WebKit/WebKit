/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPlayerPrivateMediaStreamAVFObjC_h
#define MediaPlayerPrivateMediaStreamAVFObjC_h

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#include "MediaPlayerPrivate.h"
#include "MediaStreamPrivate.h"
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVStreamSession;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class AudioTrackPrivateMediaStream;
class AVVideoCaptureSource;
class Clock;
class MediaSourcePrivateClient;
class VideoTrackPrivateMediaStream;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
class VideoFullscreenLayerManager;
#endif

class MediaPlayerPrivateMediaStreamAVFObjC : public MediaPlayerPrivateInterface, public MediaStreamPrivate::Observer {
public:
    explicit MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaStreamAVFObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    MediaPlayer::NetworkState networkState() const override;
    void setNetworkState(MediaPlayer::NetworkState);
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);

    WeakPtr<MediaPlayerPrivateMediaStreamAVFObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

private:
    // MediaPlayerPrivateInterface

    // FIXME(146853): Implement necessary conformations to standard in HTMLMediaElement for MediaStream

    void load(const String&) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) override;
#endif
    void load(MediaStreamPrivate&) override;
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformLayer* platformLayer() const override;

    bool supportsFullscreen() const override { return true; }

    void play() override;
    void pause() override;
    bool paused() const override;

    void setVolume(float) override;
    void internalSetVolume(float, bool);
    void setMuted(bool) override;
    bool supportsMuting() const override { return true; }

    bool supportsScanning() const override { return false; }

    FloatSize naturalSize() const override { return m_intrinsicSize; }

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setVisible(bool) override { /* No-op */ }

    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override;

    bool seeking() const override { return false; }

    std::unique_ptr<PlatformTimeRanges> seekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;

    bool didLoadingProgress() const override { return m_playing; }

    void setSize(const IntSize&) override { /* No-op */ }

    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    bool metaDataAvailable() const { return m_mediaStreamPrivate && m_readyState >= MediaPlayer::HaveMetadata; }

    bool supportsAcceleratedRendering() const override { return true; }

    bool hasSingleSecurityOrigin() const override { return true; }

    MediaPlayer::MovieLoadType movieLoadType() const override { return MediaPlayer::LiveStream; }

    String engineDescription() const override;

    size_t extraMemoryCost() const override { return 0; }

    bool ended() const override { return m_ended; }

    bool shouldBePlaying() const;

    MediaPlayer::ReadyState currentReadyState();
    void updateReadyState();

    void updateIntrinsicSize(const FloatSize&);
    void createPreviewLayers();
    void updateTracks();
    void renderingModeChanged();

    void scheduleDeferredTask(std::function<void()>);

    enum DisplayMode {
        None,
        PaintItBlack,
        PausedImage,
        LivePreview,
    };
    DisplayMode currentDisplayMode() const;
    void updateDisplayMode();

    // MediaStreamPrivate::Observer
    void activeStatusChanged() override;
    void characteristicsChanged() override;
    void didAddTrack(MediaStreamTrackPrivate&) override;
    void didRemoveTrack(MediaStreamTrackPrivate&) override;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void setVideoFullscreenLayer(PlatformLayer*) override;
    void setVideoFullscreenFrame(FloatRect) override;
#endif

    MediaPlayer* m_player { nullptr };
    WeakPtrFactory<MediaPlayerPrivateMediaStreamAVFObjC> m_weakPtrFactory;
    RefPtr<MediaStreamPrivate> m_mediaStreamPrivate;
    mutable RetainPtr<CALayer> m_previewLayer;
    mutable RetainPtr<PlatformLayer> m_videoBackgroundLayer;
    RetainPtr<CGImageRef> m_pausedImage;
    std::unique_ptr<Clock> m_clock;

    HashMap<String, RefPtr<AudioTrackPrivateMediaStream>> m_audioTrackMap;
    HashMap<String, RefPtr<VideoTrackPrivateMediaStream>> m_videoTrackMap;

    MediaPlayer::NetworkState m_networkState { MediaPlayer::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::HaveNothing };
    FloatSize m_intrinsicSize;
    float m_volume { 1 };
    DisplayMode m_displayMode { None };
    bool m_playing { false };
    bool m_muted { false };
    bool m_haveEverPlayed { false };
    bool m_ended { false };

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    std::unique_ptr<VideoFullscreenLayerManager> m_videoFullscreenLayerManager;
#endif
};
    
}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaStreamAVFObjC_h
