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

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#if ENABLE(VIDEO)
#include "HTMLElement.h"
#include "ActiveDOMObject.h"
#include "GenericEventQueue.h"
#include "HTMLMediaSession.h"
#include "MediaCanStartListener.h"
#include "MediaControllerInterface.h"
#include "MediaPlayer.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLFrameOwnerElement.h"
#include "HTMLPlugInImageElement.h"
#include "MediaPlayerProxy.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "AudioTrack.h"
#include "CaptionUserPreferences.h"
#include "PODIntervalTree.h"
#include "TextTrack.h"
#include "TextTrackCue.h"
#include "VTTCue.h"
#include "VideoTrack.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#endif


namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class MediaElementAudioSourceNode;
#endif
class Event;
class HTMLSourceElement;
class HTMLTrackElement;
class URL;
class MediaController;
class MediaControls;
class MediaControlsHost;
class MediaError;
class PageActivityAssertionToken;
class TimeRanges;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
class Widget;
#endif
#if PLATFORM(COCOA)
class DisplaySleepDisabler;
#endif
#if ENABLE(ENCRYPTED_MEDIA_V2)
class MediaKeys;
#endif
#if ENABLE(MEDIA_SOURCE)
class MediaSource;
class VideoPlaybackQuality;
#endif

#if ENABLE(VIDEO_TRACK)
class AudioTrackList;
class AudioTrackPrivate;
class InbandTextTrackPrivate;
class TextTrackList;
class VideoTrackList;
class VideoTrackPrivate;

typedef PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;
#endif

class HTMLMediaElement
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    : public HTMLFrameOwnerElement
#else
    : public HTMLElement
#endif
    , private MediaPlayerClient, public MediaPlayerSupportsTypeClient, private MediaCanStartListener, public ActiveDOMObject, public MediaControllerInterface , public MediaSessionClient
#if ENABLE(VIDEO_TRACK)
    , private AudioTrackClient
    , private TextTrackClient
    , private VideoTrackClient
#endif
#if USE(PLATFORM_TEXT_TRACK_MENU)
    , public PlatformTextTrackMenuClient
#endif
{
public:
    MediaPlayer* player() const { return m_player.get(); }

    virtual bool isVideo() const { return false; }
    virtual bool hasVideo() const override { return false; }
    virtual bool hasAudio() const override;

    void rewind(double timeDelta);
    virtual void returnToRealtime() override;

    // Eventually overloaded in HTMLVideoElement
    virtual bool supportsFullscreen() const override { return false; };

    virtual bool supportsSave() const;
    virtual bool supportsScanning() const override;
    
    virtual bool doesHaveAttribute(const AtomicString&) const override;

    PlatformMedia platformMedia() const;
    PlatformLayer* platformLayer() const;
#if PLATFORM(IOS)
    void setVideoFullscreenLayer(PlatformLayer*);
    void setVideoFullscreenFrame(FloatRect);
    void setVideoFullscreenGravity(MediaPlayer::VideoGravity);
#endif

    enum DelayedActionType {
        LoadMediaResource = 1 << 0,
        ConfigureTextTracks = 1 << 1,
        TextTrackChangesNotification = 1 << 2,
        ConfigureTextTrackDisplay = 1 << 3,
    };
    void scheduleDelayedAction(DelayedActionType);
    
    MediaPlayer::MovieLoadType movieLoadType() const;
    
    bool inActiveDocument() const { return m_inActiveDocument; }
    
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
    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_NO_SOURCE };
    NetworkState networkState() const;

    String preload() const;    
    void setPreload(const String&);

    virtual PassRefPtr<TimeRanges> buffered() const override;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String(), const URL& = URL()) const;

// ready state
    virtual ReadyState readyState() const override;
    bool seeking() const;

// playback state
    virtual double currentTime() const override;
    virtual void setCurrentTime(double) override;
    virtual void setCurrentTime(double, ExceptionCode&);
    virtual double duration() const override;
    virtual bool paused() const override;
    virtual double defaultPlaybackRate() const override;
    virtual void setDefaultPlaybackRate(double) override;
    virtual double playbackRate() const override;
    virtual void setPlaybackRate(double) override;
    void updatePlaybackRate();
    bool webkitPreservesPitch() const;
    void setWebkitPreservesPitch(bool);
    virtual PassRefPtr<TimeRanges> played() override;
    virtual PassRefPtr<TimeRanges> seekable() const override;
    bool ended() const;
    bool autoplay() const;
    bool loop() const;    
    void setLoop(bool b);
    virtual void play() override;
    virtual void pause() override;
    void fastSeek(double);

// captions
    bool webkitHasClosedCaptions() const;
    bool webkitClosedCaptionsVisible() const;
    void setWebkitClosedCaptionsVisible(bool);

#if ENABLE(MEDIA_STATISTICS)
// Statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;
#endif

#if ENABLE(MEDIA_SOURCE)
//  Media Source.
    void closeMediaSource();
    void incrementDroppedFrameCount() { ++m_droppedVideoFrames; }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void webkitGenerateKeyRequest(const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionCode&);
    void webkitGenerateKeyRequest(const String& keySystem, ExceptionCode&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionCode&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionCode&);
    void webkitCancelKeyRequest(const String& keySystem, const String& sessionId, ExceptionCode&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);
#endif
#if ENABLE(ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA_V2)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitneedkey);
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    MediaKeys* keys() const { return m_mediaKeys.get(); }
    void setMediaKeys(MediaKeys*);
#endif

// controls
    bool controls() const;
    void setControls(bool);
    virtual double volume() const override;
    virtual void setVolume(double, ExceptionCode&) override;
    virtual bool muted() const override;
    virtual void setMuted(bool) override;

    void togglePlayState();
    virtual void beginScrubbing() override;
    virtual void endScrubbing() override;

    virtual void beginScanning(ScanDirection) override;
    virtual void endScanning() override;
    double nextScanRate();

    virtual bool canPlay() const override;

    double percentLoaded() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(emptied);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadedmetadata);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadeddata);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(canplay);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(canplaythrough);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(playing);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(waiting);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(durationchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(timeupdate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(play);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(pause);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ratechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(volumechange);

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
    void removeTextTrack(TextTrack*);
    void removeVideoTrack(VideoTrack*);
    void removeAllInbandTracks();
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

#if USE(PLATFORM_TEXT_TRACK_MENU)
    virtual void setSelectedTextTrack(PassRefPtr<PlatformTextTrack>) override;
    virtual Vector<RefPtr<PlatformTextTrack>> platformTextTracks() override;
    PlatformTextTrackMenuInterface* platformTextTrackMenu();
#endif

    struct TrackGroup {
        enum GroupKind { CaptionsAndSubtitles, Description, Chapter, Metadata, Other };

        TrackGroup(GroupKind kind)
            : visibleTrack(0)
            , defaultTrack(0)
            , kind(kind)
            , hasSrcLang(false)
        {
        }

        Vector<RefPtr<TextTrack>> tracks;
        RefPtr<TextTrack> visibleTrack;
        RefPtr<TextTrack> defaultTrack;
        GroupKind kind;
        bool hasSrcLang;
    };

    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    void setSelectedTextTrack(TextTrack*);

    bool textTracksAreReady() const;
    enum TextTrackVisibilityCheckType { CheckTextTrackVisibility, AssumeTextTrackVisibilityChanged };
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
#endif

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    void ensureMediaPlayer();
    void setNeedWidgetUpdate(bool needWidgetUpdate) { m_needWidgetUpdate = needWidgetUpdate; }
    void deliverNotification(MediaPlayerProxyNotificationType notification);
    void setMediaPlayerProxy(WebMediaPlayerProxy* proxy);
    void getPluginProxyParams(URL& url, Vector<String>& names, Vector<String>& values);
    void createMediaPlayerProxy();
    void updateWidget(PluginCreationOption);
#endif

#if ENABLE(IOS_AIRPLAY)
    void webkitShowPlaybackTargetPicker();
    bool webkitCurrentPlaybackTargetIsWireless() const;

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) override;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitcurrentplaybacktargetiswirelesschanged);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitplaybacktargetavailabilitychanged);
#endif

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::scriptExecutionContext;

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }
    
    virtual bool isFullscreen() const override;
    void toggleFullscreenState();
    virtual void enterFullscreen() override;
    void exitFullscreen();

    virtual bool hasClosedCaptions() const override;
    virtual bool closedCaptionsVisible() const override;
    virtual void setClosedCaptionsVisible(bool) override;

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    virtual void privateBrowsingStateDidChange() override;

    // Media cache management.
    static void getSitesInMediaCache(Vector<String>&);
    static void clearMediaCache();
    static void clearMediaCacheForSite(const String&);
    static void resetMediaEngines();

    bool isPlaying() const { return m_playing; }

    virtual bool hasPendingActivity() const override;

#if ENABLE(WEB_AUDIO)
    MediaElementAudioSourceNode* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(MediaElementAudioSourceNode*);

    AudioSourceProvider* audioSourceProvider();
#endif

    enum InvalidURLAction { DoNothing, Complain };
    bool isSafeToLoadURL(const URL&, InvalidURLAction);

    const String& mediaGroup() const;
    void setMediaGroup(const String&);

    MediaController* controller() const;
    void setController(PassRefPtr<MediaController>);

#if !PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

    void enteredOrExitedFullscreen() { configureMediaControls(); }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    bool shouldUseVideoPluginProxy() const;
#endif

    unsigned long long fileSize() const;

    void mediaLoadingFailed(MediaPlayer::NetworkState);
    void mediaLoadingFailedFatally(MediaPlayer::NetworkState);

#if ENABLE(MEDIA_SOURCE)
    RefPtr<VideoPlaybackQuality> getVideoPlaybackQuality();
#endif

    MediaPlayer::Preload preloadValue() const { return m_preload; }
    HTMLMediaSession& mediaSession() const { return *m_mediaSession; }

protected:
    HTMLMediaElement(const QualifiedName&, Document&, bool);
    virtual ~HTMLMediaElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void finishParsingChildren() override;
    virtual bool isURLAttribute(const Attribute&) const override;
    virtual void willAttachRenderers() override;
    virtual void didAttachRenderers() override;

    virtual void didMoveToNewDocument(Document* oldDocument) override;

    enum DisplayMode { Unknown, None, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    
    virtual bool isMediaElement() const override { return true; }

#if ENABLE(VIDEO_TRACK)
    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0 || !m_textTracks || !m_cueTree.size(); }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();
#endif

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;

private:
    void createMediaPlayer();

    virtual bool alwaysCreateUserAgentShadowRoot() const override { return true; }

    virtual bool hasCustomFocusLogic() const override;
    virtual bool supportsFocus() const override;
    virtual bool isMouseFocusable() const override;
    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual bool childShouldCreateRenderer(const Node&) const override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void didRecalcStyle(Style::Change) override;

    virtual void defaultEventHandler(Event*) override;

    virtual void didBecomeFullscreenElement() override;
    virtual void willStopBeingFullscreenElement() override;

    // ActiveDOMObject functions.
    virtual bool canSuspend() const override;
    virtual void suspend(ReasonForSuspension) override;
    virtual void resume() override;
    virtual void stop() override;
    
    virtual void mediaVolumeDidChange() override;

#if ENABLE(PAGE_VISIBILITY_API)
    virtual void visibilityStateChanged() override;
#endif

    virtual void updateDisplayState() { }
    
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    virtual Document* mediaPlayerOwningDocument() override;
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
    virtual bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) override;
#endif

#if ENABLE(IOS_AIRPLAY)
    virtual void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*) override;
    virtual void mediaPlayerPlaybackTargetAvailabilityChanged(MediaPlayer*) override;
    void enqueuePlaybackTargetAvailabilityChangedEvent();
#endif

    virtual String mediaPlayerReferrer() const override;
    virtual String mediaPlayerUserAgent() const override;
    virtual CORSMode mediaPlayerCORSMode() const override;

    virtual bool mediaPlayerNeedsSiteSpecificHacks() const override;
    virtual String mediaPlayerDocumentHost() const override;

    virtual void mediaPlayerEnterFullscreen() override;
    virtual void mediaPlayerExitFullscreen() override;
    virtual bool mediaPlayerIsFullscreen() const override;
    virtual bool mediaPlayerIsFullscreenPermitted() const override;
    virtual bool mediaPlayerIsVideo() const override;
    virtual LayoutRect mediaPlayerContentBoxRect() const override;
    virtual void mediaPlayerSetSize(const IntSize&) override;
    virtual void mediaPlayerPause() override;
    virtual void mediaPlayerPlay() override;
    virtual bool mediaPlayerPlatformVolumeConfigurationRequired() const override;
    virtual bool mediaPlayerIsPaused() const override;
    virtual bool mediaPlayerIsLooping() const override;
    virtual HostWindow* mediaPlayerHostWindow() override;
    virtual IntRect mediaPlayerWindowClipRect() override;
    virtual CachedResourceLoader* mediaPlayerCachedResourceLoader() override;

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* mediaPlayerGraphicsDeviceAdapter(const MediaPlayer*) const override;
#endif

    virtual bool mediaPlayerShouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge&) override;

    void loadTimerFired(Timer<HTMLMediaElement>&);
    void progressEventTimerFired(Timer<HTMLMediaElement>&);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>&);
    void scanTimerFired(Timer<HTMLMediaElement>&);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(double time);
    void seekWithTolerance(double time, double negativeTolerance, double positiveTolerance);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(double start, double end);
    
    void scheduleTimeupdateEvent(bool periodicEvent);
    void scheduleEvent(const AtomicString& eventName);
    
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
    void updateActiveTextTrackCues(double);
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
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;

    double minTimeSeekable() const;
    double maxTimeSeekable() const;

#if PLATFORM(IOS)
    bool parseMediaPlayerAttribute(const QualifiedName&, const AtomicString&);
#endif

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
    virtual bool isLiveStream() const override { return movieLoadType() == MediaPlayer::LiveStream; }
    bool isAutoplaying() const { return m_autoplaying; }

    void updateSleepDisabling();
#if PLATFORM(COCOA)
    bool shouldDisableSleep() const;
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;
    DOMWrapperWorld& ensureIsolatedWorld();
    bool ensureMediaControlsInjectedScript();
#endif

    // MediaSessionClient Overrides
    virtual MediaSession::MediaType mediaType() const override;
    virtual void pausePlayback() override;
    virtual void resumePlayback() override;
    virtual String mediaSessionTitle() const override;
    virtual double mediaSessionDuration() const override { return duration(); }
    virtual double mediaSessionCurrentTime() const override { return currentTime(); }
    virtual bool canReceiveRemoteControlCommands() const override { return true; }
    virtual void didReceiveRemoteControlCommand(MediaSession::RemoteControlCommandType) override;

    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    Timer<HTMLMediaElement> m_scanTimer;
    RefPtr<TimeRanges> m_playedTimeRanges;
    GenericEventQueue m_asyncEventQueue;

    double m_playbackRate;
    double m_defaultPlaybackRate;
    bool m_webkitPreservesPitch;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    URL m_currentSrc;

    RefPtr<MediaError> m_error;

    double m_volume;
    bool m_volumeInitialized;
    double m_lastSeekTime;
    
    unsigned m_previousProgress;
    double m_previousProgressTime;

    // The last time a timeupdate event was sent (based on monotonic clock).
    double m_clockTimeAtLastUpdateEvent;

    // The last time a timeupdate event was sent in movie time.
    double m_lastTimeUpdateEventMovieTime;
    
    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtr<HTMLSourceElement> m_currentSourceNode;
    RefPtr<Node> m_nextChildNodeToConsider;

#if PLATFORM(IOS)
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    FloatRect m_videoFullscreenFrame;
    MediaPlayer::VideoGravity m_videoFullscreenGravity;
#endif

    OwnPtr<MediaPlayer> m_player;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    RefPtr<Widget> m_proxyWidget;
#endif

    MediaPlayer::Preload m_preload;

    DisplayMode m_displayMode;

    // Counter incremented while processing a callback from the media player, so we can avoid
    // calling the media engine recursively.
    int m_processingMediaPlayerCallback;

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSource> m_mediaSource;
    unsigned long m_droppedVideoFrames;
#endif

    mutable double m_cachedTime;
    mutable double m_clockTimeAtLastCachedTimeUpdate;
    mutable double m_minimumClockTimeToUpdateCachedTime;

    double m_fragmentStartTime;
    double m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    enum ActionAfterScanType {
        Nothing, Play, Pause
    };
    ActionAfterScanType m_actionAfterScan;

    enum ScanType { Seek, Scan };
    ScanType m_scanType;
    ScanDirection m_scanDirection;

    bool m_playing : 1;
    bool m_isWaitingUntilMediaCanStart : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_inActiveDocument : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_explicitlyMuted : 1;
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

    bool m_isFullscreen : 1;
    bool m_closedCaptionsVisible : 1;
    bool m_webkitLegacyClosedCaptionOverride : 1;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    bool m_needWidgetUpdate : 1;
#endif

    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_parsingInProgress : 1;
#if ENABLE(PAGE_VISIBILITY_API)
    bool m_isDisplaySleepDisablingSuspended : 1;
#endif

#if PLATFORM(IOS)
    bool m_requestingPlay : 1;
#endif

#if ENABLE(VIDEO_TRACK)
    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;

    String m_subtitleTrackLanguage;
    float m_lastTextTrackUpdateTime;

    CaptionUserPreferences::CaptionDisplayMode m_captionDisplayMode;

    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;
    RefPtr<VideoTrackList> m_videoTracks;
    Vector<RefPtr<TextTrack>> m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate;
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

#if PLATFORM(COCOA)
    OwnPtr<DisplaySleepDisabler> m_sleepDisabler;
#endif

    friend class TrackDisplayUpdateScope;

#if ENABLE(ENCRYPTED_MEDIA_V2)
    RefPtr<MediaKeys> m_mediaKeys;
#endif

#if USE(PLATFORM_TEXT_TRACK_MENU)
    RefPtr<PlatformTextTrackMenuInterface> m_platformMenu;
#endif

    std::unique_ptr<HTMLMediaSession> m_mediaSession;
    std::unique_ptr<PageActivityAssertionToken> m_activityToken;
    size_t m_reportedExtraMemoryCost;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    RefPtr<MediaControlsHost> m_mediaControlsHost;
    RefPtr<DOMWrapperWorld> m_isolatedWorld;
#endif

#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStream> m_mediaStreamSrcObject;
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
        return String::format("%p id=%s interval=%f-->%f cue=%s)", cue, cue->id().utf8().data(), cue->startTime(), cue->endTime(), text.utf8().data());
    }
};
#endif
#endif

void isHTMLMediaElement(const HTMLMediaElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isHTMLMediaElement(const Element& element) { return element.isMediaElement(); }
inline bool isHTMLMediaElement(const Node& node) { return node.isElementNode() && toElement(node).isMediaElement(); }
template <> inline bool isElementOfType<const HTMLMediaElement>(const Element& element) { return element.isMediaElement(); }

NODE_TYPE_CASTS(HTMLMediaElement)

} //namespace

#endif
#endif
