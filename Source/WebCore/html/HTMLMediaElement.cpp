/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"

#include "ApplicationCacheHost.h"
#include "ApplicationCacheResource.h"
#include "Attribute.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ClientRect.h"
#include "ClientRectList.h"
#include "ContentSecurityPolicy.h"
#include "ContentType.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentLoader.h"
#include "ElementShadow.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include "Language.h"
#include "Logging.h"
#include "MediaController.h"
#include "MediaControls.h"
#include "MediaDocument.h"
#include "MediaError.h"
#include "MediaFragmentURIParser.h"
#include "MediaKeyError.h"
#include "MediaKeyEvent.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "MediaQueryEvaluator.h"
#include "MouseEvent.h"
#include "MIMETypeRegistry.h"
#include "NodeRenderingContext.h"
#include "Page.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptEventListener.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TimeRanges.h"
#include "UUID.h"
#include <limits>
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/NonCopyingSort.h>
#include <wtf/Uint8Array.h>
#include <wtf/text/CString.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderView.h"
#include "RenderLayerCompositor.h"
#endif

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "RenderEmbeddedObject.h"
#include "Widget.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "HTMLTrackElement.h"
#include "RuntimeEnabledFeatures.h"
#include "TextTrackCueList.h"
#include "TextTrackList.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProvider.h"
#include "MediaElementAudioSourceNode.h"
#endif

#if PLATFORM(MAC)
#include "DisplaySleepDisabler.h"
#endif

using namespace std;

namespace WebCore {

#if !LOG_DISABLED
static String urlForLogging(const KURL& url)
{
    static const unsigned maximumURLLengthForLogging = 128;

    if (url.string().length() < maximumURLLengthForLogging)
        return url.string();
    return url.string().substring(0, maximumURLLengthForLogging) + "...";
}

static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

#ifndef LOG_MEDIA_EVENTS
// Default to not logging events because so many are generated they can overwhelm the rest of 
// the logging.
#define LOG_MEDIA_EVENTS 0
#endif

#ifndef LOG_CACHED_TIME_WARNINGS
// Default to not logging warnings about excessive drift in the cached media time because it adds a
// fair amount of overhead and logging.
#define LOG_CACHED_TIME_WARNINGS 0
#endif

static const float invalidMediaTime = -1;

#if ENABLE(MEDIA_SOURCE)
// URL protocol used to signal that the media source API is being used.
static const char* mediaSourceURLProtocol = "x-media-source";
#endif

using namespace HTMLNames;
using namespace std;

typedef HashMap<Document*, HashSet<HTMLMediaElement*> > DocumentElementSetMap;
static DocumentElementSetMap& documentToElementSetMap()
{
    DEFINE_STATIC_LOCAL(DocumentElementSetMap, map, ());
    return map;
}

static void addElementToDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(document);
    set.add(element);
    map.add(document, set);
}

static void removeElementFromDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(document);
    set.remove(element);
    if (!set.isEmpty())
        map.add(document, set);
}

#if ENABLE(ENCRYPTED_MEDIA)
static ExceptionCode exceptionCodeForMediaKeyException(MediaPlayer::MediaKeyException exception)
{
    switch (exception) {
    case MediaPlayer::NoError:
        return 0;
    case MediaPlayer::InvalidPlayerState:
        return INVALID_STATE_ERR;
    case MediaPlayer::KeySystemNotSupported:
        return NOT_SUPPORTED_ERR;
    }

    ASSERT_NOT_REACHED();
    return INVALID_STATE_ERR;
}
#endif

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(document, this)
    , m_loadTimer(this, &HTMLMediaElement::loadTimerFired)
    , m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_playedTimeRanges()
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_playbackRate(1.0f)
    , m_defaultPlaybackRate(1.0f)
    , m_webkitPreservesPitch(true)
    , m_networkState(NETWORK_EMPTY)
    , m_readyState(HAVE_NOTHING)
    , m_readyStateMaximum(HAVE_NOTHING)
    , m_volume(1.0f)
    , m_lastSeekTime(0)
    , m_previousProgress(0)
    , m_previousProgressTime(numeric_limits<double>::max())
    , m_lastTimeUpdateEventWallTime(0)
    , m_lastTimeUpdateEventMovieTime(numeric_limits<float>::max())
    , m_loadState(WaitingForSource)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_proxyWidget(0)
#endif
    , m_restrictions(RequireUserGestureForFullscreenRestriction | RequirePageConsentToLoadMediaRestriction)
    , m_preload(MediaPlayer::Auto)
    , m_displayMode(Unknown)
    , m_processingMediaPlayerCallback(0)
#if ENABLE(MEDIA_SOURCE)      
    , m_sourceState(SOURCE_CLOSED)
#endif
    , m_cachedTime(invalidMediaTime)
    , m_cachedTimeWallClockUpdateTime(0)
    , m_minimumWallClockTimeToCacheMediaTime(0)
    , m_fragmentStartTime(invalidMediaTime)
    , m_fragmentEndTime(invalidMediaTime)
    , m_pendingLoadFlags(0)
    , m_playing(false)
    , m_isWaitingUntilMediaCanStart(false)
    , m_shouldDelayLoadEvent(false)
    , m_haveFiredLoadedData(false)
    , m_inActiveDocument(true)
    , m_autoplaying(true)
    , m_muted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_pausedInternal(false)
    , m_sendProgressEvents(true)
    , m_isFullscreen(false)
    , m_closedCaptionsVisible(false)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_needWidgetUpdate(false)
#endif
    , m_dispatchingCanPlayEvent(false)
    , m_loadInitiatedByUserGesture(false)
    , m_completelyLoaded(false)
    , m_havePreparedToPlay(false)
    , m_parsingInProgress(createdByParser)
#if ENABLE(VIDEO_TRACK)
    , m_tracksAreReady(true)
    , m_haveVisibleTextTrack(false)
    , m_lastTextTrackUpdateTime(-1)
    , m_textTracks(0)
    , m_ignoreTrackDisplayUpdate(0)
#endif
#if ENABLE(WEB_AUDIO)
    , m_audioSourceNode(0)
#endif
{
    LOG(Media, "HTMLMediaElement::HTMLMediaElement");
    document->registerForMediaVolumeCallbacks(this);
    document->registerForPrivateBrowsingStateChangedCallbacks(this);
    
    if (document->settings() && document->settings()->mediaPlaybackRequiresUserGesture()) {
        addBehaviorRestriction(RequireUserGestureForRateChangeRestriction);
        addBehaviorRestriction(RequireUserGestureForLoadRestriction);
    }

#if ENABLE(MEDIA_SOURCE)
    m_mediaSourceURL.setProtocol(mediaSourceURLProtocol);
    m_mediaSourceURL.setPath(createCanonicalUUIDString());
#endif

    setHasCustomWillOrDidRecalcStyle();
    addElementToDocumentMap(this, document);
}

HTMLMediaElement::~HTMLMediaElement()
{
    LOG(Media, "HTMLMediaElement::~HTMLMediaElement");
    if (m_isWaitingUntilMediaCanStart)
        document()->removeMediaCanStartListener(this);
    setShouldDelayLoadEvent(false);
    document()->unregisterForMediaVolumeCallbacks(this);
    document()->unregisterForPrivateBrowsingStateChangedCallbacks(this);
#if ENABLE(VIDEO_TRACK)
    if (m_textTracks)
        m_textTracks->clearOwner();
    if (m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i)
            m_textTracks->item(i)->clearClient();
    }
#endif

    if (m_mediaController)
        m_mediaController->removeMediaElement(this);

    removeElementFromDocumentMap(this, document());
}

void HTMLMediaElement::didMoveToNewDocument(Document* oldDocument)
{
    if (m_isWaitingUntilMediaCanStart) {
        if (oldDocument)
            oldDocument->removeMediaCanStartListener(this);
        document()->addMediaCanStartListener(this);
    }

    if (m_shouldDelayLoadEvent) {
        if (oldDocument)
            oldDocument->decrementLoadEventDelayCount();
        document()->incrementLoadEventDelayCount();
    }

    if (oldDocument) {
        oldDocument->unregisterForMediaVolumeCallbacks(this);
        removeElementFromDocumentMap(this, oldDocument);
    }

    document()->registerForMediaVolumeCallbacks(this);
    addElementToDocumentMap(this, document());

    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLMediaElement::supportsFocus() const
{
    if (ownerDocument()->isMediaDocument())
        return false;

    // If no controls specified, we should still be able to focus the element if it has tabIndex.
    return controls() ||  HTMLElement::supportsFocus();
}

bool HTMLMediaElement::isMouseFocusable() const
{
    return false;
}

void HTMLMediaElement::parseAttribute(Attribute* attr)
{
    const QualifiedName& attrName = attr->name();

    if (attrName == srcAttr) {
        // Trigger a reload, as long as the 'src' attribute is present.
        if (fastHasAttribute(srcAttr))
            scheduleLoad(MediaResource);
    } else if (attrName == controlsAttr)
        configureMediaControls();
#if PLATFORM(MAC)
    else if (attrName == loopAttr)
        updateDisableSleep();
#endif
    else if (attrName == preloadAttr) {
        String value = attr->value();

        if (equalIgnoringCase(value, "none"))
            m_preload = MediaPlayer::None;
        else if (equalIgnoringCase(value, "metadata"))
            m_preload = MediaPlayer::MetaData;
        else {
            // The spec does not define an "invalid value default" but "auto" is suggested as the
            // "missing value default", so use it for everything except "none" and "metadata"
            m_preload = MediaPlayer::Auto;
        }

        // The attribute must be ignored if the autoplay attribute is present
        if (!autoplay() && m_player)
            m_player->setPreload(m_preload);

    } else if (attrName == mediagroupAttr)
        setMediaGroup(attr->value());
    else if (attrName == onabortAttr)
        setAttributeEventListener(eventNames().abortEvent, createAttributeEventListener(this, attr));
    else if (attrName == onbeforeloadAttr)
        setAttributeEventListener(eventNames().beforeloadEvent, createAttributeEventListener(this, attr));
    else if (attrName == oncanplayAttr)
        setAttributeEventListener(eventNames().canplayEvent, createAttributeEventListener(this, attr));
    else if (attrName == oncanplaythroughAttr)
        setAttributeEventListener(eventNames().canplaythroughEvent, createAttributeEventListener(this, attr));
    else if (attrName == ondurationchangeAttr)
        setAttributeEventListener(eventNames().durationchangeEvent, createAttributeEventListener(this, attr));
    else if (attrName == onemptiedAttr)
        setAttributeEventListener(eventNames().emptiedEvent, createAttributeEventListener(this, attr));
    else if (attrName == onendedAttr)
        setAttributeEventListener(eventNames().endedEvent, createAttributeEventListener(this, attr));
    else if (attrName == onerrorAttr)
        setAttributeEventListener(eventNames().errorEvent, createAttributeEventListener(this, attr));
    else if (attrName == onloadeddataAttr)
        setAttributeEventListener(eventNames().loadeddataEvent, createAttributeEventListener(this, attr));
    else if (attrName == onloadedmetadataAttr)
        setAttributeEventListener(eventNames().loadedmetadataEvent, createAttributeEventListener(this, attr));
    else if (attrName == onloadstartAttr)
        setAttributeEventListener(eventNames().loadstartEvent, createAttributeEventListener(this, attr));
    else if (attrName == onpauseAttr)
        setAttributeEventListener(eventNames().pauseEvent, createAttributeEventListener(this, attr));
    else if (attrName == onplayAttr)
        setAttributeEventListener(eventNames().playEvent, createAttributeEventListener(this, attr));
    else if (attrName == onplayingAttr)
        setAttributeEventListener(eventNames().playingEvent, createAttributeEventListener(this, attr));
    else if (attrName == onprogressAttr)
        setAttributeEventListener(eventNames().progressEvent, createAttributeEventListener(this, attr));
    else if (attrName == onratechangeAttr)
        setAttributeEventListener(eventNames().ratechangeEvent, createAttributeEventListener(this, attr));
    else if (attrName == onseekedAttr)
        setAttributeEventListener(eventNames().seekedEvent, createAttributeEventListener(this, attr));
    else if (attrName == onseekingAttr)
        setAttributeEventListener(eventNames().seekingEvent, createAttributeEventListener(this, attr));
    else if (attrName == onstalledAttr)
        setAttributeEventListener(eventNames().stalledEvent, createAttributeEventListener(this, attr));
    else if (attrName == onsuspendAttr)
        setAttributeEventListener(eventNames().suspendEvent, createAttributeEventListener(this, attr));
    else if (attrName == ontimeupdateAttr)
        setAttributeEventListener(eventNames().timeupdateEvent, createAttributeEventListener(this, attr));
    else if (attrName == onvolumechangeAttr)
        setAttributeEventListener(eventNames().volumechangeEvent, createAttributeEventListener(this, attr));
    else if (attrName == onwaitingAttr)
        setAttributeEventListener(eventNames().waitingEvent, createAttributeEventListener(this, attr));
    else if (attrName == onwebkitbeginfullscreenAttr)
        setAttributeEventListener(eventNames().webkitbeginfullscreenEvent, createAttributeEventListener(this, attr));
    else if (attrName == onwebkitendfullscreenAttr)
        setAttributeEventListener(eventNames().webkitendfullscreenEvent, createAttributeEventListener(this, attr));
    else
        HTMLElement::parseAttribute(attr);
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    m_parsingInProgress = false;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    document()->updateStyleIfNeeded();
    createMediaPlayerProxy();
#endif
    
#if ENABLE(VIDEO_TRACK)
    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return;
    
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(trackTag)) {
            scheduleLoad(TextTrackResource);
            break;
        }
    }
#endif
}

bool HTMLMediaElement::rendererIsNeeded(const NodeRenderingContext& context)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    UNUSED_PARAM(context);
    Frame* frame = document()->frame();
    if (!frame)
        return false;

    return true;
#else
    return controls() ? HTMLElement::rendererIsNeeded(context) : false;
#endif
}

RenderObject* HTMLMediaElement::createRenderer(RenderArena* arena, RenderStyle*)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    // Setup the renderer if we already have a proxy widget.
    RenderEmbeddedObject* mediaRenderer = new (arena) RenderEmbeddedObject(this);
    if (m_proxyWidget) {
        mediaRenderer->setWidget(m_proxyWidget);

        if (Frame* frame = document()->frame())
            frame->loader()->client()->showMediaPlayerProxyPlugin(m_proxyWidget.get());
    }
    return mediaRenderer;
#else
    return new (arena) RenderMedia(this);
#endif
}

bool HTMLMediaElement::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    return childContext.isOnUpperEncapsulationBoundary() && HTMLElement::childShouldCreateRenderer(childContext);
}

Node::InsertionNotificationRequest HTMLMediaElement::insertedInto(Node* insertionPoint)
{
    LOG(Media, "HTMLMediaElement::insertedInto");
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument() && !getAttribute(srcAttr).isEmpty() && m_networkState == NETWORK_EMPTY)
        scheduleLoad(MediaResource);
    configureMediaControls();
    return InsertionDone;
}

void HTMLMediaElement::removedFrom(Node* insertionPoint)
{
    if (insertionPoint->inDocument()) {
        LOG(Media, "HTMLMediaElement::removedFromDocument");
        configureMediaControls();
        if (m_networkState > NETWORK_EMPTY)
            pause();
        if (m_isFullscreen)
            exitFullscreen();
    }

    HTMLElement::removedFrom(insertionPoint);
}

void HTMLMediaElement::attach()
{
    ASSERT(!attached());

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    m_needWidgetUpdate = true;
#endif

    HTMLElement::attach();

    if (renderer())
        renderer()->updateFromElement();
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    else if (m_proxyWidget) {
        if (Frame* frame = document()->frame())
            frame->loader()->client()->hideMediaPlayerProxyPlugin(m_proxyWidget.get());
    }
#endif
}

void HTMLMediaElement::didRecalcStyle(StyleChange)
{
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::scheduleLoad(LoadType loadType)
{
    LOG(Media, "HTMLMediaElement::scheduleLoad");

    if ((loadType & MediaResource) && !(m_pendingLoadFlags & MediaResource)) {
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        createMediaPlayerProxy();
#endif
        
        prepareForLoad();
        m_pendingLoadFlags |= MediaResource;
    }

#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled() && (loadType & TextTrackResource))
        m_pendingLoadFlags |= TextTrackResource;
#endif

    if (!m_loadTimer.isActive())
        m_loadTimer.startOneShot(0);
}

void HTMLMediaElement::scheduleNextSourceChild()
{
    // Schedule the timer to try the next <source> element WITHOUT resetting state ala prepareForLoad.
    m_pendingLoadFlags |= MediaResource;
    m_loadTimer.startOneShot(0);
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName)
{
#if LOG_MEDIA_EVENTS
    LOG(Media, "HTMLMediaElement::scheduleEvent - scheduling '%s'", eventName.string().ascii().data());
#endif
    RefPtr<Event> event = Event::create(eventName, false, true);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event.release());
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    RefPtr<HTMLMediaElement> protect(this); // loadNextSourceChild may fire 'beforeload', which can make arbitrary DOM mutations.

#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled() && (m_pendingLoadFlags & TextTrackResource))
        configureNewTextTracks();
#endif

    if (m_pendingLoadFlags & MediaResource) {
        if (m_loadState == LoadingFromSourceElement)
            loadNextSourceChild();
        else
            loadInternal();
    }

    m_pendingLoadFlags = 0;
}

PassRefPtr<MediaError> HTMLMediaElement::error() const 
{
    return m_error;
}

void HTMLMediaElement::setSrc(const String& url)
{
    setAttribute(srcAttr, url);
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType, const String& keySystem) const
{
    MediaPlayer::SupportsType support = MediaPlayer::supportsType(ContentType(mimeType), keySystem);
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case MediaPlayer::IsNotSupported:
            canPlay = "";
            break;
        case MediaPlayer::MayBeSupported:
            canPlay = "maybe";
            break;
        case MediaPlayer::IsSupported:
            canPlay = "probably";
            break;
    }
    
    LOG(Media, "HTMLMediaElement::canPlayType(%s, %s) -> %s", mimeType.utf8().data(), keySystem.utf8().data(), canPlay.utf8().data());

    return canPlay;
}

void HTMLMediaElement::load(ExceptionCode& ec)
{
    RefPtr<HTMLMediaElement> protect(this); // loadInternal may result in a 'beforeload' event, which can make arbitrary DOM mutations.
    
    LOG(Media, "HTMLMediaElement::load()");
    
    if (userGestureRequiredForLoad() && !ScriptController::processingUserGesture()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    
    m_loadInitiatedByUserGesture = ScriptController::processingUserGesture();
    if (m_loadInitiatedByUserGesture)
        removeBehaviorsRestrictionsAfterFirstUserGesture();
    prepareForLoad();
    loadInternal();
    prepareToPlay();
}

void HTMLMediaElement::prepareForLoad()
{
    LOG(Media, "HTMLMediaElement::prepareForLoad");

    // Perform the cleanup required for the resource load algorithm to run.
    stopPeriodicTimers();
    m_loadTimer.stop();
    m_sentEndEvent = false;
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;
    m_completelyLoaded = false;
    m_havePreparedToPlay = false;
    m_displayMode = Unknown;

    // 1 - Abort any already-running instance of the resource selection algorithm for this element.
    m_loadState = WaitingForSource;
    m_currentSourceNode = 0;

    // 2 - If there are any tasks from the media element's media element event task source in 
    // one of the task queues, then remove those tasks.
    cancelPendingEventsAndCallbacks();

    // 3 - If the media element's networkState is set to NETWORK_LOADING or NETWORK_IDLE, queue
    // a task to fire a simple event named abort at the media element.
    if (m_networkState == NETWORK_LOADING || m_networkState == NETWORK_IDLE)
        scheduleEvent(eventNames().abortEvent);

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    createMediaPlayer();
#else
    if (m_player)
        m_player->cancelLoad();
    else
        createMediaPlayerProxy();
#endif

#if ENABLE(MEDIA_SOURCE)
    if (m_sourceState != SOURCE_CLOSED)
        setSourceState(SOURCE_CLOSED);
#endif

    // 4 - If the media element's networkState is not set to NETWORK_EMPTY, then run these substeps
    if (m_networkState != NETWORK_EMPTY) {
        m_networkState = NETWORK_EMPTY;
        m_readyState = HAVE_NOTHING;
        m_readyStateMaximum = HAVE_NOTHING;
        refreshCachedTime();
        m_paused = true;
        m_seeking = false;
        invalidateCachedTime();
        scheduleEvent(eventNames().emptiedEvent);
        updateMediaController();
#if ENABLE(VIDEO_TRACK)
        if (RuntimeEnabledFeatures::webkitVideoTrackEnabled())
            updateActiveTextTrackCues(0);
#endif
    }

    // 5 - Set the playbackRate attribute to the value of the defaultPlaybackRate attribute.
    setPlaybackRate(defaultPlaybackRate());

    // 6 - Set the error attribute to null and the autoplaying flag to true.
    m_error = 0;
    m_autoplaying = true;

    // 7 - Invoke the media element's resource selection algorithm.

    // 8 - Note: Playback of any previously playing media resource for this element stops.

    // The resource selection algorithm
    // 1 - Set the networkState to NETWORK_NO_SOURCE
    m_networkState = NETWORK_NO_SOURCE;

    // 2 - Asynchronously await a stable state.

    m_playedTimeRanges = TimeRanges::create();
    m_lastSeekTime = 0;
    m_closedCaptionsVisible = false;

    // The spec doesn't say to block the load event until we actually run the asynchronous section
    // algorithm, but do it now because we won't start that until after the timer fires and the 
    // event may have already fired by then.
    setShouldDelayLoadEvent(true);

    configureMediaControls();
}

void HTMLMediaElement::loadInternal()
{
    // Some of the code paths below this function dispatch the BeforeLoad event. This ASSERT helps
    // us catch those bugs more quickly without needing all the branches to align to actually
    // trigger the event.
    ASSERT(!eventDispatchForbidden());

    // If we can't start a load right away, start it later.
    Page* page = document()->page();
    if (pageConsentRequiredForLoad() && page && !page->canStartMedia()) {
        if (m_isWaitingUntilMediaCanStart)
            return;
        document()->addMediaCanStartListener(this);
        m_isWaitingUntilMediaCanStart = true;
        return;
    }
    
    // Once the page has allowed an element to load media, it is free to load at will. This allows a 
    // playlist that starts in a foreground tab to continue automatically if the tab is subsequently 
    // put in the the background.
    removeBehaviorRestriction(RequirePageConsentToLoadMediaRestriction);

#if ENABLE(VIDEO_TRACK)
    // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose mode was not in the
    // disabled state when the element's resource selection algorithm last started".
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled()) {
        m_textTracksWhenResourceSelectionBegan.clear();
        if (m_textTracks) {
            for (unsigned i = 0; i < m_textTracks->length(); ++i) {
                TextTrack* track = m_textTracks->item(i);
                if (track->mode() != TextTrack::DISABLED)
                    m_textTracksWhenResourceSelectionBegan.append(track);
            }
        }
    }
#endif

    selectMediaResource();
}

void HTMLMediaElement::selectMediaResource()
{
    LOG(Media, "HTMLMediaElement::selectMediaResource");

    enum Mode { attribute, children };

    // 3 - If the media element has a src attribute, then let mode be attribute.
    Mode mode = attribute;
    if (!fastHasAttribute(srcAttr)) {
        Node* node;
        for (node = firstChild(); node; node = node->nextSibling()) {
            if (node->hasTagName(sourceTag))
                break;
        }

        // Otherwise, if the media element does not have a src attribute but has a source 
        // element child, then let mode be children and let candidate be the first such 
        // source element child in tree order.
        if (node) {
            mode = children;
            m_nextChildNodeToConsider = node;
            m_currentSourceNode = 0;
        } else {
            // Otherwise the media element has neither a src attribute nor a source element 
            // child: set the networkState to NETWORK_EMPTY, and abort these steps; the 
            // synchronous section ends.
            m_loadState = WaitingForSource;
            setShouldDelayLoadEvent(false);
            m_networkState = NETWORK_EMPTY;

            LOG(Media, "HTMLMediaElement::selectMediaResource, nothing to load");
            return;
        }
    }

    // 4 - Set the media element's delaying-the-load-event flag to true (this delays the load event), 
    // and set its networkState to NETWORK_LOADING.
    setShouldDelayLoadEvent(true);
    m_networkState = NETWORK_LOADING;

    // 5 - Queue a task to fire a simple event named loadstart at the media element.
    scheduleEvent(eventNames().loadstartEvent);

    // 6 - If mode is attribute, then run these substeps
    if (mode == attribute) {
        m_loadState = LoadingFromSrcAttr;

        // If the src attribute's value is the empty string ... jump down to the failed step below
        KURL mediaURL = getNonEmptyURLAttribute(srcAttr);
        if (mediaURL.isEmpty()) {
            mediaLoadingFailed(MediaPlayer::FormatError);
            LOG(Media, "HTMLMediaElement::selectMediaResource, empty 'src'");
            return;
        }

        if (!isSafeToLoadURL(mediaURL, Complain) || !dispatchBeforeLoadEvent(mediaURL.string())) {
            mediaLoadingFailed(MediaPlayer::FormatError);
            return;
        }

        // No type or key system information is available when the url comes
        // from the 'src' attribute so MediaPlayer
        // will have to pick a media engine based on the file extension.
        ContentType contentType((String()));
        loadResource(mediaURL, contentType, String());
        LOG(Media, "HTMLMediaElement::selectMediaResource, using 'src' attribute url");
        return;
    }

    // Otherwise, the source elements will be used
    loadNextSourceChild();
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType((String()));
    String keySystem;
    KURL mediaURL = selectNextSourceChild(&contentType, &keySystem, Complain);
    if (!mediaURL.isValid()) {
        waitForSourceChange();
        return;
    }

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    // Recreate the media player for the new url
    createMediaPlayer();
#endif

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaURL, contentType, keySystem);
}

#if !PLATFORM(CHROMIUM)
static KURL createFileURLForApplicationCacheResource(const String& path)
{
    // KURL should have a function to create a url from a path, but it does not. This function
    // is not suitable because KURL::setPath uses encodeWithURLEscapeSequences, which it notes
    // does not correctly escape '#' and '?'. This function works for our purposes because
    // app cache media files are always created with encodeForFileName(createCanonicalUUIDString()).

#if USE(CF) && PLATFORM(WIN)
    RetainPtr<CFStringRef> cfPath(AdoptCF, path.createCFString());
    RetainPtr<CFURLRef> cfURL(AdoptCF, CFURLCreateWithFileSystemPath(0, cfPath.get(), kCFURLWindowsPathStyle, false));
    KURL url(cfURL.get());
#else
    KURL url;

    url.setProtocol("file");
    url.setPath(path);
#endif
    return url;
}
#endif

void HTMLMediaElement::loadResource(const KURL& initialURL, ContentType& contentType, const String& keySystem)
{
    ASSERT(isSafeToLoadURL(initialURL, Complain));

    LOG(Media, "HTMLMediaElement::loadResource(%s, %s, %s)", urlForLogging(initialURL).utf8().data(), contentType.raw().utf8().data(), keySystem.utf8().data());

    Frame* frame = document()->frame();
    if (!frame) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }

    KURL url = initialURL;
    if (!frame->loader()->willLoadMediaElementURL(url)) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }
    
#if ENABLE(MEDIA_SOURCE)
    // If this is a media source URL, make sure it is the one for this media element.
    if (url.protocolIs(mediaSourceURLProtocol) && url != m_mediaSourceURL) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }
#endif

    // The resource fetch algorithm 
    m_networkState = NETWORK_LOADING;

#if !PLATFORM(CHROMIUM)
    // If the url should be loaded from the application cache, pass the url of the cached file
    // to the media engine.
    ApplicationCacheHost* cacheHost = frame->loader()->documentLoader()->applicationCacheHost();
    ApplicationCacheResource* resource = 0;
    if (cacheHost && cacheHost->shouldLoadResourceFromApplicationCache(ResourceRequest(url), resource)) {
        // Resources that are not present in the manifest will always fail to load (at least, after the
        // cache has been primed the first time), making the testing of offline applications simpler.
        if (!resource || resource->path().isEmpty()) {
            mediaLoadingFailed(MediaPlayer::NetworkError);
            return;
        }
    }
#endif

    // Set m_currentSrc *before* changing to the cache url, the fact that we are loading from the app
    // cache is an internal detail not exposed through the media element API.
    m_currentSrc = url;

#if !PLATFORM(CHROMIUM)
    if (resource) {
        url = createFileURLForApplicationCacheResource(resource->path());
        LOG(Media, "HTMLMediaElement::loadResource - will load from app cache -> %s", urlForLogging(url).utf8().data());
    }
#endif

    LOG(Media, "HTMLMediaElement::loadResource - m_currentSrc -> %s", urlForLogging(m_currentSrc).utf8().data());

    if (m_sendProgressEvents) 
        startProgressEventTimer();

    Settings* settings = document()->settings();
    bool privateMode = !settings || settings->privateBrowsingEnabled();
    m_player->setPrivateBrowsingMode(privateMode);

    // Reset display mode to force a recalculation of what to show because we are resetting the player.
    setDisplayMode(Unknown);

    if (!autoplay())
        m_player->setPreload(m_preload);
    m_player->setPreservesPitch(m_webkitPreservesPitch);

    if (fastHasAttribute(mutedAttr))
        m_muted = true;
    updateVolume();

    if (!m_player->load(url, contentType, keySystem))
        mediaLoadingFailed(MediaPlayer::FormatError);

    // If there is no poster to display, allow the media engine to render video frames as soon as
    // they are available.
    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

#if ENABLE(VIDEO_TRACK)
static bool trackIndexCompare(TextTrack* a,
                              TextTrack* b)
{
    return a->trackIndex() - b->trackIndex() < 0;
}

static bool eventTimeCueCompare(const std::pair<double, TextTrackCue*>& a,
                                const std::pair<double, TextTrackCue*>& b)
{
    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    if (a.first != b.first)
        return a.first - b.first < 0;

    // If the cues belong to different text tracks, it doesn't make sense to
    // compare the two tracks by the relative cue order, so return the relative
    // track order.
    if (a.second->track() != b.second->track())
        return trackIndexCompare(a.second->track(), b.second->track());

    // 12 - Further sort tasks in events that have the same time by the
    // relative text track cue order of the text track cues associated
    // with these tasks.
    return a.second->cueIndex() - b.second->cueIndex() < 0;
}


void HTMLMediaElement::updateActiveTextTrackCues(float movieTime)
{
    LOG(Media, "HTMLMediaElement::updateActiveTextTracks");

    // 4.8.10.8 Playing the media resource

    //  If the current playback position changes while the steps are running,
    //  then the user agent must wait for the steps to complete, and then must
    //  immediately rerun the steps.
    if (ignoreTrackDisplayUpdateRequests())
        return;

    // 1 - Let current cues be a list of cues, initialized to contain all the
    // cues of all the hidden, showing, or showing by default text tracks of the
    // media element (not the disabled ones) whose start times are less than or
    // equal to the current playback position and whose end times are greater
    // than the current playback position.
    Vector<CueIntervalTree::IntervalType> currentCues;

    // The user agent must synchronously unset [the text track cue active] flag
    // whenever ... the media element's readyState is changed back to HAVE_NOTHING.
    if (m_readyState != HAVE_NOTHING && m_player)
        currentCues = m_cueTree.allOverlaps(m_cueTree.createInterval(movieTime, movieTime));

    Vector<CueIntervalTree::IntervalType> affectedCues;
    Vector<CueIntervalTree::IntervalType> previousCues;
    Vector<CueIntervalTree::IntervalType> missedCues;

    // 2 - Let other cues be a list of cues, initialized to contain all the cues
    // of hidden, showing, and showing by default text tracks of the media
    // element that are not present in current cues.
    previousCues = m_currentlyActiveCues;

    // 3 - Let last time be the current playback position at the time this
    // algorithm was last run for this media element, if this is not the first
    // time it has run.
    float lastTime = m_lastTextTrackUpdateTime;

    // 4 - If the current playback position has, since the last time this
    // algorithm was run, only changed through its usual monotonic increase
    // during normal playback, then let missed cues be the list of cues in other
    // cues whose start times are greater than or equal to last time and whose
    // end times are less than or equal to the current playback position.
    // Otherwise, let missed cues be an empty list.
    if (lastTime >= 0 && m_lastSeekTime < movieTime) {
        Vector<CueIntervalTree::IntervalType> potentiallySkippedCues =
            m_cueTree.allOverlaps(m_cueTree.createInterval(lastTime, movieTime));

        for (size_t i = 0; i < potentiallySkippedCues.size(); ++i) {
            float cueStartTime = potentiallySkippedCues[i].low();
            float cueEndTime = potentiallySkippedCues[i].high();

            // Consider cues that may have been missed since the last seek time.
            if (cueStartTime > max(m_lastSeekTime, lastTime) && cueEndTime < movieTime)
                missedCues.append(potentiallySkippedCues[i]);
        }
    }

    m_lastTextTrackUpdateTime = movieTime;

    // 5 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and if the user agent
    // has not fired a timeupdate event at the element in the past 15 to 250ms
    // and is not still running event handlers for such an event, then the user
    // agent must queue a task to fire a simple event named timeupdate at the
    // element. (In the other cases, such as explicit seeks, relevant events get
    // fired as part of the overall process of changing the current playback
    // position.)
    if (m_lastSeekTime <= lastTime)
        scheduleTimeupdateEvent(false);

    // Explicitly cache vector sizes, as their content is constant from here.
    size_t currentCuesSize = currentCues.size();
    size_t missedCuesSize = missedCues.size();
    size_t previousCuesSize = previousCues.size();

    // 6 - If all of the cues in current cues have their text track cue active
    // flag set, none of the cues in other cues have their text track cue active
    // flag set, and missed cues is empty, then abort these steps.
    bool activeSetChanged = missedCuesSize;

    for (size_t i = 0; !activeSetChanged && i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]) && previousCues[i].data()->isActive())
            activeSetChanged = true;

    for (size_t i = 0; !activeSetChanged && i < currentCuesSize; ++i) {
        if (!currentCues[i].data()->isActive())
            activeSetChanged = true;
    }

    if (!activeSetChanged)
        return;

    // 7 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and there are cues in
    // other cues that have their text track cue pause-on-exi flag set and that
    // either have their text track cue active flag set or are also in missed
    // cues, then immediately pause the media element.
    for (size_t i = 0; !m_paused && i < previousCuesSize; ++i) {
        if (previousCues[i].data()->pauseOnExit()
            && previousCues[i].data()->isActive()
            && !currentCues.contains(previousCues[i]))
            pause();
    }

    for (size_t i = 0; !m_paused && i < missedCuesSize; ++i) {
        if (missedCues[i].data()->pauseOnExit())
            pause();
    }

    // 8 - Let events be a list of tasks, initially empty. Each task in this
    // list will be associated with a text track, a text track cue, and a time,
    // which are used to sort the list before the tasks are queued.
    Vector<std::pair<double, TextTrackCue*> > eventTasks;

    // 8 - Let affected tracks be a list of text tracks, initially empty.
    Vector<TextTrack*> affectedTracks;

    for (size_t i = 0; i < missedCuesSize; ++i) {
        // 9 - For each text track cue in missed cues, prepare an event named enter
        // for the TextTrackCue object with the text track cue start time.
        eventTasks.append(std::make_pair(missedCues[i].data()->startTime(),
                                         missedCues[i].data()));

        // 10 - For each text track in missed cues, prepare an event
        // named exit for the TextTrackCue object with the text track cue end
        // time.
        eventTasks.append(std::make_pair(missedCues[i].data()->endTime(),
                                         missedCues[i].data()));
    }

    for (size_t i = 0; i < previousCuesSize; ++i) {
        // 10 - For each text track cue in other cues that has its text
        // track cue active flag set prepare an event named exit for the
        // TextTrackCue object with the text track cue end time.
        if (!currentCues.contains(previousCues[i]))
            eventTasks.append(std::make_pair(previousCues[i].data()->endTime(),
                                             previousCues[i].data()));
    }

    for (size_t i = 0; i < currentCuesSize; ++i) {
        // 11 - For each text track cue in current cues that does not have its
        // text track cue active flag set, prepare an event named enter for the
        // TextTrackCue object with the text track cue start time.
        if (!previousCues.contains(currentCues[i]))
            eventTasks.append(std::make_pair(currentCues[i].data()->startTime(),
                                             currentCues[i].data()));
    }

    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    nonCopyingSort(eventTasks.begin(), eventTasks.end(), eventTimeCueCompare);

    for (size_t i = 0; i < eventTasks.size(); ++i) {
        if (!affectedTracks.contains(eventTasks[i].second->track()))
            affectedTracks.append(eventTasks[i].second->track());

        // 13 - Queue each task in events, in list order.
        RefPtr<Event> event;

        // Each event in eventTasks may be either an enterEvent or an exitEvent,
        // depending on the time that is associated with the event. This
        // correctly identifies the type of the event, since the startTime is
        // always less than the endTime.
        if (eventTasks[i].first == eventTasks[i].second->startTime())
            event = Event::create(eventNames().enterEvent, false, false);
        else
            event = Event::create(eventNames().exitEvent, false, false);

        event->setTarget(eventTasks[i].second);
        m_asyncEventQueue->enqueueEvent(event.release());
    }

    // 14 - Sort affected tracks in the same order as the text tracks appear in
    // the media element's list of text tracks, and remove duplicates.
    nonCopyingSort(affectedTracks.begin(), affectedTracks.end(), trackIndexCompare);

    // 15 - For each text track in affected tracks, in the list order, queue a
    // task to fire a simple event named cuechange at the TextTrack object, and, ...
    for (size_t i = 0; i < affectedTracks.size(); ++i) {
        RefPtr<Event> event = Event::create(eventNames().cuechangeEvent, false, false);
        event->setTarget(affectedTracks[i]);

        m_asyncEventQueue->enqueueEvent(event.release());

        // ... if the text track has a corresponding track element, to then fire a
        // simple event named cuechange at the track element as well.
        if (affectedTracks[i]->trackType() == TextTrack::TrackElement) {
            RefPtr<Event> event = Event::create(eventNames().cuechangeEvent, false, false);
            HTMLTrackElement* trackElement = static_cast<LoadableTextTrack*>(affectedTracks[i])->trackElement();
            ASSERT(trackElement);
            event->setTarget(trackElement);
            
            m_asyncEventQueue->enqueueEvent(event.release());
        }
    }

    // 16 - Set the text track cue active flag of all the cues in the current
    // cues, and unset the text track cue active flag of all the cues in the
    // other cues.
    for (size_t i = 0; i < currentCuesSize; ++i)
        currentCues[i].data()->setIsActive(true);

    for (size_t i = 0; i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]))
            previousCues[i].data()->setIsActive(false);

    // Update the current active cues.
    m_currentlyActiveCues = currentCues;

    if (activeSetChanged && hasMediaControls())
        mediaControls()->updateTextTrackDisplay();
}

bool HTMLMediaElement::textTracksAreReady() const
{
    // 4.8.10.12.1 Text track model
    // ...
    // The text tracks of a media element are ready if all the text tracks whose mode was not 
    // in the disabled state when the element's resource selection algorithm last started now
    // have a text track readiness state of loaded or failed to load.
    for (unsigned i = 0; i < m_textTracksWhenResourceSelectionBegan.size(); ++i) {
        if (m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::Loading)
            return false;
    }

    return true;
}

void HTMLMediaElement::textTrackReadyStateChanged(TextTrack* track)
{
    if (m_player && m_textTracksWhenResourceSelectionBegan.contains(track)) {
        if (track->readinessState() != TextTrack::Loading)
            setReadyState(m_player->readyState());
    }
}

void HTMLMediaElement::textTrackModeChanged(TextTrack* track)
{
    if (track->trackType() == TextTrack::TrackElement) {
        // 4.8.10.12.3 Sourcing out-of-band text tracks
        // ... when a text track corresponding to a track element is created with text track
        // mode set to disabled and subsequently changes its text track mode to hidden, showing,
        // or showing by default for the first time, the user agent must immediately and synchronously
        // run the following algorithm ...

        for (Node* node = firstChild(); node; node = node->nextSibling()) {
            if (!node->hasTagName(trackTag))
                continue;
            HTMLTrackElement* trackElement = static_cast<HTMLTrackElement*>(node);
            if (trackElement->track() != track)
                continue;
            
            // Mark this track as "configured" so configureNewTextTracks won't change the mode again.
            trackElement->setHasBeenConfigured(true);
            if (track->mode() != TextTrack::DISABLED) {
                if (trackElement->readyState() == HTMLTrackElement::LOADED)
                    textTrackAddCues(track, track->cues());
                else if (trackElement->readyState() == HTMLTrackElement::NONE)
                    trackElement->scheduleLoad();
            }
            break;
        }
    }

    configureTextTrackDisplay();
}

void HTMLMediaElement::textTrackKindChanged(TextTrack*)
{
    // FIXME(62885): Implement.
}

void HTMLMediaElement::textTrackAddCues(TextTrack*, const TextTrackCueList* cues) 
{
    beginIgnoringTrackDisplayUpdateRequests();
    for (size_t i = 0; i < cues->length(); ++i)
        textTrackAddCue(cues->item(i)->track(), cues->item(i));
    endIgnoringTrackDisplayUpdateRequests();
    updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::textTrackRemoveCues(TextTrack*, const TextTrackCueList* cues) 
{
    beginIgnoringTrackDisplayUpdateRequests();
    for (size_t i = 0; i < cues->length(); ++i)
        textTrackRemoveCue(cues->item(i)->track(), cues->item(i));
    endIgnoringTrackDisplayUpdateRequests();
    updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue> cue)
{
    m_cueTree.add(m_cueTree.createInterval(cue->startTime(), cue->endTime(), cue.get()));
    updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue> cue)
{
    m_cueTree.remove(m_cueTree.createInterval(cue->startTime(), cue->endTime(), cue.get()));
    updateActiveTextTrackCues(currentTime());
}

#endif

bool HTMLMediaElement::isSafeToLoadURL(const KURL& url, InvalidURLAction actionIfInvalid)
{
    if (!url.isValid()) {
        LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> FALSE because url is invalid", urlForLogging(url).utf8().data());
        return false;
    }

    Frame* frame = document()->frame();
    if (!frame || !document()->securityOrigin()->canDisplay(url)) {
        if (actionIfInvalid == Complain)
            FrameLoader::reportLocalLoadFailed(frame, url.string());
        LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> FALSE rejected by SecurityOrigin", urlForLogging(url).utf8().data());
        return false;
    }

    if (!document()->contentSecurityPolicy()->allowMediaFromSource(url)) {
        LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> rejected by Content Security Policy", urlForLogging(url).utf8().data());
        return false;
    }

    return true;
}

void HTMLMediaElement::startProgressEventTimer()
{
    if (m_progressEventTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_previousProgress = 0;
    // 350ms is not magic, it is in the spec!
    m_progressEventTimer.startRepeating(0.350);
}

void HTMLMediaElement::waitForSourceChange()
{
    LOG(Media, "HTMLMediaElement::waitForSourceChange");

    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 6.17 - Waiting: Set the element's networkState attribute to the NETWORK_NO_SOURCE value
    m_networkState = NETWORK_NO_SOURCE;

    // 6.18 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::noneSupported()
{
    LOG(Media, "HTMLMediaElement::noneSupported");

    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = 0;

    // 4.8.10.5 
    // 6 - Reaching this step indicates that the media resource failed to load or that the given 
    // URL could not be resolved. In one atomic operation, run the following steps:

    // 6.1 - Set the error attribute to a new MediaError object whose code attribute is set to
    // MEDIA_ERR_SRC_NOT_SUPPORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);

    // 6.2 - Forget the media element's media-resource-specific text tracks.

    // 6.3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 7 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

#if ENABLE(MEDIA_SOURCE)
    if (m_sourceState != SOURCE_CLOSED)
        setSourceState(SOURCE_CLOSED);
#endif

    // 8 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 9 - Abort these steps. Until the load() method is invoked or the src attribute is changed, 
    // the element won't attempt to load another resource.

    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::mediaEngineError(PassRefPtr<MediaError> err)
{
    LOG(Media, "HTMLMediaElement::mediaEngineError(%d)", static_cast<int>(err->code()));

    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is 
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    m_error = err;

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().errorEvent);

#if ENABLE(MEDIA_SOURCE)
    if (m_sourceState != SOURCE_CLOSED)
        setSourceState(SOURCE_CLOSED);
#endif

    // 4 - Set the element's networkState attribute to the NETWORK_EMPTY value and queue a
    // task to fire a simple event called emptied at the element.
    m_networkState = NETWORK_EMPTY;
    scheduleEvent(eventNames().emptiedEvent);

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = 0;
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks()
{
    LOG(Media, "HTMLMediaElement::cancelPendingEventsAndCallbacks");
    m_asyncEventQueue->cancelAllEvents();

    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (node->hasTagName(sourceTag))
            static_cast<HTMLSourceElement*>(node)->cancelPendingErrorEvent();
    }
}

Document* HTMLMediaElement::mediaPlayerOwningDocument()
{
    Document* d = document();
    
    if (!d)
        d = ownerDocument();
    
    return d;
}

void HTMLMediaElement::mediaPlayerNetworkStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    setNetworkState(m_player->networkState());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaLoadingFailed(MediaPlayer::NetworkState error)
{
    stopPeriodicTimers();
    
    // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
    // <source> children, schedule the next one
    if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement) {
        
        if (m_currentSourceNode)
            m_currentSourceNode->scheduleErrorEvent();
        else
            LOG(Media, "HTMLMediaElement::setNetworkState - error event not sent, <source> was removed");
        
        if (havePotentialSourceChild()) {
            LOG(Media, "HTMLMediaElement::setNetworkState - scheduling next <source>");
            scheduleNextSourceChild();
        } else {
            LOG(Media, "HTMLMediaElement::setNetworkState - no more <source> elements, waiting");
            waitForSourceChange();
        }
        
        return;
    }
    
    if (error == MediaPlayer::NetworkError && m_readyState >= HAVE_METADATA)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_NETWORK));
    else if (error == MediaPlayer::DecodeError)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_DECODE));
    else if ((error == MediaPlayer::FormatError || error == MediaPlayer::NetworkError) && m_loadState == LoadingFromSrcAttr)
        noneSupported();
    
    updateDisplayState();
    if (hasMediaControls()) {
        mediaControls()->reset();
        mediaControls()->reportedError();
    }
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    LOG(Media, "HTMLMediaElement::setNetworkState(%d) - current state is %d", static_cast<int>(state), static_cast<int>(m_networkState));

    if (state == MediaPlayer::Empty) {
        // Just update the cached state and leave, we can't do anything.
        m_networkState = NETWORK_EMPTY;
        return;
    }

    if (state == MediaPlayer::FormatError || state == MediaPlayer::NetworkError || state == MediaPlayer::DecodeError) {
        mediaLoadingFailed(state);
        return;
    }

    if (state == MediaPlayer::Idle) {
        if (m_networkState > NETWORK_IDLE) {
            changeNetworkStateFromLoadingToIdle();
            scheduleEvent(eventNames().suspendEvent);
            setShouldDelayLoadEvent(false);
        } else {
            m_networkState = NETWORK_IDLE;
        }
    }

    if (state == MediaPlayer::Loading) {
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        m_networkState = NETWORK_LOADING;
    }

    if (state == MediaPlayer::Loaded) {
        if (m_networkState != NETWORK_IDLE)
            changeNetworkStateFromLoadingToIdle();
        m_completelyLoaded = true;
    }

    if (hasMediaControls())
        mediaControls()->updateStatusDisplay();
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle()
{
    m_progressEventTimer.stop();
    if (hasMediaControls() && m_player->bytesLoaded() != m_previousProgress)
        mediaControls()->bufferingProgressed();

    // Schedule one last progress event so we guarantee that at least one is fired
    // for files that load very quickly.
    scheduleEvent(eventNames().progressEvent);
    m_networkState = NETWORK_IDLE;
}

void HTMLMediaElement::mediaPlayerReadyStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    setReadyState(m_player->readyState());

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::setReadyState(MediaPlayer::ReadyState state)
{
    LOG(Media, "HTMLMediaElement::setReadyState(%d) - current state is %d,", static_cast<int>(state), static_cast<int>(m_readyState));

    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    ReadyState newState = static_cast<ReadyState>(state);

#if ENABLE(VIDEO_TRACK)
    bool tracksAreReady = !RuntimeEnabledFeatures::webkitVideoTrackEnabled() || textTracksAreReady();

    if (newState == oldState && m_tracksAreReady == tracksAreReady)
        return;

    m_tracksAreReady = tracksAreReady;
#else
    if (newState == oldState)
        return;
    bool tracksAreReady = true;
#endif
    
    if (tracksAreReady)
        m_readyState = newState;
    else {
        // If a media file has text tracks the readyState may not progress beyond HAVE_FUTURE_DATA until
        // the text tracks are ready, regardless of the state of the media file.
        if (newState <= HAVE_METADATA)
            m_readyState = newState;
        else
            m_readyState = HAVE_CURRENT_DATA;
    }
    
    if (oldState > m_readyStateMaximum)
        m_readyStateMaximum = oldState;

    if (m_networkState == NETWORK_EMPTY)
        return;

    if (m_seeking) {
        // 4.8.10.9, step 11
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA)
            scheduleEvent(eventNames().waitingEvent);

        // 4.8.10.10 step 14 & 15.
        if (m_readyState >= HAVE_CURRENT_DATA)
            finishSeek();
    } else {
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            // 4.8.10.8
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().waitingEvent);
        }
    }

    if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
        prepareMediaFragmentURI();
        scheduleEvent(eventNames().durationchangeEvent);
        scheduleEvent(eventNames().loadedmetadataEvent);
        if (hasMediaControls())
            mediaControls()->loadedMetadata();
        if (renderer())
            renderer()->updateFromElement();
    }

    bool shouldUpdateDisplayState = false;

    if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA && !m_haveFiredLoadedData) {
        m_haveFiredLoadedData = true;
        shouldUpdateDisplayState = true;
        scheduleEvent(eventNames().loadeddataEvent);
        setShouldDelayLoadEvent(false);
        applyMediaFragmentURI();
    }

    bool isPotentiallyPlaying = potentiallyPlaying();
    if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA && tracksAreReady) {
        scheduleEvent(eventNames().canplayEvent);
        if (isPotentiallyPlaying)
            scheduleEvent(eventNames().playingEvent);
        shouldUpdateDisplayState = true;
    }

    if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA && tracksAreReady) {
        if (oldState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().canplayEvent);

        scheduleEvent(eventNames().canplaythroughEvent);

        if (isPotentiallyPlaying && oldState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().playingEvent);

        if (m_autoplaying && m_paused && autoplay() && !document()->isSandboxed(SandboxAutomaticFeatures)) {
            m_paused = false;
            invalidateCachedTime();
            scheduleEvent(eventNames().playEvent);
            scheduleEvent(eventNames().playingEvent);
        }

        shouldUpdateDisplayState = true;
    }

    if (shouldUpdateDisplayState) {
        updateDisplayState();
        if (hasMediaControls())
            mediaControls()->updateStatusDisplay();
    }

    updatePlayState();
    updateMediaController();
#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        updateActiveTextTrackCues(currentTime());
#endif
}

#if ENABLE(MEDIA_SOURCE)
void HTMLMediaElement::mediaPlayerSourceOpened()
{
    beginProcessingMediaPlayerCallback();

    setSourceState(SOURCE_OPEN);

    endProcessingMediaPlayerCallback();
}

String HTMLMediaElement::mediaPlayerSourceURL() const
{
    return m_mediaSourceURL.string();
}

bool HTMLMediaElement::isValidSourceId(const String& id, ExceptionCode& ec) const
{
    if (id.isNull() || id.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return false;
    }

    if (!m_sourceIDs.contains(id)) {
        ec = SYNTAX_ERR;
        return false;
    }

    return true;
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)
void HTMLMediaElement::mediaPlayerKeyAdded(MediaPlayer*, const String& keySystem, const String& sessionId)
{
    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtr<Event> event = MediaKeyEvent::create(eventNames().webkitkeyaddedEvent, initializer);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void HTMLMediaElement::mediaPlayerKeyError(MediaPlayer*, const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case MediaPlayerClient::UnknownError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case MediaPlayerClient::ClientError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    case MediaPlayerClient::ServiceError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_SERVICE;
        break;
    case MediaPlayerClient::OutputError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_OUTPUT;
        break;
    case MediaPlayerClient::HardwareChangeError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_HARDWARECHANGE;
        break;
    case MediaPlayerClient::DomainError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_DOMAIN;
        break;
    }

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.errorCode = MediaKeyError::create(mediaKeyErrorCode);
    initializer.systemCode = systemCode;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtr<Event> event = MediaKeyEvent::create(eventNames().webkitkeyerrorEvent, initializer);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void HTMLMediaElement::mediaPlayerKeyMessage(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength)
{
    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.message = Uint8Array::create(message, messageLength);
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtr<Event> event = MediaKeyEvent::create(eventNames().webkitkeymessageEvent, initializer);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void HTMLMediaElement::mediaPlayerKeyNeeded(MediaPlayer*, const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength)
{
    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.initData = Uint8Array::create(initData, initDataLength);
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtr<Event> event = MediaKeyEvent::create(eventNames().webkitneedkeyEvent, initializer);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}
#endif

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);
    if (m_networkState != NETWORK_LOADING)
        return;

    unsigned progress = m_player->bytesLoaded();
    double time = WTF::currentTime();
    double timedelta = time - m_previousProgressTime;

    if (progress == m_previousProgress) {
        if (timedelta > 3.0 && !m_sentStalledEvent) {
            scheduleEvent(eventNames().stalledEvent);
            m_sentStalledEvent = true;
            setShouldDelayLoadEvent(false);
        }
    } else {
        scheduleEvent(eventNames().progressEvent);
        m_previousProgress = progress;
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
        if (renderer())
            renderer()->updateFromElement();
        if (hasMediaControls())
            mediaControls()->bufferingProgressed();
    }
}

void HTMLMediaElement::rewind(float timeDelta)
{
    LOG(Media, "HTMLMediaElement::rewind(%f)", timeDelta);

    ExceptionCode e;
    setCurrentTime(max(currentTime() - timeDelta, minTimeSeekable()), e);
}

void HTMLMediaElement::returnToRealtime()
{
    LOG(Media, "HTMLMediaElement::returnToRealtime");
    ExceptionCode e;
    setCurrentTime(maxTimeSeekable(), e);
}  

void HTMLMediaElement::addPlayedRange(float start, float end)
{
    LOG(Media, "HTMLMediaElement::addPlayedRange(%f, %f)", start, end);
    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();
    m_playedTimeRanges->add(start, end);
}  

bool HTMLMediaElement::supportsSave() const
{
    return m_player ? m_player->supportsSave() : false;
}

bool HTMLMediaElement::supportsScanning() const
{
    return m_player ? m_player->supportsScanning() : false;
}

void HTMLMediaElement::prepareToPlay()
{
    LOG(Media, "HTMLMediaElement::prepareToPlay(%p)", this);
    if (m_havePreparedToPlay)
        return;
    m_havePreparedToPlay = true;
    m_player->prepareToPlay();
}

void HTMLMediaElement::seek(float time, ExceptionCode& ec)
{
    LOG(Media, "HTMLMediaElement::seek(%f)", time);

    // 4.8.9.9 Seeking

    // 1 - If the media element's readyState is HAVE_NOTHING, then raise an INVALID_STATE_ERR exception.
    if (m_readyState == HAVE_NOTHING || !m_player) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // If the media engine has been told to postpone loading data, let it go ahead now.
    if (m_preload < MediaPlayer::Auto && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();

    // Get the current time before setting m_seeking, m_lastSeekTime is returned once it is set.
    refreshCachedTime();
    float now = currentTime();

    // 2 - If the element's seeking IDL attribute is true, then another instance of this algorithm is
    // already running. Abort that other instance of the algorithm without waiting for the step that
    // it is running to complete.
    // Nothing specific to be done here.

    // 3 - Set the seeking IDL attribute to true.
    // The flag will be cleared when the engine tells us the time has actually changed.
    m_seeking = true;

    // 5 - If the new playback position is later than the end of the media resource, then let it be the end 
    // of the media resource instead.
    time = min(time, duration());

    // 6 - If the new playback position is less than the earliest possible position, let it be that position instead.
    float earliestTime = m_player->startTime();
    time = max(time, earliestTime);

    // Ask the media engine for the time value in the movie's time scale before comparing with current time. This
    // is necessary because if the seek time is not equal to currentTime but the delta is less than the movie's
    // time scale, we will ask the media engine to "seek" to the current movie time, which may be a noop and
    // not generate a timechanged callback. This means m_seeking will never be cleared and we will never 
    // fire a 'seeked' event.
#if !LOG_DISABLED
    float mediaTime = m_player->mediaTimeForTimeValue(time);
    if (time != mediaTime)
        LOG(Media, "HTMLMediaElement::seek(%f) - media timeline equivalent is %f", time, mediaTime);
#endif
    time = m_player->mediaTimeForTimeValue(time);

    // 7 - If the (possibly now changed) new playback position is not in one of the ranges given in the 
    // seekable attribute, then let it be the position in one of the ranges given in the seekable attribute 
    // that is the nearest to the new playback position. ... If there are no ranges given in the seekable
    // attribute then set the seeking IDL attribute to false and abort these steps.
    RefPtr<TimeRanges> seekableRanges = seekable();

    // Short circuit seeking to the current time by just firing the events if no seek is required.
    // Don't skip calling the media engine if we are in poster mode because a seek should always 
    // cancel poster display.
    bool noSeekRequired = !seekableRanges->length() || (time == now && displayMode() != Poster);

#if ENABLE(MEDIA_SOURCE)
    // Always notify the media engine of a seek if the source is not closed. This ensures that the source is
    // always in a flushed state when the 'seeking' event fires.
    if (m_sourceState != SOURCE_CLOSED)
      noSeekRequired = false;
#endif

    if (noSeekRequired) {
        if (time == now) {
            scheduleEvent(eventNames().seekingEvent);
            scheduleTimeupdateEvent(false);
            scheduleEvent(eventNames().seekedEvent);
        }
        m_seeking = false;
        return;
    }
    time = seekableRanges->nearest(time);

    if (m_playing) {
        if (m_lastSeekTime < now)
            addPlayedRange(m_lastSeekTime, now);
    }
    m_lastSeekTime = time;
    m_sentEndEvent = false;

#if ENABLE(MEDIA_SOURCE)
    if (m_sourceState == SOURCE_ENDED)
        setSourceState(SOURCE_OPEN);
#endif

    // 8 - Set the current playback position to the given new playback position
    m_player->seek(time);

    // 9 - Queue a task to fire a simple event named seeking at the element.
    scheduleEvent(eventNames().seekingEvent);

    // 10 - Queue a task to fire a simple event named timeupdate at the element.
    scheduleTimeupdateEvent(false);

    // 11-15 are handled, if necessary, when the engine signals a readystate change.
}

void HTMLMediaElement::finishSeek()
{
    LOG(Media, "HTMLMediaElement::finishSeek");

    // 4.8.10.9 Seeking step 14
    m_seeking = false;

    // 4.8.10.9 Seeking step 15
    scheduleEvent(eventNames().seekedEvent);

    setDisplayMode(Video);
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

MediaPlayer::MovieLoadType HTMLMediaElement::movieLoadType() const
{
    return m_player ? m_player->movieLoadType() : MediaPlayer::Unknown;
}

bool HTMLMediaElement::hasAudio() const
{
    return m_player ? m_player->hasAudio() : false;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

void HTMLMediaElement::refreshCachedTime() const
{
    m_cachedTime = m_player->currentTime();
    m_cachedTimeWallClockUpdateTime = WTF::currentTime();
}

void HTMLMediaElement::invalidateCachedTime()
{
    LOG(Media, "HTMLMediaElement::invalidateCachedTime");

    // Don't try to cache movie time when playback first starts as the time reported by the engine
    // sometimes fluctuates for a short amount of time, so the cached time will be off if we take it
    // too early.
    static const double minimumTimePlayingBeforeCacheSnapshot = 0.5;

    m_minimumWallClockTimeToCacheMediaTime = WTF::currentTime() + minimumTimePlayingBeforeCacheSnapshot;
    m_cachedTime = invalidMediaTime;
}

// playback state
float HTMLMediaElement::currentTime() const
{
#if LOG_CACHED_TIME_WARNINGS
    static const double minCachedDeltaForWarning = 0.01;
#endif

    if (!m_player)
        return 0;

    if (m_seeking) {
        LOG(Media, "HTMLMediaElement::currentTime - seeking, returning %f", m_lastSeekTime);
        return m_lastSeekTime;
    }

    if (m_cachedTime != invalidMediaTime && m_paused) {
#if LOG_CACHED_TIME_WARNINGS
        float delta = m_cachedTime - m_player->currentTime();
        if (delta > minCachedDeltaForWarning)
            LOG(Media, "HTMLMediaElement::currentTime - WARNING, cached time is %f seconds off of media time when paused", delta);
#endif
        return m_cachedTime;
    }

    // Is it too soon use a cached time?
    double now = WTF::currentTime();
    double maximumDurationToCacheMediaTime = m_player->maximumDurationToCacheMediaTime();

    if (maximumDurationToCacheMediaTime && m_cachedTime != invalidMediaTime && !m_paused && now > m_minimumWallClockTimeToCacheMediaTime) {
        double wallClockDelta = now - m_cachedTimeWallClockUpdateTime;

        // Not too soon, use the cached time only if it hasn't expired.
        if (wallClockDelta < maximumDurationToCacheMediaTime) {
            float adjustedCacheTime = static_cast<float>(m_cachedTime + (m_playbackRate * wallClockDelta));

#if LOG_CACHED_TIME_WARNINGS
            float delta = adjustedCacheTime - m_player->currentTime();
            if (delta > minCachedDeltaForWarning)
                LOG(Media, "HTMLMediaElement::currentTime - WARNING, cached time is %f seconds off of media time when playing", delta);
#endif
            return adjustedCacheTime;
        }
    }

#if LOG_CACHED_TIME_WARNINGS
    if (maximumDurationToCacheMediaTime && now > m_minimumWallClockTimeToCacheMediaTime && m_cachedTime != invalidMediaTime) {
        double wallClockDelta = now - m_cachedTimeWallClockUpdateTime;
        float delta = m_cachedTime + (m_playbackRate * wallClockDelta) - m_player->currentTime();
        LOG(Media, "HTMLMediaElement::currentTime - cached time was %f seconds off of media time when it expired", delta);
    }
#endif

    refreshCachedTime();

    return m_cachedTime;
}

void HTMLMediaElement::setCurrentTime(float time, ExceptionCode& ec)
{
    if (m_mediaController) {
        ec = INVALID_STATE_ERR;
        return;
    }
    seek(time, ec);
}

float HTMLMediaElement::startTime() const
{
    if (!m_player)
        return 0;
    return m_player->startTime();
}

double HTMLMediaElement::initialTime() const
{
    if (m_fragmentStartTime != invalidMediaTime)
        return m_fragmentStartTime;

    if (!m_player)
        return 0;

    return m_player->initialTime();
}

float HTMLMediaElement::duration() const
{
    if (m_player && m_readyState >= HAVE_METADATA)
        return m_player->duration();

    return numeric_limits<float>::quiet_NaN();
}

bool HTMLMediaElement::paused() const
{
    return m_paused;
}

float HTMLMediaElement::defaultPlaybackRate() const
{
    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(float rate)
{
    if (m_defaultPlaybackRate != rate) {
        m_defaultPlaybackRate = rate;
        scheduleEvent(eventNames().ratechangeEvent);
    }
}

float HTMLMediaElement::playbackRate() const
{
    return m_playbackRate;
}

void HTMLMediaElement::setPlaybackRate(float rate)
{
    LOG(Media, "HTMLMediaElement::setPlaybackRate(%f)", rate);
    
    if (m_playbackRate != rate) {
        m_playbackRate = rate;
        invalidateCachedTime();
        scheduleEvent(eventNames().ratechangeEvent);
    }

    if (m_player && potentiallyPlaying() && m_player->rate() != rate && !m_mediaController)
        m_player->setRate(rate);
}

void HTMLMediaElement::updatePlaybackRate()
{
    float effectiveRate = m_mediaController ? m_mediaController->playbackRate() : m_playbackRate;
    if (m_player && potentiallyPlaying() && m_player->rate() != effectiveRate && !m_mediaController)
        m_player->setRate(effectiveRate);
}

bool HTMLMediaElement::webkitPreservesPitch() const
{
    return m_webkitPreservesPitch;
}

void HTMLMediaElement::setWebkitPreservesPitch(bool preservesPitch)
{
    LOG(Media, "HTMLMediaElement::setWebkitPreservesPitch(%s)", boolString(preservesPitch));

    m_webkitPreservesPitch = preservesPitch;
    
    if (!m_player)
        return;

    m_player->setPreservesPitch(preservesPitch);
}

bool HTMLMediaElement::ended() const
{
    // 4.8.10.8 Playing the media resource
    // The ended attribute must return true if the media element has ended 
    // playback and the direction of playback is forwards, and false otherwise.
    return endedPlayback() && m_playbackRate > 0;
}

bool HTMLMediaElement::autoplay() const
{
    return fastHasAttribute(autoplayAttr);
}

void HTMLMediaElement::setAutoplay(bool b)
{
    LOG(Media, "HTMLMediaElement::setAutoplay(%s)", boolString(b));
    setBooleanAttribute(autoplayAttr, b);
}

String HTMLMediaElement::preload() const
{
    switch (m_preload) {
    case MediaPlayer::None:
        return "none";
        break;
    case MediaPlayer::MetaData:
        return "metadata";
        break;
    case MediaPlayer::Auto:
        return "auto";
        break;
    }

    ASSERT_NOT_REACHED();
    return String();
}

void HTMLMediaElement::setPreload(const String& preload)
{
    LOG(Media, "HTMLMediaElement::setPreload(%s)", preload.utf8().data());
    setAttribute(preloadAttr, preload);
}

void HTMLMediaElement::play()
{
    LOG(Media, "HTMLMediaElement::play()");

    if (userGestureRequiredForRateChange() && !ScriptController::processingUserGesture())
        return;
    if (ScriptController::processingUserGesture())
        removeBehaviorsRestrictionsAfterFirstUserGesture();

    Settings* settings = document()->settings();
    if (settings && settings->needsSiteSpecificQuirks() && m_dispatchingCanPlayEvent && !m_loadInitiatedByUserGesture) {
        // It should be impossible to be processing the canplay event while handling a user gesture
        // since it is dispatched asynchronously.
        ASSERT(!ScriptController::processingUserGesture());
        String host = document()->baseURL().host();
        if (host.endsWith(".npr.org", false) || equalIgnoringCase(host, "npr.org"))
            return;
    }

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    LOG(Media, "HTMLMediaElement::playInternal");

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        scheduleLoad(MediaResource);

    if (endedPlayback()) {
        ExceptionCode unused;
        seek(0, unused);
    }

    if (m_mediaController)
        m_mediaController->bringElementUpToSpeed(this);

    if (m_paused) {
        m_paused = false;
        invalidateCachedTime();
        scheduleEvent(eventNames().playEvent);

        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(eventNames().waitingEvent);
        else if (m_readyState >= HAVE_FUTURE_DATA)
            scheduleEvent(eventNames().playingEvent);
    }
    m_autoplaying = false;

    updatePlayState();
    updateMediaController();
}

void HTMLMediaElement::pause()
{
    LOG(Media, "HTMLMediaElement::pause()");

    if (userGestureRequiredForRateChange() && !ScriptController::processingUserGesture())
        return;

    pauseInternal();
}


void HTMLMediaElement::pauseInternal()
{
    LOG(Media, "HTMLMediaElement::pauseInternal");

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        scheduleLoad(MediaResource);

    m_autoplaying = false;
    
    if (!m_paused) {
        m_paused = true;
        scheduleTimeupdateEvent(false);
        scheduleEvent(eventNames().pauseEvent);
    }

    updatePlayState();
}

#if ENABLE(MEDIA_SOURCE)
void HTMLMediaElement::webkitSourceAddId(const String& id, const String& type, ExceptionCode& ec)
{
    if (id.isNull() || id.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (m_sourceIDs.contains(id)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (type.isNull() || type.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!m_player || m_currentSrc != m_mediaSourceURL || m_sourceState != SOURCE_OPEN) {
        ec = INVALID_STATE_ERR;
        return;
    }

    switch (m_player->sourceAddId(id, type)) {
    case MediaPlayer::Ok:
        m_sourceIDs.add(id);
        return;
    case MediaPlayer::NotSupported:
        ec = NOT_SUPPORTED_ERR;
        return;
    case MediaPlayer::ReachedIdLimit:
        ec = QUOTA_EXCEEDED_ERR;
        return;
    }

    ASSERT_NOT_REACHED();
}

void HTMLMediaElement::webkitSourceRemoveId(const String& id, ExceptionCode& ec)
{
    if (!isValidSourceId(id, ec))
        return;

    if (!m_player || m_currentSrc != m_mediaSourceURL || m_sourceState == SOURCE_CLOSED) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!m_player->sourceRemoveId(id))
        ASSERT_NOT_REACHED();

    m_sourceIDs.remove(id);
}

void HTMLMediaElement::webkitSourceAppend(PassRefPtr<Uint8Array> data, ExceptionCode& ec)
{
    if (!m_player || m_currentSrc != m_mediaSourceURL || m_sourceState != SOURCE_OPEN) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!data.get() || !m_player->sourceAppend(data->data(), data->length())) {
        ec = SYNTAX_ERR;
        return;
    }
}

void HTMLMediaElement::webkitSourceEndOfStream(unsigned short status, ExceptionCode& ec)
{
    if (!m_player || m_currentSrc != m_mediaSourceURL || m_sourceState != SOURCE_OPEN) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaPlayer::EndOfStreamStatus eosStatus = MediaPlayer::EosNoError;

    switch (status) {
    case EOS_NO_ERROR:
        eosStatus = MediaPlayer::EosNoError;
        break;
    case EOS_NETWORK_ERR:
        eosStatus = MediaPlayer::EosNetworkError;
        break;
    case EOS_DECODE_ERR:
        eosStatus = MediaPlayer::EosDecodeError;
        break;
    default:
        ec = SYNTAX_ERR;
        return;
    }

    setSourceState(SOURCE_ENDED);
    m_player->sourceEndOfStream(eosStatus);
}

HTMLMediaElement::SourceState HTMLMediaElement::webkitSourceState() const
{
    return m_sourceState;
}

void HTMLMediaElement::setSourceState(SourceState state)
{
    SourceState oldState = m_sourceState;
    m_sourceState = static_cast<SourceState>(state);

    if (m_sourceState == oldState)
        return;

    if (m_sourceState == SOURCE_CLOSED) {
        m_sourceIDs.clear();
        scheduleEvent(eventNames().webkitsourcecloseEvent);
        return;
    }

    if (oldState == SOURCE_OPEN && m_sourceState == SOURCE_ENDED) {
        scheduleEvent(eventNames().webkitsourceendedEvent);
        return;
    }

    if (m_sourceState == SOURCE_OPEN) {
        scheduleEvent(eventNames().webkitsourceopenEvent);
        return;
    }
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void HTMLMediaElement::webkitGenerateKeyRequest(const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionCode& ec)
{
    if (keySystem.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!m_player) {
        ec = INVALID_STATE_ERR;
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    MediaPlayer::MediaKeyException result = m_player->generateKeyRequest(keySystem, initDataPointer, initDataLength);
    ec = exceptionCodeForMediaKeyException(result);
}

void HTMLMediaElement::webkitGenerateKeyRequest(const String& keySystem, ExceptionCode& ec)
{
    webkitGenerateKeyRequest(keySystem, Uint8Array::create(0), ec);
}

void HTMLMediaElement::webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionCode& ec)
{
    if (keySystem.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!key->length()) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (!m_player) {
        ec = INVALID_STATE_ERR;
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    MediaPlayer::MediaKeyException result = m_player->addKey(keySystem, key->data(), key->length(), initDataPointer, initDataLength, sessionId);
    ec = exceptionCodeForMediaKeyException(result);
}

void HTMLMediaElement::webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionCode& ec)
{
    webkitAddKey(keySystem, key, Uint8Array::create(0), String(), ec);
}

void HTMLMediaElement::webkitCancelKeyRequest(const String& keySystem, const String& sessionId, ExceptionCode& ec)
{
    if (keySystem.isEmpty()) {
        ec = SYNTAX_ERR;
        return;
    }

    if (!m_player) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaPlayer::MediaKeyException result = m_player->cancelKeyRequest(keySystem, sessionId);
    ec = exceptionCodeForMediaKeyException(result);
}

#endif

bool HTMLMediaElement::loop() const
{
    return fastHasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    LOG(Media, "HTMLMediaElement::setLoop(%s)", boolString(b));
    setBooleanAttribute(loopAttr, b);
#if PLATFORM(MAC)
    updateDisableSleep();
#endif
}

bool HTMLMediaElement::controls() const
{
    Frame* frame = document()->frame();

    // always show controls when scripting is disabled
    if (frame && !frame->script()->canExecuteScripts(NotAboutToExecuteScript))
        return true;

    // always show controls for video when fullscreen playback is required.
    if (isVideo() && document()->page() && document()->page()->chrome()->requiresFullscreenForVideoPlayback())
        return true;

    // Always show controls when in full screen mode.
    if (isFullscreen())
        return true;

    return fastHasAttribute(controlsAttr);
}

void HTMLMediaElement::setControls(bool b)
{
    LOG(Media, "HTMLMediaElement::setControls(%s)", boolString(b));
    setBooleanAttribute(controlsAttr, b);
}

float HTMLMediaElement::volume() const
{
    return m_volume;
}

void HTMLMediaElement::setVolume(float vol, ExceptionCode& ec)
{
    LOG(Media, "HTMLMediaElement::setVolume(%f)", vol);

    if (vol < 0.0f || vol > 1.0f) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    if (m_volume != vol) {
        m_volume = vol;
        updateVolume();
        scheduleEvent(eventNames().volumechangeEvent);
    }
}

bool HTMLMediaElement::muted() const
{
    return m_muted;
}

void HTMLMediaElement::setMuted(bool muted)
{
    LOG(Media, "HTMLMediaElement::setMuted(%s)", boolString(muted));

    if (m_muted != muted) {
        m_muted = muted;
        // Avoid recursion when the player reports volume changes.
        if (!processingMediaPlayerCallback()) {
            if (m_player) {
                m_player->setMuted(m_muted);
                if (hasMediaControls())
                    mediaControls()->changedMute();
            }
        }
        scheduleEvent(eventNames().volumechangeEvent);
    }
}

void HTMLMediaElement::togglePlayState()
{
    LOG(Media, "HTMLMediaElement::togglePlayState - canPlay() is %s", boolString(canPlay()));

    // We can safely call the internal play/pause methods, which don't check restrictions, because
    // this method is only called from the built-in media controller
    if (canPlay()) {
        updatePlaybackRate();
        playInternal();
    } else 
        pauseInternal();
}

void HTMLMediaElement::beginScrubbing()
{
    LOG(Media, "HTMLMediaElement::beginScrubbing - paused() is %s", boolString(paused()));

    if (!paused()) {
        if (ended()) {
            // Because a media element stays in non-paused state when it reaches end, playback resumes 
            // when the slider is dragged from the end to another position unless we pause first. Do 
            // a "hard pause" so an event is generated, since we want to stay paused after scrubbing finishes.
            pause();
        } else {
            // Not at the end but we still want to pause playback so the media engine doesn't try to
            // continue playing during scrubbing. Pause without generating an event as we will 
            // unpause after scrubbing finishes.
            setPausedInternal(true);
        }
    }
}

void HTMLMediaElement::endScrubbing()
{
    LOG(Media, "HTMLMediaElement::endScrubbing - m_pausedInternal is %s", boolString(m_pausedInternal));

    if (m_pausedInternal)
        setPausedInternal(false);
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const double maxTimeupdateEventFrequency = 0.25;

static const double timeWithoutMouseMovementBeforeHidingControls = 3;

void HTMLMediaElement::startPlaybackProgressTimer()
{
    if (m_playbackProgressTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_previousProgress = 0;
    m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency);
}

void HTMLMediaElement::playbackProgressTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);

    if (m_fragmentEndTime != invalidMediaTime && currentTime() >= m_fragmentEndTime && m_playbackRate > 0) {
        m_fragmentEndTime = invalidMediaTime;
        if (!m_mediaController && !m_paused) {
            // changes paused to true and fires a simple event named pause at the media element.
            pauseInternal();
        }
    }
    
    scheduleTimeupdateEvent(true);

    if (!m_playbackRate)
        return;

    if (!m_paused && hasMediaControls())
        mediaControls()->playbackProgressed();
    
#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        updateActiveTextTrackCues(currentTime());
#endif
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent)
{
    double now = WTF::currentTime();
    double timedelta = now - m_lastTimeUpdateEventWallTime;

    // throttle the periodic events
    if (periodicEvent && timedelta < maxTimeupdateEventFrequency)
        return;

    // Some media engines make multiple "time changed" callbacks at the same time, but we only want one
    // event at a given time so filter here
    float movieTime = currentTime();
    if (movieTime != m_lastTimeUpdateEventMovieTime) {
        scheduleEvent(eventNames().timeupdateEvent);
        m_lastTimeUpdateEventWallTime = now;
        m_lastTimeUpdateEventMovieTime = movieTime;
    }
}

bool HTMLMediaElement::canPlay() const
{
    return paused() || ended() || m_readyState < HAVE_METADATA;
}

float HTMLMediaElement::percentLoaded() const
{
    if (!m_player)
        return 0;
    float duration = m_player->duration();

    if (!duration || isinf(duration))
        return 0;

    float buffered = 0;
    RefPtr<TimeRanges> timeRanges = m_player->buffered();
    for (unsigned i = 0; i < timeRanges->length(); ++i) {
        ExceptionCode ignoredException;
        float start = timeRanges->start(i, ignoredException);
        float end = timeRanges->end(i, ignoredException);
        buffered += end - start;
    }
    return buffered / duration;
}

#if ENABLE(VIDEO_TRACK)
PassRefPtr<TextTrack> HTMLMediaElement::addTextTrack(const String& kind, const String& label, const String& language, ExceptionCode& ec)
{
    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return 0;

    // 4.8.10.12.4 Text track API
    // The addTextTrack(kind, label, language) method of media elements, when invoked, must run the following steps:
    
    // 1. If kind is not one of the following strings, then throw a SyntaxError exception and abort these steps
    if (!TextTrack::isValidKindKeyword(kind)) {
        ec = SYNTAX_ERR;
        return 0;
    }

    // 2. If the label argument was omitted, let label be the empty string.
    // 3. If the language argument was omitted, let language be the empty string.
    // 4. Create a new TextTrack object.
    RefPtr<TextTrack> textTrack = TextTrack::create(ActiveDOMObject::scriptExecutionContext(), this, kind, label, language);

    // 5. Create a new text track corresponding to the new object, and set its text track kind to kind, its text 
    // track label to label, its text track language to language, its text track readiness state to the text track
    // loaded state, its text track mode to the text track hidden mode, and its text track list of cues to an empty list.
    
    // 6. Add the new text track to the media element's list of text tracks.
    textTracks()->append(textTrack);

    return textTrack.release();
}

TextTrackList* HTMLMediaElement::textTracks() 
{
    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return 0;

    if (!m_textTracks)
        m_textTracks = TextTrackList::create(this, ActiveDOMObject::scriptExecutionContext());

    return m_textTracks.get();
}

HTMLTrackElement* HTMLMediaElement::showingTrackWithSameKind(HTMLTrackElement* trackElement) const
{
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (trackElement == node)
            continue;
        if (!node->hasTagName(trackTag))
            continue;

        HTMLTrackElement* showingTrack = static_cast<HTMLTrackElement*>(node);
        if (showingTrack->kind() == trackElement->kind() && showingTrack->track()->mode() == TextTrack::SHOWING)
            return showingTrack;
    }
    
    return 0;
}

void HTMLMediaElement::didAddTrack(HTMLTrackElement* trackElement)
{
    ASSERT(trackElement->hasTagName(trackTag));

    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the new parent is a media element, 
    // then the user agent must add the track element's corresponding text track to the 
    // media element's list of text tracks ... [continues in TextTrackList::append]
    RefPtr<TextTrack> textTrack = trackElement->track();
    if (!textTrack)
        return;
    
    textTracks()->append(textTrack);
    
    // Do not schedule the track loading until parsing finishes so we don't start before all tracks
    // in the markup have been added.
    if (!m_parsingInProgress)
        scheduleLoad(TextTrackResource);
}

void HTMLMediaElement::willRemoveTrack(HTMLTrackElement* trackElement)
{
    ASSERT(trackElement->hasTagName(trackTag));

    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return;

#if !LOG_DISABLED
    if (trackElement->hasTagName(trackTag)) {
        KURL url = trackElement->getNonEmptyURLAttribute(srcAttr);
        LOG(Media, "HTMLMediaElement::willRemoveTrack - 'src' is %s", urlForLogging(url).utf8().data());
    }
#endif

    trackElement->setHasBeenConfigured(false);

    if (!m_textTracks)
        return;
    
    RefPtr<TextTrack> textTrack = trackElement->track();
    if (!textTrack)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the old parent was a media element, 
    // then the user agent must remove the track element's corresponding text track from the 
    // media element's list of text tracks.
    m_textTracks->remove(textTrack.get());
    size_t index = m_textTracksWhenResourceSelectionBegan.find(textTrack.get());
    if (index != notFound)
        m_textTracksWhenResourceSelectionBegan.remove(index);
}

bool HTMLMediaElement::userIsInterestedInThisLanguage(const String&) const
{
    // FIXME: check the user's language preference - bugs.webkit.org/show_bug.cgi?id=74121
    return true;
}

bool HTMLMediaElement::userIsInterestedInThisTrackKind(String kind) const
{
    // If ... the user has indicated an interest in having a track with this text track kind, text track language, ... 
    Settings* settings = document()->settings();
    if (!settings)
        return false;

    if (kind == TextTrack::subtitlesKeyword())
        return settings->shouldDisplaySubtitles();
    if (kind == TextTrack::captionsKeyword())
        return settings->shouldDisplayCaptions();
    if (kind == TextTrack::descriptionsKeyword())
        return settings->shouldDisplayTextDescriptions();

    return false;
}

void HTMLMediaElement::configureTextTrackGroup(const TrackGroup& group) const
{
    ASSERT(group.tracks.size());

    String bestMatchingLanguage;
    if (group.hasSrcLang) {
        Vector<String> languages;
        languages.reserveInitialCapacity(group.tracks.size());
        for (size_t i = 0; i < group.tracks.size(); ++i) {
            String srcLanguage = group.tracks[i]->track()->language();
            if (srcLanguage.length())
                languages.append(srcLanguage);
        }
        bestMatchingLanguage = preferredLanguageFromList(languages);
    }

    // First, find the track in the group that should be enabled (if any).
    HTMLTrackElement* trackElementToEnable = 0;
    HTMLTrackElement* defaultTrack = 0;
    HTMLTrackElement* fallbackTrack = 0;
    for (size_t i = 0; !trackElementToEnable && i < group.tracks.size(); ++i) {
        HTMLTrackElement* trackElement = group.tracks[i];
        RefPtr<TextTrack> textTrack = trackElement->track();

        if (userIsInterestedInThisTrackKind(textTrack->kind())) {
            // * If the text track kind is { [subtitles or captions] [descriptions] } and the user has indicated an interest in having a
            // track with this text track kind, text track language, and text track label enabled, and there is no
            // other text track in the media element's list of text tracks with a text track kind of either subtitles
            // or captions whose text track mode is showing
            // ...
            // * If the text track kind is chapters and the text track language is one that the user agent has reason
            // to believe is appropriate for the user, and there is no other text track in the media element's list of
            // text tracks with a text track kind of chapters whose text track mode is showing
            //    Let the text track mode be showing.
            if (bestMatchingLanguage.length()) {
                if (textTrack->language() == bestMatchingLanguage)
                    trackElementToEnable = trackElement;
            } else if (trackElement->isDefault()) {
                // The user is interested in this type of track, but their language preference doesn't match any track so we will
                // enable the 'default' track.
                defaultTrack = trackElement;
            }

            // Remember the first track that doesn't match language or have 'default' to potentially use as fallback.
            if (!fallbackTrack)
                fallbackTrack = trackElement;
        } else if (!group.visibleTrack && !defaultTrack && trackElement->isDefault()) {
            // * If the track element has a default attribute specified, and there is no other text track in the media
            // element's list of text tracks whose text track mode is showing or showing by default
            //    Let the text track mode be showing by default.
            defaultTrack = trackElement;
        }
    }

    if (!trackElementToEnable && defaultTrack)
        trackElementToEnable = defaultTrack;

    // If no track matches the user's preferred language and non was marked 'default', enable the first track
    // because the user has explicitly stated a preference for this kind of track.
    if (!trackElementToEnable && fallbackTrack)
        trackElementToEnable = fallbackTrack;

    for (size_t i = 0; i < group.tracks.size(); ++i) {
        HTMLTrackElement* trackElement = group.tracks[i];
        RefPtr<TextTrack> textTrack = trackElement->track();
        ExceptionCode unusedException;
        
        if (trackElementToEnable == trackElement) {
            textTrack->setMode(TextTrack::SHOWING, unusedException);
            if (defaultTrack == trackElement)
                textTrack->setShowingByDefault(true);
        } else {
            if (textTrack->showingByDefault()) {
                // If there is a text track in the media element's list of text tracks whose text track
                // mode is showing by default, the user agent must furthermore change that text track's
                // text track mode to hidden.
                textTrack->setShowingByDefault(false);
                textTrack->setMode(TextTrack::HIDDEN, unusedException);
            } else
                textTrack->setMode(TextTrack::DISABLED, unusedException);
        }
    }

    if (trackElementToEnable && group.defaultTrack && group.defaultTrack != trackElementToEnable) {
        RefPtr<TextTrack> textTrack = group.defaultTrack->track();
        if (textTrack && textTrack->showingByDefault()) {
            ExceptionCode unusedException;
            textTrack->setShowingByDefault(false);
            textTrack->setMode(TextTrack::HIDDEN, unusedException);
        }
    }
}

void HTMLMediaElement::configureNewTextTracks()
{
    TrackGroup captionAndSubtitleTracks(TrackGroup::CaptionsAndSubtitles);
    TrackGroup descriptionTracks(TrackGroup::Description);
    TrackGroup chapterTracks(TrackGroup::Chapter);
    TrackGroup metadataTracks(TrackGroup::Metadata);
    TrackGroup otherTracks(TrackGroup::Other);

    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (!node->hasTagName(trackTag))
            continue;

        HTMLTrackElement* trackElement = static_cast<HTMLTrackElement*>(node);
        RefPtr<TextTrack> textTrack = trackElement->track();
        if (!textTrack)
            continue;

        String kind = textTrack->kind();
        TrackGroup* currentGroup;
        if (kind == TextTrack::subtitlesKeyword() || kind == TextTrack::captionsKeyword())
            currentGroup = &captionAndSubtitleTracks;
        else if (kind == TextTrack::descriptionsKeyword())
            currentGroup = &descriptionTracks;
        else if (kind == TextTrack::chaptersKeyword())
            currentGroup = &chapterTracks;
        else if (kind == TextTrack::metadataKeyword())
            currentGroup = &metadataTracks;
        else
            currentGroup = &otherTracks;

        if (!currentGroup->visibleTrack && textTrack->mode() == TextTrack::SHOWING)
            currentGroup->visibleTrack = trackElement;
        if (!currentGroup->defaultTrack && trackElement->isDefault())
            currentGroup->defaultTrack = trackElement;

        // Do not add this track to the group if it has already been automatically configured
        // as we only want to call configureTextTrack once per track so that adding another 
        // track after the initial configuration doesn't reconfigure every track - only those 
        // that should be changed by the new addition. For example all metadata tracks are 
        // disabled by default, and we don't want a track that has been enabled by script 
        // to be disabled automatically when a new metadata track is added later.
        if (trackElement->hasBeenConfigured())
            continue;
        
        if (textTrack->language().length())
            currentGroup->hasSrcLang = true;
        currentGroup->tracks.append(trackElement);
    }
    
    if (captionAndSubtitleTracks.tracks.size())
        configureTextTrackGroup(captionAndSubtitleTracks);
    if (descriptionTracks.tracks.size())
        configureTextTrackGroup(descriptionTracks);
    if (chapterTracks.tracks.size())
        configureTextTrackGroup(chapterTracks);
    if (metadataTracks.tracks.size())
        configureTextTrackGroup(metadataTracks);
    if (otherTracks.tracks.size())
        configureTextTrackGroup(otherTracks);
}
#endif

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node and next nodes so we can restore them after checking
    // to see there is another potential.
    RefPtr<HTMLSourceElement> currentSourceNode = m_currentSourceNode;
    RefPtr<Node> nextNode = m_nextChildNodeToConsider;

    KURL nextURL = selectNextSourceChild(0, 0, DoNothing);

    m_currentSourceNode = currentSourceNode;
    m_nextChildNodeToConsider = nextNode;

    return nextURL.isValid();
}

KURL HTMLMediaElement::selectNextSourceChild(ContentType* contentType, String* keySystem, InvalidURLAction actionIfInvalid)
{
#if !LOG_DISABLED
    // Don't log if this was just called to find out if there are any valid <source> elements.
    bool shouldLog = actionIfInvalid != DoNothing;
    if (shouldLog)
        LOG(Media, "HTMLMediaElement::selectNextSourceChild");
#endif

    if (!m_nextChildNodeToConsider) {
#if !LOG_DISABLED
        if (shouldLog)
            LOG(Media, "HTMLMediaElement::selectNextSourceChild -> 0x0000, \"\"");
#endif
        return KURL();
    }

    KURL mediaURL;
    Node* node;
    HTMLSourceElement* source = 0;
    String type;
    String system;
    bool lookingForStartNode = m_nextChildNodeToConsider;
    bool canUseSourceElement = false;
    bool okToLoadSourceURL;

    NodeVector potentialSourceNodes;
    getChildNodes(this, potentialSourceNodes);

    for (unsigned i = 0; !canUseSourceElement && i < potentialSourceNodes.size(); ++i) {
        node = potentialSourceNodes[i].get();
        if (lookingForStartNode && m_nextChildNodeToConsider != node)
            continue;
        lookingForStartNode = false;

        if (!node->hasTagName(sourceTag))
            continue;
        if (node->parentNode() != this)
            continue;

        source = static_cast<HTMLSourceElement*>(node);

        // If candidate does not have a src attribute, or if its src attribute's value is the empty string ... jump down to the failed step below
        mediaURL = source->getNonEmptyURLAttribute(srcAttr);
#if !LOG_DISABLED
        if (shouldLog)
            LOG(Media, "HTMLMediaElement::selectNextSourceChild - 'src' is %s", urlForLogging(mediaURL).utf8().data());
#endif
        if (mediaURL.isEmpty())
            goto check_again;
        
        if (source->fastHasAttribute(mediaAttr)) {
            MediaQueryEvaluator screenEval("screen", document()->frame(), renderer() ? renderer()->style() : 0);
            RefPtr<MediaQuerySet> media = MediaQuerySet::createAllowingDescriptionSyntax(source->media());
#if !LOG_DISABLED
            if (shouldLog)
                LOG(Media, "HTMLMediaElement::selectNextSourceChild - 'media' is %s", source->media().utf8().data());
#endif
            if (!screenEval.eval(media.get())) 
                goto check_again;
        }

        type = source->type();
        // FIXME(82965): Add support for keySystem in <source> and set system from source.
        if (type.isEmpty() && mediaURL.protocolIsData())
            type = mimeTypeFromDataURL(mediaURL);
        if (!type.isEmpty() || !system.isEmpty()) {
#if !LOG_DISABLED
            if (shouldLog)
                LOG(Media, "HTMLMediaElement::selectNextSourceChild - 'type' is '%s' - key system is '%s'", type.utf8().data(), system.utf8().data());
#endif
            if (!MediaPlayer::supportsType(ContentType(type), system))
                goto check_again;
        }

        // Is it safe to load this url?
        okToLoadSourceURL = isSafeToLoadURL(mediaURL, actionIfInvalid) && dispatchBeforeLoadEvent(mediaURL.string());

        // A 'beforeload' event handler can mutate the DOM, so check to see if the source element is still a child node.
        if (node->parentNode() != this) {
            LOG(Media, "HTMLMediaElement::selectNextSourceChild : 'beforeload' removed current element");
            source = 0;
            goto check_again;
        }

        if (!okToLoadSourceURL)
            goto check_again;

        // Making it this far means the <source> looks reasonable.
        canUseSourceElement = true;

check_again:
        if (!canUseSourceElement && actionIfInvalid == Complain && source)
            source->scheduleErrorEvent();
    }

    if (canUseSourceElement) {
        if (contentType)
            *contentType = ContentType(type);
        if (keySystem)
            *keySystem = system;
        m_currentSourceNode = source;
        m_nextChildNodeToConsider = source->nextSibling();
    } else {
        m_currentSourceNode = 0;
        m_nextChildNodeToConsider = 0;
    }

#if !LOG_DISABLED
    if (shouldLog)
        LOG(Media, "HTMLMediaElement::selectNextSourceChild -> %p, %s", m_currentSourceNode.get(), canUseSourceElement ? urlForLogging(mediaURL).utf8().data() : "");
#endif
    return canUseSourceElement ? mediaURL : KURL();
}

void HTMLMediaElement::sourceWasAdded(HTMLSourceElement* source)
{
    LOG(Media, "HTMLMediaElement::sourceWasAdded(%p)", source);

#if !LOG_DISABLED
    if (source->hasTagName(sourceTag)) {
        KURL url = source->getNonEmptyURLAttribute(srcAttr);
        LOG(Media, "HTMLMediaElement::sourceWasAdded - 'src' is %s", urlForLogging(url).utf8().data());
    }
#endif
    
    // We should only consider a <source> element when there is not src attribute at all.
    if (fastHasAttribute(srcAttr))
        return;

    // 4.8.8 - If a source element is inserted as a child of a media element that has no src 
    // attribute and whose networkState has the value NETWORK_EMPTY, the user agent must invoke 
    // the media element's resource selection algorithm.
    if (networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        scheduleLoad(MediaResource);
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_currentSourceNode && source == m_currentSourceNode->nextSibling()) {
        LOG(Media, "HTMLMediaElement::sourceWasAdded - <source> inserted immediately after current source");
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_nextChildNodeToConsider)
        return;
    
    // 4.8.9.5, resource selection algorithm, source elements section:
    // 21. Wait until the node after pointer is a node other than the end of the list. (This step might wait forever.)
    // 22. Asynchronously await a stable state...
    // 23. Set the element's delaying-the-load-event flag back to true (this delays the load event again, in case 
    // it hasn't been fired yet).
    setShouldDelayLoadEvent(true);

    // 24. Set the networkState back to NETWORK_LOADING.
    m_networkState = NETWORK_LOADING;
    
    // 25. Jump back to the find next candidate step above.
    m_nextChildNodeToConsider = source;
    scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement* source)
{
    LOG(Media, "HTMLMediaElement::sourceWasRemoved(%p)", source);

#if !LOG_DISABLED
    if (source->hasTagName(sourceTag)) {
        KURL url = source->getNonEmptyURLAttribute(srcAttr);
        LOG(Media, "HTMLMediaElement::sourceWasRemoved - 'src' is %s", urlForLogging(url).utf8().data());
    }
#endif

    if (source != m_currentSourceNode && source != m_nextChildNodeToConsider)
        return;

    if (source == m_nextChildNodeToConsider) {
        if (m_currentSourceNode)
            m_nextChildNodeToConsider = m_currentSourceNode->nextSibling();
        LOG(Media, "HTMLMediaElement::sourceRemoved - m_nextChildNodeToConsider set to %p", m_nextChildNodeToConsider.get());
    } else if (source == m_currentSourceNode) {
        // Clear the current source node pointer, but don't change the movie as the spec says:
        // 4.8.8 - Dynamically modifying a source element and its attribute when the element is already 
        // inserted in a video or audio element will have no effect.
        m_currentSourceNode = 0;
        LOG(Media, "HTMLMediaElement::sourceRemoved - m_currentSourceNode set to 0");
    }
}

void HTMLMediaElement::mediaPlayerTimeChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerTimeChanged");

#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        updateActiveTextTrackCues(currentTime());
#endif

    beginProcessingMediaPlayerCallback();

    invalidateCachedTime();

    // 4.8.10.9 step 14 & 15.  Needed if no ReadyState change is associated with the seek.
    if (m_seeking && m_readyState >= HAVE_CURRENT_DATA)
        finishSeek();
    
    // Always call scheduleTimeupdateEvent when the media engine reports a time discontinuity, 
    // it will only queue a 'timeupdate' event if we haven't already posted one at the current
    // movie time.
    scheduleTimeupdateEvent(false);

    float now = currentTime();
    float dur = duration();
    
    // When the current playback position reaches the end of the media resource when the direction of
    // playback is forwards, then the user agent must follow these steps:
    if (!isnan(dur) && dur && now >= dur && m_playbackRate > 0) {
        // If the media element has a loop attribute specified and does not have a current media controller,
        if (loop() && !m_mediaController) {
            ExceptionCode ignoredException;
            m_sentEndEvent = false;
            //  then seek to the earliest possible position of the media resource and abort these steps.
            seek(startTime(), ignoredException);
        } else {
            // If the media element does not have a current media controller, and the media element
            // has still ended playback, and the direction of playback is still forwards, and paused
            // is false,
            if (!m_mediaController && !m_paused) {
                // changes paused to true and fires a simple event named pause at the media element.
                m_paused = true;
                scheduleEvent(eventNames().pauseEvent);
            }
            // Queue a task to fire a simple event named ended at the media element.
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleEvent(eventNames().endedEvent);
            }
            // If the media element has a current media controller, then report the controller state
            // for the media element's current media controller.
            updateMediaController();
        }
    }
    else
        m_sentEndEvent = false;

    updatePlayState();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerVolumeChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerVolumeChanged");

    beginProcessingMediaPlayerCallback();
    if (m_player) {
        float vol = m_player->volume();
        if (vol != m_volume) {
            m_volume = vol;
            updateVolume();
            scheduleEvent(eventNames().volumechangeEvent);
        }
    }
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerMuteChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerMuteChanged");

    beginProcessingMediaPlayerCallback();
    if (m_player)
        setMuted(m_player->muted());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerDurationChanged(MediaPlayer* player)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerDurationChanged");

    beginProcessingMediaPlayerCallback();
    scheduleEvent(eventNames().durationchangeEvent);
    mediaPlayerCharacteristicChanged(player);
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRateChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerRateChanged");

    beginProcessingMediaPlayerCallback();

    // Stash the rate in case the one we tried to set isn't what the engine is
    // using (eg. it can't handle the rate we set)
    m_playbackRate = m_player->rate();
    if (m_playing)
        invalidateCachedTime();

#if PLATFORM(MAC)
    updateDisableSleep();
#endif

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerPlaybackStateChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerPlaybackStateChanged");

    if (!m_player || m_pausedInternal)
        return;

    beginProcessingMediaPlayerCallback();
    if (m_player->paused())
        pauseInternal();
    else
        playInternal();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSawUnsupportedTracks(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerSawUnsupportedTracks");

    // The MediaPlayer came across content it cannot completely handle.
    // This is normally acceptable except when we are in a standalone
    // MediaDocument. If so, tell the document what has happened.
    if (ownerDocument()->isMediaDocument()) {
        MediaDocument* mediaDocument = static_cast<MediaDocument*>(ownerDocument());
        mediaDocument->mediaElementSawUnsupportedTracks();
    }
}

void HTMLMediaElement::mediaPlayerResourceNotSupported(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerResourceNotSupported");

    // The MediaPlayer came across content which no installed engine supports.
    mediaLoadingFailed(MediaPlayer::FormatError);
}

// MediaPlayerPresentation methods
void HTMLMediaElement::mediaPlayerRepaint(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    updateDisplayState();
    if (renderer())
        renderer()->repaint();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerSizeChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerSizeChanged");

    beginProcessingMediaPlayerCallback();
    if (renderer())
        renderer()->updateFromElement();
    endProcessingMediaPlayerCallback();
}

#if USE(ACCELERATED_COMPOSITING)
bool HTMLMediaElement::mediaPlayerRenderingCanBeAccelerated(MediaPlayer*)
{
    if (renderer() && renderer()->isVideo()) {
        ASSERT(renderer()->view());
        return renderer()->view()->compositor()->canAccelerateVideoRendering(toRenderVideo(renderer()));
    }
    return false;
}

void HTMLMediaElement::mediaPlayerRenderingModeChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerRenderingModeChanged");

    // Kick off a fake recalcStyle that will update the compositing tree.
    setNeedsStyleRecalc(SyntheticStyleChange);
}
#endif

void HTMLMediaElement::mediaPlayerEngineUpdated(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerEngineUpdated");
    beginProcessingMediaPlayerCallback();
    if (renderer())
        renderer()->updateFromElement();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerFirstVideoFrameAvailable(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerFirstVideoFrameAvailable");
    beginProcessingMediaPlayerCallback();
    if (displayMode() == PosterWaitingForVideo) {
        setDisplayMode(Video);
#if USE(ACCELERATED_COMPOSITING)
        mediaPlayerRenderingModeChanged(m_player.get());
#endif
    }
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerCharacteristicChanged(MediaPlayer*)
{
    LOG(Media, "HTMLMediaElement::mediaPlayerCharacteristicChanged");
    
    beginProcessingMediaPlayerCallback();
    if (hasMediaControls())
        mediaControls()->reset();
    if (renderer())
        renderer()->updateFromElement();
    endProcessingMediaPlayerCallback();
}

PassRefPtr<TimeRanges> HTMLMediaElement::buffered() const
{
    if (!m_player)
        return TimeRanges::create();
    return m_player->buffered();
}

PassRefPtr<TimeRanges> HTMLMediaElement::played()
{
    if (m_playing) {
        float time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);
    }

    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();

    return m_playedTimeRanges->copy();
}

PassRefPtr<TimeRanges> HTMLMediaElement::seekable() const
{
    return m_player ? m_player->seekable() : TimeRanges::create();
}

bool HTMLMediaElement::potentiallyPlaying() const
{
    // "pausedToBuffer" means the media engine's rate is 0, but only because it had to stop playing
    // when it ran out of buffered data. A movie is this state is "potentially playing", modulo the
    // checks in couldPlayIfEnoughData().
    bool pausedToBuffer = m_readyStateMaximum >= HAVE_FUTURE_DATA && m_readyState < HAVE_FUTURE_DATA;
    return (pausedToBuffer || m_readyState >= HAVE_FUTURE_DATA) && couldPlayIfEnoughData() && !isBlockedOnMediaController();
}

bool HTMLMediaElement::couldPlayIfEnoughData() const
{
    return !paused() && !endedPlayback() && !stoppedDueToErrors() && !pausedForUserInteraction();
}

bool HTMLMediaElement::endedPlayback() const
{
    float dur = duration();
    if (!m_player || isnan(dur))
        return false;

    // 4.8.10.8 Playing the media resource

    // A media element is said to have ended playback when the element's 
    // readyState attribute is HAVE_METADATA or greater, 
    if (m_readyState < HAVE_METADATA)
        return false;

    // and the current playback position is the end of the media resource and the direction
    // of playback is forwards, Either the media element does not have a loop attribute specified,
    // or the media element has a current media controller.
    float now = currentTime();
    if (m_playbackRate > 0)
        return dur > 0 && now >= dur && (!loop() || m_mediaController);

    // or the current playback position is the earliest possible position and the direction 
    // of playback is backwards
    if (m_playbackRate < 0)
        return now <= 0;

    return false;
}

bool HTMLMediaElement::stoppedDueToErrors() const
{
    if (m_readyState >= HAVE_METADATA && m_error) {
        RefPtr<TimeRanges> seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }
    
    return false;
}

bool HTMLMediaElement::pausedForUserInteraction() const
{
//    return !paused() && m_readyState >= HAVE_FUTURE_DATA && [UA requires a decitions from the user]
    return false;
}

float HTMLMediaElement::minTimeSeekable() const
{
    return 0;
}

float HTMLMediaElement::maxTimeSeekable() const
{
    return m_player ? m_player->maxTimeSeekable() : 0;
}
    
void HTMLMediaElement::updateVolume()
{
    if (!m_player)
        return;

    // Avoid recursion when the player reports volume changes.
    if (!processingMediaPlayerCallback()) {
        Page* page = document()->page();
        float volumeMultiplier = page ? page->mediaVolume() : 1;
        bool shouldMute = m_muted;

        if (m_mediaController) {
            volumeMultiplier *= m_mediaController->volume();
            shouldMute = m_mediaController->muted();
        }

        m_player->setMuted(shouldMute);
        m_player->setVolume(m_volume * volumeMultiplier);
    }

    if (hasMediaControls())
        mediaControls()->changedVolume();
}

void HTMLMediaElement::updatePlayState()
{
    if (!m_player)
        return;

    if (m_pausedInternal) {
        if (!m_player->paused())
            m_player->pause();
        refreshCachedTime();
        m_playbackProgressTimer.stop();
        if (hasMediaControls())
            mediaControls()->playbackStopped();
        return;
    }
    
    bool shouldBePlaying = potentiallyPlaying();
    bool playerPaused = m_player->paused();

    LOG(Media, "HTMLMediaElement::updatePlayState - shouldBePlaying = %s, playerPaused = %s", 
        boolString(shouldBePlaying), boolString(playerPaused));

    if (shouldBePlaying) {
        setDisplayMode(Video);
        invalidateCachedTime();

        if (playerPaused) {
            if (!m_isFullscreen && isVideo() && document() && document()->page() && document()->page()->chrome()->requiresFullscreenForVideoPlayback())
                enterFullscreen();

            // Set rate, muted before calling play in case they were set before the media engine was setup.
            // The media engine should just stash the rate and muted values since it isn't already playing.
            m_player->setRate(m_playbackRate);
            m_player->setMuted(m_muted);

            m_player->play();
        }

        if (hasMediaControls())
            mediaControls()->playbackStarted();
        startPlaybackProgressTimer();
        m_playing = true;

    } else { // Should not be playing right now
        if (!playerPaused)
            m_player->pause();
        refreshCachedTime();

        m_playbackProgressTimer.stop();
        m_playing = false;
        float time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);

        if (couldPlayIfEnoughData())
            prepareToPlay();

        if (hasMediaControls())
            mediaControls()->playbackStopped();
    }

    updateMediaController();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::setPausedInternal(bool b)
{
    m_pausedInternal = b;
    updatePlayState();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
}

void HTMLMediaElement::userCancelledLoad()
{
    LOG(Media, "HTMLMediaElement::userCancelledLoad");

    if (m_networkState == NETWORK_EMPTY || m_completelyLoaded)
        return;

    // If the media data fetching process is aborted by the user:

    // 1 - The user agent should cancel the fetching process.
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    m_player.clear();
#endif
    stopPeriodicTimers();
    m_loadTimer.stop();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is set to MEDIA_ERR_ABORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(eventNames().abortEvent);

#if ENABLE(MEDIA_SOURCE)
    if (m_sourceState != SOURCE_CLOSED)
        setSourceState(SOURCE_CLOSED);
#endif

    // 4 - If the media element's readyState attribute has a value equal to HAVE_NOTHING, set the 
    // element's networkState attribute to the NETWORK_EMPTY value and queue a task to fire a 
    // simple event named emptied at the element. Otherwise, set the element's networkState 
    // attribute to the NETWORK_IDLE value.
    if (m_readyState == HAVE_NOTHING) {
        m_networkState = NETWORK_EMPTY;
        scheduleEvent(eventNames().emptiedEvent);
    }
    else
        m_networkState = NETWORK_IDLE;

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = 0;

    // Reset m_readyState since m_player is gone.
    m_readyState = HAVE_NOTHING;
    updateMediaController();
#if ENABLE(VIDEO_TRACK)
    if (RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        updateActiveTextTrackCues(0);
#endif
}

bool HTMLMediaElement::canSuspend() const
{
    return true; 
}

void HTMLMediaElement::stop()
{
    LOG(Media, "HTMLMediaElement::stop");
    if (m_isFullscreen)
        exitFullscreen();
    
    m_inActiveDocument = false;
    userCancelledLoad();
    
    // Stop the playback without generating events
    setPausedInternal(true);
    
    if (renderer())
        renderer()->updateFromElement();
    
    stopPeriodicTimers();
    cancelPendingEventsAndCallbacks();
}

void HTMLMediaElement::suspend(ReasonForSuspension why)
{
    LOG(Media, "HTMLMediaElement::suspend");

    switch (why)
    {
        case DocumentWillBecomeInactive:
            stop();
            break;
        case PageWillBeSuspended:
        case JavaScriptDebuggerPaused:
        case WillDeferLoading:
            // Do nothing, we don't pause media playback in these cases.
            break;
    }
}

void HTMLMediaElement::resume()
{
    LOG(Media, "HTMLMediaElement::resume");

    m_inActiveDocument = true;
    setPausedInternal(false);

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_ABORTED) {
        // Restart the load if it was aborted in the middle by moving the document to the page cache.
        // m_error is only left at MEDIA_ERR_ABORTED when the document becomes inactive (it is set to
        //  MEDIA_ERR_ABORTED while the abortEvent is being sent, but cleared immediately afterwards).
        // This behavior is not specified but it seems like a sensible thing to do.
        ExceptionCode ec;
        load(ec);
    }

    if (renderer())
        renderer()->updateFromElement();
}

bool HTMLMediaElement::hasPendingActivity() const
{
    return m_asyncEventQueue->hasPendingEvents();
}

void HTMLMediaElement::mediaVolumeDidChange()
{
    LOG(Media, "HTMLMediaElement::mediaVolumeDidChange");
    updateVolume();
}

void HTMLMediaElement::defaultEventHandler(Event* event)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;

    Widget* widget = toRenderWidget(r)->widget();
    if (widget)
        widget->handleEvent(event);
#else
    HTMLElement::defaultEventHandler(event);
#endif
}

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)

void HTMLMediaElement::ensureMediaPlayer()
{
    if (!m_player)
        createMediaPlayer();
}

void HTMLMediaElement::deliverNotification(MediaPlayerProxyNotificationType notification)
{
    if (notification == MediaPlayerNotificationPlayPauseButtonPressed) {
        togglePlayState();
        return;
    }

    if (m_player)
        m_player->deliverNotification(notification);
}

void HTMLMediaElement::setMediaPlayerProxy(WebMediaPlayerProxy* proxy)
{
    ensureMediaPlayer();
    m_player->setMediaPlayerProxy(proxy);
}

void HTMLMediaElement::getPluginProxyParams(KURL& url, Vector<String>& names, Vector<String>& values)
{
    RefPtr<HTMLMediaElement> protect(this); // selectNextSourceChild may fire 'beforeload', which can make arbitrary DOM mutations.

    Frame* frame = document()->frame();

    if (isVideo()) {
        KURL posterURL = getNonEmptyURLAttribute(posterAttr);
        if (!posterURL.isEmpty() && frame && frame->loader()->willLoadMediaElementURL(posterURL)) {
            names.append("_media_element_poster_");
            values.append(posterURL.string());
        }
    }

    if (controls()) {
        names.append("_media_element_controls_");
        values.append("true");
    }

    url = src();
    if (!isSafeToLoadURL(url, Complain))
        url = selectNextSourceChild(0, 0, DoNothing);

    m_currentSrc = url;
    if (url.isValid() && frame && frame->loader()->willLoadMediaElementURL(url)) {
        names.append("_media_element_src_");
        values.append(m_currentSrc.string());
    }
}

void HTMLMediaElement::createMediaPlayerProxy()
{
    ensureMediaPlayer();

    if (m_proxyWidget || (inDocument() && !m_needWidgetUpdate))
        return;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    LOG(Media, "HTMLMediaElement::createMediaPlayerProxy");

    KURL url;
    Vector<String> paramNames;
    Vector<String> paramValues;

    getPluginProxyParams(url, paramNames, paramValues);
    
    // Hang onto the proxy widget so it won't be destroyed if the plug-in is set to
    // display:none
    m_proxyWidget = frame->loader()->subframeLoader()->loadMediaPlayerProxyPlugin(this, url, paramNames, paramValues);
    if (m_proxyWidget)
        m_needWidgetUpdate = false;
}

void HTMLMediaElement::updateWidget(PluginCreationOption)
{
    mediaElement->setNeedWidgetUpdate(false);

    Vector<String> paramNames;
    Vector<String> paramValues;
    // FIXME: Rename kurl to something more sensible.
    KURL kurl;

    mediaElement->getPluginProxyParams(kurl, paramNames, paramValues);
    // FIXME: What if document()->frame() is 0?
    SubframeLoader* loader = document()->frame()->loader()->subframeLoader();
    loader->loadMediaPlayerProxyPlugin(mediaElement, kurl, paramNames, paramValues);
}

#endif // ENABLE(PLUGIN_PROXY_FOR_VIDEO)

bool HTMLMediaElement::isFullscreen() const
{
    if (m_isFullscreen)
        return true;
    
#if ENABLE(FULLSCREEN_API)
    if (document()->webkitIsFullScreen() && document()->webkitCurrentFullScreenElement() == this)
        return true;
#endif
    
    return false;
}

void HTMLMediaElement::enterFullscreen()
{
    LOG(Media, "HTMLMediaElement::enterFullscreen");

#if ENABLE(FULLSCREEN_API)
    if (document() && document()->settings() && document()->settings()->fullScreenEnabled()) {
        document()->requestFullScreenForElement(this, 0, Document::ExemptIFrameAllowFulScreenRequirement);
        return;
    }
#endif
    ASSERT(!m_isFullscreen);
    m_isFullscreen = true;
    if (hasMediaControls())
        mediaControls()->enteredFullscreen();
    if (document() && document()->page()) {
        document()->page()->chrome()->client()->enterFullscreenForNode(this);
        scheduleEvent(eventNames().webkitbeginfullscreenEvent);
    }
}

void HTMLMediaElement::exitFullscreen()
{
    LOG(Media, "HTMLMediaElement::exitFullscreen");

#if ENABLE(FULLSCREEN_API)
    if (document() && document()->settings() && document()->settings()->fullScreenEnabled()) {
        if (document()->webkitIsFullScreen() && document()->webkitCurrentFullScreenElement() == this)
            document()->webkitCancelFullScreen();
        return;
    }
#endif
    ASSERT(m_isFullscreen);
    m_isFullscreen = false;
    if (hasMediaControls())
        mediaControls()->exitedFullscreen();
    if (document() && document()->page()) {
        if (document()->page()->chrome()->requiresFullscreenForVideoPlayback())
            pauseInternal();
        document()->page()->chrome()->client()->exitFullscreenForNode(this);
        scheduleEvent(eventNames().webkitendfullscreenEvent);
    }
}

void HTMLMediaElement::didBecomeFullscreenElement()
{
    if (hasMediaControls())
        mediaControls()->enteredFullscreen();
}

void HTMLMediaElement::willStopBeingFullscreenElement()
{
    if (hasMediaControls())
        mediaControls()->exitedFullscreen();
}

PlatformMedia HTMLMediaElement::platformMedia() const
{
    return m_player ? m_player->platformMedia() : NoPlatformMedia;
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* HTMLMediaElement::platformLayer() const
{
    return m_player ? m_player->platformLayer() : 0;
}
#endif

bool HTMLMediaElement::hasClosedCaptions() const
{
    return m_player && m_player->hasClosedCaptions();
}

bool HTMLMediaElement::closedCaptionsVisible() const
{
    return m_closedCaptionsVisible;
}

void HTMLMediaElement::setClosedCaptionsVisible(bool closedCaptionVisible)
{
    LOG(Media, "HTMLMediaElement::setClosedCaptionsVisible(%s)", boolString(closedCaptionVisible));

    if (!m_player ||!hasClosedCaptions())
        return;

    m_closedCaptionsVisible = closedCaptionVisible;
    m_player->setClosedCaptionsVisible(closedCaptionVisible);
    if (hasMediaControls())
        mediaControls()->changedClosedCaptionsVisibility();
}

void HTMLMediaElement::setWebkitClosedCaptionsVisible(bool visible)
{
    setClosedCaptionsVisible(visible);
}

bool HTMLMediaElement::webkitClosedCaptionsVisible() const
{
    return closedCaptionsVisible();
}


bool HTMLMediaElement::webkitHasClosedCaptions() const
{
    return hasClosedCaptions();
}

#if ENABLE(MEDIA_STATISTICS)
unsigned HTMLMediaElement::webkitAudioDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->audioDecodedByteCount();
}

unsigned HTMLMediaElement::webkitVideoDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->videoDecodedByteCount();
}
#endif

void HTMLMediaElement::mediaCanStart()
{
    LOG(Media, "HTMLMediaElement::mediaCanStart");

    ASSERT(m_isWaitingUntilMediaCanStart);
    m_isWaitingUntilMediaCanStart = false;
    loadInternal();
}

bool HTMLMediaElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMediaElement::setShouldDelayLoadEvent(bool shouldDelay)
{
    if (m_shouldDelayLoadEvent == shouldDelay)
        return;

    LOG(Media, "HTMLMediaElement::setShouldDelayLoadEvent(%s)", boolString(shouldDelay));

    m_shouldDelayLoadEvent = shouldDelay;
    if (shouldDelay)
        document()->incrementLoadEventDelayCount();
    else
        document()->decrementLoadEventDelayCount();
}
    

void HTMLMediaElement::getSitesInMediaCache(Vector<String>& sites)
{
    MediaPlayer::getSitesInMediaCache(sites);
}

void HTMLMediaElement::clearMediaCache()
{
    MediaPlayer::clearMediaCache();
}

void HTMLMediaElement::clearMediaCacheForSite(const String& site)
{
    MediaPlayer::clearMediaCacheForSite(site);
}

void HTMLMediaElement::privateBrowsingStateDidChange()
{
    if (!m_player)
        return;

    Settings* settings = document()->settings();
    bool privateMode = !settings || settings->privateBrowsingEnabled();
    LOG(Media, "HTMLMediaElement::privateBrowsingStateDidChange(%s)", boolString(privateMode));
    m_player->setPrivateBrowsingMode(privateMode);
}

MediaControls* HTMLMediaElement::mediaControls()
{
    return toMediaControls(shadow()->oldestShadowRoot()->firstChild());
}

bool HTMLMediaElement::hasMediaControls()
{
    ElementShadow* elementShadow = shadow();
    if (!elementShadow)
        return false;

    Node* node = elementShadow->oldestShadowRoot()->firstChild();
    return node && node->isMediaControls();
}

bool HTMLMediaElement::createMediaControls()
{
    if (hasMediaControls())
        return true;

    ExceptionCode ec;
    RefPtr<MediaControls> controls = MediaControls::create(document());
    if (!controls)
        return false;

    controls->setMediaController(m_mediaController ? m_mediaController.get() : static_cast<MediaControllerInterface*>(this));
    controls->reset();
    if (isFullscreen())
        controls->enteredFullscreen();

    ensureShadowRoot()->appendChild(controls, ec);
    return true;
}

void HTMLMediaElement::configureMediaControls()
{
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (!controls() || !inDocument()) {
        if (hasMediaControls())
            mediaControls()->hide();
        return;
    }

    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->show();
#else
    if (m_player)
        m_player->setControls(controls());
#endif
}

#if ENABLE(VIDEO_TRACK)
void HTMLMediaElement::configureTextTrackDisplay()
{
    ASSERT(m_textTracks);

    bool haveVisibleTextTrack = false;
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        if (m_textTracks->item(i)->mode() == TextTrack::SHOWING) {
            haveVisibleTextTrack = true;
            break;
        }
    }

    if (m_haveVisibleTextTrack == haveVisibleTextTrack)
        return;
    m_haveVisibleTextTrack = haveVisibleTextTrack;

    if (!m_haveVisibleTextTrack && !hasMediaControls())
        return;
    if (!hasMediaControls() && !createMediaControls())
        return;
    mediaControls()->updateTextTrackDisplay();
}
#endif

void* HTMLMediaElement::preDispatchEventHandler(Event* event)
{
    if (event && event->type() == eventNames().webkitfullscreenchangeEvent)
        configureMediaControls();

    return 0;
}

void HTMLMediaElement::createMediaPlayer()
{
#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode)
        m_audioSourceNode->lock();
#endif
        
    m_player = MediaPlayer::create(this);

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode) {
        // When creating the player, make sure its AudioSourceProvider knows about the MediaElementAudioSourceNode.
        if (audioSourceProvider())
            audioSourceProvider()->setClient(m_audioSourceNode);

        m_audioSourceNode->unlock();
    }
#endif
}

#if ENABLE(WEB_AUDIO)
void HTMLMediaElement::setAudioSourceNode(MediaElementAudioSourceNode* sourceNode)
{
    m_audioSourceNode = sourceNode;

    if (audioSourceProvider())
        audioSourceProvider()->setClient(m_audioSourceNode);
}

AudioSourceProvider* HTMLMediaElement::audioSourceProvider()
{
    if (m_player)
        return m_player->audioSourceProvider();

    return 0;
}
#endif

#if ENABLE(MICRODATA)
String HTMLMediaElement::itemValueText() const
{
    return getURLAttribute(srcAttr);
}

void HTMLMediaElement::setItemValueText(const String& value, ExceptionCode&)
{
    setAttribute(srcAttr, value);
}
#endif

const String& HTMLMediaElement::mediaGroup() const
{
    return m_mediaGroup;
}

void HTMLMediaElement::setMediaGroup(const String& group)
{
    if (m_mediaGroup == group)
        return;
    m_mediaGroup = group;

    // When a media element is created with a mediagroup attribute, and when a media element's mediagroup 
    // attribute is set, changed, or removed, the user agent must run the following steps:
    // 1. Let m [this] be the media element in question.
    // 2. Let m have no current media controller, if it currently has one.
    setController(0);

    // 3. If m's mediagroup attribute is being removed, then abort these steps.
    if (group.isNull() || group.isEmpty())
        return;

    // 4. If there is another media element whose Document is the same as m's Document (even if one or both
    // of these elements are not actually in the Document), 
    HashSet<HTMLMediaElement*> elements = documentToElementSetMap().get(document());
    for (HashSet<HTMLMediaElement*>::iterator i = elements.begin(); i != elements.end(); ++i) {
        if (*i == this)
            continue;

        // and which also has a mediagroup attribute, and whose mediagroup attribute has the same value as
        // the new value of m's mediagroup attribute,        
        if ((*i)->mediaGroup() == group) {
            //  then let controller be that media element's current media controller.
            setController((*i)->controller());
            return;
        }
    }

    // Otherwise, let controller be a newly created MediaController.
    setController(MediaController::create(Node::scriptExecutionContext()));
}

MediaController* HTMLMediaElement::controller() const
{
    return m_mediaController.get();
}

void HTMLMediaElement::setController(PassRefPtr<MediaController> controller)
{
    if (m_mediaController)
        m_mediaController->removeMediaElement(this);

    m_mediaController = controller;

    if (m_mediaController)
        m_mediaController->addMediaElement(this);

    if (hasMediaControls())
        mediaControls()->setMediaController(m_mediaController ? m_mediaController.get() : static_cast<MediaControllerInterface*>(this));
}

void HTMLMediaElement::updateMediaController()
{
    if (m_mediaController)
        m_mediaController->reportControllerState();
}

bool HTMLMediaElement::dispatchEvent(PassRefPtr<Event> event)
{
    bool dispatchResult;
    bool isCanPlayEvent;

    isCanPlayEvent = (event->type() == eventNames().canplayEvent);

    if (isCanPlayEvent)
        m_dispatchingCanPlayEvent = true;

    dispatchResult = HTMLElement::dispatchEvent(event);

    if (isCanPlayEvent)
        m_dispatchingCanPlayEvent = false;

    return dispatchResult;
}

bool HTMLMediaElement::isBlocked() const
{
    // A media element is a blocked media element if its readyState attribute is in the
    // HAVE_NOTHING state, the HAVE_METADATA state, or the HAVE_CURRENT_DATA state,
    if (m_readyState <= HAVE_CURRENT_DATA)
        return true;

    // or if the element has paused for user interaction.
    return pausedForUserInteraction();
}

bool HTMLMediaElement::isBlockedOnMediaController() const
{
    if (!m_mediaController)
        return false;

    // A media element is blocked on its media controller if the MediaController is a blocked 
    // media controller,
    if (m_mediaController->isBlocked())
        return true;

    // or if its media controller position is either before the media resource's earliest possible 
    // position relative to the MediaController's timeline or after the end of the media resource 
    // relative to the MediaController's timeline.
    float mediaControllerPosition = m_mediaController->currentTime();
    if (mediaControllerPosition < startTime() || mediaControllerPosition > startTime() + duration())
        return true;

    return false;
}

void HTMLMediaElement::prepareMediaFragmentURI()
{
    MediaFragmentURIParser fragmentParser(m_currentSrc);
    float dur = duration();
    
    double start = fragmentParser.startTime();
    if (start != MediaFragmentURIParser::invalidTimeValue() && start > 0) {
        m_fragmentStartTime = start;
        if (m_fragmentStartTime > dur)
            m_fragmentStartTime = dur;
    } else
        m_fragmentStartTime = invalidMediaTime;
    
    double end = fragmentParser.endTime();
    if (end != MediaFragmentURIParser::invalidTimeValue() && end > 0 && end > m_fragmentStartTime) {
        m_fragmentEndTime = end;
        if (m_fragmentEndTime > dur)
            m_fragmentEndTime = dur;
    } else
        m_fragmentEndTime = invalidMediaTime;
    
    if (m_fragmentStartTime != invalidMediaTime && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();
}

void HTMLMediaElement::applyMediaFragmentURI()
{
    if (m_fragmentStartTime != invalidMediaTime) {
        ExceptionCode ignoredException;
        m_sentEndEvent = false;
        seek(m_fragmentStartTime, ignoredException);
    }
}

#if PLATFORM(MAC)
void HTMLMediaElement::updateDisableSleep()
{
    if (!shouldDisableSleep() && m_sleepDisabler)
        m_sleepDisabler = nullptr;
    else if (shouldDisableSleep() && !m_sleepDisabler)
        m_sleepDisabler = DisplaySleepDisabler::create("com.apple.WebCore: HTMLMediaElement playback");
}

bool HTMLMediaElement::shouldDisableSleep() const
{
    return m_player && !m_player->paused() && hasVideo() && hasAudio() && !loop();
}
#endif

String HTMLMediaElement::mediaPlayerReferrer() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return String();

    return SecurityPolicy::generateReferrerHeader(document()->referrerPolicy(), m_currentSrc, frame->loader()->outgoingReferrer());
}

String HTMLMediaElement::mediaPlayerUserAgent() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return String();

    return frame->loader()->userAgent(m_currentSrc);

}

void HTMLMediaElement::removeBehaviorsRestrictionsAfterFirstUserGesture()
{
    m_restrictions = NoRestrictions;
}

}
#endif
