/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO)

#include "ContentType.h"
#include "Cookie.h"
#include "FourCC.h"
#include "GraphicsTypesGL.h"
#include "LayoutRect.h"
#include "MediaPlayerEnums.h"
#include "MediaPlayerIdentifier.h"
#include "PlatformLayer.h"
#include "PlatformTextTrack.h"
#include "SecurityOriginData.h"
#include "Timer.h"
#include "VideoFrame.h"
#include "VideoFrameMetadata.h"
#include "VideoPlaybackQualityMetrics.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/MediaTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/StringHash.h>

OBJC_CLASS AVPlayer;
OBJC_CLASS NSArray;

#if USE(AVFOUNDATION)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

enum class AudioSessionCategory : uint8_t;
enum class DynamicRangeMode : uint8_t;

class AudioSourceProvider;
class AudioTrackPrivate;
class CDMInstance;
class CachedResourceLoader;
class DestinationColorSpace;
class GraphicsContextGL;
class GraphicsContext;
class InbandTextTrackPrivate;
class LegacyCDM;
class LegacyCDMSession;
class LegacyCDMSessionClient;
class MediaPlaybackTarget;
class MediaPlayer;
class MediaPlayerFactory;
class MediaPlayerPrivateInterface;
class MediaSourcePrivateClient;
class MediaStreamPrivate;
class NativeImage;
class PlatformMediaResourceLoader;
class PlatformTimeRanges;
class SharedBuffer;
class TextTrackRepresentation;
class VideoTrackPrivate;

struct GraphicsDeviceAdapter;
struct SecurityOriginData;

struct MediaEngineSupportParameters {
    ContentType type;
    URL url;
    bool isMediaSource { false };
    bool isMediaStream { false };
    bool requiresRemotePlayback { false };
    Vector<ContentType> contentTypesRequiringHardwareSupport;
    std::optional<Vector<String>> allowedMediaContainerTypes;
    std::optional<Vector<String>> allowedMediaCodecTypes;
    std::optional<Vector<FourCC>> allowedMediaVideoCodecIDs;
    std::optional<Vector<FourCC>> allowedMediaAudioCodecIDs;
    std::optional<Vector<FourCC>> allowedMediaCaptionFormatTypes;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << type;
        encoder << url;
        encoder << isMediaSource;
        encoder << isMediaStream;
        encoder << requiresRemotePlayback;
        encoder << contentTypesRequiringHardwareSupport;
        encoder << allowedMediaContainerTypes;
        encoder << allowedMediaCodecTypes;
        encoder << allowedMediaVideoCodecIDs;
        encoder << allowedMediaAudioCodecIDs;
        encoder << allowedMediaCaptionFormatTypes;
    }

    template <class Decoder>
    static bool decode(Decoder& decoder, MediaEngineSupportParameters& parameters)
    {
        return decoder.decode(parameters.type)
            && decoder.decode(parameters.url)
            && decoder.decode(parameters.isMediaSource)
            && decoder.decode(parameters.isMediaStream)
            && decoder.decode(parameters.requiresRemotePlayback)
            && decoder.decode(parameters.contentTypesRequiringHardwareSupport)
            && decoder.decode(parameters.allowedMediaContainerTypes)
            && decoder.decode(parameters.allowedMediaCodecTypes)
            && decoder.decode(parameters.allowedMediaVideoCodecIDs)
            && decoder.decode(parameters.allowedMediaAudioCodecIDs)
            && decoder.decode(parameters.allowedMediaCaptionFormatTypes);
    }
};

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() = default;

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged() { }

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged() { }

    // the volume state has changed
    virtual void mediaPlayerVolumeChanged() { }

    // the mute state has changed
    virtual void mediaPlayerMuteChanged() { }

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged() { }

    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged() { }

    // the playback rate has changed
    virtual void mediaPlayerRateChanged() { }

    // the play/pause status changed
    virtual void mediaPlayerPlaybackStateChanged() { }

    // The MediaPlayer could not discover an engine which supports the requested resource.
    virtual void mediaPlayerResourceNotSupported() { }

// Presentation-related methods
    // a new frame of video is available
    virtual void mediaPlayerRepaint() { }

    // the movie size has changed
    virtual void mediaPlayerSizeChanged() { }

    virtual void mediaPlayerEngineUpdated() { }

    // The first frame of video is available to render. A media engine need only make this callback if the
    // first frame is not available immediately when prepareForRendering is called.
    virtual void mediaPlayerFirstVideoFrameAvailable() { }

    // A characteristic of the media file, eg. video, audio, closed captions, etc, has changed.
    virtual void mediaPlayerCharacteristicChanged() { }
    
    // whether the rendering system can accelerate the display of this MediaPlayer.
    virtual bool mediaPlayerRenderingCanBeAccelerated() { return false; }

    // called when the media player's rendering mode changed, which indicates a change in the
    // availability of the platformLayer().
    virtual void mediaPlayerRenderingModeChanged() { }

    // whether accelerated compositing is enabled for video rendering
    virtual bool mediaPlayerAcceleratedCompositingEnabled() { return false; }

    virtual void mediaPlayerActiveSourceBuffersChanged() { }

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter() const { return nullptr; }
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    virtual RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const = 0;
    virtual void mediaPlayerKeyNeeded(const SharedBuffer&) { }
    virtual String mediaPlayerMediaKeysStorageDirectory() const { return emptyString(); }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    virtual void mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&) { }
    virtual void mediaPlayerWaitingForKeyChanged() { }
#endif
    
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool) { };
#endif

    virtual void mediaPlayerWillInitializeMediaEngine() { }
    virtual void mediaPlayerDidInitializeMediaEngine() { }

    virtual String mediaPlayerReferrer() const { return String(); }
    virtual String mediaPlayerUserAgent() const { return String(); }
    virtual bool mediaPlayerIsFullscreen() const { return false; }
    virtual bool mediaPlayerIsFullscreenPermitted() const { return false; }
    virtual bool mediaPlayerIsVideo() const { return false; }
    virtual LayoutRect mediaPlayerContentBoxRect() const { return LayoutRect(); }
    virtual float mediaPlayerContentsScale() const { return 1; }
    virtual bool mediaPlayerPlatformVolumeConfigurationRequired() const { return false; }
    virtual bool mediaPlayerIsLooping() const { return false; }
    virtual CachedResourceLoader* mediaPlayerCachedResourceLoader() { return nullptr; }
    virtual RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() = 0;
    virtual bool doesHaveAttribute(const AtomString&, AtomString* = nullptr) const { return false; }
    virtual bool mediaPlayerShouldUsePersistentCache() const { return true; }
    virtual const String& mediaPlayerMediaCacheDirectory() const { return emptyString(); }

    virtual void mediaPlayerDidAddAudioTrack(AudioTrackPrivate&) { }
    virtual void mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&) { }
    virtual void mediaPlayerDidAddVideoTrack(VideoTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate&) { }

    virtual void mediaPlayerReloadAndResumePlaybackIfNeeded() { }

    virtual void textTrackRepresentationBoundsChanged(const IntRect&) { }

    virtual Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() { return { }; }

#if PLATFORM(IOS_FAMILY)
    virtual String mediaPlayerNetworkInterfaceName() const { return String(); }

    using GetRawCookiesCallback = CompletionHandler<void(Vector<Cookie>&&)>;
    virtual void mediaPlayerGetRawCookies(const URL&, GetRawCookiesCallback&& completionHandler) const { completionHandler({ }); }
#endif

    virtual String mediaPlayerSourceApplicationIdentifier() const { return emptyString(); }

    virtual String mediaPlayerElementId() const { return emptyString(); }

    virtual void mediaPlayerEngineFailedToLoad() const { }

    virtual double mediaPlayerRequestedPlaybackRate() const { return 0; }
    virtual MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const { return MediaPlayerEnums::VideoFullscreenModeNone; }
    virtual bool mediaPlayerIsVideoFullscreenStandby() const { return false; }
    virtual Vector<String> mediaPlayerPreferredAudioCharacteristics() const { return Vector<String>(); }

    virtual bool mediaPlayerShouldDisableSleep() const { return false; }
    virtual const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const = 0;
    virtual bool mediaPlayerShouldCheckHardwareSupport() const { return false; }

    virtual const std::optional<Vector<String>>& allowedMediaContainerTypes() const = 0;
    virtual const std::optional<Vector<String>>& allowedMediaCodecTypes() const = 0;
    virtual const std::optional<Vector<FourCC>>& allowedMediaVideoCodecIDs() const = 0;
    virtual const std::optional<Vector<FourCC>>& allowedMediaAudioCodecIDs() const = 0;
    virtual const std::optional<Vector<FourCC>>& allowedMediaCaptionFormatTypes() const = 0;

    virtual void mediaPlayerBufferedTimeRangesChanged() { }
    virtual void mediaPlayerSeekableTimeRangesChanged() { }

    virtual SecurityOriginData documentSecurityOrigin() const { return { }; }

    virtual String audioOutputDeviceId() const { return { }; }
    virtual String audioOutputDeviceIdOverride() const { return { }; }

    virtual void mediaPlayerQueueTaskOnEventLoop(Function<void()>&& task) { callOnMainThread(WTFMove(task)); }

#if PLATFORM(COCOA)
    virtual void mediaPlayerOnNewVideoFrameMetadata(VideoFrameMetadata&&, RetainPtr<CVPixelBufferRef>&&) { }
#endif

    virtual bool mediaPlayerPrefersSandboxedParsing() const { return false; }

    virtual bool mediaPlayerShouldDisableHDR() const { return false; }

#if !RELEASE_LOG_DISABLED
    virtual const void* mediaPlayerLogIdentifier() { return nullptr; }
    virtual const Logger& mediaPlayerLogger() = 0;
#endif
};

class WEBCORE_EXPORT MediaPlayer : public MediaPlayerEnums, public ThreadSafeRefCounted<MediaPlayer, WTF::DestructionThread::Main>, public CanMakeWeakPtr<MediaPlayer> {
    WTF_MAKE_NONCOPYABLE(MediaPlayer); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MediaPlayer> create(MediaPlayerClient&);
    static Ref<MediaPlayer> create(MediaPlayerClient&, MediaPlayerEnums::MediaEngineIdentifier);
    virtual ~MediaPlayer();

    void invalidate();

    // Media engine support.
    using MediaPlayerEnums::SupportsType;
    static const MediaPlayerFactory* mediaEngine(MediaPlayerEnums::MediaEngineIdentifier);
    static SupportsType supportsType(const MediaEngineSupportParameters&);
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static bool isAvailable();
    static HashSet<SecurityOriginData> originsInMediaCache(const String& path);
    static void clearMediaCache(const String& path, WallTime modifiedSince);
    static void clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    bool supportsPictureInPicture() const;
    bool supportsFullscreen() const;
    bool supportsScanning() const;
    bool canSaveMediaData() const;
    bool supportsProgressMonitoring() const;
    bool requiresImmediateCompositing() const;
    bool doesHaveAttribute(const AtomString&, AtomString* value = nullptr) const;
    PlatformLayer* platformLayer() const;

    void reloadAndResumePlaybackIfNeeded();

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RetainPtr<PlatformLayer> createVideoFullscreenLayer();
    void setVideoFullscreenLayer(PlatformLayer*, Function<void()>&& completionHandler = [] { });
    void setVideoFullscreenFrame(FloatRect);
    void updateVideoFullscreenInlineImage();
    using MediaPlayerEnums::VideoGravity;
    void setVideoFullscreenGravity(VideoGravity);
    void setVideoFullscreenMode(VideoFullscreenMode);
    VideoFullscreenMode fullscreenMode() const;
    void videoFullscreenStandbyChanged();
    bool isVideoFullscreenStandby() const;
#endif

#if PLATFORM(IOS_FAMILY)
    NSArray *timedMetadata() const;
    String accessLog() const;
    String errorLog() const;
#endif

    FloatSize naturalSize();
    bool hasVideo() const;
    bool hasAudio() const;

    IntSize presentationSize() const { return m_presentationSize; }
    void setPresentationSize(const IntSize& size);

    bool load(const URL&, const ContentType&, const String&, bool);
#if ENABLE(MEDIA_SOURCE)
    bool load(const URL&, const ContentType&, MediaSourcePrivateClient&);
#endif
#if ENABLE(MEDIA_STREAM)
    bool load(MediaStreamPrivate&);
#endif
    void cancelLoad();

    void setPageIsVisible(bool);
    void setVisibleForCanvas(bool);
    bool isVisibleForCanvas() const { return m_visibleForCanvas; }

    void setVisibleInViewport(bool);
    bool isVisibleInViewport() const { return m_visibleInViewport; }

    void prepareToPlay();
    void play();
    void pause();

    using MediaPlayerEnums::BufferingPolicy;
    void setBufferingPolicy(BufferingPolicy);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    // Represents synchronous exceptions that can be thrown from the Encrypted Media methods.
    // This is different from the asynchronous MediaKeyError.
    enum MediaKeyException { NoError, InvalidPlayerState, KeySystemNotSupported };

    std::unique_ptr<LegacyCDMSession> createSession(const String& keySystem, LegacyCDMSessionClient&);
    void setCDM(LegacyCDM*);
    void setCDMSession(LegacyCDMSession*);
    void keyAdded();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&);
    void cdmInstanceDetached(CDMInstance&);
    void attemptToDecryptWithInstance(CDMInstance&);
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    void setShouldContinueAfterKeyNeeded(bool);
    bool shouldContinueAfterKeyNeeded() const { return m_shouldContinueAfterKeyNeeded; }
#endif

    void queueTaskOnEventLoop(Function<void()>&&);

    bool paused() const;
    bool seeking() const;

    static double invalidTime() { return -1.0;}
    MediaTime duration() const;
    MediaTime currentTime() const;
    void seek(const MediaTime&);
    void seekWhenPossible(const MediaTime&);
    void seekWithTolerance(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance);

    using CurrentTimeDidChangeCallback = std::function<void(const MediaTime&)>;
    bool setCurrentTimeDidChangeCallback(CurrentTimeDidChangeCallback&&);

    MediaTime startTime() const;
    MediaTime initialTime() const;

    MediaTime getStartDate() const;

    double rate() const;
    void setRate(double);
    double effectiveRate() const;

    double requestedRate() const;

    bool supportsPlayAtHostTime() const;
    bool supportsPauseAtHostTime() const;
    bool playAtHostTime(const MonotonicTime&);
    bool pauseAtHostTime(const MonotonicTime&);

    bool preservesPitch() const;
    void setPreservesPitch(bool);

    void setPitchCorrectionAlgorithm(PitchCorrectionAlgorithm);
    PitchCorrectionAlgorithm pitchCorrectionAlgorithm() const { return m_pitchCorrectionAlgorithm; }

    std::unique_ptr<PlatformTimeRanges> buffered();
    std::unique_ptr<PlatformTimeRanges> seekable();
    void bufferedTimeRangesChanged();
    void seekableTimeRangesChanged();
    MediaTime minTimeSeekable();
    MediaTime maxTimeSeekable();

    double seekableTimeRangesLastModifiedTime();
    double liveUpdateInterval();

    using DidLoadingProgressCompletionHandler = CompletionHandler<void(bool)>;
    void didLoadingProgress(DidLoadingProgressCompletionHandler&&) const;

    double volume() const;
    void setVolume(double);
    bool platformVolumeConfigurationRequired() const { return client().mediaPlayerPlatformVolumeConfigurationRequired(); }

    bool muted() const;
    void setMuted(bool);

    bool hasClosedCaptions() const;
    void setClosedCaptionsVisible(bool closedCaptionsVisible);

    void paint(GraphicsContext&, const FloatRect&);

    // copyVideoTextureToPlatformTexture() is used to do the GPU-GPU textures copy without a readback to system memory.
    // The first five parameters denote the corresponding GraphicsContext, destination texture, requested level, requested type and the required internalFormat for destination texture.
    // The last two parameters premultiplyAlpha and flipY denote whether addtional premultiplyAlpha and flip operation are required during the copy.
    // It returns true on success and false on failure.

    // In the GPU-GPU textures copy, the source texture(Video texture) should have valid target, internalFormat and size, etc.
    // The destination texture may need to be resized to to the dimensions of the source texture or re-defined to the required internalFormat.
    // The current restrictions require that format shoud be RGB or RGBA, type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
#if !USE(AVFOUNDATION)
    bool copyVideoTextureToPlatformTexture(GraphicsContextGL*, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY);
#endif

#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    void willBeAskedToPaintGL();
#endif

    RefPtr<VideoFrame> videoFrameForCurrentTime();
    RefPtr<NativeImage> nativeImageForCurrentTime();
    DestinationColorSpace colorSpace();
    bool shouldGetNativeImageForCanvasDrawing() const;

    using MediaPlayerEnums::NetworkState;
    NetworkState networkState();

    using MediaPlayerEnums::ReadyState;
    ReadyState readyState();

    using MediaPlayerEnums::MovieLoadType;
    MovieLoadType movieLoadType() const;

    using MediaPlayerEnums::Preload;
    Preload preload() const;
    void setPreload(Preload);

    void networkStateChanged();
    void readyStateChanged();
    void volumeChanged(double);
    void muteChanged(bool);
    void timeChanged();
    void sizeChanged();
    void rateChanged();
    void playbackStateChanged();
    void durationChanged();
    void firstVideoFrameAvailable();
    void characteristicChanged();

    void repaint();

    bool hasAvailableVideoFrame() const;
    void prepareForRendering();

    using MediaPlayerEnums::WirelessPlaybackTargetType;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    WirelessPlaybackTargetType wirelessPlaybackTargetType() const;

    String wirelessPlaybackTargetName() const;

    bool wirelessVideoPlaybackDisabled() const;
    void setWirelessVideoPlaybackDisabled(bool);

    void currentPlaybackTargetIsWirelessChanged(bool);
    void playbackTargetAvailabilityChanged();

    bool isCurrentPlaybackTargetWireless() const;
    bool canPlayToWirelessPlaybackTarget() const;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&);

    void setShouldPlayToPlaybackTarget(bool);
#endif

    double minFastReverseRate() const;
    double maxFastForwardRate() const;

    // whether accelerated rendering is supported by the media engine for the current media.
    bool supportsAcceleratedRendering() const;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged();

    void setShouldMaintainAspectRatio(bool);

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    GraphicsDeviceAdapter* graphicsDeviceAdapter() const;
#endif

    bool hasSingleSecurityOrigin() const;
    bool didPassCORSAccessCheck() const;
    bool wouldTaintOrigin(const SecurityOrigin&) const;

    MediaTime mediaTimeForTimeValue(const MediaTime&) const;

    double maximumDurationToCacheMediaTime() const;

    unsigned decodedFrameCount() const;
    unsigned droppedFrameCount() const;
    unsigned audioDecodedByteCount() const;
    unsigned videoDecodedByteCount() const;

    void setPrivateBrowsingMode(bool);

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider();
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> cachedKeyForKeyId(const String& keyId) const;
    void keyNeeded(const SharedBuffer& initData);
    String mediaKeysStorageDirectory() const;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void initializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&);
    void waitingForKeyChanged();
    bool waitingForKey() const;
#endif

    String referrer() const;
    String userAgent() const;

    String engineDescription() const;
    long platformErrorCode() const;

    String elementId() const;

    CachedResourceLoader* cachedResourceLoader();
    RefPtr<PlatformMediaResourceLoader> createResourceLoader();

    void addAudioTrack(AudioTrackPrivate&);
    void addTextTrack(InbandTextTrackPrivate&);
    void addVideoTrack(VideoTrackPrivate&);
    void removeAudioTrack(AudioTrackPrivate&);
    void removeTextTrack(InbandTextTrackPrivate&);
    void removeVideoTrack(VideoTrackPrivate&);

#if PLATFORM(COCOA)
    void onNewVideoFrameMetadata(VideoFrameMetadata&&, RetainPtr<CVPixelBufferRef>&&);
#endif

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
    void tracksChanged();

    void notifyTrackModeChanged();
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources();

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const;
    void getRawCookies(const URL&, MediaPlayerClient::GetRawCookiesCallback&&) const;
#endif

    static void resetMediaEngines();

#if USE(GSTREAMER)
    void simulateAudioInterruption();
#endif

    void beginSimulatedHDCPError();
    void endSimulatedHDCPError();

    String languageOfPrimaryAudioTrack() const;

    size_t extraMemoryCost() const;

    unsigned long long fileSize() const;

    std::optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics();

    String sourceApplicationIdentifier() const;
    Vector<String> preferredAudioCharacteristics() const;

    bool ended() const;

    void setShouldDisableSleep(bool);
    bool shouldDisableSleep() const;

    String contentMIMEType() const { return m_contentType.containerType(); }
    String contentTypeCodecs() const { return m_contentType.parameter(ContentType::codecsParameter()); }
    bool contentMIMETypeWasInferredFromExtension() const { return m_contentMIMETypeWasInferredFromExtension; }

    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const;
    bool shouldCheckHardwareSupport() const;

    const std::optional<Vector<String>>& allowedMediaContainerTypes() const;
    const std::optional<Vector<String>>& allowedMediaCodecTypes() const;
    const std::optional<Vector<FourCC>>& allowedMediaVideoCodecIDs() const;
    const std::optional<Vector<FourCC>>& allowedMediaAudioCodecIDs() const;
    const std::optional<Vector<FourCC>>& allowedMediaCaptionFormatTypes() const;

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger();
    const void* mediaPlayerLogIdentifier() { return client().mediaPlayerLogIdentifier(); }
#endif

    void applicationWillResignActive();
    void applicationDidBecomeActive();

#if USE(AVFOUNDATION)
    AVPlayer *objCAVFoundationAVPlayer() const;
#endif

    bool performTaskAtMediaTime(Function<void()>&&, const MediaTime&);

    bool shouldIgnoreIntrinsicSize();

    bool renderingCanBeAccelerated() const { return client().mediaPlayerRenderingCanBeAccelerated(); }
    void renderingModeChanged() const  { client().mediaPlayerRenderingModeChanged(); }
    bool acceleratedCompositingEnabled() { return client().mediaPlayerAcceleratedCompositingEnabled(); }
    void activeSourceBuffersChanged() { client().mediaPlayerActiveSourceBuffersChanged(); }
    LayoutRect playerContentBoxRect() const { return client().mediaPlayerContentBoxRect(); }
    float playerContentsScale() const { return client().mediaPlayerContentsScale(); }
    bool shouldUsePersistentCache() const { return client().mediaPlayerShouldUsePersistentCache(); }
    const String& mediaCacheDirectory() const { return client().mediaPlayerMediaCacheDirectory(); }
    bool isVideoPlayer() const { return client().mediaPlayerIsVideo(); }
    void mediaEngineUpdated() { client().mediaPlayerEngineUpdated(); }
    void resourceNotSupported() { client().mediaPlayerResourceNotSupported(); }
    bool isLooping() const { return client().mediaPlayerIsLooping(); }

    void remoteEngineFailedToLoad();
    SecurityOriginData documentSecurityOrigin() const;

    const MediaPlayerPrivateInterface* playerPrivate() const;
    MediaPlayerPrivateInterface* playerPrivate();

    DynamicRangeMode preferredDynamicRangeMode() const { return m_preferredDynamicRangeMode; }
    void setPreferredDynamicRangeMode(DynamicRangeMode);

    String audioOutputDeviceId() const;
    String audioOutputDeviceIdOverride() const;
    void audioOutputDeviceChanged();

    MediaPlayerIdentifier identifier() const;
    bool hasMediaEngine() const;

    std::optional<VideoFrameMetadata> videoFrameMetadata();
    void startVideoFrameMetadataGathering();
    void stopVideoFrameMetadataGathering();

    void playerContentBoxRectChanged(const LayoutRect&);

    String lastErrorMessage() const;

    bool prefersSandboxedParsing() const { return client().mediaPlayerPrefersSandboxedParsing(); }

    void renderVideoWillBeDestroyed();

    void setShouldDisableHDR(bool);
    bool shouldDisableHDR() const { return client().mediaPlayerShouldDisableHDR(); }

    bool requiresRemotePlayback() const { return m_requiresRemotePlayback; }

private:
    MediaPlayer(MediaPlayerClient&);
    MediaPlayer(MediaPlayerClient&, MediaPlayerEnums::MediaEngineIdentifier);

    MediaPlayerClient& client() const { return *m_client; }

    const MediaPlayerFactory* nextBestMediaEngine(const MediaPlayerFactory*);
    void loadWithNextMediaEngine(const MediaPlayerFactory*);
    const MediaPlayerFactory* nextMediaEngine(const MediaPlayerFactory*);
    void reloadTimerFired();

    MediaPlayerClient* m_client;
    Timer m_reloadTimer;
    std::unique_ptr<MediaPlayerPrivateInterface> m_private;
    const MediaPlayerFactory* m_currentMediaEngine { nullptr };
    HashSet<const MediaPlayerFactory*> m_attemptedEngines;
    URL m_url;
    ContentType m_contentType;
    String m_keySystem;
    std::optional<MediaPlayerEnums::MediaEngineIdentifier> m_activeEngineIdentifier;
    std::optional<MediaTime> m_pendingSeekRequest;
    IntSize m_presentationSize;
    Preload m_preload { Preload::Auto };
    double m_volume { 1 };
    bool m_pageIsVisible { false };
    bool m_visibleForCanvas { false };
    bool m_visibleInViewport { false };
    bool m_muted { false };
    bool m_preservesPitch { true };
    bool m_privateBrowsing { false };
    bool m_shouldPrepareToRender { false };
    bool m_contentMIMETypeWasInferredFromExtension { false };
    bool m_initializingMediaEngine { false };
    DynamicRangeMode m_preferredDynamicRangeMode;
    PitchCorrectionAlgorithm m_pitchCorrectionAlgorithm { PitchCorrectionAlgorithm::BestAllAround };

#if ENABLE(MEDIA_SOURCE)
    WeakPtr<MediaSourcePrivateClient> m_mediaSource;
#endif
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_mediaStream;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
    bool m_shouldContinueAfterKeyNeeded { false };
#endif
    bool m_isGatheringVideoFrameMetadata { false };
    bool m_requiresRemotePlayback { false };
    String m_lastErrorMessage;
};

class MediaPlayerFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerFactory() = default;
    virtual ~MediaPlayerFactory() = default;

    virtual MediaPlayerEnums::MediaEngineIdentifier identifier() const  = 0;
    virtual std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer*) const = 0;
    virtual void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&) const = 0;
    virtual MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters&) const = 0;

    virtual HashSet<SecurityOriginData> originsInMediaCache(const String&) const { return { }; }
    virtual void clearMediaCache(const String&, WallTime) const { }
    virtual void clearMediaCacheForOrigins(const String&, const HashSet<SecurityOriginData>&) const { }
    virtual bool supportsKeySystem(const String& /* keySystem */, const String& /* mimeType */) const { return false; }
};

using MediaEngineRegistrar = void(std::unique_ptr<MediaPlayerFactory>&&);
using MediaEngineRegister = void(MediaEngineRegistrar);

class MediaPlayerFactorySupport {
public:
    WEBCORE_EXPORT static void callRegisterMediaEngine(MediaEngineRegister);
};

class RemoteMediaPlayerSupport {
public:
    using RegisterRemotePlayerCallback = Function<void(MediaEngineRegistrar, MediaPlayerEnums::MediaEngineIdentifier)>;
    WEBCORE_EXPORT static void setRegisterRemotePlayerCallback(RegisterRemotePlayerCallback&&);
};

inline String MediaPlayer::audioOutputDeviceId() const
{
    return m_client ? m_client->audioOutputDeviceId() : String { };
}

inline String MediaPlayer::audioOutputDeviceIdOverride() const
{
    return m_client ? m_client->audioOutputDeviceIdOverride() : String { };
}

inline bool MediaPlayer::hasMediaEngine() const
{
    return m_currentMediaEngine;
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
