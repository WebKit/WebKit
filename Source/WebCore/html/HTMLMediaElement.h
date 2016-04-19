/*
 * Copyright (C) 2007-2015 Apple Inc. All rights reserved.
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

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#if ENABLE(VIDEO)
#include "HTMLElement.h"
#include "ActiveDOMObject.h"
#include "GenericEventQueue.h"
#include "GenericTaskQueue.h"
#include "HTMLMediaElementEnums.h"
#include "MediaCanStartListener.h"
#include "MediaControllerInterface.h"
#include "MediaElementSession.h"
#include "MediaProducer.h"
#include "PageThrottler.h"

#if ENABLE(VIDEO_TRACK)
#include "AudioTrack.h"
#include "CaptionUserPreferences.h"
#include "PODIntervalTree.h"
#include "TextTrack.h"
#include "TextTrackCue.h"
#include "VTTCue.h"
#include "VideoTrack.h"
#endif

#ifndef NDEBUG
#include <wtf/StringPrintStream.h>
#endif

namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class MediaElementAudioSourceNode;
#endif
class DisplaySleepDisabler;
class Event;
class HTMLSourceElement;
class HTMLTrackElement;
class URL;
class MediaController;
class MediaControls;
class MediaControlsHost;
class MediaError;
class MediaPlayer;
class TimeRanges;
#if ENABLE(ENCRYPTED_MEDIA_V2)
class MediaKeys;
#endif
#if ENABLE(MEDIA_SESSION)
class MediaSession;
#endif
#if ENABLE(MEDIA_SOURCE)
class MediaSource;
class SourceBuffer;
class VideoPlaybackQuality;
#endif

#if ENABLE(VIDEO_TRACK)
class AudioTrackList;
class AudioTrackPrivate;
class InbandTextTrackPrivate;
class TextTrackList;
class VideoTrackList;
class VideoTrackPrivate;

typedef PODIntervalTree<MediaTime, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;
#endif

#if ENABLE(MEDIA_STREAM)
class MediaStream;
class ScriptExecutionContext;
#endif

class HTMLMediaElement
    : public HTMLElement
    , private MediaPlayerClient, public MediaPlayerSupportsTypeClient, private MediaCanStartListener, public ActiveDOMObject, public MediaControllerInterface , public PlatformMediaSessionClient, private MediaProducer
#if ENABLE(VIDEO_TRACK)
    , private AudioTrackClient
    , private TextTrackClient
    , private VideoTrackClient
#endif
{
public:
    MediaPlayer* player() const { return m_player.get(); }

    virtual bool isVideo() const { return false; }
    bool hasVideo() const override { return false; }
    bool hasAudio() const override;

    static HashSet<HTMLMediaElement*>& allMediaElements();

    void rewind(double timeDelta);
    WEBCORE_EXPORT void returnToRealtime() override;

    // Eventually overloaded in HTMLVideoElement
    bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const override { return false; };

    bool supportsScanning() const override;

    bool canSaveMediaData() const;

    bool doesHaveAttribute(const AtomicString&, AtomicString* value = nullptr) const override;

    WEBCORE_EXPORT PlatformMedia platformMedia() const;
    PlatformLayer* platformLayer() const;
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*);
    PlatformLayer* videoFullscreenLayer() const { return m_videoFullscreenLayer.get(); }
    void setVideoFullscreenFrame(FloatRect);
    void setVideoFullscreenGravity(MediaPlayerEnums::VideoGravity);
    MediaPlayerEnums::VideoGravity videoFullscreenGravity() const { return m_videoFullscreenGravity; }
#endif

    using HTMLMediaElementEnums::DelayedActionType;
    void scheduleDelayedAction(DelayedActionType);
    
    MediaPlayerEnums::MovieLoadType movieLoadType() const;
    
    bool inActiveDocument() const { return m_inActiveDocument; }

    const Document* hostingDocument() const override { return &document(); }

// DOM API
// error state
    PassRefPtr<MediaError> error() const;

    void setSrc(const String&);
    const URL& currentSrc() const { return m_currentSrc; }

#if ENABLE(MEDIA_STREAM)
    MediaStream* srcObject() const { return m_mediaStreamSrcObject.get(); }
    void setSrcObject(ScriptExecutionContext&, MediaStream*);
#endif

// network state
    using HTMLMediaElementEnums::NetworkState;
    NetworkState networkState() const;

    String preload() const;    
    void setPreload(const String&);

    PassRefPtr<TimeRanges> buffered() const override;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String(), const URL& = URL()) const;

// ready state
    using HTMLMediaElementEnums::ReadyState;
    ReadyState readyState() const override;
    bool seeking() const;

// playback state
    WEBCORE_EXPORT double currentTime() const override;
    void setCurrentTime(double) override;
    virtual void setCurrentTime(double, ExceptionCode&);
    virtual double getStartDate() const;
    WEBCORE_EXPORT double duration() const override;
    WEBCORE_EXPORT bool paused() const override;
    double defaultPlaybackRate() const override;
    void setDefaultPlaybackRate(double) override;
    WEBCORE_EXPORT double playbackRate() const override;
    void setPlaybackRate(double) override;

// MediaTime versions of playback state
    MediaTime currentMediaTime() const;
    void setCurrentTime(const MediaTime&);
    MediaTime durationMediaTime() const;
    void fastSeek(const MediaTime&);

    void updatePlaybackRate();
    bool webkitPreservesPitch() const;
    void setWebkitPreservesPitch(bool);
    PassRefPtr<TimeRanges> played() override;
    PassRefPtr<TimeRanges> seekable() const override;
    WEBCORE_EXPORT bool ended() const;
    bool autoplay() const;
    bool isAutoplaying() const { return m_autoplaying; }
    bool loop() const;
    void setLoop(bool b);
    WEBCORE_EXPORT void play() override;
    WEBCORE_EXPORT void pause() override;
    void setShouldBufferData(bool) override;
    void fastSeek(double);
    double minFastReverseRate() const;
    double maxFastForwardRate() const;

    void purgeBufferedDataIfPossible();

// captions
    bool webkitHasClosedCaptions() const;
    bool webkitClosedCaptionsVisible() const;
    void setWebkitClosedCaptionsVisible(bool);

    bool elementIsHidden() const override { return m_elementIsHidden; }

#if ENABLE(MEDIA_STATISTICS)
// Statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;
#endif

#if ENABLE(MEDIA_SOURCE)
//  Media Source.
    void closeMediaSource();
    void incrementDroppedFrameCount() { ++m_droppedVideoFrames; }
    size_t maximumSourceBufferSize(const SourceBuffer&) const;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void webkitGenerateKeyRequest(const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionCode&);
    void webkitGenerateKeyRequest(const String& keySystem, ExceptionCode&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionCode&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionCode&);
    void webkitCancelKeyRequest(const String& keySystem, const String& sessionId, ExceptionCode&);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    MediaKeys* keys() const { return m_mediaKeys.get(); }
    void setMediaKeys(MediaKeys*);

    void keyAdded();
#endif

// controls
    bool controls() const;
    void setControls(bool);
    WEBCORE_EXPORT double volume() const override;
    void setVolume(double, ExceptionCode&) override;
    WEBCORE_EXPORT bool muted() const override;
    WEBCORE_EXPORT void setMuted(bool) override;

    WEBCORE_EXPORT void togglePlayState();
    WEBCORE_EXPORT void beginScrubbing() override;
    WEBCORE_EXPORT void endScrubbing() override;

    void beginScanning(ScanDirection) override;
    void endScanning() override;
    double nextScanRate();

    WEBCORE_EXPORT bool canPlay() const override;

    double percentLoaded() const;

#if ENABLE(VIDEO_TRACK)
    PassRefPtr<TextTrack> addTextTrack(const String& kind, const String& label, const String& language, ExceptionCode&);
    PassRefPtr<TextTrack> addTextTrack(const String& kind, const String& label, ExceptionCode& ec) { return addTextTrack(kind, label, emptyString(), ec); }
    PassRefPtr<TextTrack> addTextTrack(const String& kind, ExceptionCode& ec) { return addTextTrack(kind, emptyString(), emptyString(), ec); }

    AudioTrackList* audioTracks();
    TextTrackList* textTracks();
    VideoTrackList* videoTracks();

    CueList currentlyActiveCues() const { return m_currentlyActiveCues; }

    void addAudioTrack(PassRefPtr<AudioTrack>);
    void addTextTrack(PassRefPtr<TextTrack>);
    void addVideoTrack(PassRefPtr<VideoTrack>);
    void removeAudioTrack(AudioTrack*);
    void removeTextTrack(TextTrack*, bool scheduleEvent = true);
    void removeVideoTrack(VideoTrack*);
    void forgetResourceSpecificTracks();
    void closeCaptionTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    virtual void didAddTextTrack(HTMLTrackElement*);
    virtual void didRemoveTextTrack(HTMLTrackElement*);

    void mediaPlayerDidAddAudioTrack(PassRefPtr<AudioTrackPrivate>) override;
    void mediaPlayerDidAddTextTrack(PassRefPtr<InbandTextTrackPrivate>) override;
    void mediaPlayerDidAddVideoTrack(PassRefPtr<VideoTrackPrivate>) override;
    void mediaPlayerDidRemoveAudioTrack(PassRefPtr<AudioTrackPrivate>) override;
    void mediaPlayerDidRemoveTextTrack(PassRefPtr<InbandTextTrackPrivate>) override;
    void mediaPlayerDidRemoveVideoTrack(PassRefPtr<VideoTrackPrivate>) override;

#if ENABLE(AVF_CAPTIONS)
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() override;
#endif

    struct TrackGroup;
    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    void setSelectedTextTrack(TextTrack*);

    bool textTracksAreReady() const;
    using HTMLMediaElementEnums::TextTrackVisibilityCheckType;
    void configureTextTrackDisplay(TextTrackVisibilityCheckType checkType = CheckTextTrackVisibility);
    void updateTextTrackDisplay();

    // AudioTrackClient
    void audioTrackEnabledChanged(AudioTrack*) override;

    // TextTrackClient
    virtual void textTrackReadyStateChanged(TextTrack*);
    void textTrackKindChanged(TextTrack*) override;
    void textTrackModeChanged(TextTrack*) override;
    void textTrackAddCues(TextTrack*, const TextTrackCueList*) override;
    void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) override;
    void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) override;
    void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) override;

    // VideoTrackClient
    void videoTrackSelectedChanged(VideoTrack*) override;

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void webkitShowPlaybackTargetPicker();
    bool addEventListener(const AtomicString& eventType, RefPtr<EventListener>&&, bool useCapture) override;
    bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;

    void wirelessRoutesAvailableDidChange() override;
    bool canPlayToWirelessPlaybackTarget() const override;
    bool isPlayingToWirelessPlaybackTarget() const override;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
#endif
    bool webkitCurrentPlaybackTargetIsWireless() const;

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::scriptExecutionContext;

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }
    
    WEBCORE_EXPORT bool isFullscreen() const override;
    bool isStandardFullscreen() const;
    void toggleStandardFullscreenState();

    using MediaPlayerEnums::VideoFullscreenMode;
    VideoFullscreenMode fullscreenMode() const { return m_videoFullscreenMode; }
    virtual void fullscreenModeChanged(VideoFullscreenMode mode) { m_videoFullscreenMode = mode; }

    void enterFullscreen(VideoFullscreenMode);
    void enterFullscreen() override;
    WEBCORE_EXPORT void exitFullscreen();

    bool hasClosedCaptions() const override;
    bool closedCaptionsVisible() const override;
    void setClosedCaptionsVisible(bool) override;

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    void privateBrowsingStateDidChange() override;

    // Media cache management.
    WEBCORE_EXPORT static void setMediaCacheDirectory(const String&);
    WEBCORE_EXPORT static const String& mediaCacheDirectory();
    WEBCORE_EXPORT static HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String&);
    WEBCORE_EXPORT static void clearMediaCache(const String&, std::chrono::system_clock::time_point modifiedSince = { });
    WEBCORE_EXPORT static void clearMediaCacheForOrigins(const String&, const HashSet<RefPtr<SecurityOrigin>>&);
    static void resetMediaEngines();

    bool isPlaying() const { return m_playing; }

    bool hasPendingActivity() const override;

#if ENABLE(WEB_AUDIO)
    MediaElementAudioSourceNode* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(MediaElementAudioSourceNode*);

    AudioSourceProvider* audioSourceProvider();
#endif

    using HTMLMediaElementEnums::InvalidURLAction;
    bool isSafeToLoadURL(const URL&, InvalidURLAction);

    const String& mediaGroup() const;
    void setMediaGroup(const String&);

    MediaController* controller() const;
    void setController(PassRefPtr<MediaController>);

    void enteredOrExitedFullscreen() { configureMediaControls(); }

    unsigned long long fileSize() const;

    void mediaLoadingFailed(MediaPlayerEnums::NetworkState);
    void mediaLoadingFailedFatally(MediaPlayerEnums::NetworkState);

#if ENABLE(MEDIA_SESSION)
    WEBCORE_EXPORT double playerVolume() const;

    const String& kind() const { return m_kind; }
    void setKind(const String& kind) { m_kind = kind; }

    MediaSession* session() const;
    void setSession(MediaSession*);

    void setShouldDuck(bool);

    static HTMLMediaElement* elementWithID(uint64_t);
    uint64_t elementID() const { return m_elementID; }
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<VideoPlaybackQuality> getVideoPlaybackQuality();
#endif

    MediaPlayerEnums::Preload preloadValue() const { return m_preload; }
    MediaElementSession& mediaSession() const { return *m_mediaSession; }

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    void pageScaleFactorChanged();
    WEBCORE_EXPORT String getCurrentMediaControlsStatus();
#endif

    MediaControlsHost* mediaControlsHost() { return m_mediaControlsHost.get(); }

    bool isDisablingSleep() const { return m_sleepDisabler.get(); }

    double maxBufferedTime() const;

    MediaProducer::MediaStateFlags mediaState() const override;

    void layoutSizeChanged();
    void visibilityDidChange();

    void allowsMediaDocumentInlinePlaybackChanged();
    void updateShouldPlay();

protected:
    HTMLMediaElement(const QualifiedName&, Document&, bool);
    virtual ~HTMLMediaElement();

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void finishParsingChildren() override;
    bool isURLAttribute(const Attribute&) const override;
    void willAttachRenderers() override;
    void didAttachRenderers() override;
    void willDetachRenderers() override;
    void didDetachRenderers() override;

    void didMoveToNewDocument(Document* oldDocument) override;

    enum DisplayMode { Unknown, None, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    
    bool isMediaElement() const final { return true; }

#if ENABLE(VIDEO_TRACK)
    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0 || !m_textTracks || !m_cueTree.size(); }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();
#endif

    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool mediaControlsDependOnPageScaleFactor() const { return m_mediaControlsDependOnPageScaleFactor; }
    void setMediaControlsDependOnPageScaleFactor(bool);
#endif

    void scheduleEvent(const AtomicString& eventName);

private:
    void createMediaPlayer();

    bool alwaysCreateUserAgentShadowRoot() const override { return true; }

    // FIXME: Shadow DOM spec says we should be able to create shadow root on audio and video elements
    bool canHaveUserAgentShadowRoot() const final { return true; }

    bool hasCustomFocusLogic() const override;
    bool supportsFocus() const override;
    bool isMouseFocusable() const override;
    bool rendererIsNeeded(const RenderStyle&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;
    void didRecalcStyle(Style::Change) override;

    void willBecomeFullscreenElement() override;
    void didBecomeFullscreenElement() override;
    void willStopBeingFullscreenElement() override;

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void suspend(ReasonForSuspension) override;
    void resume() override;
    void stop() override;
    void stopWithoutDestroyingMediaPlayer();
    void contextDestroyed() override;
    
    void mediaVolumeDidChange() override;

    void visibilityStateChanged() override;

    virtual void updateDisplayState() { }
    
    void setReadyState(MediaPlayerEnums::ReadyState);
    void setNetworkState(MediaPlayerEnums::NetworkState);

    double effectivePlaybackRate() const;
    double requestedPlaybackRate() const;

    void mediaPlayerNetworkStateChanged(MediaPlayer*) override;
    void mediaPlayerReadyStateChanged(MediaPlayer*) override;
    void mediaPlayerTimeChanged(MediaPlayer*) override;
    void mediaPlayerVolumeChanged(MediaPlayer*) override;
    void mediaPlayerMuteChanged(MediaPlayer*) override;
    void mediaPlayerDurationChanged(MediaPlayer*) override;
    void mediaPlayerRateChanged(MediaPlayer*) override;
    void mediaPlayerPlaybackStateChanged(MediaPlayer*) override;
    void mediaPlayerSawUnsupportedTracks(MediaPlayer*) override;
    void mediaPlayerResourceNotSupported(MediaPlayer*) override;
    void mediaPlayerRepaint(MediaPlayer*) override;
    void mediaPlayerSizeChanged(MediaPlayer*) override;
    bool mediaPlayerRenderingCanBeAccelerated(MediaPlayer*) override;
    void mediaPlayerRenderingModeChanged(MediaPlayer*) override;
    void mediaPlayerEngineUpdated(MediaPlayer*) override;
    void mediaEngineWasUpdated();

    void mediaPlayerFirstVideoFrameAvailable(MediaPlayer*) override;
    void mediaPlayerCharacteristicChanged(MediaPlayer*) override;

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaPlayerKeyAdded(MediaPlayer*, const String& keySystem, const String& sessionId) override;
    void mediaPlayerKeyError(MediaPlayer*, const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode) override;
    void mediaPlayerKeyMessage(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const URL& defaultURL) override;
    bool mediaPlayerKeyNeeded(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength) override;
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String& keyId) const override;
    bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) override;
    String mediaPlayerMediaKeysStorageDirectory() const override;
#endif
    
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*) override;
    void enqueuePlaybackTargetAvailabilityChangedEvent();

    using EventTarget::dispatchEvent;
    bool dispatchEvent(Event&) override;
#endif

#if ENABLE(MEDIA_SESSION)
    void setSessionInternal(MediaSession&);
#endif

    String mediaPlayerReferrer() const override;
    String mediaPlayerUserAgent() const override;

    bool mediaPlayerNeedsSiteSpecificHacks() const override;
    String mediaPlayerDocumentHost() const override;

    void mediaPlayerEnterFullscreen() override;
    void mediaPlayerExitFullscreen() override;
    bool mediaPlayerIsFullscreen() const override;
    bool mediaPlayerIsFullscreenPermitted() const override;
    bool mediaPlayerIsVideo() const override;
    LayoutRect mediaPlayerContentBoxRect() const override;
    float mediaPlayerContentsScale() const override;
    void mediaPlayerSetSize(const IntSize&) override;
    void mediaPlayerPause() override;
    void mediaPlayerPlay() override;
    bool mediaPlayerPlatformVolumeConfigurationRequired() const override;
    bool mediaPlayerIsPaused() const override;
    bool mediaPlayerIsLooping() const override;
    CachedResourceLoader* mediaPlayerCachedResourceLoader() override;
    RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() override;
    bool mediaPlayerShouldUsePersistentCache() const override;
    const String& mediaPlayerMediaCacheDirectory() const override;

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const override;
#endif

    bool mediaPlayerShouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge&) override;
    void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType command) override { didReceiveRemoteControlCommand(command); }
    String mediaPlayerSourceApplicationIdentifier() const override;
    Vector<String> mediaPlayerPreferredAudioCharacteristics() const override;

#if PLATFORM(IOS)
    String mediaPlayerNetworkInterfaceName() const override;
    bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const override;
#endif

    bool mediaPlayerIsInMediaDocument() const final;
    void mediaPlayerEngineFailedToLoad() const final;

    double mediaPlayerRequestedPlaybackRate() const final;
    VideoFullscreenMode mediaPlayerFullscreenMode() const final { return fullscreenMode(); }

#if USE(GSTREAMER)
    void requestInstallMissingPlugins(const String& details, const String& description, MediaPlayerRequestInstallMissingPluginsCallback&) final;
#endif

    void pendingActionTimerFired();
    void progressEventTimerFired();
    void playbackProgressTimerFired();
    void scanTimerFired();
    void seekTask();
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(const MediaTime&);
    void seekInternal(const MediaTime&);
    void seekWithTolerance(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance, bool fromDOM);
    void finishSeek();
    void clearSeeking();
    void addPlayedRange(const MediaTime& start, const MediaTime& end);
    
    void scheduleTimeupdateEvent(bool periodicEvent);
    virtual void scheduleResizeEvent() { }
    virtual void scheduleResizeEventIfSizeChanged() { }

    // loading
    void selectMediaResource();
    void loadResource(const URL&, ContentType&, const String& keySystem);
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer(DelayedActionType flags);
    bool havePotentialSourceChild();
    void noneSupported();
    void cancelPendingEventsAndCallbacks();
    void waitForSourceChange();
    void prepareToPlay();

    URL selectNextSourceChild(ContentType*, String* keySystem, InvalidURLAction);

#if ENABLE(VIDEO_TRACK)
    void updateActiveTextTrackCues(const MediaTime&);
    HTMLTrackElement* showingTrackWithSameKind(HTMLTrackElement*) const;

    enum ReconfigureMode {
        Immediately,
        AfterDelay,
    };
    void markCaptionAndSubtitleTracksAsUnconfigured(ReconfigureMode);
    void captionPreferencesChanged() override;
#endif

    // These "internal" functions do not check user gesture restrictions.
    void loadInternal();
    void playInternal();
    void pauseInternal();

    void prepareForLoad();
    void allowVideoRendering();

    bool processingMediaPlayerCallback() const { return m_processingMediaPlayerCallback > 0; }
    void beginProcessingMediaPlayerCallback() { ++m_processingMediaPlayerCallback; }
    void endProcessingMediaPlayerCallback() { ASSERT(m_processingMediaPlayerCallback); --m_processingMediaPlayerCallback; }

    void updateVolume();
    void updatePlayState();
    void setPlaying(bool);
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;
    bool canTransitionFromAutoplayToPlay() const;

    MediaTime minTimeSeekable() const;
    MediaTime maxTimeSeekable() const;

    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);

    void setPlaybackRateInternal(double);

    void mediaCanStart() override;

    void setShouldDelayLoadEvent(bool);
    void invalidateCachedTime() const;
    void refreshCachedTime() const;

    bool hasMediaControls() const;
    bool createMediaControls();
    void configureMediaControls();

    void prepareMediaFragmentURI();
    void applyMediaFragmentURI();

    void changeNetworkStateFromLoadingToIdle();

    void removeBehaviorsRestrictionsAfterFirstUserGesture();

    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool hasCurrentSrc() const override { return !m_currentSrc.isEmpty(); }
    bool isLiveStream() const override { return movieLoadType() == MediaPlayerEnums::LiveStream; }

    void updateSleepDisabling();
    bool shouldDisableSleep() const;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    void didAddUserAgentShadowRoot(ShadowRoot*) override;
    DOMWrapperWorld& ensureIsolatedWorld();
    bool ensureMediaControlsInjectedScript();
#endif

    // PlatformMediaSessionClient Overrides
    PlatformMediaSession::MediaType mediaType() const override;
    PlatformMediaSession::MediaType presentationType() const override;
    PlatformMediaSession::DisplayType displayType() const override;
    PlatformMediaSession::CharacteristicsFlags characteristics() const final;

    void suspendPlayback() override;
    void resumeAutoplaying() override;
    void mayResumePlayback(bool shouldResume) override;
    String mediaSessionTitle() const override;
    double mediaSessionDuration() const override { return duration(); }
    double mediaSessionCurrentTime() const override { return currentTime(); }
    bool canReceiveRemoteControlCommands() const override { return true; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType) override;
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const override;

    void pageMutedStateDidChange() override;

    bool effectiveMuted() const;

    void registerWithDocument(Document&);
    void unregisterWithDocument(Document&);

    void updateCaptionContainer();
    void ensureMediaControlsShadowRoot();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void prepareForDocumentSuspension() final;
    void resumeFromDocumentSuspension() final;

    enum class UpdateMediaState {
        Asynchronously,
        Synchronously,
    };
    void updateMediaState(UpdateMediaState updateState = UpdateMediaState::Synchronously);
    bool hasPlaybackTargetAvailabilityListeners() const { return m_hasPlaybackTargetAvailabilityListeners; }
#endif

    void isVisibleInViewportChanged() final;
    void updateShouldAutoplay();

    Timer m_pendingActionTimer;
    Timer m_progressEventTimer;
    Timer m_playbackProgressTimer;
    Timer m_scanTimer;
    GenericTaskQueue<ScriptExecutionContext> m_seekTaskQueue;
    GenericTaskQueue<ScriptExecutionContext> m_resizeTaskQueue;
    GenericTaskQueue<ScriptExecutionContext> m_shadowDOMTaskQueue;
    RefPtr<TimeRanges> m_playedTimeRanges;
    GenericEventQueue m_asyncEventQueue;

    double m_requestedPlaybackRate;
    double m_reportedPlaybackRate;
    double m_defaultPlaybackRate;
    bool m_webkitPreservesPitch;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    URL m_currentSrc;

    RefPtr<MediaError> m_error;

    struct PendingSeek {
        PendingSeek(const MediaTime& now, const MediaTime& targetTime, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
            : now(now)
            , targetTime(targetTime)
            , negativeTolerance(negativeTolerance)
            , positiveTolerance(positiveTolerance)
        {
        }
        MediaTime now;
        MediaTime targetTime;
        MediaTime negativeTolerance;
        MediaTime positiveTolerance;
    };
    std::unique_ptr<PendingSeek> m_pendingSeek;
    SeekType m_pendingSeekType { NoSeek };

    double m_volume;
    bool m_volumeInitialized;
    MediaTime m_lastSeekTime;
    
    double m_previousProgressTime;

    // The last time a timeupdate event was sent (based on monotonic clock).
    double m_clockTimeAtLastUpdateEvent;

    // The last time a timeupdate event was sent in movie time.
    MediaTime m_lastTimeUpdateEventMovieTime;
    
    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtr<HTMLSourceElement> m_currentSourceNode;
    RefPtr<Node> m_nextChildNodeToConsider;

    VideoFullscreenMode m_videoFullscreenMode;
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    FloatRect m_videoFullscreenFrame;
    MediaPlayerEnums::VideoGravity m_videoFullscreenGravity;
#endif

    std::unique_ptr<MediaPlayer> m_player;

    MediaPlayerEnums::Preload m_preload;

    DisplayMode m_displayMode;

    // Counter incremented while processing a callback from the media player, so we can avoid
    // calling the media engine recursively.
    int m_processingMediaPlayerCallback;

#if ENABLE(MEDIA_SESSION)
    String m_kind;
    RefPtr<MediaSession> m_session;
    bool m_shouldDuck { false };
    uint64_t m_elementID;
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSource> m_mediaSource;
    unsigned long m_droppedVideoFrames;
#endif

    mutable MediaTime m_cachedTime;
    mutable double m_clockTimeAtLastCachedTimeUpdate;
    mutable double m_minimumClockTimeToUpdateCachedTime;

    MediaTime m_fragmentStartTime;
    MediaTime m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    enum ActionAfterScanType {
        Nothing, Play, Pause
    };
    ActionAfterScanType m_actionAfterScan;

    enum ScanType { Seek, Scan };
    ScanType m_scanType;
    ScanDirection m_scanDirection;

    bool m_firstTimePlaying : 1;
    bool m_playing : 1;
    bool m_isWaitingUntilMediaCanStart : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_inActiveDocument : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_explicitlyMuted : 1;
    bool m_initiallyMuted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_pausedInternal : 1;

    // Not all media engines provide enough information about a file to be able to
    // support progress events so setting m_sendProgressEvents disables them 
    bool m_sendProgressEvents : 1;

    bool m_closedCaptionsVisible : 1;
    bool m_webkitLegacyClosedCaptionOverride : 1;
    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_parsingInProgress : 1;
    bool m_elementIsHidden : 1;
    bool m_creatingControls : 1;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool m_mediaControlsDependOnPageScaleFactor : 1;
    bool m_haveSetUpCaptionContainer : 1;
#endif

#if ENABLE(VIDEO_TRACK)
    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;

    String m_subtitleTrackLanguage;
    MediaTime m_lastTextTrackUpdateTime;

    CaptionUserPreferences::CaptionDisplayMode m_captionDisplayMode;

    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;
    RefPtr<VideoTrackList> m_videoTracks;
    Vector<RefPtr<TextTrack>> m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate;

    bool m_requireCaptionPreferencesChangedCallbacks { false };
#endif

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // The value is set just after the MediaElementAudioSourceNode is created.
    // The value is cleared in MediaElementAudioSourceNode::~MediaElementAudioSourceNode().
    MediaElementAudioSourceNode* m_audioSourceNode;
#endif

    String m_mediaGroup;
    friend class MediaController;
    RefPtr<MediaController> m_mediaController;

    std::unique_ptr<DisplaySleepDisabler> m_sleepDisabler;

    friend class TrackDisplayUpdateScope;

#if ENABLE(ENCRYPTED_MEDIA_V2)
    RefPtr<MediaKeys> m_mediaKeys;
#endif

    std::unique_ptr<MediaElementSession> m_mediaSession;
    PageActivityAssertionToken m_activityToken;
    size_t m_reportedExtraMemoryCost;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    friend class MediaControlsHost;
    RefPtr<MediaControlsHost> m_mediaControlsHost;
    RefPtr<DOMWrapperWorld> m_isolatedWorld;
#endif

#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStream> m_mediaStreamSrcObject;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    MediaProducer::MediaStateFlags m_mediaState { MediaProducer::IsNotPlaying };
    bool m_hasPlaybackTargetAvailabilityListeners { false };
    bool m_failedToPlayToWirelessTarget { false };
#endif
};

#if ENABLE(VIDEO_TRACK)
#ifndef NDEBUG
// Template specialization required by PodIntervalTree in debug mode.
template <>
struct ValueToString<TextTrackCue*> {
    static String string(TextTrackCue* const& cue)
    {
        String text;
        if (cue->isRenderable())
            text = toVTTCue(cue)->text();
        return String::format("%p id=%s interval=%s-->%s cue=%s)", cue, cue->id().utf8().data(), toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data(), text.utf8().data());
    }
};
#endif
#endif

#ifndef NDEBUG
template<>
struct ValueToString<MediaTime> {
    static String string(const MediaTime& time)
    {
        return toString(time);
    }
};
#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLMediaElement)
    static bool isType(const WebCore::Element& element) { return element.isMediaElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
#endif
