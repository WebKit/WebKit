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

#include "MediaPlayerPrivateAVFoundation.h"
#include <wtf/HashMap.h>

OBJC_CLASS AVAssetImageGenerator;
OBJC_CLASS AVAssetResourceLoadingRequest;
OBJC_CLASS AVMediaSelectionGroup;
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
typedef struct OpaqueVTPixelTransferSession* VTPixelTransferSessionRef;

namespace WebCore {

class WebCoreAVFResourceLoader;
class InbandMetadataTextTrackPrivateAVF;
class InbandTextTrackPrivateAVFObjC;
class AudioTrackPrivateAVFObjC;
class VideoTrackPrivateAVFObjC;

class MediaPlayerPrivateAVFoundationObjC : public MediaPlayerPrivateAVFoundation {
public:
    virtual ~MediaPlayerPrivateAVFoundationObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    void setAsset(id);
    virtual void tracksChanged() override;

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

    virtual void setShouldBufferData(bool);

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    void outputMediaDataWillChange(AVPlayerItemVideoOutput*);
#endif

#if ENABLE(IOS_AIRPLAY)
    void playbackTargetIsWirelessDidChange();
#endif
    
#if ENABLE(AVF_CAPTIONS)
    virtual void notifyTrackModeChanged() override;
    virtual void synchronizeTextTrackState() override;
#endif
    
    WeakPtr<MediaPlayerPrivateAVFoundationObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

private:
    MediaPlayerPrivateAVFoundationObjC(MediaPlayer*);

    // engine support
    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    static bool isAvailable();

    virtual void cancelLoad();

    virtual PlatformMedia platformMedia() const;

    virtual void platformSetVisible(bool);
    virtual void platformPlay();
    virtual void platformPause();
    virtual MediaTime currentMediaTime() const override;
    virtual void setVolume(float);
    virtual void setClosedCaptionsVisible(bool);
    virtual void paint(GraphicsContext*, const FloatRect&);
    virtual void paintCurrentFrameInContext(GraphicsContext*, const FloatRect&);
    virtual PlatformLayer* platformLayer() const;
#if PLATFORM(IOS)
    virtual void setVideoFullscreenLayer(PlatformLayer*);
    virtual void setVideoFullscreenFrame(FloatRect);
    virtual void setVideoFullscreenGravity(MediaPlayer::VideoGravity);

    virtual NSArray *timedMetadata() const override;
    virtual String accessLog() const;
    virtual String errorLog() const;
#endif

    virtual bool supportsAcceleratedRendering() const { return true; }
    virtual MediaTime mediaTimeForTimeValue(const MediaTime&) const;
    virtual double maximumDurationToCacheMediaTime() const { return 5; }

    virtual void createAVPlayer();
    virtual void createAVPlayerItem();
    virtual void createAVPlayerLayer();
    virtual void createAVAssetForURL(const String& url);
    virtual MediaPlayerPrivateAVFoundation::ItemStatus playerItemStatus() const;
    virtual MediaPlayerPrivateAVFoundation::AssetStatus assetStatus() const;

    virtual void checkPlayability();
    virtual void updateRate();
    virtual float rate() const;
    virtual void seekToTime(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance);
    virtual unsigned long long totalBytes() const;
    virtual std::unique_ptr<PlatformTimeRanges> platformBufferedTimeRanges() const;
    virtual MediaTime platformMinTimeSeekable() const;
    virtual MediaTime platformMaxTimeSeekable() const;
    virtual MediaTime platformDuration() const;
    virtual MediaTime platformMaxTimeLoaded() const;
    virtual void beginLoadingMetadata();
    virtual void sizeChanged();

    virtual bool hasAvailableVideoFrame() const;

    virtual void createContextVideoRenderer();
    virtual void destroyContextVideoRenderer();

    virtual void createVideoLayer();
    virtual void destroyVideoLayer();

    virtual bool hasContextRenderer() const;
    virtual bool hasLayerRenderer() const;

    virtual void updateVideoLayerGravity() override;

    virtual bool hasSingleSecurityOrigin() const;

#if ENABLE(VIDEO_TRACK)
    virtual bool requiresTextTrackRepresentation() const override;
    virtual void setTextTrackRepresentation(TextTrackRepresentation*) override;
    virtual void syncTextTrackBounds() override;
#endif

    void setAVPlayerItem(AVPlayerItem *);

    void createImageGenerator();
    void destroyImageGenerator();
    RetainPtr<CGImageRef> createImageForTimeInRect(float, const FloatRect&);
    void paintWithImageGenerator(GraphicsContext*, const FloatRect&);

#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    void createVideoOutput();
    void destroyVideoOutput();
    RetainPtr<CVPixelBufferRef> createPixelBuffer();
    void updateLastImage();
    bool videoOutputHasAvailableFrame();
    void paintWithVideoOutput(GraphicsContext*, const FloatRect&);
    virtual PassNativeImagePtr nativeImageForCurrentTime() override;
    void waitForVideoOutputMediaDataWillChange();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    virtual MediaPlayer::MediaKeyException addKey(const String&, const unsigned char*, unsigned, const unsigned char*, unsigned, const String&);
    virtual MediaPlayer::MediaKeyException generateKeyRequest(const String&, const unsigned char*, unsigned);
    virtual MediaPlayer::MediaKeyException cancelKeyRequest(const String&, const String&);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    std::unique_ptr<CDMSession> createSession(const String& keySystem);
#endif

    virtual String languageOfPrimaryAudioTrack() const override;

#if HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)
    void processMediaSelectionOptions();
    AVMediaSelectionGroup* safeMediaSelectionGroupForLegibleMedia();
#endif

#if ENABLE(DATACUE_VALUE)
    void processMetadataTrack();
#endif

    virtual void setCurrentTextTrack(InbandTextTrackPrivateAVF*) override;
    virtual InbandTextTrackPrivateAVF* currentTextTrack() const override { return m_currentTextTrack; }

#if !HAVE(AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT)
    void processLegacyClosedCaptionsTracks();
#endif

#if ENABLE(VIDEO_TRACK)
    void updateAudioTracks();
    void updateVideoTracks();
#endif

#if ENABLE(IOS_AIRPLAY)
    virtual bool isCurrentPlaybackTargetWireless() const override;
    virtual String wirelessPlaybackTargetName() const override;
    virtual MediaPlayer::WirelessPlaybackTargetType wirelessPlaybackTargetType() const override;
    virtual bool wirelessVideoPlaybackDisabled() const override;
    virtual void setWirelessVideoPlaybackDisabled(bool) override;
    void updateDisableExternalPlayback();
#endif

    virtual double maxFastForwardRate() const override { return m_cachedCanPlayFastForward ? std::numeric_limits<double>::infinity() : 2.0; }
    virtual double minFastReverseRate() const override { return m_cachedCanPlayFastReverse ? -std::numeric_limits<double>::infinity() : 0.0; }

    virtual URL resolvedURL() const override;

    WeakPtrFactory<MediaPlayerPrivateAVFoundationObjC> m_weakPtrFactory;

    RetainPtr<AVURLAsset> m_avAsset;
    RetainPtr<AVPlayer> m_avPlayer;
    RetainPtr<AVPlayerItem> m_avPlayerItem;
    RetainPtr<AVPlayerLayer> m_videoLayer;
#if PLATFORM(IOS)
    RetainPtr<PlatformLayer> m_videoInlineLayer;
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    FloatRect m_videoFullscreenFrame;
    MediaPlayer::VideoGravity m_videoFullscreenGravity;
    RetainPtr<PlatformLayer> m_textTrackRepresentationLayer;
#endif
    RetainPtr<WebCoreAVFMovieObserver> m_objcObserver;
    RetainPtr<id> m_timeObserver;
    mutable String m_languageOfPrimaryAudioTrack;
    bool m_videoFrameHasDrawn;
    bool m_haveCheckedPlayability;

    RetainPtr<AVAssetImageGenerator> m_imageGenerator;
#if HAVE(AVFOUNDATION_VIDEO_OUTPUT)
    RetainPtr<AVPlayerItemVideoOutput> m_videoOutput;
    RetainPtr<WebCoreAVFPullDelegate> m_videoOutputDelegate;
    RetainPtr<CGImageRef> m_lastImage;
    dispatch_semaphore_t m_videoOutputSemaphore;
#endif

#if USE(VIDEOTOOLBOX)
    RetainPtr<VTPixelTransferSessionRef> m_pixelTransferSession;
#endif

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
#endif

    InbandTextTrackPrivateAVF* m_currentTextTrack;

#if ENABLE(DATACUE_VALUE)
    RefPtr<InbandMetadataTextTrackPrivateAVF> m_metadataTrack;
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
#if ENABLE(IOS_AIRPLAY)
    mutable bool m_allowsWirelessVideoPlayback;
#endif
};

}

#endif
#endif
