/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#include "AudioTrackPrivate.h"
#include "ContentType.h"
#include "GraphicsTypes3D.h"
#include "InbandTextTrackPrivate.h"
#include "LayoutRect.h"
#include "LegacyCDMSession.h"
#include "MediaPlayerEnums.h"
#include "NativeImage.h"
#include "PlatformLayer.h"
#include "PlatformMediaResourceLoader.h"
#include "PlatformMediaSession.h"
#include "SecurityOriginHash.h"
#include "Timer.h"
#include <wtf/URL.h>
#include "VideoTrackPrivate.h"
#include <JavaScriptCore/Uint8Array.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Logger.h>
#include <wtf/MediaTime.h>
#include <wtf/WallTime.h>
#include <wtf/text/StringHash.h>

#if ENABLE(AVF_CAPTIONS)
#include "PlatformTextTrack.h"
#endif

OBJC_CLASS AVPlayer;
OBJC_CLASS NSArray;

namespace WebCore {

class AudioSourceProvider;
class CDMInstance;
class CachedResourceLoader;
class GraphicsContext3D;
class GraphicsContext;
class LegacyCDMSessionClient;
class MediaPlaybackTarget;
class MediaPlayer;
class MediaPlayerPrivateInterface;
class MediaPlayerRequestInstallMissingPluginsCallback;
class MediaSourcePrivateClient;
class MediaStreamPrivate;
class PlatformTimeRanges;
class TextTrackRepresentation;

struct Cookie;
struct GraphicsDeviceAdapter;
struct MediaPlayerFactory;

struct MediaEngineSupportParameters {
    ContentType type;
    URL url;
    bool isMediaSource { false };
    bool isMediaStream { false };
    Vector<ContentType> contentTypesRequiringHardwareSupport;
};

struct VideoPlaybackQualityMetrics {
    uint32_t totalVideoFrames { 0 };
    uint32_t droppedVideoFrames { 0 };
    uint32_t corruptedVideoFrames { 0 };
    double totalFrameDelay { 0 };
    uint32_t displayCompositedVideoFrames { 0 };
};

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() = default;

    // the network state has changed
    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*) { }

    // the ready state has changed
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*) { }

    // the volume state has changed
    virtual void mediaPlayerVolumeChanged(MediaPlayer*) { }

    // the mute state has changed
    virtual void mediaPlayerMuteChanged(MediaPlayer*) { }

    // time has jumped, eg. not as a result of normal playback
    virtual void mediaPlayerTimeChanged(MediaPlayer*) { }

    // the media file duration has changed, or is now known
    virtual void mediaPlayerDurationChanged(MediaPlayer*) { }

    // the playback rate has changed
    virtual void mediaPlayerRateChanged(MediaPlayer*) { }

    // the play/pause status changed
    virtual void mediaPlayerPlaybackStateChanged(MediaPlayer*) { }

    // The MediaPlayer has found potentially problematic media content.
    // This is used internally to trigger swapping from a <video>
    // element to an <embed> in standalone documents
    virtual void mediaPlayerSawUnsupportedTracks(MediaPlayer*) { }

    // The MediaPlayer could not discover an engine which supports the requested resource.
    virtual void mediaPlayerResourceNotSupported(MediaPlayer*) { }

// Presentation-related methods
    // a new frame of video is available
    virtual void mediaPlayerRepaint(MediaPlayer*) { }

    // the movie size has changed
    virtual void mediaPlayerSizeChanged(MediaPlayer*) { }

    virtual void mediaPlayerEngineUpdated(MediaPlayer*) { }

    // The first frame of video is available to render. A media engine need only make this callback if the
    // first frame is not available immediately when prepareForRendering is called.
    virtual void mediaPlayerFirstVideoFrameAvailable(MediaPlayer*) { }

    // A characteristic of the media file, eg. video, audio, closed captions, etc, has changed.
    virtual void mediaPlayerCharacteristicChanged(MediaPlayer*) { }
    
    // whether the rendering system can accelerate the display of this MediaPlayer.
    virtual bool mediaPlayerRenderingCanBeAccelerated(MediaPlayer*) { return false; }

    // called when the media player's rendering mode changed, which indicates a change in the
    // availability of the platformLayer().
    virtual void mediaPlayerRenderingModeChanged(MediaPlayer*) { }

    // whether accelerated compositing is enabled for video rendering
    virtual bool mediaPlayerAcceleratedCompositingEnabled() { return false; }

    virtual void mediaPlayerActiveSourceBuffersChanged(const MediaPlayer*) { }

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const { return nullptr; }
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    virtual RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const { return nullptr; }
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) { return false; }
    virtual String mediaPlayerMediaKeysStorageDirectory() const { return emptyString(); }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    virtual void mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&) { }
    virtual void mediaPlayerWaitingForKeyChanged() { }
#endif
    
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*) { };
#endif

    virtual String mediaPlayerReferrer() const { return String(); }
    virtual String mediaPlayerUserAgent() const { return String(); }
    virtual void mediaPlayerEnterFullscreen() { }
    virtual void mediaPlayerExitFullscreen() { }
    virtual bool mediaPlayerIsFullscreen() const { return false; }
    virtual bool mediaPlayerIsFullscreenPermitted() const { return false; }
    virtual bool mediaPlayerIsVideo() const { return false; }
    virtual LayoutRect mediaPlayerContentBoxRect() const { return LayoutRect(); }
    virtual float mediaPlayerContentsScale() const { return 1; }
    virtual void mediaPlayerSetSize(const IntSize&) { }
    virtual void mediaPlayerPause() { }
    virtual void mediaPlayerPlay() { }
    virtual bool mediaPlayerPlatformVolumeConfigurationRequired() const { return false; }
    virtual bool mediaPlayerIsPaused() const { return true; }
    virtual bool mediaPlayerIsLooping() const { return false; }
    virtual CachedResourceLoader* mediaPlayerCachedResourceLoader() { return nullptr; }
    virtual RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() { return nullptr; }
    virtual bool doesHaveAttribute(const AtomString&, AtomString* = nullptr) const { return false; }
    virtual bool mediaPlayerShouldUsePersistentCache() const { return true; }
    virtual const String& mediaPlayerMediaCacheDirectory() const { return emptyString(); }

#if ENABLE(VIDEO_TRACK)
    virtual void mediaPlayerDidAddAudioTrack(AudioTrackPrivate&) { }
    virtual void mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&) { }
    virtual void mediaPlayerDidAddVideoTrack(VideoTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&) { }
    virtual void mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate&) { }

    virtual void textTrackRepresentationBoundsChanged(const IntRect&) { }
#endif

#if ENABLE(VIDEO_TRACK) && ENABLE(AVF_CAPTIONS)
    virtual Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() { return { }; }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual String mediaPlayerNetworkInterfaceName() const { return String(); }
    virtual bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const { return false; }
#endif

    virtual void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType) { }

    virtual String mediaPlayerSourceApplicationIdentifier() const { return emptyString(); }

    virtual bool mediaPlayerIsInMediaDocument() const { return false; }
    virtual void mediaPlayerEngineFailedToLoad() const { }

    virtual double mediaPlayerRequestedPlaybackRate() const { return 0; }
    virtual MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const { return MediaPlayerEnums::VideoFullscreenModeNone; }
    virtual bool mediaPlayerIsVideoFullscreenStandby() const { return false; }
    virtual Vector<String> mediaPlayerPreferredAudioCharacteristics() const { return Vector<String>(); }

#if USE(GSTREAMER)
    virtual void requestInstallMissingPlugins(const String&, const String&, MediaPlayerRequestInstallMissingPluginsCallback&) { };
#endif

    virtual bool mediaPlayerShouldDisableSleep() const { return false; }
    virtual const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const;
    virtual bool mediaPlayerShouldCheckHardwareSupport() const { return false; }

#if !RELEASE_LOG_DISABLED
    virtual const void* mediaPlayerLogIdentifier() { return nullptr; }
    virtual const Logger& mediaPlayerLogger() = 0;
#endif
};

class MediaPlayer : public MediaPlayerEnums, public RefCounted<MediaPlayer> {
    WTF_MAKE_NONCOPYABLE(MediaPlayer); WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MediaPlayer> create(MediaPlayerClient&);
    virtual ~MediaPlayer();

    void invalidate();

    // Media engine support.
    using MediaPlayerEnums::SupportsType;
    static SupportsType supportsType(const MediaEngineSupportParameters&);
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static bool isAvailable();
    static HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String& path);
    static void clearMediaCache(const String& path, WallTime modifiedSince);
    static void clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    bool supportsPictureInPicture() const;
    bool supportsFullscreen() const;
    bool supportsScanning() const;
    bool canSaveMediaData() const;
    bool requiresImmediateCompositing() const;
    bool doesHaveAttribute(const AtomString&, AtomString* value = nullptr) const;
    PlatformLayer* platformLayer() const;

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler = [] { });
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

    bool inMediaDocument() const;

    IntSize size() const { return m_size; }
    void setSize(const IntSize& size);

    bool load(const URL&, const ContentType&, const String& keySystem);
#if ENABLE(MEDIA_SOURCE)
    bool load(const URL&, const ContentType&, MediaSourcePrivateClient*);
#endif
#if ENABLE(MEDIA_STREAM)
    bool load(MediaStreamPrivate&);
#endif
    void cancelLoad();

    bool visible() const;
    void setVisible(bool);

    void prepareToPlay();
    void play();
    void pause();

    using MediaPlayerEnums::BufferingPolicy;
    void setBufferingPolicy(BufferingPolicy);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    // Represents synchronous exceptions that can be thrown from the Encrypted Media methods.
    // This is different from the asynchronous MediaKeyError.
    enum MediaKeyException { NoError, InvalidPlayerState, KeySystemNotSupported };

    std::unique_ptr<LegacyCDMSession> createSession(const String& keySystem, LegacyCDMSessionClient*);
    void setCDMSession(LegacyCDMSession*);
    void keyAdded();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void cdmInstanceAttached(CDMInstance&);
    void cdmInstanceDetached(CDMInstance&);
    void attemptToDecryptWithInstance(CDMInstance&);
#endif

    bool paused() const;
    bool seeking() const;

    static double invalidTime() { return -1.0;}
    MediaTime duration() const;
    MediaTime currentTime() const;
    void seek(const MediaTime&);
    void seekWithTolerance(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance);

    MediaTime startTime() const;
    MediaTime initialTime() const;

    MediaTime getStartDate() const;

    double rate() const;
    void setRate(double);
    double requestedRate() const;

    bool preservesPitch() const;
    void setPreservesPitch(bool);

    std::unique_ptr<PlatformTimeRanges> buffered();
    std::unique_ptr<PlatformTimeRanges> seekable();
    MediaTime minTimeSeekable();
    MediaTime maxTimeSeekable();

    double seekableTimeRangesLastModifiedTime();
    double liveUpdateInterval();

    bool didLoadingProgress();

    double volume() const;
    void setVolume(double);
    bool platformVolumeConfigurationRequired() const { return client().mediaPlayerPlatformVolumeConfigurationRequired(); }

    bool muted() const;
    void setMuted(bool);

    bool hasClosedCaptions() const;
    void setClosedCaptionsVisible(bool closedCaptionsVisible);

    void paint(GraphicsContext&, const FloatRect&);
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

    // copyVideoTextureToPlatformTexture() is used to do the GPU-GPU textures copy without a readback to system memory.
    // The first five parameters denote the corresponding GraphicsContext, destination texture, requested level, requested type and the required internalFormat for destination texture.
    // The last two parameters premultiplyAlpha and flipY denote whether addtional premultiplyAlpha and flip operation are required during the copy.
    // It returns true on success and false on failure.

    // In the GPU-GPU textures copy, the source texture(Video texture) should have valid target, internalFormat and size, etc.
    // The destination texture may need to be resized to to the dimensions of the source texture or re-defined to the required internalFormat.
    // The current restrictions require that format shoud be RGB or RGBA, type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.

    bool copyVideoTextureToPlatformTexture(GraphicsContext3D*, Platform3DObject texture, GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY);

    NativeImagePtr nativeImageForCurrentTime();

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

    MediaPlayerClient& client() const { return *m_client; }

    bool hasAvailableVideoFrame() const;
    void prepareForRendering();

    bool canLoadPoster() const;
    void setPoster(const String&);

#if USE(NATIVE_FULLSCREEN_VIDEO)
    void enterFullscreen();
    void exitFullscreen();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    enum WirelessPlaybackTargetType { TargetTypeNone, TargetTypeAirPlay, TargetTypeTVOut };
    WirelessPlaybackTargetType wirelessPlaybackTargetType() const;

    String wirelessPlaybackTargetName() const;

    bool wirelessVideoPlaybackDisabled() const;
    void setWirelessVideoPlaybackDisabled(bool);

    void currentPlaybackTargetIsWirelessChanged();
    void playbackTargetAvailabilityChanged();

    bool isCurrentPlaybackTargetWireless() const;
    bool canPlayToWirelessPlaybackTarget() const;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&);

    void setShouldPlayToPlaybackTarget(bool);
#endif

    double minFastReverseRate() const;
    double maxFastForwardRate() const;

#if USE(NATIVE_FULLSCREEN_VIDEO)
    bool canEnterFullscreen() const;
#endif

    // whether accelerated rendering is supported by the media engine for the current media.
    bool supportsAcceleratedRendering() const;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged();

    bool shouldMaintainAspectRatio() const;
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
    bool keyNeeded(Uint8Array* initData);
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

    CachedResourceLoader* cachedResourceLoader();
    RefPtr<PlatformMediaResourceLoader> createResourceLoader();

#if ENABLE(VIDEO_TRACK)
    void addAudioTrack(AudioTrackPrivate&);
    void addTextTrack(InbandTextTrackPrivate&);
    void addVideoTrack(VideoTrackPrivate&);
    void removeAudioTrack(AudioTrackPrivate&);
    void removeTextTrack(InbandTextTrackPrivate&);
    void removeVideoTrack(VideoTrackPrivate&);

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
    void tracksChanged();
#endif

#if ENABLE(VIDEO_TRACK) && ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged();
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources();
#endif

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const;
    bool getRawCookies(const URL&, Vector<Cookie>&) const;
#endif

    static void resetMediaEngines();

#if USE(GSTREAMER)
    WEBCORE_EXPORT void simulateAudioInterruption();
#endif

    WEBCORE_EXPORT void beginSimulatedHDCPError();
    WEBCORE_EXPORT void endSimulatedHDCPError();

    String languageOfPrimaryAudioTrack() const;

    size_t extraMemoryCost() const;

    unsigned long long fileSize() const;

    Optional<VideoPlaybackQualityMetrics> videoPlaybackQualityMetrics();

    void handlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType);
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

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger();
    const void* mediaPlayerLogIdentifier() { return client().mediaPlayerLogIdentifier(); }
#endif

    void applicationWillResignActive();
    void applicationDidBecomeActive();

#if USE(AVFOUNDATION)
    WEBCORE_EXPORT AVPlayer *objCAVFoundationAVPlayer() const;
#endif

    bool performTaskAtMediaTime(WTF::Function<void()>&&, MediaTime);

    bool shouldIgnoreIntrinsicSize();

private:
    MediaPlayer(MediaPlayerClient&);

    const MediaPlayerFactory* nextBestMediaEngine(const MediaPlayerFactory*) const;
    void loadWithNextMediaEngine(const MediaPlayerFactory*);
    void reloadTimerFired();

    MediaPlayerClient* m_client;
    Timer m_reloadTimer;
    std::unique_ptr<MediaPlayerPrivateInterface> m_private;
    const MediaPlayerFactory* m_currentMediaEngine { nullptr };
    URL m_url;
    ContentType m_contentType;
    String m_keySystem;
    IntSize m_size;
    Preload m_preload { Auto };
    double m_volume { 1 };
    bool m_visible { false };
    bool m_muted { false };
    bool m_preservesPitch { true };
    bool m_privateBrowsing { false };
    bool m_shouldPrepareToRender { false };
    bool m_contentMIMETypeWasInferredFromExtension { false };
    bool m_initializingMediaEngine { false };

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
#endif
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_mediaStream;
#endif
};

using CreateMediaEnginePlayer = WTF::Function<std::unique_ptr<MediaPlayerPrivateInterface> (MediaPlayer*)>;
typedef void (*MediaEngineSupportedTypes)(HashSet<String, ASCIICaseInsensitiveHash>& types);
typedef MediaPlayer::SupportsType (*MediaEngineSupportsType)(const MediaEngineSupportParameters& parameters);
typedef HashSet<RefPtr<SecurityOrigin>> (*MediaEngineOriginsInMediaCache)(const String& path);
typedef void (*MediaEngineClearMediaCache)(const String& path, WallTime modifiedSince);
typedef void (*MediaEngineClearMediaCacheForOrigins)(const String& path, const HashSet<RefPtr<SecurityOrigin>>&);
typedef bool (*MediaEngineSupportsKeySystem)(const String& keySystem, const String& mimeType);

typedef void (*MediaEngineRegistrar)(CreateMediaEnginePlayer&&, MediaEngineSupportedTypes, MediaEngineSupportsType,
    MediaEngineOriginsInMediaCache, MediaEngineClearMediaCache, MediaEngineClearMediaCacheForOrigins, MediaEngineSupportsKeySystem);
typedef void (*MediaEngineRegister)(MediaEngineRegistrar);

class MediaPlayerFactorySupport {
public:
    WEBCORE_EXPORT static void callRegisterMediaEngine(MediaEngineRegister);
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
