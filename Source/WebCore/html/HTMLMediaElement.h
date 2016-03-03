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
    virtual bool hasVideo() const override { return false; }
    virtual bool hasAudio() const override;

    static HashSet<HTMLMediaElement*>& allMediaElements();

    void rewind(double timeDelta);
    WEBCORE_EXPORT virtual void returnToRealtime() override;

    // Eventually overloaded in HTMLVideoElement
    virtual bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const override { return false; };

    virtual bool supportsScanning() const override;

    bool canSaveMediaData() const;

    virtual bool doesHaveAttribute(const AtomicString&, AtomicString* value = nullptr) const override;

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
    void setSrcObject(MediaStream*);
#endif

// network state
    using HTMLMediaElementEnums::NetworkState;
    NetworkState networkState() const;

    String preload() const;    
    void setPreload(const String&);

    virtual PassRefPtr<TimeRanges> buffered() const override;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String(), const URL& = URL()) const;

// ready state
    using HTMLMediaElementEnums::ReadyState;
    virtual ReadyState readyState() const override;
    bool seeking() const;

// playback state
    WEBCORE_EXPORT virtual double currentTime() const override;
    virtual void setCurrentTime(double) override;
    virtual void setCurrentTime(double, ExceptionCode&);
    virtual double getStartDate() const;
    WEBCORE_EXPORT virtual double duration() const override;
    WEBCORE_EXPORT virtual bool paused() const override;
    virtual double defaultPlaybackRate() const override;
    virtual void setDefaultPlaybackRate(double) override;
    WEBCORE_EXPORT virtual double playbackRate() const override;
    virtual void setPlaybackRate(double) override;

// MediaTime versions of playback state
    MediaTime currentMediaTime() const;
    void setCurrentTime(const MediaTime&);
    MediaTime durationMediaTime() const;
    void fastSeek(const MediaTime&);

    void updatePlaybackRate();
    bool webkitPreservesPitch() const;
    void setWebkitPreservesPitch(bool);
    virtual PassRefPtr<TimeRanges> played() override;
    virtual PassRefPtr<TimeRanges> seekable() const override;
    WEBCORE_EXPORT bool ended() const;
    bool autoplay() const;
    bool isAutoplaying() const { return m_autoplaying; }
    bool loop() const;
    void setLoop(bool b);
    WEBCORE_EXPORT virtual void play() override;
    WEBCORE_EXPORT virtual void pause() override;
    virtual void setShouldBufferData(bool) override;
    void fastSeek(double);
    double minFastReverseRate() const;
    double maxFastForwardRate() const;

    void purgeBufferedDataIfPossible();

// captions
    bool webkitHasClosedCaptions() const;
    bool webkitClosedCaptionsVisible() const;
    void setWebkitClosedCaptionsVisible(bool);

    virtual bool elementIsHidden() const override { return m_elementIsHidden; }

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
    WEBCORE_EXPORT virtual double volume() const override;
    virtual void setVolume(double, ExceptionCode&) override;
    WEBCORE_EXPORT virtual bool muted() const override;
    WEBCORE_EXPORT virtual void setMuted(bool) override;

    WEBCORE_EXPORT void togglePlayState();
    WEBCORE_EXPORT virtual void beginScrubbing() override;
    WEBCORE_EXPORT virtual void endScrubbing() override;

    virtual void beginScanning(ScanDirection) override;
    virtual void endScanning() override;
    double nextScanRate();

    WEBCORE_EXPORT virtual bool canPlay() const override;

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

    virtual void mediaPlayerDidAddAudioTrack(PassRefPtr<AudioTrackPrivate>) override;
    virtual void mediaPlayerDidAddTextTrack(PassRefPtr<InbandTextTrackPrivate>) override;
    virtual void mediaPlayerDidAddVideoTrack(PassRefPtr<VideoTrackPrivate>) override;
    virtual void mediaPlayerDidRemoveAudioTrack(PassRefPtr<AudioTrackPrivate>) override;
    virtual void mediaPlayerDidRemoveTextTrack(PassRefPtr<InbandTextTrackPrivate>) override;
    virtual void mediaPlayerDidRemoveVideoTrack(PassRefPtr<VideoTrackPrivate>) override;

#if ENABLE(AVF_CAPTIONS)
    virtual Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() override;
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
    virtual void audioTrackEnabledChanged(AudioTrack*) override;

    // TextTrackClient
    virtual void textTrackReadyStateChanged(TextTrack*);
    virtual void textTrackKindChanged(TextTrack*) override;
    virtual void textTrackModeChanged(TextTrack*) override;
    virtual void textTrackAddCues(TextTrack*, const TextTrackCueList*) override;
    virtual void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) override;
    virtual void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) override;
    virtual void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) override;

    // VideoTrackClient
    virtual void videoTrackSelectedChanged(VideoTrack*) override;

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void webkitShowPlaybackTargetPicker();
    virtual bool addEventListener(const AtomicString& eventType, RefPtr<EventListener>&&, bool useCapture) override;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;

    virtual void wirelessRoutesAvailableDidChange() override;
    virtual bool canPlayToWirelessPlaybackTarget() const override;
    virtual bool isPlayingToWirelessPlaybackTarget() const override;
    virtual void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    virtual void setShouldPlayToPlaybackTarget(bool) override;
    virtual void customPlaybackActionSelected() override;
    String playbackTargetPickerCustomActionName() const;
#endif
    bool webkitCurrentPlaybackTargetIsWireless() const;

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::scriptExecutionContext;

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }
    
    WEBCORE_EXPORT virtual bool isFullscreen() const override;
    void toggleFullscreenState();

    using MediaPlayerEnums::VideoFullscreenMode;
    VideoFullscreenMode fullscreenMode() const { return m_videoFullscreenMode; }
    virtual void fullscreenModeChanged(VideoFullscreenMode mode) { m_videoFullscreenMode = mode; }

    void enterFullscreen(VideoFullscreenMode);
    virtual void enterFullscreen() override;
    WEBCORE_EXPORT void exitFullscreen();

    virtual bool hasClosedCaptions() const override;
    virtual bool closedCaptionsVisible() const override;
    virtual void setClosedCaptionsVisible(bool) override;

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    virtual void privateBrowsingStateDidChange() override;

    // Media cache management.
    WEBCORE_EXPORT static void getSitesInMediaCache(Vector<String>&);
    WEBCORE_EXPORT static void clearMediaCache();
    WEBCORE_EXPORT static void clearMediaCacheForSite(const String&);
    static void resetMediaEngines();

    bool isPlaying() const { return m_playing; }

    virtual bool hasPendingActivity() const override;

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

    virtual MediaProducer::MediaStateFlags mediaState() const override;

    void layoutSizeChanged();
    void visibilityDidChange();

    void allowsMediaDocumentInlinePlaybackChanged();

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
    
    virtual bool isMediaElement() const override final { return true; }

#if ENABLE(VIDEO_TRACK)
    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0 || !m_textTracks || !m_cueTree.size(); }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();
#endif

    virtual RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool mediaControlsDependOnPageScaleFactor() const { return m_mediaControlsDependOnPageScaleFactor; }
    void setMediaControlsDependOnPageScaleFactor(bool);
#endif

    void scheduleEvent(const AtomicString& eventName);

private:
    void createMediaPlayer();

    virtual bool alwaysCreateUserAgentShadowRoot() const override { return true; }

    // FIXME: Shadow DOM spec says we should be able to create shadow root on audio and video elements
    virtual bool canHaveUserAgentShadowRoot() const override final { return true; }

    virtual bool hasCustomFocusLogic() const override;
    virtual bool supportsFocus() const override;
    virtual bool isMouseFocusable() const override;
    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual bool childShouldCreateRenderer(const Node&) const override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void didRecalcStyle(Style::Change) override;

    virtual void willBecomeFullscreenElement() override;
    virtual void didBecomeFullscreenElement() override;
    virtual void willStopBeingFullscreenElement() override;

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;
    void suspend(ReasonForSuspension) override;
    void resume() override;
    void stop() override;
    void stopWithoutDestroyingMediaPlayer();
    virtual void contextDestroyed() override;
    
    virtual void mediaVolumeDidChange() override;

    virtual void visibilityStateChanged() override;

    virtual void updateDisplayState() { }
    
    void setReadyState(MediaPlayerEnums::ReadyState);
    void setNetworkState(MediaPlayerEnums::NetworkState);

    double effectivePlaybackRate() const;
    double requestedPlaybackRate() const;

    virtual void mediaPlayerNetworkStateChanged(MediaPlayer*) override;
    virtual void mediaPlayerReadyStateChanged(MediaPlayer*) override;
    virtual void mediaPlayerTimeChanged(MediaPlayer*) override;
    virtual void mediaPlayerVolumeChanged(MediaPlayer*) override;
    virtual void mediaPlayerMuteChanged(MediaPlayer*) override;
    virtual void mediaPlayerDurationChanged(MediaPlayer*) override;
    virtual void mediaPlayerRateChanged(MediaPlayer*) override;
    virtual void mediaPlayerPlaybackStateChanged(MediaPlayer*) override;
    virtual void mediaPlayerSawUnsupportedTracks(MediaPlayer*) override;
    virtual void mediaPlayerResourceNotSupported(MediaPlayer*) override;
    virtual void mediaPlayerRepaint(MediaPlayer*) override;
    virtual void mediaPlayerSizeChanged(MediaPlayer*) override;
    virtual bool mediaPlayerRenderingCanBeAccelerated(MediaPlayer*) override;
    virtual void mediaPlayerRenderingModeChanged(MediaPlayer*) override;
    virtual void mediaPlayerEngineUpdated(MediaPlayer*) override;
    
    virtual void mediaPlayerFirstVideoFrameAvailable(MediaPlayer*) override;
    virtual void mediaPlayerCharacteristicChanged(MediaPlayer*) override;

#if ENABLE(ENCRYPTED_MEDIA)
    virtual void mediaPlayerKeyAdded(MediaPlayer*, const String& keySystem, const String& sessionId) override;
    virtual void mediaPlayerKeyError(MediaPlayer*, const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode) override;
    virtual void mediaPlayerKeyMessage(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const URL& defaultURL) override;
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength) override;
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    virtual RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String& keyId) const override;
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) override;
    virtual String mediaPlayerMediaKeysStorageDirectory() const override;
#endif
    
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*) override;
    void enqueuePlaybackTargetAvailabilityChangedEvent();

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(Event&) override;
#endif

#if ENABLE(MEDIA_SESSION)
    void setSessionInternal(MediaSession&);
#endif

    virtual String mediaPlayerReferrer() const override;
    virtual String mediaPlayerUserAgent() const override;

    virtual bool mediaPlayerNeedsSiteSpecificHacks() const override;
    virtual String mediaPlayerDocumentHost() const override;

    virtual void mediaPlayerEnterFullscreen() override;
    virtual void mediaPlayerExitFullscreen() override;
    virtual bool mediaPlayerIsFullscreen() const override;
    virtual bool mediaPlayerIsFullscreenPermitted() const override;
    virtual bool mediaPlayerIsVideo() const override;
    virtual LayoutRect mediaPlayerContentBoxRect() const override;
    virtual float mediaPlayerContentsScale() const override;
    virtual void mediaPlayerSetSize(const IntSize&) override;
    virtual void mediaPlayerPause() override;
    virtual void mediaPlayerPlay() override;
    virtual bool mediaPlayerPlatformVolumeConfigurationRequired() const override;
    virtual bool mediaPlayerIsPaused() const override;
    virtual bool mediaPlayerIsLooping() const override;
    virtual CachedResourceLoader* mediaPlayerCachedResourceLoader() override;
    virtual RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() override;

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const override;
#endif

    virtual bool mediaPlayerShouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge&) override;
    virtual void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType command) override { didReceiveRemoteControlCommand(command); }
    virtual String mediaPlayerSourceApplicationIdentifier() const override;
    virtual Vector<String> mediaPlayerPreferredAudioCharacteristics() const override;

#if PLATFORM(IOS)
    virtual String mediaPlayerNetworkInterfaceName() const override;
    virtual bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const override;
#endif

    virtual bool mediaPlayerIsInMediaDocument() const override final;
    virtual void mediaPlayerEngineFailedToLoad() const override final;

    virtual double mediaPlayerRequestedPlaybackRate() const override final;
    virtual VideoFullscreenMode mediaPlayerFullscreenMode() const override final { return fullscreenMode(); }

#if USE(GSTREAMER)
    virtual void requestInstallMissingPlugins(const String& details, const String& description, MediaPlayerRequestInstallMissingPluginsCallback&) override final;
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
    void clearMediaPlayer(int flags);
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
    virtual void captionPreferencesChanged() override;
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

    MediaTime minTimeSeekable() const;
    MediaTime maxTimeSeekable() const;

    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);

    void setPlaybackRateInternal(double);

    virtual void mediaCanStart() override;

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
    virtual bool hasCurrentSrc() const override { return !m_currentSrc.isEmpty(); }
    virtual bool isLiveStream() const override { return movieLoadType() == MediaPlayerEnums::LiveStream; }

    void updateSleepDisabling();
    bool shouldDisableSleep() const;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;
    DOMWrapperWorld& ensureIsolatedWorld();
    bool ensureMediaControlsInjectedScript();
#endif

    // PlatformMediaSessionClient Overrides
    PlatformMediaSession::MediaType mediaType() const override;
    PlatformMediaSession::MediaType presentationType() const override;
    PlatformMediaSession::DisplayType displayType() const override;
    void suspendPlayback() override;
    void resumeAutoplaying() override;
    void mayResumePlayback(bool shouldResume) override;
    String mediaSessionTitle() const override;
    double mediaSessionDuration() const override { return duration(); }
    double mediaSessionCurrentTime() const override { return currentTime(); }
    bool canReceiveRemoteControlCommands() const override { return true; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType) override;
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const override;

    virtual void pageMutedStateDidChange() override;

    bool effectiveMuted() const;

    void registerWithDocument(Document&);
    void unregisterWithDocument(Document&);

    void updateCaptionContainer();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual void prepareForDocumentSuspension() override final;
    virtual void resumeFromDocumentSuspension() override final;

    enum class UpdateMediaState {
        Asynchronously,
        Synchronously,
    };
    void updateMediaState(UpdateMediaState updateState = UpdateMediaState::Synchronously);
    bool hasPlaybackTargetAvailabilityListeners() const { return m_hasPlaybackTargetAvailabilityListeners; }
#endif

    void isVisibleInViewportChanged() override final;
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

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool m_mediaControlsDependOnPageScaleFactor : 1;
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
