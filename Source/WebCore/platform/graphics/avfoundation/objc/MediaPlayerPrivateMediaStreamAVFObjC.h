/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "MediaSample.h"
#include "MediaStreamPrivate.h"
#include <CoreGraphics/CGAffineTransform.h>
#include <wtf/Function.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVStreamSession;
OBJC_CLASS NSNumber;
OBJC_CLASS WebAVSampleBufferStatusChangeListener;

namespace PAL {
class Clock;
}

namespace WebCore {

class AudioTrackPrivateMediaStreamCocoa;
class AVVideoCaptureSource;
class MediaSourcePrivateClient;
class PixelBufferConformerCV;
class VideoFullscreenLayerManagerObjC;
class VideoTrackPrivateMediaStream;

class MediaPlayerPrivateMediaStreamAVFObjC final : public CanMakeWeakPtr<MediaPlayerPrivateMediaStreamAVFObjC>, public MediaPlayerPrivateInterface, private MediaStreamPrivate::Observer, private MediaStreamTrackPrivate::Observer
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
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

    void ensureLayers();
    void destroyLayers();

    void layerStatusDidChange(AVSampleBufferDisplayLayer*);
    void layerErrorDidChange(AVSampleBufferDisplayLayer*);
    void backgroundLayerBoundsChanged();

    PlatformLayer* displayLayer();
    PlatformLayer* backgroundLayer();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "MediaPlayerPrivateMediaStreamAVFObjC"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
#endif

private:
    // MediaPlayerPrivateInterface

    // FIXME(146853): Implement necessary conformations to standard in HTMLMediaElement for MediaStream

    bool didPassCORSAccessCheck() const final;

    void load(const String&) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) override;
#endif
    void load(MediaStreamPrivate&) override;
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformLayer* platformLayer() const override;
    
    bool supportsPictureInPicture() const override;
    bool supportsFullscreen() const override { return true; }

    void play() override;
    void pause() override;
    bool paused() const override { return !playing(); }

    void setVolume(float) override;
    void setMuted(bool) override;
    bool supportsMuting() const override { return true; }

    bool supportsScanning() const override { return false; }

    FloatSize naturalSize() const override { return m_intrinsicSize; }

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setVisible(bool) final;

    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override;

    bool seeking() const override { return false; }

    std::unique_ptr<PlatformTimeRanges> seekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;

    bool didLoadingProgress() const override { return m_playing; }

    void setSize(const IntSize&) override { /* No-op */ }

    void flushRenderers();

    using PendingSampleQueue = Deque<Ref<MediaSample>>;
    void addSampleToPendingQueue(PendingSampleQueue&, MediaSample&);
    void removeOldSamplesFromPendingQueue(PendingSampleQueue&);

    MediaTime calculateTimelineOffset(const MediaSample&, double);
    
    void enqueueVideoSample(MediaStreamTrackPrivate&, MediaSample&);
    void enqueueCorrectedVideoSample(MediaSample&);
    void flushAndRemoveVideoSampleBuffers();
    void requestNotificationWhenReadyForVideoData();

    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    bool metaDataAvailable() const { return m_mediaStreamPrivate && m_readyState >= MediaPlayer::HaveMetadata; }

    void acceleratedRenderingStateChanged() override;
    bool supportsAcceleratedRendering() const override { return true; }

    bool hasSingleSecurityOrigin() const override { return true; }

    MediaPlayer::MovieLoadType movieLoadType() const override { return MediaPlayer::LiveStream; }

    String engineDescription() const override;

    size_t extraMemoryCost() const override { return 0; }

    bool ended() const override { return m_ended; }

    void setShouldBufferData(bool) override;

    MediaPlayer::ReadyState currentReadyState();
    void updateReadyState();

    void updateTracks();
    void updateRenderingMode();
    void checkSelectedVideoTrack();
    void updateDisplayLayer();

    void scheduleDeferredTask(Function<void ()>&&);

    enum DisplayMode {
        None,
        PaintItBlack,
        WaitingForFirstImage,
        PausedImage,
        LivePreview,
    };
    DisplayMode currentDisplayMode() const;
    bool updateDisplayMode();
    void updateCurrentFrameImage();

    enum class PlaybackState {
        None,
        Playing,
        Paused,
    };
    bool playing() const { return m_playbackState == PlaybackState::Playing; }

    // MediaStreamPrivate::Observer
    void activeStatusChanged() override;
    void characteristicsChanged() override;
    void didAddTrack(MediaStreamTrackPrivate&) override;
    void didRemoveTrack(MediaStreamTrackPrivate&) override;

    // MediaStreamPrivateTrack::Observer
    void trackStarted(MediaStreamTrackPrivate&) override { };
    void trackEnded(MediaStreamTrackPrivate&) override { };
    void trackMutedChanged(MediaStreamTrackPrivate&) override { };
    void trackSettingsChanged(MediaStreamTrackPrivate&) override { };
    void trackEnabledChanged(MediaStreamTrackPrivate&) override { };
    void sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample&) override;
    void readyStateChanged(MediaStreamTrackPrivate&) override;

    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler) override;
    void setVideoFullscreenFrame(FloatRect) override;

    MediaTime streamTime() const;

    AudioSourceProvider* audioSourceProvider() final;

    CGAffineTransform videoTransformationMatrix(MediaSample&, bool forceUpdate = false);

    void applicationDidBecomeActive() final;

    bool hideBackgroundLayer() const { return (!m_activeVideoTrack || m_waitingForFirstImage) && m_displayMode != PaintItBlack; }

    MediaPlayer* m_player { nullptr };
    RefPtr<MediaStreamPrivate> m_mediaStreamPrivate;
    RefPtr<MediaStreamTrackPrivate> m_activeVideoTrack;
    RetainPtr<WebAVSampleBufferStatusChangeListener> m_statusChangeListener;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    RetainPtr<PlatformLayer> m_backgroundLayer;
    std::unique_ptr<PAL::Clock> m_clock;

    MediaTime m_pausedTime;

    struct CurrentFramePainter {
        CurrentFramePainter() = default;
        void reset();

        RetainPtr<CGImageRef> cgImage;
        RefPtr<MediaSample> mediaSample;
        std::unique_ptr<PixelBufferConformerCV> pixelBufferConformer;
    };
    CurrentFramePainter m_imagePainter;

    HashMap<String, RefPtr<AudioTrackPrivateMediaStreamCocoa>> m_audioTrackMap;
    HashMap<String, RefPtr<VideoTrackPrivateMediaStream>> m_videoTrackMap;
    PendingSampleQueue m_pendingVideoSampleQueue;

    MediaPlayer::NetworkState m_networkState { MediaPlayer::Empty };
    MediaPlayer::ReadyState m_readyState { MediaPlayer::HaveNothing };
    FloatSize m_intrinsicSize;
    float m_volume { 1 };
    DisplayMode m_displayMode { None };
    PlaybackState m_playbackState { PlaybackState::None };
    MediaSample::VideoRotation m_videoRotation { MediaSample::VideoRotation::None };
    CGAffineTransform m_videoTransform;
    std::unique_ptr<VideoFullscreenLayerManagerObjC> m_videoFullscreenLayerManager;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    bool m_videoMirrored { false };
    bool m_playing { false };
    bool m_muted { false };
    bool m_ended { false };
    bool m_hasEverEnqueuedVideoFrame { false };
    bool m_pendingSelectedTrackCheck { false };
    bool m_transformIsValid { false };
    bool m_visible { false };
    bool m_haveSeenMetadata { false };
    bool m_waitingForFirstImage { false };
};
    
}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaStreamAVFObjC_h
