/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateMediaSourceAVFObjC_h
#define MediaPlayerPrivateMediaSourceAVFObjC_h

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "MediaPlayerPrivate.h"
#include "SourceBufferPrivateClient.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVStreamSession;

typedef struct OpaqueCMTimebase* CMTimebaseRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVBuffer *CVOpenGLTextureRef;

namespace WebCore {

class CDMSessionMediaSourceAVFObjC;
class MediaSourcePrivateAVFObjC;
class PixelBufferConformerCV;
class PlatformClockCM;
class TextureCacheCV;
class VideoFullscreenLayerManagerObjC;
class VideoTextureCopierCV;
class WebCoreDecompressionSession;


class MediaPlayerPrivateMediaSourceAVFObjC
    : public MediaPlayerPrivateInterface
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    explicit MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaSourceAVFObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void addAudioRenderer(AVSampleBufferAudioRenderer*);
    void removeAudioRenderer(AVSampleBufferAudioRenderer*);
    ALLOW_NEW_API_WITHOUT_GUARDS_END

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal();
    void waitForSeekCompleted();
    void seekCompleted();
    void setLoadingProgresssed(bool flag) { m_loadingProgressed = flag; }
    void setHasAvailableVideoFrame(bool);
    bool hasAvailableVideoFrame() const override;
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    void setHasAvailableAudioSample(AVSampleBufferAudioRenderer*, bool);
    ALLOW_NEW_API_WITHOUT_GUARDS_END
    bool allRenderersHaveAvailableSamples() const { return m_allRenderersHaveAvailableSamples; }
    void updateAllRenderersHaveAvailableSamples();
    void durationChanged();

    void effectiveRateChanged();
    void sizeWillChangeAtTime(const MediaTime&, const FloatSize&);
    void setNaturalSize(const FloatSize&);
    void flushPendingSizeChanges();
    void characteristicsChanged();

    MediaTime currentMediaTime() const override;
    AVSampleBufferDisplayLayer* sampleBufferDisplayLayer() const { return m_sampleBufferDisplayLayer.get(); }
    WebCoreDecompressionSession* decompressionSession() const { return m_decompressionSession.get(); }

    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler) override;
    void setVideoFullscreenFrame(FloatRect) override;

    bool requiresTextTrackRepresentation() const override;
    void setTextTrackRepresentation(TextTrackRepresentation*) override;
    void syncTextTrackBounds() override;
    
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#if HAVE(AVSTREAMSESSION)
    bool hasStreamSession() { return m_streamSession; }
    AVStreamSession *streamSession();
#endif
    void setCDMSession(LegacyCDMSession*) override;
    CDMSessionMediaSourceAVFObjC* cdmSession() const { return m_session.get(); }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&) final;
    void cdmInstanceDetached(CDMInstance&) final;
    void attemptToDecryptWithInstance(CDMInstance&) final;
    bool waitingForKey() const final;
    void waitingForKeyChanged();
#endif

    void outputObscuredDueToInsufficientExternalProtectionChanged(bool);
    void beginSimulatedHDCPError() override { outputObscuredDueToInsufficientExternalProtectionChanged(true); }
    void endSimulatedHDCPError() override { outputObscuredDueToInsufficientExternalProtectionChanged(false); }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    void keyNeeded(Uint8Array*);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void initializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&);
#endif

    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const;
    bool shouldCheckHardwareSupport() const;

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(*this); }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const override { return "MediaPlayerPrivateMediaSourceAVFObjC"; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;

    const void* mediaPlayerLogIdentifier() { return logIdentifier(); }
    const Logger& mediaPlayerLogger() { return logger(); }
#endif

    enum SeekState {
        Seeking,
        WaitingForAvailableFame,
        SeekCompleted,
    };

private:
    // MediaPlayerPrivateInterface
    void load(const String& url) override;
    void load(const String& url, MediaSourcePrivateClient*) override;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformLayer* platformLayer() const override;

    bool supportsPictureInPicture() const override { return true; }
    bool supportsFullscreen() const override { return true; }

    void play() override;
    void playInternal();

    void pause() override;
    void pauseInternal();

    bool paused() const override;

    void setVolume(float volume) override;
    bool supportsMuting() const override { return true; }
    void setMuted(bool) override;

    bool supportsScanning() const override;

    FloatSize naturalSize() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setVisible(bool) override;

    MediaTime durationMediaTime() const override;
    MediaTime startTime() const override;
    MediaTime initialTime() const override;

    void seekWithTolerance(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold) override;
    bool seeking() const override;
    void setRateDouble(double) override;

    void setPreservesPitch(bool) override;

    std::unique_ptr<PlatformTimeRanges> seekable() const override;
    MediaTime maxMediaTimeSeekable() const override;
    MediaTime minMediaTimeSeekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;

    bool didLoadingProgress() const override;

    void setSize(const IntSize&) override;

    NativeImagePtr nativeImageForCurrentTime() override;
    bool updateLastPixelBuffer();
    bool updateLastImage();
    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY) override;

    bool supportsAcceleratedRendering() const override;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged() override;
    void notifyActiveSourceBuffersChanged() override;

    // NOTE: Because the only way for MSE to recieve data is through an ArrayBuffer provided by
    // javascript running in the page, the video will, by necessity, always be CORS correct and
    // in the page's origin.
    bool hasSingleSecurityOrigin() const override { return true; }
    bool didPassCORSAccessCheck() const override { return true; }

    MediaPlayer::MovieLoadType movieLoadType() const override;

    void prepareForRendering() override;

    String engineDescription() const override;

    String languageOfPrimaryAudioTrack() const override;

    size_t extraMemoryCost() const override;

    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics() override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const override;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
    bool wirelessVideoPlaybackDisabled() const override { return false; }
#endif

    bool performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime) final;

    void ensureLayer();
    void destroyLayer();
    void ensureDecompressionSession();
    void destroyDecompressionSession();

    bool shouldBePlaying() const;

    friend class MediaSourcePrivateAVFObjC;

    struct PendingSeek {
        PendingSeek(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
            : targetTime(targetTime)
            , negativeThreshold(negativeThreshold)
            , positiveThreshold(positiveThreshold)
        {
        }
        MediaTime targetTime;
        MediaTime negativeThreshold;
        MediaTime positiveThreshold;
    };
    std::unique_ptr<PendingSeek> m_pendingSeek;

    MediaPlayer* m_player;
    WeakPtrFactory<MediaPlayerPrivateMediaSourceAVFObjC> m_weakPtrFactory;
    WeakPtrFactory<MediaPlayerPrivateMediaSourceAVFObjC> m_sizeChangeObserverWeakPtrFactory;
    RefPtr<MediaSourcePrivateAVFObjC> m_mediaSourcePrivate;
    RetainPtr<AVAsset> m_asset;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;

    struct AudioRendererProperties {
        bool hasAudibleSample { false };
    };
    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    HashMap<RetainPtr<CFTypeRef>, AudioRendererProperties> m_sampleBufferAudioRendererMap;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    ALLOW_NEW_API_WITHOUT_GUARDS_END
    RetainPtr<id> m_timeJumpedObserver;
    RetainPtr<id> m_durationObserver;
    RetainPtr<id> m_performTaskObserver;
    RetainPtr<AVStreamSession> m_streamSession;
    RetainPtr<CVPixelBufferRef> m_lastPixelBuffer;
    RetainPtr<CGImageRef> m_lastImage;
    std::unique_ptr<PixelBufferConformerCV> m_rgbConformer;
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;
    Deque<RetainPtr<id>> m_sizeChangeObservers;
    Timer m_seekTimer;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    WeakPtr<CDMSessionMediaSourceAVFObjC> m_session;
#endif
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    bool m_readyStateIsWaitingForAvailableFrame { false };
    MediaTime m_lastSeekTime;
    FloatSize m_naturalSize;
    double m_rate;
    bool m_playing;
    bool m_seeking;
    SeekState m_seekCompleted { SeekCompleted };
    mutable bool m_loadingProgressed;
    bool m_hasBeenAskedToPaintGL { false };
    bool m_hasAvailableVideoFrame { false };
    bool m_allRenderersHaveAvailableSamples { false };
    bool m_visible { false };
    std::unique_ptr<TextureCacheCV> m_textureCache;
    std::unique_ptr<VideoTextureCopierCV> m_videoTextureCopier;
    RetainPtr<CVOpenGLTextureRef> m_lastTexture;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_shouldPlayToTarget { false };
#endif
    std::unique_ptr<VideoFullscreenLayerManagerObjC> m_videoFullscreenLayerManager;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

String convertEnumerationToString(MediaPlayerPrivateMediaSourceAVFObjC::SeekState);

}

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::MediaPlayerPrivateMediaSourceAVFObjC::SeekState> {
    static String toString(const WebCore::MediaPlayerPrivateMediaSourceAVFObjC::SeekState state)
    {
        return convertEnumerationToString(state);
    }
};

} // namespace WTF

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaSourceAVFObjC_h

