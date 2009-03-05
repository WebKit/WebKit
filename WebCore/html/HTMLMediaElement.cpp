/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "ContentType.h"
#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include <limits>
#include "MediaError.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "RenderVideo.h"
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "RenderPartObject.h"
#endif
#include "TimeRanges.h"
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "Widget.h"
#endif
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
    , m_loadTimer(this, &HTMLMediaElement::loadTimerFired)
    , m_asyncEventTimer(this, &HTMLMediaElement::asyncEventTimerFired)
    , m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired)
    , m_defaultPlaybackRate(1.0f)
    , m_networkState(EMPTY)
    , m_readyState(DATA_UNAVAILABLE)
    , m_begun(false)
    , m_loadedFirstFrame(false)
    , m_autoplaying(true)
    , m_volume(1.0f)
    , m_muted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_currentTimeDuringSeek(0)
    , m_previousProgress(0)
    , m_previousProgressTime(numeric_limits<double>::max())
    , m_sentStalledEvent(false)
    , m_loadNestingLevel(0)
    , m_terminateLoadBelowNestingLevel(0)
    , m_pausedInternal(false)
    , m_inActiveDocument(true)
    , m_player(0)
    , m_restrictions(NoRestrictions)
    , m_processingMediaPlayerCallback(0)
    , m_sendProgressEvents(true)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_needWidgetUpdate(false)
#endif
{
    document()->registerForDocumentActivationCallbacks(this);
    document()->registerForMediaVolumeCallbacks(this);
}

HTMLMediaElement::~HTMLMediaElement()
{
    document()->unregisterForDocumentActivationCallbacks(this);
    document()->unregisterForMediaVolumeCallbacks(this);
}

bool HTMLMediaElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(sourceTag) || HTMLElement::checkDTD(newChild);
}

void HTMLMediaElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    HTMLElement::attributeChanged(attr, preserveDecls);

    const QualifiedName& attrName = attr->name();
    if (attrName == srcAttr) {
        // 3.14.9.2.
        // change to src attribute triggers load()
        if (inDocument() && m_networkState == EMPTY)
            scheduleLoad();
    } 
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    else if (attrName == controlsAttr) {
        if (!isVideo() && attached() && (controls() != (renderer() != 0))) {
            detach();
            attach();
        }
        if (renderer())
            renderer()->updateFromElement();
    }
#endif
}
    
bool HTMLMediaElement::rendererIsNeeded(RenderStyle* style)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    UNUSED_PARAM(style);
    Frame* frame = document()->frame();
    if (!frame)
        return false;

    return true;
#else
    return controls() ? HTMLElement::rendererIsNeeded(style) : false;
#endif
}

RenderObject* HTMLMediaElement::createRenderer(RenderArena* arena, RenderStyle*)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    return new (arena) RenderPartObject(this);
#else
    return new (arena) RenderMedia(this);
#endif
}
 
void HTMLMediaElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    if (!src().isEmpty())
        scheduleLoad();
}

void HTMLMediaElement::removedFromDocument()
{
    if (networkState() != EMPTY) {
        ExceptionCode ec;
        pause(ec);
    }
    HTMLElement::removedFromDocument();
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
}

void HTMLMediaElement::recalcStyle(StyleChange change)
{
    HTMLElement::recalcStyle(change);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::scheduleLoad()
{
    m_loadTimer.startOneShot(0);
}

void HTMLMediaElement::initAndDispatchProgressEvent(const AtomicString& eventName)
{
    if (!m_sendProgressEvents)
        return;

    bool totalKnown = m_player && m_player->totalBytesKnown();
    unsigned loaded = m_player ? m_player->bytesLoaded() : 0;
    unsigned total = m_player ? m_player->totalBytes() : 0;
    dispatchProgressEvent(eventName, totalKnown, loaded, total);
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::dispatchEventAsync(const AtomicString& eventName)
{
    m_asyncEventsToDispatch.append(eventName);
    if (!m_asyncEventTimer.isActive())                            
        m_asyncEventTimer.startOneShot(0);
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    ExceptionCode ec;

    loadInternal(ec);
}

void HTMLMediaElement::asyncEventTimerFired(Timer<HTMLMediaElement>*)
{
    Vector<AtomicString> asyncEventsToDispatch;
    m_asyncEventsToDispatch.swap(asyncEventsToDispatch);
    unsigned count = asyncEventsToDispatch.size();
    for (unsigned n = 0; n < count; ++n)
        dispatchEventForType(asyncEventsToDispatch[n], false, true);
}

static String serializeTimeOffset(float time)
{
    String timeString = String::number(time);
    // FIXME serialize time offset values properly (format not specified yet)
    timeString.append("s");
    return timeString;
}

static float parseTimeOffset(const String& timeString, bool* ok = 0)
{
    const UChar* characters = timeString.characters();
    unsigned length = timeString.length();
    
    if (length && characters[length - 1] == 's')
        length--;
    
    // FIXME parse time offset values (format not specified yet)
    float val = charactersToFloat(characters, length, ok);
    return val;
}

float HTMLMediaElement::getTimeOffsetAttribute(const QualifiedName& name, float valueOnError) const
{
    bool ok;
    String timeString = getAttribute(name);
    float result = parseTimeOffset(timeString, &ok);
    if (ok)
        return result;
    return valueOnError;
}

void HTMLMediaElement::setTimeOffsetAttribute(const QualifiedName& name, float value)
{
    setAttribute(name, serializeTimeOffset(value));
}

PassRefPtr<MediaError> HTMLMediaElement::error() const 
{
    return m_error;
}

KURL HTMLMediaElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLMediaElement::setSrc(const String& url)
{
    setAttribute(srcAttr, url);
}

String HTMLMediaElement::currentSrc() const
{
    return m_currentSrc;
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType) const
{
    MediaPlayer::SupportsType support = MediaPlayer::supportsType(ContentType(mimeType));
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case MediaPlayer::IsNotSupported:
            canPlay = "no";
            break;
        case MediaPlayer::MayBeSupported:
            canPlay = "maybe";
            break;
        case MediaPlayer::IsSupported:
            canPlay = "probably";
            break;
    }
    
    return canPlay;
}

void HTMLMediaElement::load(ExceptionCode& ec)
{
    if (m_restrictions & RequireUserGestureForLoadRestriction && !processingUserGesture()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    loadInternal(ec);
}

void HTMLMediaElement::loadInternal(ExceptionCode& ec)
{
    String mediaSrc;
    ContentType contentType("");

    // 3.14.9.4. Loading the media resource
    // 1
    // if an event generated during load() ends up re-entering load(), terminate previous instances
    m_loadNestingLevel++;
    m_terminateLoadBelowNestingLevel = m_loadNestingLevel;
    
    m_progressEventTimer.stop();
    m_sentStalledEvent = false;
    
    m_loadTimer.stop();
    
    // 2
    if (m_begun) {
        m_begun = false;
        m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);
        if (m_sendProgressEvents)
            initAndDispatchProgressEvent(eventNames().abortEvent);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            goto end;
    }
    
    // 3
    m_error = 0;
    m_loadedFirstFrame = false;
    m_autoplaying = true;

    // 4
    setPlaybackRate(defaultPlaybackRate(), ec);
    
    // 5
    if (networkState() != EMPTY) {
        m_networkState = EMPTY;
        m_readyState = DATA_UNAVAILABLE;
        m_paused = true;
        m_seeking = false;
        if (m_player) {
            m_player->pause();
            m_player->seek(0);
        }
        dispatchEventForType(eventNames().emptiedEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            goto end;
    }
    
    // 6
    mediaSrc = selectMediaURL(contentType);
    if (mediaSrc.isEmpty()) {
        ec = INVALID_STATE_ERR;
        goto end;
    }
    
    // 7
    m_networkState = LOADING;
    
    // 8
    m_currentSrc = mediaSrc;

    // 9
    m_begun = true;        
    if (m_sendProgressEvents)
        dispatchProgressEvent(eventNames().loadstartEvent, false, 0, 0);
    if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
        goto end;
    
    // 10, 11, 12, 13
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    m_player.clear();
    m_player.set(new MediaPlayer(this));
#else
    if (!m_player)
        m_player.set(new MediaPlayer(this));
#endif

    updateVolume();
    m_player->load(m_currentSrc, contentType);
    if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
        goto end;
    
    if (renderer())
        renderer()->updateFromElement();
    
    // 14
    if (m_sendProgressEvents) {
        m_previousProgressTime = WTF::currentTime();
        m_previousProgress = 0;
        if (m_begun)
            // 350ms is not magic, it is in the spec!
            m_progressEventTimer.startRepeating(0.350);
    }

end:
    ASSERT(m_loadNestingLevel);
    m_loadNestingLevel--;
}


void HTMLMediaElement::mediaPlayerNetworkStateChanged(MediaPlayer*)
{
    if (!m_begun)
        return;
    
    beginProcessingMediaPlayerCallback();
    setNetworkState(m_player->networkState());
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    if (state == MediaPlayer::Empty) {
        // just update the cached state and leave, we can't do anything 
        m_networkState = EMPTY;
        return;
    }
    
    m_terminateLoadBelowNestingLevel = m_loadNestingLevel;
    
    // 3.14.9.4. Loading the media resource
    // 14
    if (state == MediaPlayer::LoadFailed) {
        //delete m_player;
        //m_player = 0;
        // FIXME better error handling
        m_error = MediaError::create(MediaError::MEDIA_ERR_NETWORK);
        m_begun = false;
        m_progressEventTimer.stop();
        
        initAndDispatchProgressEvent(eventNames().errorEvent); 
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        m_networkState = EMPTY;

        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();

        dispatchEventForType(eventNames().emptiedEvent, false, true);
        return;
    }
    
    if (state >= MediaPlayer::Loading && m_networkState < LOADING)
        m_networkState = LOADING;
    
    if (state >= MediaPlayer::LoadedMetaData && m_networkState < LOADED_METADATA) {
        m_networkState = LOADED_METADATA;
        
        dispatchEventForType(eventNames().durationchangeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        dispatchEventForType(eventNames().loadedmetadataEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
    }
    
    if (state >= MediaPlayer::LoadedFirstFrame && m_networkState < LOADED_FIRST_FRAME) {
        m_networkState = LOADED_FIRST_FRAME;
        
        setReadyState(CAN_SHOW_CURRENT_FRAME);
        
        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();

        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        m_loadedFirstFrame = true;
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        if (renderer()) {
            ASSERT(!renderer()->isImage());
            static_cast<RenderVideo*>(renderer())->videoSizeChanged();
        }
#endif        

        dispatchEventForType(eventNames().loadedfirstframeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        dispatchEventForType(eventNames().canshowcurrentframeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
    }
    
    // 15
    if (state == MediaPlayer::Loaded && m_networkState < LOADED) {
        m_begun = false;
        m_networkState = LOADED;
        m_progressEventTimer.stop();
        initAndDispatchProgressEvent(eventNames().loadEvent); 
    }
}

void HTMLMediaElement::mediaPlayerReadyStateChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    MediaPlayer::ReadyState state = m_player->readyState();
    setReadyState((ReadyState)state);

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::setReadyState(ReadyState state)
{
    // 3.14.9.6. The ready states
    if (m_readyState == state)
        return;
    
    bool wasActivelyPlaying = activelyPlaying();
    m_readyState = state;
    
    if (state >= CAN_PLAY)
        m_seeking = false;
    
    if (networkState() == EMPTY)
        return;
    
    if (state == DATA_UNAVAILABLE) {
        dispatchEventForType(eventNames().dataunavailableEvent, false, true);
        if (wasActivelyPlaying) {
            dispatchEventForType(eventNames().timeupdateEvent, false, true);
            dispatchEventForType(eventNames().waitingEvent, false, true);
        }
    } else if (state == CAN_SHOW_CURRENT_FRAME) {
        if (m_loadedFirstFrame)
            dispatchEventForType(eventNames().canshowcurrentframeEvent, false, true);
        if (wasActivelyPlaying) {
            dispatchEventForType(eventNames().timeupdateEvent, false, true);
            dispatchEventForType(eventNames().waitingEvent, false, true);
        }
    } else if (state == CAN_PLAY) {
        dispatchEventForType(eventNames().canplayEvent, false, true);
    } else if (state == CAN_PLAY_THROUGH) {
        dispatchEventForType(eventNames().canplaythroughEvent, false, true);
        if (m_autoplaying && m_paused && autoplay()) {
            m_paused = false;
            dispatchEventForType(eventNames().playEvent, false, true);
        }
    }
    updatePlayState();
}

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);
    unsigned progress = m_player->bytesLoaded();
    double time = WTF::currentTime();
    double timedelta = time - m_previousProgressTime;
    
    if (progress == m_previousProgress) {
        if (timedelta > 3.0 && !m_sentStalledEvent) {
            initAndDispatchProgressEvent(eventNames().stalledEvent);
            m_sentStalledEvent = true;
        }
    } else {
        initAndDispatchProgressEvent(eventNames().progressEvent);
        m_previousProgress = progress;
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
    }
}

void HTMLMediaElement::seek(float time, ExceptionCode& ec)
{
    // 3.14.9.8. Seeking
    // 1
    if (networkState() < LOADED_METADATA) {
        ec = INVALID_STATE_ERR;
        return;
    }

    time = min(time, duration());
    time = max(time, 0.0f);
    
    // 6
    RefPtr<TimeRanges> seekableRanges = seekable();
    if (!seekableRanges->contain(time)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // 7
    m_currentTimeDuringSeek = time;

    // 8
    m_seeking = true;
    
    // 9
    dispatchEventForType(eventNames().timeupdateEvent, false, true);
    
    // 10
    // As soon as the user agent has established whether or not the media data for the new playback position is available, 
    // and, if it is, decoded enough data to play back that position, the seeking DOM attribute must be set to false.
    if (m_player)
        m_player->seek(time);
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

// playback state
float HTMLMediaElement::currentTime() const
{
    if (!m_player)
        return 0;
    if (m_seeking)
        return m_currentTimeDuringSeek;
    return m_player->currentTime();
}

void HTMLMediaElement::setCurrentTime(float time, ExceptionCode& ec)
{
    seek(time, ec);
}

float HTMLMediaElement::duration() const
{
    return m_player ? m_player->duration() : 0;
}

bool HTMLMediaElement::paused() const
{
    return m_paused;
}

float HTMLMediaElement::defaultPlaybackRate() const
{
    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(float rate, ExceptionCode& ec)
{
    if (rate == 0.0f) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    if (m_defaultPlaybackRate != rate) {
        m_defaultPlaybackRate = rate;
        dispatchEventAsync(eventNames().ratechangeEvent);
    }
}

float HTMLMediaElement::playbackRate() const
{
    return m_player ? m_player->rate() : 0;
}

void HTMLMediaElement::setPlaybackRate(float rate, ExceptionCode& ec)
{
    if (rate == 0.0f) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    if (m_player && m_player->rate() != rate) {
        m_player->setRate(rate);
        dispatchEventAsync(eventNames().ratechangeEvent);
    }
}

bool HTMLMediaElement::ended() const
{
    return endedPlayback();
}

bool HTMLMediaElement::autoplay() const
{
    return hasAttribute(autoplayAttr);
}

void HTMLMediaElement::setAutoplay(bool b)
{
    setBooleanAttribute(autoplayAttr, b);
}

void HTMLMediaElement::play(ExceptionCode& ec)
{
    if (m_restrictions & RequireUserGestureForRateChangeRestriction && !processingUserGesture()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    playInternal(ec);
}

void HTMLMediaElement::playInternal(ExceptionCode& ec)
{
    // 3.14.9.7. Playing the media resource
    if (!m_player || networkState() == EMPTY) {
        ec = 0;
        load(ec);
        if (ec)
            return;
    }
    ExceptionCode unused;
    if (endedPlayback()) {
        seek(0, unused);
    }
    setPlaybackRate(defaultPlaybackRate(), unused);
    
    if (m_paused) {
        m_paused = false;
        dispatchEventAsync(eventNames().playEvent);
    }

    m_autoplaying = false;

    updatePlayState();
}

void HTMLMediaElement::pause(ExceptionCode& ec)
{
    if (m_restrictions & RequireUserGestureForRateChangeRestriction && !processingUserGesture()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    pauseInternal(ec);
}


void HTMLMediaElement::pauseInternal(ExceptionCode& ec)
{
    // 3.14.9.7. Playing the media resource
    if (!m_player || networkState() == EMPTY) {
        ec = 0;
        load(ec);
        if (ec)
            return;
    }

    if (!m_paused) {
        m_paused = true;
        dispatchEventAsync(eventNames().timeupdateEvent);
        dispatchEventAsync(eventNames().pauseEvent);
    }

    m_autoplaying = false;
    
    updatePlayState();
}

bool HTMLMediaElement::loop() const
{
    return hasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::controls() const
{
    Frame* frame = document()->frame();

    // always show controls when scripting is disabled
    if (frame && !frame->script()->isEnabled())
        return true;

    return hasAttribute(controlsAttr);
}

void HTMLMediaElement::setControls(bool b)
{
    setBooleanAttribute(controlsAttr, b);
}

float HTMLMediaElement::volume() const
{
    return m_volume;
}

void HTMLMediaElement::setVolume(float vol, ExceptionCode& ec)
{
    if (vol < 0.0f || vol > 1.0f) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    if (m_volume != vol) {
        m_volume = vol;
        updateVolume();
        dispatchEventAsync(eventNames().volumechangeEvent);
    }
}

bool HTMLMediaElement::muted() const
{
    return m_muted;
}

void HTMLMediaElement::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;
        updateVolume();
        dispatchEventAsync(eventNames().volumechangeEvent);
    }
}

void HTMLMediaElement::togglePlayState(ExceptionCode& ec)
{
    // We can safely call the internal play/pause methods, which don't check restrictions, because
    // this method is only called from the built-in media controller
    if (canPlay())
        playInternal(ec);
    else 
        pauseInternal(ec);
}

void HTMLMediaElement::beginScrubbing()
{
    if (!paused()) {
        if (ended()) {
            // Because a media element stays in non-paused state when it reaches end, playback resumes 
            // when the slider is dragged from the end to another position unless we pause first. Do 
            // a "hard pause" so an event is generated, since we want to stay paused after scrubbing finishes.
            ExceptionCode ec;
            pause(ec);
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
    if (m_pausedInternal)
        setPausedInternal(false);
}

bool HTMLMediaElement::canPlay() const
{
    return paused() || ended() || networkState() < LOADED_METADATA;
}

String HTMLMediaElement::selectMediaURL(ContentType& contentType)
{
    // 3.14.9.2. Location of the media resource
    String mediaSrc = getAttribute(srcAttr);
    if (mediaSrc.isEmpty()) {
        for (Node* n = firstChild(); n; n = n->nextSibling()) {
            if (n->hasTagName(sourceTag)) {
                HTMLSourceElement* source = static_cast<HTMLSourceElement*>(n);
                if (!source->hasAttribute(srcAttr))
                    continue; 
                if (source->hasAttribute(mediaAttr)) {
                    MediaQueryEvaluator screenEval("screen", document()->frame(), renderer() ? renderer()->style() : 0);
                    RefPtr<MediaList> media = MediaList::createAllowingDescriptionSyntax(source->media());
                    if (!screenEval.eval(media.get()))
                        continue;
                }
                if (source->hasAttribute(typeAttr)) {
                    ContentType type(source->type());
                    if (!MediaPlayer::supportsType(type))
                        continue;

                    // return type with all parameters in place so the media engine can use them
                    contentType = type;
                }
                mediaSrc = source->src().string();
                break;
            }
        }
    }
    if (!mediaSrc.isEmpty())
        mediaSrc = document()->completeURL(mediaSrc).string();
    return mediaSrc;
}

void HTMLMediaElement::mediaPlayerTimeChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();

    if (readyState() >= CAN_PLAY)
        m_seeking = false;
    
    float now = currentTime();
    if (now >= duration()) {
        if (loop()) {
            ExceptionCode ec;
            seek(0, ec);
            dispatchEventForType(eventNames().timeupdateEvent, false, true);
        } else {
            dispatchEventForType(eventNames().timeupdateEvent, false, true);
            dispatchEventForType(eventNames().endedEvent, false, true);
        }
    }

    updatePlayState();

    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerRepaint(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    if (renderer())
        renderer()->repaint();
    endProcessingMediaPlayerCallback();
}

void HTMLMediaElement::mediaPlayerVolumeChanged(MediaPlayer*)
{
    beginProcessingMediaPlayerCallback();
    updateVolume();
    endProcessingMediaPlayerCallback();
}

PassRefPtr<TimeRanges> HTMLMediaElement::buffered() const
{
    // FIXME real ranges support
    if (!m_player || !m_player->maxTimeBuffered())
        return TimeRanges::create();
    return TimeRanges::create(0, m_player->maxTimeBuffered());
}

PassRefPtr<TimeRanges> HTMLMediaElement::played() const
{
    // FIXME track played
    return TimeRanges::create();
}

PassRefPtr<TimeRanges> HTMLMediaElement::seekable() const
{
    // FIXME real ranges support
    if (!m_player || !m_player->maxTimeSeekable())
        return TimeRanges::create();
    return TimeRanges::create(0, m_player->maxTimeSeekable());
}

bool HTMLMediaElement::activelyPlaying() const
{
    return !paused() && readyState() >= CAN_PLAY && !endedPlayback(); // && !stoppedDueToErrors() && !pausedForUserInteraction();
}

bool HTMLMediaElement::endedPlayback() const
{
    return networkState() >= LOADED_METADATA && currentTime() >= duration() && !loop();
}
    
void HTMLMediaElement::updateVolume()
{
    if (!m_player)
        return;

    // Avoid recursion when the player reports volume changes.
    if (!processingMediaPlayerCallback()) {
        Page* page = document()->page();
        float volumeMultiplier = page ? page->mediaVolume() : 1;
    
        m_player->setVolume(m_muted ? 0 : m_volume * volumeMultiplier);
    }
    
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::updatePlayState()
{
    if (!m_player)
        return;
    
    if (m_pausedInternal) {
        if (!m_player->paused())
            m_player->pause();
        return;
    }
    
    bool shouldBePlaying = activelyPlaying();
    if (shouldBePlaying && m_player->paused())
        m_player->play();
    else if (!shouldBePlaying && !m_player->paused())
        m_player->pause();
    
    if (renderer())
        renderer()->updateFromElement();
}
    
void HTMLMediaElement::setPausedInternal(bool b)
{
    m_pausedInternal = b;
    updatePlayState();
}

void HTMLMediaElement::documentWillBecomeInactive()
{
    // 3.14.9.4. Loading the media resource
    // 14
    if (m_begun) {
        // For simplicity cancel the incomplete load by deleting the player
        m_player.clear();
        m_progressEventTimer.stop();

        m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);
        m_begun = false;
        initAndDispatchProgressEvent(eventNames().abortEvent);
        if (m_networkState >= LOADING) {
            m_networkState = EMPTY;
            m_readyState = DATA_UNAVAILABLE;
            dispatchEventForType(eventNames().emptiedEvent, false, true);
        }
    }
    m_inActiveDocument = false;
    // Stop the playback without generating events
    setPausedInternal(true);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::documentDidBecomeActive()
{
    m_inActiveDocument = true;
    setPausedInternal(false);

    if (m_error && m_error->code() == MediaError::MEDIA_ERR_ABORTED) {
        // Restart the load if it was aborted in the middle by moving the document to the page cache.
        // This behavior is not specified but it seems like a sensible thing to do.
        ExceptionCode ec;
        load(ec);
    }
        
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::mediaVolumeDidChange()
{
    updateVolume();
}

void HTMLMediaElement::defaultEventHandler(Event* event)
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;

    Widget* widget = static_cast<RenderWidget*>(r)->widget();
    if (widget)
        widget->handleEvent(event);
#else
    if (renderer() && renderer()->isMedia())
        static_cast<RenderMedia*>(renderer())->forwardEvent(event);
    if (event->defaultHandled())
        return;
    HTMLElement::defaultEventHandler(event);
#endif
}

bool HTMLMediaElement::processingUserGesture() const
{
    Frame* frame = document()->frame();
    FrameLoader* loader = frame ? frame->loader() : 0;

    // return 'true' for safety if we don't know the answer 
    return loader ? loader->userGestureHint() : true;
}

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
void HTMLMediaElement::deliverNotification(MediaPlayerProxyNotificationType notification)
{
    if (notification == MediaPlayerNotificationPlayPauseButtonPressed) {
        ExceptionCode ec;
        togglePlayState(ec);
        return;
    }

    if (m_player)
        m_player->deliverNotification(notification);
}

void HTMLMediaElement::setMediaPlayerProxy(WebMediaPlayerProxy* proxy)
{
    if (m_player)
        m_player->setMediaPlayerProxy(proxy);
}

String HTMLMediaElement::initialURL()
{
    ContentType ignoredType;
    String initialSrc = selectMediaURL(ignoredType);
    m_currentSrc = initialSrc;
    return initialSrc;
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    if (!m_player)
        m_player.set(new MediaPlayer(this));
    
    document()->updateRendering();
    if (m_needWidgetUpdate && renderer())
        static_cast<RenderPartObject*>(renderer())->updateWidget(true);
}
#endif

}

#endif
