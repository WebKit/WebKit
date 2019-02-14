/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "ApplicationStateChangeListener.h"
#include "AutoplayEvent.h"
#include "DeferrableTask.h"
#include "GenericEventQueue.h"
#include "HTMLElement.h"
#include "HTMLMediaElementEnums.h"
#include "MediaCanStartListener.h"
#include "MediaControllerInterface.h"
#include "MediaElementSession.h"
#include "MediaPlayer.h"
#include "MediaProducer.h"
#include "VisibilityChangeClient.h"
#include <wtf/Function.h>
#include <wtf/LoggerHelper.h>
#include <wtf/WeakPtr.h>

#if ENABLE(VIDEO_TRACK)
#include "AudioTrack.h"
#include "CaptionUserPreferences.h"
#include "PODIntervalTree.h"
#include "TextTrack.h"
#include "TextTrackCue.h"
#include "VTTCue.h"
#include "VideoTrack.h"
#endif

#if USE(AUDIO_SESSION) && PLATFORM(MAC)
#include "AudioSession.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "CDMClient.h"
#endif

#ifndef NDEBUG
#include <wtf/StringPrintStream.h>
#endif

namespace PAL {
class SleepDisabler;
}

namespace WebCore {

class AudioSourceProvider;
class AudioTrackList;
class AudioTrackPrivate;
class Blob;
class DOMException;
class DOMWrapperWorld;
class DeferredPromise;
class Event;
class HTMLSourceElement;
class HTMLTrackElement;
class InbandTextTrackPrivate;
class JSDOMGlobalObject;
class MediaController;
class MediaControls;
class MediaControlsHost;
class MediaElementAudioSourceNode;
class MediaError;
class MediaKeys;
class MediaResourceLoader;
class MediaSession;
class MediaSource;
class MediaStream;
class RenderMedia;
class ScriptController;
class ScriptExecutionContext;
class SourceBuffer;
class TextTrackList;
class TimeRanges;
class VideoPlaybackQuality;
class VideoTrackList;
class VideoTrackPrivate;
class WebKitMediaKeys;

template<typename> class DOMPromiseDeferred;

#if ENABLE(VIDEO_TRACK)
using CueIntervalTree = PODIntervalTree<MediaTime, TextTrackCue*>;
using CueInterval = CueIntervalTree::IntervalType;
using CueList = Vector<CueInterval>;
#endif

using MediaProvider = Optional<Variant<
#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStream>,
#endif
#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSource>,
#endif
    RefPtr<Blob>>>;

class HTMLMediaElement
    : public HTMLElement
    , public ActiveDOMObject
    , public MediaControllerInterface
    , public CanMakeWeakPtr<HTMLMediaElement>
    , public PlatformMediaSessionClient
    , private MediaCanStartListener
    , private MediaPlayerClient
    , private MediaProducer
    , private VisibilityChangeClient
    , private ApplicationStateChangeListener
#if ENABLE(VIDEO_TRACK)
    , private AudioTrackClient
    , private TextTrackClient
    , private VideoTrackClient
#endif
#if USE(AUDIO_SESSION) && PLATFORM(MAC)
    , private AudioSession::MutedStateObserver
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    , private CDMClient
#endif
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(HTMLMediaElement);
public:
    RefPtr<MediaPlayer> player() const { return m_player; }

    virtual bool isVideo() const { return false; }
    bool hasVideo() const override { return false; }
    bool hasAudio() const override;

    static HashSet<HTMLMediaElement*>& allMediaElements();

    WEBCORE_EXPORT static RefPtr<HTMLMediaElement> bestMediaElementForShowingPlaybackControlsManager(MediaElementSession::PlaybackControlsPurpose);

    static bool isRunningDestructor();

    WEBCORE_EXPORT void rewind(double timeDelta);
    WEBCORE_EXPORT void returnToRealtime() override;

    // Eventually overloaded in HTMLVideoElement
    bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const override { return false; };

    bool supportsScanning() const override;

    bool canSaveMediaData() const;

    bool doesHaveAttribute(const AtomicString&, AtomicString* value = nullptr) const override;

    PlatformLayer* platformLayer() const;
    bool isVideoLayerInline();
    void setPreparedToReturnVideoLayerToInline(bool);
    void waitForPreparedForInlineThen(WTF::Function<void()>&& completionHandler = [] { });
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    void setVideoFullscreenLayer(PlatformLayer*, WTF::Function<void()>&& completionHandler = [] { });
#ifdef __OBJC__
    PlatformLayer* videoFullscreenLayer() const { return m_videoFullscreenLayer.get(); }
#endif
    void setVideoFullscreenFrame(FloatRect);
    void setVideoFullscreenGravity(MediaPlayerEnums::VideoGravity);
    MediaPlayerEnums::VideoGravity videoFullscreenGravity() const { return m_videoFullscreenGravity; }
#endif

    void scheduleCheckPlaybackTargetCompatability();
    void checkPlaybackTargetCompatablity();
    void scheduleResolvePendingPlayPromises();
    void scheduleRejectPendingPlayPromises(Ref<DOMException>&&);
    using PlayPromiseVector = Vector<DOMPromiseDeferred<void>>;
    void rejectPendingPlayPromises(PlayPromiseVector&&, Ref<DOMException>&&);
    void resolvePendingPlayPromises(PlayPromiseVector&&);
    void scheduleNotifyAboutPlaying();
    void notifyAboutPlaying(PlayPromiseVector&&);
    
    MediaPlayerEnums::MovieLoadType movieLoadType() const;
    
    bool inActiveDocument() const { return m_inActiveDocument; }

    Document* hostingDocument() const final { return &document(); }

// DOM API
// error state
    WEBCORE_EXPORT MediaError* error() const;

    const URL& currentSrc() const { return m_currentSrc; }

    const MediaProvider& srcObject() const { return m_mediaProvider; }
    void setSrcObject(MediaProvider&&);

    WEBCORE_EXPORT void setCrossOrigin(const AtomicString&);
    WEBCORE_EXPORT String crossOrigin() const;

// network state
    using HTMLMediaElementEnums::NetworkState;
    WEBCORE_EXPORT NetworkState networkState() const;

    WEBCORE_EXPORT String preload() const;
    WEBCORE_EXPORT void setPreload(const String&);

    Ref<TimeRanges> buffered() const override;
    WEBCORE_EXPORT void load();
    WEBCORE_EXPORT String canPlayType(const String& mimeType) const;

// ready state
    using HTMLMediaElementEnums::ReadyState;
    ReadyState readyState() const override;
    WEBCORE_EXPORT bool seeking() const;

// playback state
    WEBCORE_EXPORT double currentTime() const override;
    WEBCORE_EXPORT void setCurrentTime(double) override;
    void setCurrentTimeWithTolerance(double, double toleranceBefore, double toleranceAfter);
    double currentTimeForBindings() const { return currentTime(); }
    WEBCORE_EXPORT ExceptionOr<void> setCurrentTimeForBindings(double);
    WEBCORE_EXPORT double getStartDate() const;
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
    WEBCORE_EXPORT void fastSeek(const MediaTime&);

    void updatePlaybackRate();
    WEBCORE_EXPORT bool webkitPreservesPitch() const;
    WEBCORE_EXPORT void setWebkitPreservesPitch(bool);
    Ref<TimeRanges> played() override;
    Ref<TimeRanges> seekable() const override;
    double seekableTimeRangesLastModifiedTime() const;
    double liveUpdateInterval() const;
    WEBCORE_EXPORT bool ended() const;
    bool autoplay() const;
    bool isAutoplaying() const { return m_autoplaying; }
    bool loop() const;
    void setLoop(bool b);

    void play(DOMPromiseDeferred<void>&&);

    WEBCORE_EXPORT void play() override;
    WEBCORE_EXPORT void pause() override;
    void setShouldBufferData(bool);
    WEBCORE_EXPORT bool shouldBufferData() const { return m_shouldBufferData; }
    WEBCORE_EXPORT void fastSeek(double);
    double minFastReverseRate() const;
    double maxFastForwardRate() const;

    void purgeBufferedDataIfPossible();

// captions
    WEBCORE_EXPORT bool webkitHasClosedCaptions() const;
    WEBCORE_EXPORT bool webkitClosedCaptionsVisible() const;
    WEBCORE_EXPORT void setWebkitClosedCaptionsVisible(bool);

    bool elementIsHidden() const { return m_elementIsHidden; }

#if ENABLE(MEDIA_STATISTICS)
// Statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;
#endif

#if ENABLE(MEDIA_SOURCE)
//  Media Source.
    void detachMediaSource();
    void incrementDroppedFrameCount() { ++m_droppedVideoFrames; }
    size_t maximumSourceBufferSize(const SourceBuffer&) const;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    WebKitMediaKeys* webkitKeys() const { return m_webKitMediaKeys.get(); }
    void webkitSetMediaKeys(WebKitMediaKeys*);

    void keyAdded();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    MediaKeys* mediaKeys() const;

    void setMediaKeys(MediaKeys*, Ref<DeferredPromise>&&);
#endif

// controls
    WEBCORE_EXPORT bool controls() const;
    WEBCORE_EXPORT void setControls(bool);
    WEBCORE_EXPORT double volume() const override;
    WEBCORE_EXPORT ExceptionOr<void> setVolume(double) override;
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

    bool shouldForceControlsDisplay() const;

#if ENABLE(VIDEO_TRACK)
    ExceptionOr<TextTrack&> addTextTrack(const String& kind, const String& label, const String& language);

    AudioTrackList& ensureAudioTracks();
    TextTrackList& ensureTextTracks();
    VideoTrackList& ensureVideoTracks();
    AudioTrackList* audioTracks() const { return m_audioTracks.get(); }
    TextTrackList* textTracks() const { return m_textTracks.get(); }
    VideoTrackList* videoTracks() const { return m_videoTracks.get(); }

    CueList currentlyActiveCues() const { return m_currentlyActiveCues; }

    void addAudioTrack(Ref<AudioTrack>&&);
    void addTextTrack(Ref<TextTrack>&&);
    void addVideoTrack(Ref<VideoTrack>&&);
    void removeAudioTrack(Ref<AudioTrack>&&);
    void removeTextTrack(Ref<TextTrack>&&, bool scheduleEvent = true);
    void removeVideoTrack(Ref<VideoTrack>&&);
    void forgetResourceSpecificTracks();
    void closeCaptionTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    virtual void didAddTextTrack(HTMLTrackElement&);
    virtual void didRemoveTextTrack(HTMLTrackElement&);

    void mediaPlayerDidAddAudioTrack(AudioTrackPrivate&) final;
    void mediaPlayerDidAddTextTrack(InbandTextTrackPrivate&) final;
    void mediaPlayerDidAddVideoTrack(VideoTrackPrivate&) final;
    void mediaPlayerDidRemoveAudioTrack(AudioTrackPrivate&) final;
    void mediaPlayerDidRemoveTextTrack(InbandTextTrackPrivate&) final;
    void mediaPlayerDidRemoveVideoTrack(VideoTrackPrivate&) final;

#if ENABLE(AVF_CAPTIONS)
    Vector<RefPtr<PlatformTextTrack>> outOfBandTrackSources() final;
#endif

    struct TrackGroup;
    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void scheduleConfigureTextTracks();
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    void setSelectedTextTrack(TextTrack*);

    bool textTracksAreReady() const;
    using HTMLMediaElementEnums::TextTrackVisibilityCheckType;
    void configureTextTrackDisplay(TextTrackVisibilityCheckType checkType = CheckTextTrackVisibility);
    void updateTextTrackDisplay();

    // AudioTrackClient
    void audioTrackEnabledChanged(AudioTrack&) final;

    void textTrackReadyStateChanged(TextTrack*);

    // TextTrackClient
    void textTrackKindChanged(TextTrack&) override;
    void textTrackModeChanged(TextTrack&) override;
    void textTrackAddCues(TextTrack&, const TextTrackCueList&) override;
    void textTrackRemoveCues(TextTrack&, const TextTrackCueList&) override;
    void textTrackAddCue(TextTrack&, TextTrackCue&) override;
    void textTrackRemoveCue(TextTrack&, TextTrackCue&) override;

    // VideoTrackClient
    void videoTrackSelectedChanged(VideoTrack&) final;

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentation(TextTrackRepresentation*);
    void syncTextTrackBounds();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void webkitShowPlaybackTargetPicker();
    bool addEventListener(const AtomicString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomicString& eventType, EventListener&, const ListenerOptions&) override;

    void wirelessRoutesAvailableDidChange() override;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
#endif
    bool isPlayingToWirelessPlaybackTarget() const override { return m_isPlayingToWirelessTarget; };
    void setIsPlayingToWirelessTarget(bool);
    bool webkitCurrentPlaybackTargetIsWireless() const;

    void setPlayingOnSecondScreen(bool value);
    bool isPlayingOnSecondScreen() const override { return m_playingOnSecondScreen; }

    bool isPlayingToExternalTarget() const { return isPlayingToWirelessPlaybackTarget() || isPlayingOnSecondScreen(); }

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::scriptExecutionContext;

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }
    bool didPassCORSAccessCheck() const { return m_player && m_player->didPassCORSAccessCheck(); }
    bool wouldTaintOrigin(const SecurityOrigin& origin) const { return m_player && m_player->wouldTaintOrigin(origin); }
    
    WEBCORE_EXPORT bool isFullscreen() const override;
    bool isStandardFullscreen() const;
    void toggleStandardFullscreenState();

    using MediaPlayerEnums::VideoFullscreenMode;
    VideoFullscreenMode fullscreenMode() const { return m_videoFullscreenMode; }
    virtual void fullscreenModeChanged(VideoFullscreenMode);

    void enterFullscreen(VideoFullscreenMode);
    void enterFullscreen() override;
    WEBCORE_EXPORT void exitFullscreen();
    WEBCORE_EXPORT void setVideoFullscreenStandby(bool);

    bool hasClosedCaptions() const override;
    bool closedCaptionsVisible() const override;
    void setClosedCaptionsVisible(bool) override;

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement&);
    void sourceWasAdded(HTMLSourceElement&);

    void privateBrowsingStateDidChange() override;

    // Media cache management.
    WEBCORE_EXPORT static void setMediaCacheDirectory(const String&);
    WEBCORE_EXPORT static const String& mediaCacheDirectory();
    WEBCORE_EXPORT static HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String&);
    WEBCORE_EXPORT static void clearMediaCache(const String&, WallTime modifiedSince = { });
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
    void setController(RefPtr<MediaController>&&);

    MediaController* controllerForBindings() const { return controller(); }
    void setControllerForBindings(MediaController*);

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

    RefPtr<VideoPlaybackQuality> getVideoPlaybackQuality();

    MediaPlayerEnums::Preload preloadValue() const { return m_preload; }
    MediaElementSession& mediaSession() const { return *m_mediaSession; }

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    void pageScaleFactorChanged();
    void userInterfaceLayoutDirectionChanged();
    WEBCORE_EXPORT String getCurrentMediaControlsStatus();

    MediaControlsHost* mediaControlsHost() { return m_mediaControlsHost.get(); }
#endif

    bool isDisablingSleep() const { return m_sleepDisabler.get(); }

    double maxBufferedTime() const;

    MediaProducer::MediaStateFlags mediaState() const override;

    void layoutSizeChanged();
    void visibilityDidChange();

    void allowsMediaDocumentInlinePlaybackChanged();
    void updateShouldPlay();

    RenderMedia* renderer() const;

    void resetPlaybackSessionState();
    bool isVisibleInViewport() const;
    bool hasEverNotifiedAboutPlaying() const;
    void setShouldDelayLoadEvent(bool);

    bool hasEverHadAudio() const { return m_hasEverHadAudio; }
    bool hasEverHadVideo() const { return m_hasEverHadVideo; }

    double playbackStartedTime() const { return m_playbackStartedTime; }

    bool isTemporarilyAllowingInlinePlaybackAfterFullscreen() const {return m_temporarilyAllowingInlinePlaybackAfterFullscreen; }

    void isVisibleInViewportChanged();
    void updateRateChangeRestrictions();

    WEBCORE_EXPORT const MediaResourceLoader* lastMediaResourceLoaderForTesting() const;

#if ENABLE(MEDIA_STREAM)
    void mediaStreamCaptureStarted() { resumeAutoplaying(); }
    bool hasMediaStreamSrcObject() const { return m_mediaProvider && WTF::holds_alternative<RefPtr<MediaStream>>(*m_mediaProvider); }
#endif

    bool supportsSeeking() const override;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return *m_logger.get(); }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(m_logIdentifier); }
    WTFLogChannel& logChannel() const final;
#endif

    bool willLog(WTFLogLevel) const;

    bool isSuspended() const final;

    WEBCORE_EXPORT void didBecomeFullscreenElement() override;
    WEBCORE_EXPORT void willExitFullscreen();

    enum class AutoplayEventPlaybackState { None, PreventedAutoplay, StartedWithUserGesture, StartedWithoutUserGesture };

protected:
    HTMLMediaElement(const QualifiedName&, Document&, bool createdByParser);
    virtual void finishInitialization();
    virtual ~HTMLMediaElement();

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void finishParsingChildren() override;
    bool isURLAttribute(const Attribute&) const override;
    void willAttachRenderers() override;
    void didAttachRenderers() override;
    void willDetachRenderers() override;
    void didDetachRenderers() override;

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

    enum DisplayMode { Unknown, None, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    
    bool isMediaElement() const final { return true; }

#if ENABLE(VIDEO_TRACK)
    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0 || !m_textTracks || !m_cueTree.size(); }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();
#endif

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool mediaControlsDependOnPageScaleFactor() const { return m_mediaControlsDependOnPageScaleFactor; }
    void setMediaControlsDependOnPageScaleFactor(bool);
    void updateMediaControlsAfterPresentationModeChange();
#endif

    void scheduleEvent(const AtomicString& eventName);

private:
    void createMediaPlayer();

    bool supportsFocus() const override;
    bool isMouseFocusable() const override;
    bool rendererIsNeeded(const RenderStyle&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void didRecalcStyle(Style::Change) override;

    void willBecomeFullscreenElement() override;
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
    bool mediaPlayerAcceleratedCompositingEnabled() override;
    void mediaPlayerEngineUpdated(MediaPlayer*) override;

    void scheduleMediaEngineWasUpdated();
    void mediaEngineWasUpdated();

    void mediaPlayerFirstVideoFrameAvailable(MediaPlayer*) override;
    void mediaPlayerCharacteristicChanged(MediaPlayer*) override;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String& keyId) const override;
    bool mediaPlayerKeyNeeded(MediaPlayer*, Uint8Array*) override;
    String mediaPlayerMediaKeysStorageDirectory() const override;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaPlayerInitializationDataEncountered(const String&, RefPtr<ArrayBuffer>&&) final;
    void mediaPlayerWaitingForKeyChanged() final;

    void attemptToDecrypt();
    void attemptToResumePlaybackIfNecessary();

    // CDMClient
    void cdmClientAttemptToResumePlaybackIfNecessary() final;
#endif
    
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void mediaPlayerCurrentPlaybackTargetIsWirelessChanged(MediaPlayer*) override;
    void enqueuePlaybackTargetAvailabilityChangedEvent();

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) override;
#endif

#if ENABLE(MEDIA_SESSION)
    void setSessionInternal(MediaSession&);
#endif

    String mediaPlayerReferrer() const override;
    String mediaPlayerUserAgent() const override;

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

    void mediaPlayerActiveSourceBuffersChanged(const MediaPlayer*) override;

    void mediaPlayerHandlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType command) override { didReceiveRemoteControlCommand(command, nullptr); }
    String sourceApplicationIdentifier() const override;
    String mediaPlayerSourceApplicationIdentifier() const override { return sourceApplicationIdentifier(); }
    Vector<String> mediaPlayerPreferredAudioCharacteristics() const override;

#if PLATFORM(IOS_FAMILY)
    String mediaPlayerNetworkInterfaceName() const override;
    bool mediaPlayerGetRawCookies(const URL&, Vector<Cookie>&) const override;
#endif

    bool mediaPlayerIsInMediaDocument() const final;
    void mediaPlayerEngineFailedToLoad() const final;

    double mediaPlayerRequestedPlaybackRate() const final;
    VideoFullscreenMode mediaPlayerFullscreenMode() const final { return fullscreenMode(); }
    bool mediaPlayerIsVideoFullscreenStandby() const final { return m_videoFullscreenStandby; }
    bool mediaPlayerShouldDisableSleep() const final { return shouldDisableSleep() == SleepType::Display; }
    bool mediaPlayerShouldCheckHardwareSupport() const final;
    const Vector<ContentType>& mediaContentTypesRequiringHardwareSupport() const final;

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
    void cancelPendingTasks();
    void closeTaskQueues();

    void seek(const MediaTime&);
    void seekInternal(const MediaTime&);
    void seekWithTolerance(const MediaTime&, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance, bool fromDOM);
    void finishSeek();
    void clearSeeking();
    void addPlayedRange(const MediaTime& start, const MediaTime& end);
    
    void scheduleTimeupdateEvent(bool periodicEvent);
    virtual void scheduleResizeEvent() { }
    virtual void scheduleResizeEventIfSizeChanged() { }

    void selectMediaResource();
    void loadResource(const URL&, ContentType&, const String& keySystem);
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer();
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
    CaptionUserPreferences::CaptionDisplayMode captionDisplayMode();
#endif

    // These "internal" functions do not check user gesture restrictions.
    void playInternal();
    void pauseInternal();

    void prepareForLoad();
    void allowVideoRendering();

    bool processingMediaPlayerCallback() const { return m_processingMediaPlayerCallback > 0; }
    void beginProcessingMediaPlayerCallback() { ++m_processingMediaPlayerCallback; }
    void endProcessingMediaPlayerCallback() { ASSERT(m_processingMediaPlayerCallback); --m_processingMediaPlayerCallback; }

    void scheduleUpdatePlayState();
    void updatePlayState();

    void updateVolume();
    void setPlaying(bool);
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;
    void dispatchPlayPauseEventsIfNeedsQuirks();
    SuccessOr<MediaPlaybackDenialReason> canTransitionFromAutoplayToPlay() const;

    void setAutoplayEventPlaybackState(AutoplayEventPlaybackState);
    void userDidInterfereWithAutoplay();
    void handleAutoplayEvent(AutoplayEvent);

    MediaTime minTimeSeekable() const;
    MediaTime maxTimeSeekable() const;

    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);

    void setPlaybackRateInternal(double);

    void mediaCanStart(Document&) final;

    void invalidateCachedTime() const;
    void refreshCachedTime() const;

    bool hasMediaControls() const;
    bool createMediaControls();
    void configureMediaControls();

    void prepareMediaFragmentURI();
    void applyMediaFragmentURI();

    void changeNetworkStateFromLoadingToIdle();

    void removeBehaviorsRestrictionsAfterFirstUserGesture(MediaElementSession::BehaviorRestrictions mask = MediaElementSession::AllRestrictions);

    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool hasCurrentSrc() const override { return !m_currentSrc.isEmpty(); }
    bool isLiveStream() const override { return movieLoadType() == MediaPlayerEnums::LiveStream; }

    void updateSleepDisabling();
    enum class SleepType {
        None,
        Display,
        System,
    };
    SleepType shouldDisableSleep() const;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    void didAddUserAgentShadowRoot(ShadowRoot&) override;
    DOMWrapperWorld& ensureIsolatedWorld();
    bool ensureMediaControlsInjectedScript();
#endif

    PlatformMediaSession::MediaType mediaType() const override;
    PlatformMediaSession::MediaType presentationType() const override;
    PlatformMediaSession::DisplayType displayType() const override;
    PlatformMediaSession::CharacteristicsFlags characteristics() const final;

    void suspendPlayback() override;
    void resumeAutoplaying() override;
    void mayResumePlayback(bool shouldResume) override;
    uint64_t mediaSessionUniqueIdentifier() const final;
    String mediaSessionTitle() const override;
    double mediaSessionDuration() const override { return duration(); }
    double mediaSessionCurrentTime() const override { return currentTime(); }
    bool canReceiveRemoteControlCommands() const override { return true; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*) override;
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const override;
    bool shouldOverrideBackgroundLoadingRestriction() const override;
    bool canProduceAudio() const final;
    bool processingUserGestureForMedia() const final;

    void pageMutedStateDidChange() override;

#if USE(AUDIO_SESSION) && PLATFORM(MAC)
    void hardwareMutedStateDidChange(AudioSession*) final;
#endif

    bool effectiveMuted() const;

    void registerWithDocument(Document&);
    void unregisterWithDocument(Document&);

    void updateCaptionContainer();
    void ensureMediaControlsShadowRoot();

    using JSSetupFunction = WTF::Function<bool(JSDOMGlobalObject&, JSC::ExecState&, ScriptController&, DOMWrapperWorld&)>;
    bool setupAndCallJS(const JSSetupFunction&);

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void prepareForDocumentSuspension() final;
    void resumeFromDocumentSuspension() final;

    void scheduleUpdateMediaState();
    void updateMediaState();
    bool hasPlaybackTargetAvailabilityListeners() const { return m_hasPlaybackTargetAvailabilityListeners; }
#endif

    bool isVideoTooSmallForInlinePlayback();
    void updateShouldAutoplay();

    void pauseAfterDetachedTask();
    void updatePlaybackControlsManager();
    void schedulePlaybackControlsManagerUpdate();
    void playbackControlsManagerBehaviorRestrictionsTimerFired();

    void updateRenderer();

    void updatePageScaleFactorJSProperty();
    void updateUsesLTRUserInterfaceLayoutDirectionJSProperty();
    void setControllerJSProperty(const char*, JSC::JSValue);

    void addBehaviorRestrictionsOnEndIfNecessary();
    void handleSeekToPlaybackPosition(double);
    void seekToPlaybackPositionEndedTimerFired();

    void applicationWillResignActive() final;
    void applicationDidBecomeActive() final;

    void setInActiveDocument(bool);

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "HTMLMediaElement"; }

    const void* mediaPlayerLogIdentifier() final { return logIdentifier(); }
    const Logger& mediaPlayerLogger() final { return logger(); }
#endif

    Timer m_progressEventTimer;
    Timer m_playbackProgressTimer;
    Timer m_scanTimer;
    Timer m_playbackControlsManagerBehaviorRestrictionsTimer;
    Timer m_seekToPlaybackPositionEndedTimer;
    DeferrableTask<Timer> m_configureTextTracksTask;
    DeferrableTask<Timer> m_checkPlaybackTargetCompatablityTask;
    DeferrableTask<Timer> m_updateMediaStateTask;
    DeferrableTask<Timer> m_mediaEngineUpdatedTask;
    DeferrableTask<Timer> m_updatePlayStateTask;
    DeferrableTask<Timer> m_resumeTaskQueue;
    DeferrableTask<Timer> m_seekTaskQueue;
    DeferrableTask<Timer> m_playbackControlsManagerBehaviorRestrictionsQueue;
    GenericTaskQueue<Timer> m_promiseTaskQueue;
    GenericTaskQueue<Timer> m_pauseAfterDetachedTaskQueue;
    GenericTaskQueue<Timer> m_resourceSelectionTaskQueue;
    GenericTaskQueue<Timer> m_visibilityChangeTaskQueue;
    GenericTaskQueue<Timer> m_fullscreenTaskQueue;
    GenericTaskQueue<Timer> m_playbackTargetIsWirelessQueue;
    RefPtr<TimeRanges> m_playedTimeRanges;
    GenericEventQueue m_asyncEventQueue;
#if PLATFORM(IOS_FAMILY)
    DeferrableTask<Timer> m_volumeRevertTaskQueue;
#endif

    PlayPromiseVector m_pendingPlayPromises;

    double m_requestedPlaybackRate { 1 };
    double m_reportedPlaybackRate { 1 };
    double m_defaultPlaybackRate { 1 };
    bool m_webkitPreservesPitch { true };
    NetworkState m_networkState { NETWORK_EMPTY };
    ReadyState m_readyState { HAVE_NOTHING };
    ReadyState m_readyStateMaximum { HAVE_NOTHING };
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

    double m_volume { 1 };
    bool m_volumeInitialized { false };
    MediaTime m_lastSeekTime;
    
    MonotonicTime m_previousProgressTime { MonotonicTime::infinity() };
    double m_playbackStartedTime { 0 };

    // The last time a timeupdate event was sent (based on monotonic clock).
    MonotonicTime m_clockTimeAtLastUpdateEvent;

    // The last time a timeupdate event was sent in movie time.
    MediaTime m_lastTimeUpdateEventMovieTime;
    
    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState { WaitingForSource };
    RefPtr<HTMLSourceElement> m_currentSourceNode;
    RefPtr<HTMLSourceElement> m_nextChildNodeToConsider;

    VideoFullscreenMode m_videoFullscreenMode { VideoFullscreenModeNone };
    bool m_videoFullscreenStandby { false };
    bool m_preparedForInline;
    WTF::Function<void()> m_preparedForInlineCompletionHandler;

    bool m_temporarilyAllowingInlinePlaybackAfterFullscreen { false };

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    FloatRect m_videoFullscreenFrame;
    MediaPlayerEnums::VideoGravity m_videoFullscreenGravity { MediaPlayer::VideoGravityResizeAspect };
#endif

    RefPtr<MediaPlayer> m_player;

    MediaPlayerEnums::Preload m_preload { MediaPlayer::Auto };

    DisplayMode m_displayMode { Unknown };

    // Counter incremented while processing a callback from the media player, so we can avoid
    // calling the media engine recursively.
    int m_processingMediaPlayerCallback { 0 };

#if ENABLE(MEDIA_SESSION)
    String m_kind;
    RefPtr<MediaSession> m_session;
    bool m_shouldDuck { false };
    uint64_t m_elementID;
#endif

#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSource> m_mediaSource;
    unsigned m_droppedVideoFrames { 0 };
#endif

    mutable MediaTime m_cachedTime;
    mutable MonotonicTime m_clockTimeAtLastCachedTimeUpdate;
    mutable MonotonicTime m_minimumClockTimeToUpdateCachedTime;

    MediaTime m_fragmentStartTime;
    MediaTime m_fragmentEndTime;

    using PendingActionFlags = unsigned;
    PendingActionFlags m_pendingActionFlags { 0 };

    enum ActionAfterScanType { Nothing, Play, Pause };
    ActionAfterScanType m_actionAfterScan { Nothing };

    enum ScanType { Seek, Scan };
    ScanType m_scanType { Scan };
    ScanDirection m_scanDirection { Forward };

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
    bool m_seekRequested : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_pausedInternal : 1;

    bool m_closedCaptionsVisible : 1;
    bool m_webkitLegacyClosedCaptionOverride : 1;
    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_parsingInProgress : 1;
    bool m_shouldBufferData : 1;
    bool m_elementIsHidden : 1;
    bool m_elementWasRemovedFromDOM : 1;
    bool m_creatingControls : 1;
    bool m_receivedLayoutSizeChanged : 1;
    bool m_hasEverNotifiedAboutPlaying : 1;

    bool m_hasEverHadAudio : 1;
    bool m_hasEverHadVideo : 1;

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    bool m_mediaControlsDependOnPageScaleFactor : 1;
    bool m_haveSetUpCaptionContainer : 1;
#endif

    bool m_isScrubbingRemotely : 1;
    bool m_waitingToEnterFullscreen : 1;

#if ENABLE(VIDEO_TRACK)
    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;

    AutoplayEventPlaybackState m_autoplayEventPlaybackState { AutoplayEventPlaybackState::None };

    String m_subtitleTrackLanguage;
    MediaTime m_lastTextTrackUpdateTime { -1, 1 };

    Optional<CaptionUserPreferences::CaptionDisplayMode> m_captionDisplayMode;

    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;
    RefPtr<VideoTrackList> m_videoTracks;
    Vector<RefPtr<TextTrack>> m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate { 0 };

    bool m_requireCaptionPreferencesChangedCallbacks { false };
#endif

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // The value is set just after the MediaElementAudioSourceNode is created.
    // The value is cleared in MediaElementAudioSourceNode::~MediaElementAudioSourceNode().
    MediaElementAudioSourceNode* m_audioSourceNode { nullptr };
#endif

    String m_mediaGroup;
    friend class MediaController;
    RefPtr<MediaController> m_mediaController;

    std::unique_ptr<PAL::SleepDisabler> m_sleepDisabler;

    WeakPtr<const MediaResourceLoader> m_lastMediaResourceLoaderForTesting;

    friend class TrackDisplayUpdateScope;

    RefPtr<Blob> m_blob;
    MediaProvider m_mediaProvider;

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<WebKitMediaKeys> m_webKitMediaKeys;
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    RefPtr<MediaKeys> m_mediaKeys;
    bool m_attachingMediaKeys { false };
    bool m_playbackBlockedWaitingForKey { false };
    GenericTaskQueue<Timer> m_encryptedMediaQueue;
#endif

    std::unique_ptr<MediaElementSession> m_mediaSession;
    size_t m_reportedExtraMemoryCost { 0 };

#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> m_logger;
    uint64_t m_logIdentifier;
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    friend class MediaControlsHost;
    RefPtr<MediaControlsHost> m_mediaControlsHost;
    RefPtr<DOMWrapperWorld> m_isolatedWorld;
#endif

#if ENABLE(MEDIA_STREAM)
    RefPtr<MediaStream> m_mediaStreamSrcObject;
    bool m_settingMediaStreamSrcObject { false };
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    MediaProducer::MediaStateFlags m_mediaState { MediaProducer::IsNotPlaying };
    bool m_hasPlaybackTargetAvailabilityListeners { false };
    bool m_failedToPlayToWirelessTarget { false };
#endif

    bool m_isPlayingToWirelessTarget { false };
    bool m_playingOnSecondScreen { false };
};

String convertEnumerationToString(HTMLMediaElement::AutoplayEventPlaybackState);

} // namespace WebCore

namespace WTF {

template <>
struct LogArgument<WebCore::HTMLMediaElement::AutoplayEventPlaybackState> {
    static String toString(const WebCore::HTMLMediaElement::AutoplayEventPlaybackState reason)
    {
        return convertEnumerationToString(reason);
    }
};
    
} // namespace WTF

#if ENABLE(VIDEO_TRACK) && !defined(NDEBUG)
namespace WTF {

// Template specialization required by PodIntervalTree in debug mode.
template <> struct ValueToString<WebCore::TextTrackCue*> {
    static String string(WebCore::TextTrackCue* const& cue)
    {
        String text;
        if (cue->isRenderable())
            text = WebCore::toVTTCue(cue)->text();
        return String::format("%p id=%s interval=%s-->%s cue=%s)", cue, cue->id().utf8().data(), toString(cue->startTime()).utf8().data(), toString(cue->endTime()).utf8().data(), text.utf8().data());
    }
};

} // namespace WTF
#endif

#ifndef NDEBUG
namespace WTF {

template<> struct ValueToString<MediaTime> {
    static String string(const MediaTime& time)
    {
        return toString(time);
    }
};

} // namespace WTF
#endif

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLMediaElement)
    static bool isType(const WebCore::Element& element) { return element.isMediaElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
