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
#include "MediaStreamPrivateAVFObjC.h"
#include "SourceBufferPrivateClient.h"
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVCaptureVideoPreviewLayer;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVStreamSession;

typedef struct OpaqueCMTimebase* CMTimebaseRef;

namespace WebCore {

class MediaPlayerPrivateMediaStreamAVFObjC : public MediaPlayerPrivateInterface {
public:
    explicit MediaPlayerPrivateMediaStreamAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaStreamAVFObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void addDisplayLayer(AVSampleBufferDisplayLayer*);
    void removeDisplayLayer(AVSampleBufferDisplayLayer*);

    void addAudioRenderer(AVSampleBufferAudioRenderer*);
    void removeAudioRenderer(AVSampleBufferAudioRenderer*);

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);

    void sizeChanged();
    void characteristicsChanged();

    WeakPtr<MediaPlayerPrivateMediaStreamAVFObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

private:
        // MediaPlayerPrivateInterface
        // FIXME(146853): Implement necessary conformations to standard in HTMLMediaElement for MediaStream 
    void load(const String&) override { };
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) override { };
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformMedia platformMedia() const override;
    PlatformLayer* platformLayer() const override;

    bool supportsFullscreen() const override { return true; }

    void play() override;
    void playInternal();

    void pause() override;
    void pauseInternal();

    bool paused() const override;

    void setVolume(float) override;
    bool supportsMuting() const override { return true; }
    void setMuted(bool) override;

    bool supportsScanning() const override;

    FloatSize naturalSize() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setVisible(bool) override;

    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override;

    void seekWithTolerance(const MediaTime&, const MediaTime&, const MediaTime&) override { };
    bool seeking() const override;
    void setRateDouble(double) override;

    std::unique_ptr<PlatformTimeRanges> seekable() const override;
    MediaTime maxMediaTimeSeekable() const override;
    MediaTime minMediaTimeSeekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;

    bool didLoadingProgress() const override;

    void setSize(const IntSize&) override;

    void paint(GraphicsContext*, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext*, const FloatRect&) override;

    bool supportsAcceleratedRendering() const override;

    MediaPlayer::MovieLoadType movieLoadType() const override;

    void prepareForRendering() override;

    String engineDescription() const override;

    String languageOfPrimaryAudioTrack() const override;

    size_t extraMemoryCost() const override;

    bool shouldBePlaying() const;

    RetainPtr<CGImageRef> createImageFromSampleBuffer(CMSampleBufferRef);

    friend class MediaStreamPrivateAVFObjC;

    MediaPlayer* m_player;
    WeakPtrFactory<MediaPlayerPrivateMediaStreamAVFObjC> m_weakPtrFactory;
    RefPtr<MediaStreamPrivateAVFObjC> m_MediaStreamPrivate;
    RetainPtr<AVAsset> m_asset;
    RetainPtr<AVCaptureVideoPreviewLayer> m_previewLayer;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> m_sampleBufferAudioRenderers;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    RetainPtr<CGImageRef> m_lastImage;
    RetainPtr<id> m_timeJumpedObserver;
    RetainPtr<id> m_durationObserver;
    RetainPtr<AVStreamSession> m_streamSession;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    MediaTime m_lastSeekTime;
    double m_rate;
    bool m_playing;
    bool m_seeking;
    bool m_seekCompleted;
    mutable bool m_loadingProgressed;
};
    
}

#endif // ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaStreamAVFObjC_h
