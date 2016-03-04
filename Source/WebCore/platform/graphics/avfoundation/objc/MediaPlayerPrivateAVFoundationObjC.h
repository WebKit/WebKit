/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateAVFoundationObjC_h
#define MediaPlayerPrivateAVFoundationObjC_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "MediaPlaybackTarget.h"
#include "MediaPlayerPrivateAVFoundation.h"
#include <wtf/HashMap.h>

OBJC_CLASS AVAssetImageGenerator;
OBJC_CLASS AVAssetResourceLoadingRequest;
OBJC_CLASS AVMediaSelectionGroup;
OBJC_CLASS AVOutputContext;
OBJC_CLASS AVPlayer;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVPlayerItemLegibleOutput;
OBJC_CLASS AVPlayerItemTrack;
OBJC_CLASS AVPlayerItemVideoOutput;
OBJC_CLASS AVPlayerLayer;
OBJC_CLASS AVURLAsset;
OBJC_CLASS NSArray;
OBJC_CLASS NSURLAuthenticationChallenge;
OBJC_CLASS WebCoreAVFMovieObserver;
OBJC_CLASS WebCoreAVFPullDelegate;

typedef struct objc_object* id;

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
OBJC_CLASS WebCoreAVFLoaderDelegate;
OBJC_CLASS AVAssetResourceLoadingRequest;
#endif

typedef struct CGImage *CGImageRef;
typedef struct __CVBuffer *CVPixelBufferRef;
#if PLATFORM(IOS)
typedef struct  __CVOpenGLESTextureCache *CVOpenGLESTextureCacheRef;
#else
typedef struct __CVOpenGLTextureCache* CVOpenGLTextureCacheRef;
#endif

namespace WebCore {

class AudioSourceProviderAVFObjC;
class AudioTrackPrivateAVFObjC;
class InbandMetadataTextTrackPrivateAVF;
class InbandTextTrackPrivateAVFObjC;
class MediaSelectionGroupAVFObjC;
class PixelBufferConformerCV;
class VideoTrackPrivateAVFObjC;
class WebCoreAVFResourceLoader;
class TextureCacheCV;
class VideoTextureCopierCV;

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
class VideoFullscreenLayerManager;
#endif

class MediaPlayerPrivateAVFoundationObjC : public MediaPlayerPrivateAVFoundation {
public:
    explicit MediaPlayerPrivateAVFoundationObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateAVFoundationObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    void setAsset(RetainPtr<id>);
    void tracksChanged() override;

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    RetainPtr<AVPlayerItem> playerItem() const { return m_avPlayerItem; }
    void processCue(NSArray *, NSArray *, const MediaTime&);
    void flushCues();
#endif
    
#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    bool shouldWaitForLoadingOfResource(AVAssetResourceLoadingRequest*);
    bool shouldWaitForResponseToAuthenticationChallenge(NSURLAuthenticationChallenge*);
    void didCancelLoadingRequest(AVAssetResourceLoadingRequest*);
    void didStopLoadingRequest(AVAssetResourceLoadingRequest *);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    RetainPtr<AVAssetResourceLoadingRequest> takeRequestForKeyURI(const String&);
    void keyAdded() override;
#endif

    void playerItemStatusDidChange(int);
    void playbackLikelyToKeepUpWillChange();
    void playbackLikelyToKeepUpDidChange(bool);
    void playbackBufferEmptyWillChange();
    void playbackBufferEmptyDidChange(bool);
    void playbackBufferFullWillChange();
    void playbackBufferFullDidChange(bool);
    void loadedTimeRangesDidChange(RetainPtr<NSArray>);
    void seekableTimeRangesDidChange(RetainPtr<NSArray>);
    void tracksDidChange(RetainPtr<NSArray>);
    void hasEnabledAudioDidChange(bool);
    void presentationSizeDidChange(FloatSize);
    void durationDidChange(const MediaTime&);
    void rateDidChange(double);
    void metadataDidArrive(RetainPtr<NSArray>, const MediaTime&);
    void firstFrameAvailableDidChange(bool);
    void trackEnabledDidChange(bool);
    void canPlayFastReverseDidChange(bool);
    void canPlayFastForwardDidChange(bool);

    void setShouldBufferData(bool) override;

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    void outputMediaDataWillChange(AVPlayerItemVideoOutput*);
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void playbackTargetIsWirelessDidChange();
#endif
    
#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged() override;
    void synchronizeTextTrackState() override;
#endif
    
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

private:
    // engine support
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    static bool isAvailable();

    void cancelLoad() override;

    PlatformMedia platformMedia() const override;

    void platformSetVisible(bool) override;
    void platformPlay() override;
    void platformPause() override;
    MediaTime currentMediaTime() const override;
    void setVolume(float) override;
    void setClosedCaptionsVisible(bool) override;
    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;
    PlatformLayer* platformLayer() const override;
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*) override;
    void setVideoFullscreenFrame(FloatRect) override;
    void setVideoFullscreenGravity(MediaPlayer::VideoGravity) override;
    void setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode) override;
#endif

#if PLATFORM(IOS)
    NSArray *timedMetadata() const override;
    String accessLog() const override;
    String errorLog() const override;
#endif

    bool supportsAcceleratedRendering() const override { return true; }
    MediaTime mediaTimeForTimeValue(const MediaTime&) const override;
    double maximumDurationToCacheMediaTime() const override;

    void createAVPlayer() override;
    void createAVPlayerItem() override;
    virtual void createAVPlayerLayer();
    void createAVAssetForURL(const String& url) override;
    MediaPlayerPrivateAVFoundation::ItemStatus playerItemStatus() const override;
    MediaPlayerPrivateAVFoundation::AssetStatus assetStatus() const override;
    long assetErrorCode() const override;

    void checkPlayability() override;
    void setRateDouble(double) override;
    double rate() const override;
    void setPreservesPitch(bool) override;
    void seekToTime(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance) override;
    unsigned long long totalBytes() const override;
    std::unique_ptr<PlatformTimeRanges> platformBufferedTimeRanges() const override;
    MediaTime platformMinTimeSeekable() const override;
    MediaTime platformMaxTimeSeekable() const override;
    MediaTime platformDuration() const override;
    MediaTime platformMaxTimeLoaded() const override;
    void beginLoadingMetadata() override;
    void sizeChanged() override;

    bool hasAvailableVideoFrame() const override;

    void createContextVideoRenderer() override;
    void destroyContextVideoRenderer() override;

    void createVideoLayer() override;
    void destroyVideoLayer() override;

    bool hasContextRenderer() const override;
    bool hasLayerRenderer() const override;

    void updateVideoLayerGravity() override;

    bool hasSingleSecurityOrigin() const override;
    bool didPassCORSAccessCheck() const override;

    MediaTime getStartDate() const override;

#if ENABLE(VIDEO_TRACK)
    bool requiresTextTrackRepresentation() const override;
    void setTextTrackRepresentation(TextTrackRepresentation*) override;
    void syncTextTrackBounds() override;
#endif

    void setAVPlayerItem(AVPlayerItem *);

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    AudioSourceProvider* audioSourceProvider() override;
#endif

    void createImageGenerator();
    void destroyImageGenerator();
    RetainPtr<CGImageRef> createImageForTimeInRect(float, const FloatRect&);
    void paintWithImageGenerator(GraphicsContext&, const FloatRect&);

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    void createVideoOutput();
    void destroyVideoOutput();
    RetainPtr<CVPixelBufferRef> createPixelBuffer();
    void updateLastImage();
    bool videoOutputHasAvailableFrame();
    void paintWithVideoOutput(GraphicsContext&, const FloatRect&);
    PassNativeImagePtr nativeImageForCurrentTime() override;
    void waitForVideoOutputMediaDataWillChange();

    void createOpenGLVideoOutput();
    void destroyOpenGLVideoOutput();
    void updateLastOpenGLImage();

    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject, GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY) override;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    MediaPlayer::MediaKeyException addKey(const String&, const unsigned char*, unsigned, const unsigned char*, unsigned, const String&) override;
    MediaPlayer::MediaKeyException generateKeyRequest(const String&, const unsigned char*, unsigned) override;
    MediaPlayer::MediaKeyException cancelKeyRequest(const String&, const String&) override;
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    std::unique_ptr<CDMSession> createSession(const String& keySystem, CDMSessionClient*) override;
#endif

    String languageOfPrimaryAudioTrack() const override;

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    void processMediaSelectionOptions();
    bool hasLoadedMediaSelectionGroups();

    AVMediaSelectionGroup* safeMediaSelectionGroupForLegibleMedia();
    AVMediaSelectionGroup* safeMediaSelectionGroupForAudibleMedia();
    AVMediaSelectionGroup* safeMediaSelectionGroupForVisualMedia();
#endif

    NSArray* safeAVAssetTracksForAudibleMedia();

#if ENABLE(DATACUE_VALUE)
    void processMetadataTrack();
#endif

    void setCurrentTextTrack(InbandTextTrackPrivateAVF*) override;
    InbandTextTrackPrivateAVF* currentTextTrack() const override { return m_currentTextTrack; }

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    void processLegacyClosedCaptionsTracks();
#endif

#if ENABLE(VIDEO_TRACK)
    void updateAudioTracks();
    void updateVideoTracks();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const override;
    String wirelessPlaybackTargetName() const override;
    MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const override;
    bool wirelessVideoPlaybackDisabled() const override;
    void setWirelessVideoPlaybackDisabled(bool) override;
    bool canPlayToWirelessPlaybackTarget() const override { return true; }
    void updateDisableExternalPlayback();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
#endif

    double maxFastForwardRate() const override { return m_cachedCanPlayFastForward ? std::numeric_limits<double>::infinity() : 2.0; }
    double minFastReverseRate() const override { return m_cachedCanPlayFastReverse ? -std::numeric_limits<double>::infinity() : 0.0; }

    URL resolvedURL() const override;

    Vector<String> preferredAudioCharacteristics() const;

    WeakPtrFactory<MediaPlayerPrivateAVFoundationObjC> m_weakPtrFactory;

    RetainPtr<AVURLAsset> m_avAsset;
    RetainPtr<AVPlayer> m_avPlayer;
    RetainPtr<AVPlayerItem> m_avPlayerItem;
    RetainPtr<AVPlayerLayer> m_videoLayer;
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    std::unique_ptr<VideoFullscreenLayerManager> m_videoFullscreenLayerManager;
    MediaPlayer::VideoGravity m_videoFullscreenGravity;
    RetainPtr<PlatformLayer> m_textTrackRepresentationLayer;
#endif
    RetainPtr<WebCoreAVFMovieObserver> m_objcObserver;
    RetainPtr<id> m_timeObserver;
    mutable String m_languageOfPrimaryAudioTrack;
    bool m_videoFrameHasDrawn;
    bool m_haveCheckedPlayability;

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)
    RefPtr<AudioSourceProviderAVFObjC> m_provider;
#endif

    RetainPtr<AVAssetImageGenerator> m_imageGenerator;
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    RetainPtr<AVPlayerItemVideoOutput> m_videoOutput;
    RetainPtr<WebCoreAVFPullDelegate> m_videoOutputDelegate;
    RetainPtr<CGImageRef> m_lastImage;
    dispatch_semaphore_t m_videoOutputSemaphore;

    RetainPtr<AVPlayerItemVideoOutput> m_openGLVideoOutput;
    std::unique_ptr<TextureCacheCV> m_textureCache;
    std::unique_ptr<VideoTextureCopierCV> m_videoTextureCopier;
    RetainPtr<CVPixelBufferRef> m_lastOpenGLImage;
#endif

    std::unique_ptr<PixelBufferConformerCV> m_pixelBufferConformer;

#if HAVE(AVFOUNDATION_LOADER_DELEGATE)
    friend class WebCoreAVFResourceLoader;
    HashMap<RetainPtr<AVAssetResourceLoadingRequest>, RefPtr<WebCoreAVFResourceLoader>> m_resourceLoaderMap;
    RetainPtr<WebCoreAVFLoaderDelegate> m_loaderDelegate;
    HashMap<String, RetainPtr<AVAssetResourceLoadingRequest>> m_keyURIToRequestMap;
    HashMap<String, RetainPtr<AVAssetResourceLoadingRequest>> m_sessionIDToRequestMap;
#endif

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP) && HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    RetainPtr<AVPlayerItemLegibleOutput> m_legibleOutput;
#endif

#if ENABLE(VIDEO_TRACK)
    Vector<RefPtr<AudioTrackPrivateAVFObjC>> m_audioTracks;
    Vector<RefPtr<VideoTrackPrivateAVFObjC>> m_videoTracks;
#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    RefPtr<MediaSelectionGroupAVFObjC> m_audibleGroup;
    RefPtr<MediaSelectionGroupAVFObjC> m_visualGroup;
#endif
#endif

    InbandTextTrackPrivateAVF* m_currentTextTrack;

#if ENABLE(DATACUE_VALUE)
    RefPtr<InbandMetadataTextTrackPrivateAVF> m_metadataTrack;
#endif

#if PLATFORM(MAC) && ENABLE(WIRELESS_PLAYBACK_TARGET)
    RetainPtr<AVOutputContext> m_outputContext;
    RefPtr<MediaPlaybackTarget> m_playbackTarget { nullptr };
#endif

    mutable RetainPtr<NSArray> m_cachedSeekableRanges;
    mutable RetainPtr<NSArray> m_cachedLoadedRanges;
    RetainPtr<NSArray> m_cachedTracks;
    RetainPtr<NSArray> m_currentMetaData;
    FloatSize m_cachedPresentationSize;
    MediaTime m_cachedDuration;
    double m_cachedRate;
    mutable long long m_cachedTotalBytes;
    unsigned m_pendingStatusChanges;
    int m_cachedItemStatus;
    bool m_cachedLikelyToKeepUp;
    bool m_cachedBufferEmpty;
    bool m_cachedBufferFull;
    bool m_cachedHasEnabledAudio;
    bool m_shouldBufferData;
    bool m_cachedIsReadyForDisplay;
    bool m_haveBeenAskedToCreateLayer;
    bool m_cachedCanPlayFastForward;
    bool m_cachedCanPlayFastReverse;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    mutable bool m_allowsWirelessVideoPlayback;
    bool m_shouldPlayToPlaybackTarget { false };
#endif
};

}

#endif
#endif
