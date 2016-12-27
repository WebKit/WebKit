/*
 * Copyright (C) 2007-2014 Apple Inc. All rights reserved.
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

#ifndef MediaPlayer_h
#define MediaPlayer_h

#if ENABLE(VIDEO)
#include "GraphicsTypes3D.h"

#include "AudioTrackPrivate.h"
#include "CDMSession.h"
#include "InbandTextTrackPrivate.h"
#include "IntRect.h"
#include "URL.h"
#include "LayoutRect.h"
#include "MediaPlayerEnums.h"
#include "NativeImagePtr.h"
#include "PlatformLayer.h"
#include "PlatformMediaResourceLoader.h"
#include "PlatformMediaSession.h"
#include "SecurityOriginHash.h"
#include "Timer.h"
#include "VideoTrackPrivate.h"
#include <runtime/Uint8Array.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/MediaTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>

#if ENABLE(AVF_CAPTIONS)
#include "PlatformTextTrack.h"
#endif

OBJC_CLASS AVAsset;
OBJC_CLASS AVPlayer;
OBJC_CLASS NSArray;
OBJC_CLASS QTMovie;

class AVCFPlayer;
class QTMovieGWorld;
class QTMovieVisualContext;

namespace WebCore {

class AudioSourceProvider;
class AuthenticationChallenge;
class MediaPlaybackTarget;
#if ENABLE(MEDIA_SOURCE)
class MediaSourcePrivateClient;
#endif
#if ENABLE(MEDIA_STREAM)
class MediaStreamPrivate;
#endif
class MediaPlayerPrivateInterface;
class TextTrackRepresentation;
struct Cookie;

// Structure that will hold every native
// types supported by the current media player.
// We have to do that has multiple media players
// backend can live at runtime.
struct PlatformMedia {
    enum {
        None,
        QTMovieType,
        QTMovieGWorldType,
        QTMovieVisualContextType,
        AVFoundationMediaPlayerType,
        AVFoundationCFMediaPlayerType,
        AVFoundationAssetType,
    } type;

    union {
        QTMovie* qtMovie;
        QTMovieGWorld* qtMovieGWorld;
        QTMovieVisualContext* qtMovieVisualContext;
        AVPlayer* avfMediaPlayer;
        AVCFPlayer* avcfMediaPlayer;
        AVAsset* avfAsset;
    } media;
};

struct MediaEngineSupportParameters {

    MediaEngineSupportParameters() { }

    String type;
    String codecs;
    URL url;
    String keySystem;
    bool isMediaSource { false };
    bool isMediaStream { false };
};

extern const PlatformMedia NoPlatformMedia;

class CDMSessionClient;
class CachedResourceLoader;
class ContentType;
class GraphicsContext;
class GraphicsContext3D;
class IntRect;
class IntSize;
class MediaPlayer;
class PlatformTimeRanges;

struct MediaPlayerFactory;

#if PLATFORM(WIN) && USE(AVFOUNDATION)
struct GraphicsDeviceAdapter;
#endif

#if USE(GSTREAMER)
class MediaPlayerRequestInstallMissingPluginsCallback;
#endif

class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() { }

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

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const { return 0; }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    enum MediaKeyErrorCode { UnknownError = 1, ClientError, ServiceError, OutputError, HardwareChangeError, DomainError };
    virtual void mediaPlayerKeyAdded(MediaPlayer*, const String& /* keySystem */, const String& /* sessionId */) { }
    virtual void mediaPlayerKeyError(MediaPlayer*, const String& /* keySystem */, const String& /* sessionId */, MediaKeyErrorCode, unsigned short /* systemCode */) { }
    virtual void mediaPlayerKeyMessage(MediaPlayer*, const String& /* keySystem */, const String& /* sessionId */, const unsigned char* /* message */, unsigned /* messageLength */, const URL& /* defaultURL */) { }
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, const String& /* keySystem */, const String& /* sessionId */, const unsigned char* /* initData */, unsigned /* initDataLength */) { return false; }
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    virtual RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const { return nullptr; }
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) { return false; }
    virtual String mediaPlayerMediaKeysStorageDirectory() const { return emptyString(); }
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
    virtual CachedResourceLoader* mediaPlayerCachedResourceLoader() { return 0; }
    virtual RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() { return nullptr; }
    virtual bool doesHaveAttribute(const AtomicString&, AtomicString* = 0) const { return false; }
    virtual bool mediaPlayerShouldUsePersistentCache() const { return true; }
    virtual const String& mediaPlayerMediaCacheDirectory() const { return emptyString(); }

#if ENABLE(VIDEO_TRACK)
    virtual void mediaPlayerDidAddAudioTrack(PassRefPtr<AudioTrackPrivate>) { }
    virtual void mediaPlayerDidAddTextTrack(PassRefPtr<InbandTextTrackPrivate>) { }
    virtual void mediaPlayerDidAddVideoTrack(PassRefPtr<VideoTrackPrivate>) { }
    virtual void mediaPlayerDidRemoveAudioTrack(PassRefPtr<AudioTrackPrivate>) { }
    virtual void mediaPlayerDidRemoveTextTrack(PassRefPtr<InbandTextTrackPrivate>) { }
    virtual void mediaPlayerDidRemoveVideoTrack(PassRefPtr<VideoTrackPrivate>) { }

    virtual void textTrackRepresentationBoundsChanged(const IntRect&) { }
#if ENABLE(AVF_CAPTIONS)
    virtual Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() { return Vector<RefPtr<PlatformTextTrack>>(); }
#endif
#endif

#if PLATFORM(IOS)
    virtual String mediaPlayerNetworkInterfaceName() const { return String(); }
    virtual bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const { return false; }
#endif
    
    virtual bool mediaPlayerShouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge&) { return false; }
    virtual void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType) { }

    virtual String mediaPlayerSourceApplicationIdentifier() const { return emptyString(); }

    virtual bool mediaPlayerIsInMediaDocument() const { return false; }
    virtual void mediaPlayerEngineFailedToLoad() const { }

    virtual double mediaPlayerRequestedPlaybackRate() const { return 0; }
    virtual MediaPlayerEnums::VideoFullscreenMode mediaPlayerFullscreenMode() const { return MediaPlayerEnums::VideoFullscreenModeNone; }
    virtual Vector<String> mediaPlayerPreferredAudioCharacteristics() const { return Vector<String>(); }

#if USE(GSTREAMER)
    virtual void requestInstallMissingPlugins(const String&, const String&, MediaPlayerRequestInstallMissingPluginsCallback&) { };
#endif
};

class MediaPlayerSupportsTypeClient {
public:
    virtual ~MediaPlayerSupportsTypeClient() { }

    virtual bool mediaPlayerNeedsSiteSpecificHacks() const { return false; }
    virtual String mediaPlayerDocumentHost() const { return String(); }
};

class MediaPlayer : public MediaPlayerEnums {
    WTF_MAKE_NONCOPYABLE(MediaPlayer); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaPlayer(MediaPlayerClient&);
    virtual ~MediaPlayer();

    // Media engine support.
    enum SupportsType { IsNotSupported, IsSupported, MayBeSupported };
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&, const MediaPlayerSupportsTypeClient*);
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static bool isAvailable();
    static HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String& path);
    static void clearMediaCache(const String& path, std::chrono::system_clock::time_point modifiedSince);
    static void clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>&);
    static bool supportsKeySystem(const String& keySystem, const String& mimeType);

    bool supportsFullscreen() const;
    bool supportsScanning() const;
    bool canSaveMediaData() const;
    bool requiresImmediateCompositing() const;
    bool doesHaveAttribute(const AtomicString&, AtomicString* value = nullptr) const;
    PlatformMedia platformMedia() const;
    PlatformLayer* platformLayer() const;

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*, std::function<void()> completionHandler = [] { });
    void setVideoFullscreenFrame(FloatRect);
    using MediaPlayerEnums::VideoGravity;
    void setVideoFullscreenGravity(VideoGravity);
    void setVideoFullscreenMode(VideoFullscreenMode);
    VideoFullscreenMode fullscreenMode() const;
#endif

#if PLATFORM(IOS)
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
    bool load(MediaStreamPrivate*);
#endif
    void cancelLoad();

    bool visible() const;
    void setVisible(bool);

    void prepareToPlay();
    void play();
    void pause();
    void setShouldBufferData(bool);

#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
    // Represents synchronous exceptions that can be thrown from the Encrypted Media methods.
    // This is different from the asynchronous MediaKeyError.
    enum MediaKeyException { NoError, InvalidPlayerState, KeySystemNotSupported };
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    MediaKeyException generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength);
    MediaKeyException addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId);
    MediaKeyException cancelKeyRequest(const String& keySystem, const String& sessionId);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    std::unique_ptr<CDMSession> createSession(const String& keySystem, CDMSessionClient*);
    void setCDMSession(CDMSession*);
    void keyAdded();
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

    bool didLoadingProgress();

    double volume() const;
    void setVolume(double);
    bool platformVolumeConfigurationRequired() const { return m_client.mediaPlayerPlatformVolumeConfigurationRequired(); }

    bool muted() const;
    void setMuted(bool);

    bool hasClosedCaptions() const;
    void setClosedCaptionsVisible(bool closedCaptionsVisible);

    bool autoplay() const;
    void setAutoplay(bool);

    void paint(GraphicsContext&, const FloatRect&);
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

    // copyVideoTextureToPlatformTexture() is used to do the GPU-GPU textures copy without a readback to system memory.
    // The first five parameters denote the corresponding GraphicsContext, destination texture, requested level, requested type and the required internalFormat for destination texture.
    // The last two parameters premultiplyAlpha and flipY denote whether addtional premultiplyAlpha and flip operation are required during the copy.
    // It returns true on success and false on failure.

    // In the GPU-GPU textures copy, the source texture(Video texture) should have valid target, internalFormat and size, etc.
    // The destination texture may need to be resized to to the dimensions of the source texture or re-defined to the required internalFormat.
    // The current restrictions require that format shoud be RGB or RGBA, type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.

    // Each platform port can have its own implementation on this function. The default implementation for it is a single "return false" in MediaPlayerPrivate.h.
    // In chromium, the implementation is based on GL_CHROMIUM_copy_texture extension which is documented at
    // http://src.chromium.org/viewvc/chrome/trunk/src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_copy_texture.txt and implemented at
    // http://src.chromium.org/viewvc/chrome/trunk/src/gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.cc via shaders.
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

    MediaPlayerClient& client() const { return m_client; }

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

#if ENABLE(ENCRYPTED_MEDIA)
    void keyAdded(const String& keySystem, const String& sessionId);
    void keyError(const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode);
    void keyMessage(const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const URL& defaultURL);
    bool keyNeeded(const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    RefPtr<ArrayBuffer> cachedKeyForKeyId(const String& keyId) const;
    bool keyNeeded(Uint8Array* initData);
    String mediaKeysStorageDirectory() const;
#endif

    String referrer() const;
    String userAgent() const;

    String engineDescription() const;
    long platformErrorCode() const;

    CachedResourceLoader* cachedResourceLoader();
    PassRefPtr<PlatformMediaResourceLoader> createResourceLoader();

#if ENABLE(VIDEO_TRACK)
    void addAudioTrack(PassRefPtr<AudioTrackPrivate>);
    void addTextTrack(PassRefPtr<InbandTextTrackPrivate>);
    void addVideoTrack(PassRefPtr<VideoTrackPrivate>);
    void removeAudioTrack(PassRefPtr<AudioTrackPrivate>);
    void removeTextTrack(PassRefPtr<InbandTextTrackPrivate>);
    void removeVideoTrack(PassRefPtr<VideoTrackPrivate>);

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
    void tracksChanged();
#if ENABLE(AVF_CAPTIONS)
    void notifyTrackModeChanged();
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources();
#endif
#endif

#if PLATFORM(IOS)
    String mediaPlayerNetworkInterfaceName() const;
    bool getRawCookies(const URL&, Vector<Cookie>&) const;
#endif

    static void resetMediaEngines();

#if USE(GSTREAMER)
    WEBCORE_EXPORT void simulateAudioInterruption();
#endif

    String languageOfPrimaryAudioTrack() const;

    size_t extraMemoryCost() const;

    unsigned long long fileSize() const;

#if ENABLE(MEDIA_SOURCE)
    unsigned long totalVideoFrames();
    unsigned long droppedVideoFrames();
    unsigned long corruptedVideoFrames();
    MediaTime totalFrameDelay();
#endif

    bool shouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge&);
    void handlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType);
    String sourceApplicationIdentifier() const;
    Vector<String> preferredAudioCharacteristics() const;

    bool ended() const;

private:
    const MediaPlayerFactory* nextBestMediaEngine(const MediaPlayerFactory*) const;
    void loadWithNextMediaEngine(const MediaPlayerFactory*);
    void reloadTimerFired();

    static void initializeMediaEngines();

    MediaPlayerClient& m_client;
    Timer m_reloadTimer;
    std::unique_ptr<MediaPlayerPrivateInterface> m_private;
    const MediaPlayerFactory* m_currentMediaEngine;
    URL m_url;
    String m_contentMIMEType;
    String m_contentTypeCodecs;
    String m_keySystem;
    IntSize m_size;
    Preload m_preload;
    bool m_visible;
    double m_volume;
    bool m_muted;
    bool m_preservesPitch;
    bool m_privateBrowsing;
    bool m_shouldPrepareToRender;
    bool m_contentMIMETypeWasInferredFromExtension;
    bool m_initializingMediaEngine { false };

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
#endif
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStreamPrivate> m_mediaStream;
#endif
};

typedef std::function<std::unique_ptr<MediaPlayerPrivateInterface> (MediaPlayer*)> CreateMediaEnginePlayer;
typedef void (*MediaEngineSupportedTypes)(HashSet<String, ASCIICaseInsensitiveHash>& types);
typedef MediaPlayer::SupportsType (*MediaEngineSupportsType)(const MediaEngineSupportParameters& parameters);
typedef HashSet<RefPtr<SecurityOrigin>> (*MediaEngineOriginsInMediaCache)(const String& path);
typedef void (*MediaEngineClearMediaCache)(const String& path, std::chrono::system_clock::time_point modifiedSince);
typedef void (*MediaEngineClearMediaCacheForOrigins)(const String& path, const HashSet<RefPtr<SecurityOrigin>>&);
typedef bool (*MediaEngineSupportsKeySystem)(const String& keySystem, const String& mimeType);

typedef void (*MediaEngineRegistrar)(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType,
    MediaEngineOriginsInMediaCache, MediaEngineClearMediaCache, MediaEngineClearMediaCacheForOrigins, MediaEngineSupportsKeySystem);
typedef void (*MediaEngineRegister)(MediaEngineRegistrar);

class MediaPlayerFactorySupport {
public:
    WEBCORE_EXPORT static void callRegisterMediaEngine(MediaEngineRegister);
};

}

#endif // ENABLE(VIDEO)

#endif
