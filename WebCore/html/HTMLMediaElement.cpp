/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "csshelper.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLSourceElement.h"
#include "HTMLVideoElement.h"
#include <limits>
#include "MediaError.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MIMETypeRegistry.h"
#include "Movie.h"
#include "RenderVideo.h"
#include "SystemTime.h"
#include "TimeRanges.h"
#include "VoidCallback.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
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
    , m_wasPlayingBeforeMovingToPageCache(false)
    , m_currentLoop(0)
    , m_volume(0.5f)
    , m_muted(false)
    , m_previousProgress(0)
    , m_previousProgressTime(numeric_limits<double>::max())
    , m_sentStalledEvent(false)
    , m_bufferingRate(0)
    , m_loadNestingLevel(0)
    , m_terminateLoadBelowNestingLevel(0)
    , m_movie(0)
{
    document()->registerForCacheCallbacks(this);
}

HTMLMediaElement::~HTMLMediaElement()
{
    document()->unregisterForCacheCallbacks(this);
    delete m_movie;
    for (HashMap<float, CallbackVector*>::iterator it = m_cuePoints.begin(); it != m_cuePoints.end(); ++it)
        delete it->second;
}

bool HTMLMediaElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(sourceTag) || HTMLElement::checkDTD(newChild);
}

void HTMLMediaElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    if (!src().isEmpty())
        scheduleLoad();
}

void HTMLMediaElement::removedFromDocument()
{
    delete m_movie;
    m_movie = 0;
    HTMLElement::removedFromDocument();
}

void HTMLMediaElement::scheduleLoad()
{
    m_loadTimer.startOneShot(0);
}

void HTMLMediaElement::initAndDispatchProgressEvent(const AtomicString& eventName)
{
    bool totalKnown = m_movie && m_movie->totalBytesKnown();
    unsigned loaded = m_movie ? m_movie->bytesLoaded() : 0;
    unsigned total = m_movie ? m_movie->totalBytes() : 0;
    dispatchProgressEvent(eventName, totalKnown, loaded, total);
}

void HTMLMediaElement::dispatchEventAsync(const AtomicString& eventName)
{
    m_asyncEventsToDispatch.add(eventName);
    if (!m_asyncEventTimer.isActive())                            
        m_asyncEventTimer.startOneShot(0);
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    ExceptionCode ec;
    load(ec);
}

void HTMLMediaElement::asyncEventTimerFired(Timer<HTMLMediaElement>*)
{
    HashSet<String>::const_iterator end = m_asyncEventsToDispatch.end();
    for (HashSet<String>::const_iterator it = m_asyncEventsToDispatch.begin(); it != end; ++it)
        dispatchHTMLEvent(*it, false, true);
    m_asyncEventsToDispatch.clear();
}

String serializeTimeOffset(float time)
{
    String timeString = String::number(time);
    // FIXME serialize time offset values properly (format not specified yet)
    timeString.append("s");
    return timeString;
}

float parseTimeOffset(String timeString, bool* ok = 0)
{
    if (timeString.endsWith("s"))
        timeString = timeString.left(timeString.length() - 1);
    // FIXME parse time offset values (format not specified yet)
    float val = (float)timeString.toDouble(ok);
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

String HTMLMediaElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLMediaElement::HTMLMediaElement::setSrc(const String& url)
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

float HTMLMediaElement::bufferingRate()
{
    if (!m_movie)
        return 0;
    return m_bufferingRate;
    //return m_movie->dataRate();
}

void HTMLMediaElement::load(ExceptionCode& ec)
{
    String mediaSrc;
    
    // 3.14.9.4. Loading the media resource
    // 1
    // if an event generated during load() ends up re-entering load(), terminate previous instances
    m_loadNestingLevel++;
    m_terminateLoadBelowNestingLevel = m_loadNestingLevel;
    
    m_progressEventTimer.stop();
    m_sentStalledEvent = false;
    m_bufferingRate = 0;
    
    m_loadTimer.stop();
    
    // 2
    if (m_begun) {
        m_begun = false;
        m_error = new MediaError(MediaError::MEDIA_ERR_ABORTED);
        initAndDispatchProgressEvent(abortEvent);
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
        if (m_movie) {
            m_movie->pause();
            m_movie->seek(0);
        }
        m_currentLoop = 0;
        dispatchHTMLEvent(emptiedEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            goto end;
    }
    
    // 6
    mediaSrc = pickMedia();
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
    dispatchProgressEvent(beginEvent, false, 0, 0); // progress event draft calls this loadstart
    if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
        goto end;
    
    // 10, 11, 12, 13
    delete m_movie;
    m_movie = new Movie(this);
    m_movie->setVolume(m_volume);
    m_movie->setMuted(m_muted);
    for (HashMap<float, CallbackVector*>::iterator it = m_cuePoints.begin(); it != m_cuePoints.end(); ++it)
        m_movie->addCuePoint(it->first);
    m_movie->load(m_currentSrc);
    if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
        goto end;
    
    if (renderer()) {
        renderer()->updateFromElement();
        m_movie->setVisible(true);
    }
    
    // 14
    m_previousProgressTime = WebCore::currentTime();
    m_previousProgress = 0;
    if (m_begun)
        // 350ms is not magic, it is in the spec!
        m_progressEventTimer.startRepeating(0.350);
end:
    ASSERT(m_loadNestingLevel);
    m_loadNestingLevel--;
}


void HTMLMediaElement::movieNetworkStateChanged(Movie*)
{
    if (!m_begun || m_networkState == EMPTY)
        return;
    
    m_terminateLoadBelowNestingLevel = m_loadNestingLevel;

    Movie::NetworkState state = m_movie->networkState();
    
    // 3.14.9.4. Loading the media resource
    // 14
    if (state == Movie::LoadFailed) {
        //delete m_movie;
        //m_movie = 0;
        // FIXME better error handling
        m_error = new MediaError(MediaError::MEDIA_ERR_NETWORK);
        m_begun = false;
        m_progressEventTimer.stop();
        m_bufferingRate = 0;
        
        initAndDispatchProgressEvent(errorEvent); 
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        m_networkState = EMPTY;
        
        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();

        dispatchHTMLEvent(emptiedEvent, false, true);
        return;
    }
    
    if (state >= Movie::Loading && m_networkState < LOADING)
        m_networkState = LOADING;
    
    if (state >= Movie::LoadedMetaData && m_networkState < LOADED_METADATA) {
        m_movie->seek(effectiveStart());
        m_movie->setEndTime(currentLoop() == loopCount() - 1 ? effectiveEnd() : effectiveLoopEnd());
        m_networkState = LOADED_METADATA;
        
        dispatchHTMLEvent(durationchangeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        dispatchHTMLEvent(loadedmetadataEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
    }
    
    if (state >= Movie::LoadedFirstFrame && m_networkState < LOADED_FIRST_FRAME) {
        m_networkState = LOADED_FIRST_FRAME;
        
        setReadyState(CAN_SHOW_CURRENT_FRAME);
        
        if (isVideo())
            static_cast<HTMLVideoElement*>(this)->updatePosterImage();
        
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        m_loadedFirstFrame = true;
        if (renderer()) {
            ASSERT(!renderer()->isImage());
            static_cast<RenderVideo*>(renderer())->videoSizeChanged();
        }
        
        dispatchHTMLEvent(loadedfirstframeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
        
        dispatchHTMLEvent(canshowcurrentframeEvent, false, true);
        if (m_loadNestingLevel < m_terminateLoadBelowNestingLevel)
            return;
    }
    
    // 15
    if (state == Movie::Loaded && m_networkState < LOADED) {
        m_begun = false;
        m_networkState = LOADED;
        m_progressEventTimer.stop();
        m_bufferingRate = 0;
        initAndDispatchProgressEvent(loadEvent); 
    }
}

void HTMLMediaElement::movieReadyStateChanged(Movie*)
{
    Movie::ReadyState state = m_movie->readyState();
    setReadyState((ReadyState)state);
}

void HTMLMediaElement::setReadyState(ReadyState state)
{
    // 3.14.9.6. The ready states
    if (m_readyState == state)
        return;
    
    bool wasActivelyPlaying = activelyPlaying();
    m_readyState = state;
    
    if (networkState() == EMPTY)
        return;
    
    if (state == DATA_UNAVAILABLE) {
        dispatchHTMLEvent(dataunavailableEvent, false, true);
        if (wasActivelyPlaying) {
            dispatchHTMLEvent(timeupdateEvent, false, true);
            dispatchHTMLEvent(waitingEvent, false, true);
        }
    } else if (state == CAN_SHOW_CURRENT_FRAME) {
        if (m_loadedFirstFrame)
            dispatchHTMLEvent(canshowcurrentframeEvent, false, true);
        if (wasActivelyPlaying) {
            dispatchHTMLEvent(timeupdateEvent, false, true);
            dispatchHTMLEvent(waitingEvent, false, true);
        }
    } else if (state == CAN_PLAY) {
        dispatchHTMLEvent(canplayEvent, false, true);
    } else if (state == CAN_PLAY_THROUGH) {
        dispatchHTMLEvent(canplaythroughEvent, false, true);
        if (m_autoplaying && paused() && autoplay()) {
            m_movie->play();
            dispatchHTMLEvent(playEvent, false, true);
        }
    }
}

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_movie);
    unsigned progress = m_movie->bytesLoaded();
    double time = WebCore::currentTime();
    double timedelta = time - m_previousProgressTime;
    if (timedelta)
        m_bufferingRate = (float)(0.8 * m_bufferingRate + 0.2 * ((float)(progress - m_previousProgress)) / timedelta);
    
    if (progress == m_previousProgress) {
        if (timedelta > 3.0 && !m_sentStalledEvent) {
            m_bufferingRate = 0;
            initAndDispatchProgressEvent(stalledEvent);
            m_sentStalledEvent = true;
        }
    } else {
        initAndDispatchProgressEvent(progressEvent);
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
    
    // 2
    float minTime;
    if (currentLoop() == 0)
        minTime = effectiveStart();
    else
        minTime = effectiveLoopStart();
 
    // 3
    float maxTime = currentLoop() == loopCount() - 1 ? effectiveEnd() : effectiveLoopEnd();
    
    // 4
    time = min(time, maxTime);
    
    // 5
    time = max(time, minTime);
    
    // 6
    RefPtr<TimeRanges> seekableRanges = seekable();
    if (!seekableRanges->contain(time)) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    
    // 7
    if (m_movie) {
        m_movie->seek(time);
        m_movie->setEndTime(maxTime);
    }
    
    // 8
    // The seeking DOM attribute is implicitly set to true
    
    // 9
    dispatchHTMLEvent(timeupdateEvent, false, true);
    
    // 10
    // As soon as the user agent has established whether or not the media data for the new playback position is available, 
    // and, if it is, decoded enough data to play back that position, the seeking DOM attribute must be set to false.
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

bool HTMLMediaElement::seeking() const
{
    if (!m_movie)
        return false;
    RefPtr<TimeRanges> seekableRanges = seekable();
    return m_movie->seeking() && seekableRanges->contain(currentTime());
}

// playback state
float HTMLMediaElement::currentTime() const
{
    return m_movie ? m_movie->currentTime() : 0;
}

void HTMLMediaElement::setCurrentTime(float time, ExceptionCode& ec)
{
    seek(time, ec);
}

float HTMLMediaElement::duration() const
{
    return m_movie ? m_movie->duration() : 0;
}

bool HTMLMediaElement::paused() const
{
    return m_movie ? m_movie->paused() : true;
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
        dispatchEventAsync(ratechangeEvent);
    }
}

float HTMLMediaElement::playbackRate() const
{
    return m_movie ? m_movie->rate() : 0;
}

void HTMLMediaElement::setPlaybackRate(float rate, ExceptionCode& ec)
{
    if (rate == 0.0f) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    if (m_movie && m_movie->rate() != rate) {
        m_movie->setRate(rate);
        dispatchEventAsync(ratechangeEvent);
    }
}

bool HTMLMediaElement::ended()
{
    return networkState() >= LOADED_METADATA && currentTime() >= effectiveEnd() && currentLoop() == loopCount() - 1;
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
    // 3.14.9.7. Playing the media resource
    if (!m_movie || networkState() == EMPTY) {
        load(ec);
        if (ec)
            return;
    }
    if (endedPlayback()) {
        m_currentLoop = 0;
        seek(effectiveStart(), ec);
        if (ec)
            return;
    }
    setPlaybackRate(defaultPlaybackRate(), ec);
    if (ec)
        return;
    
    m_autoplaying = false;
    
    if (m_movie->paused()) {
        dispatchHTMLEvent(playEvent, false, true);
        m_movie->play();
    }
}

void HTMLMediaElement::pause(ExceptionCode& ec)
{
    // 3.14.9.7. Playing the media resource
    if (!m_movie || networkState() == EMPTY) {
        load(ec);
    }

    m_autoplaying = false;

    if (!m_movie->paused()) {
        dispatchHTMLEvent(pauseEvent, false, true);
        m_movie->pause();
    }
}

unsigned HTMLMediaElement::loopCount() const
{
    String val = getAttribute(loopcountAttr);
    int count = val.toInt();
    return max(count, 1); 
}

void HTMLMediaElement::setLoopCount(unsigned count, ExceptionCode& ec)
{
    if (!count) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    setAttribute(loopcountAttr, String::number(count));
    checkIfSeekNeeded();
}

float HTMLMediaElement::start() const 
{ 
    return getTimeOffsetAttribute(startAttr, 0); 
}

void HTMLMediaElement::setStart(float time) 
{ 
    setTimeOffsetAttribute(startAttr, time); 
    checkIfSeekNeeded();
}

float HTMLMediaElement::end() const 
{ 
    return getTimeOffsetAttribute(endAttr, std::numeric_limits<float>::infinity()); 
}

void HTMLMediaElement::setEnd(float time) 
{ 
    setTimeOffsetAttribute(endAttr, time); 
    checkIfSeekNeeded();
}

float HTMLMediaElement::loopStart() const 
{ 
    return getTimeOffsetAttribute(loopstartAttr, 0); 
}

void HTMLMediaElement::setLoopStart(float time) 
{
    setTimeOffsetAttribute(loopstartAttr, time); 
    checkIfSeekNeeded();
}

float HTMLMediaElement::loopEnd() const 
{ 
    return getTimeOffsetAttribute(loopendAttr, std::numeric_limits<float>::infinity()); 
}

void HTMLMediaElement::setLoopEnd(float time) 
{ 
    setTimeOffsetAttribute(loopendAttr, time); 
    checkIfSeekNeeded();
}

unsigned HTMLMediaElement::currentLoop() const
{
    return m_currentLoop;
}

void HTMLMediaElement::setCurrentLoop(unsigned currentLoop)
{
    m_currentLoop = currentLoop;
}

bool HTMLMediaElement::controls() const
{
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
        dispatchEventAsync(volumechangeEvent);
    
        if (m_movie)
            m_movie->setVolume(vol);
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
        dispatchEventAsync(volumechangeEvent);
        if (m_movie)
            m_movie->setMuted(muted);
    }
}

String HTMLMediaElement::pickMedia()
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
                    MediaQueryEvaluator screenEval("screen", document()->page(), renderer() ? renderer()->style() : 0);
                    RefPtr<MediaList> media = new MediaList((CSSStyleSheet*)0, source->media(), true);
                    if (!screenEval.eval(media.get()))
                        continue;
                }
                if (source->hasAttribute(typeAttr)) {
                    String type = source->type();
                    if (!MIMETypeRegistry::isSupportedMovieMIMEType(type))
                        continue;
                }
                mediaSrc = source->src();
                break;
            }
        }
    }
    if (!mediaSrc.isEmpty())
        mediaSrc = document()->completeURL(mediaSrc);
    return mediaSrc;
}

void HTMLMediaElement::checkIfSeekNeeded()
{
    // 3.14.9.5. Offsets into the media resource
    // 1
    if (loopCount() - 1 < m_currentLoop)
        m_currentLoop = loopCount() - 1;
    
    // 2
    if (networkState() <= LOADING)
        return;
    
    // 3
    ExceptionCode ec;
    float time = currentTime();
    if (!m_currentLoop && time < effectiveStart())
        seek(effectiveStart(), ec);

    // 4
    if (m_currentLoop && time < effectiveLoopStart())
        seek(effectiveLoopStart(), ec);
        
    // 5
    if (m_currentLoop < loopCount() - 1 && time > effectiveLoopEnd()) {
        seek(effectiveLoopStart(), ec);
        m_currentLoop++;
    }
    
    // 6
    if (m_currentLoop == loopCount() - 1 && time > effectiveEnd())
        seek(effectiveEnd(), ec);
}

void HTMLMediaElement::movieVolumeChanged(Movie*)
{
    if (!m_movie)
        return;
    if (m_movie->volume() != m_volume || m_movie->muted() != m_muted) {
        m_volume = m_movie->volume();
        m_muted = m_movie->muted();
        dispatchEventAsync(volumechangeEvent);
    }
}

void HTMLMediaElement::movieDidEnd(Movie*)
{
    if (m_currentLoop < loopCount() - 1 && currentTime() >= effectiveLoopEnd()) {
        m_movie->seek(effectiveLoopStart());
        m_currentLoop++;
        m_movie->setEndTime(m_currentLoop == loopCount() - 1 ? effectiveEnd() : effectiveLoopEnd());
        if (m_movie)
            m_movie->play();
        dispatchHTMLEvent(timeupdateEvent, false, true);
    }
    
    if (m_currentLoop == loopCount() - 1 && currentTime() >= effectiveEnd()) {
        dispatchHTMLEvent(timeupdateEvent, false, true);
        dispatchHTMLEvent(endedEvent, false, true);
    }
}

void HTMLMediaElement::movieCuePointReached(Movie*, float cueTime)
{
    CallbackVector* callbackVector = m_cuePoints.get(cueTime);
    if (!callbackVector)
        return;
    for (unsigned n = 0; n < callbackVector->size(); n++) {
        CallbackEntry ce = (*callbackVector)[n];
        if (ce.m_pause) {
            ExceptionCode ec;
            pause(ec);
            break;
        }
    }    
    
    dispatchHTMLEvent(timeupdateEvent, false, true);
    
    for (unsigned n = 0; n < callbackVector->size(); n++) {
        CallbackEntry ce = (*callbackVector)[n];
        if (ce.m_voidCallback) 
            ce.m_voidCallback->execute(document()->frame());
    }      
}

void HTMLMediaElement::addCuePoint(float time, VoidCallback* voidCallback, bool pause)
{
    if (time < 0 || !isfinite(time))
        return;
    CallbackVector* callbackVector = m_cuePoints.get(time);
    if (!callbackVector) {
        callbackVector = new CallbackVector;
        m_cuePoints.add(time, callbackVector);
    }
    callbackVector->append(CallbackEntry(voidCallback, pause));
    
    if (m_movie)
        m_movie->addCuePoint(time);
}

void HTMLMediaElement::removeCuePoint(float time, VoidCallback* callback)
{
    if (time < 0 || !isfinite(time))
        return;
    CallbackVector* callbackVector = m_cuePoints.get(time);
    if (callbackVector) {
        for (unsigned n = 0; n < callbackVector->size(); n++) {
            if (*(*callbackVector)[n].m_voidCallback == *callback) {
                callbackVector->remove(n);
                break;
            }
        }
        if (!callbackVector->size()) {
            delete callbackVector;
            m_cuePoints.remove(time);
            if (m_movie)
                m_movie->removeCuePoint(time);
        }
    }
}

PassRefPtr<TimeRanges> HTMLMediaElement::buffered() const
{
    // FIXME real ranges support
    if (!m_movie || !m_movie->maxTimeBuffered())
        return new TimeRanges;
    return new TimeRanges(0, m_movie->maxTimeBuffered());
}

PassRefPtr<TimeRanges> HTMLMediaElement::played() const
{
    // FIXME track played
    return new TimeRanges;
}

PassRefPtr<TimeRanges> HTMLMediaElement::seekable() const
{
    // FIXME real ranges support
    if (!m_movie || !m_movie->maxTimeSeekable())
        return new TimeRanges;
    return new TimeRanges(0, m_movie->maxTimeSeekable());
}

float HTMLMediaElement::effectiveStart() const
{
    if (!m_movie)
        return 0;
    return min(start(), m_movie->duration());
}

float HTMLMediaElement::effectiveEnd() const
{
    if (!m_movie)
        return 0;
    return min(max(end(), max(start(), loopStart())), m_movie->duration());
}

float HTMLMediaElement::effectiveLoopStart() const
{
    if (!m_movie)
        return 0;
    return min(loopStart(), m_movie->duration());
}

float HTMLMediaElement::effectiveLoopEnd() const
{
    if (!m_movie)
        return 0;
    return min(max(start(), max(loopStart(), loopEnd())), m_movie->duration());
}

bool HTMLMediaElement::activelyPlaying() const
{
    return !paused() && readyState() >= CAN_PLAY && !endedPlayback(); // && !stoppedDueToErrors() && !pausedForUserInteraction();
}

bool HTMLMediaElement::endedPlayback() const
{
    return networkState() >= LOADED_METADATA && currentTime() >= effectiveEnd() && currentLoop() == loopCount() - 1;
}

void HTMLMediaElement::willSaveToCache()
{
    // 3.14.9.4. Loading the media resource
    // 14
    if (m_begun) {
        if (m_movie)
            m_movie->cancelLoad();
        m_error = new MediaError(MediaError::MEDIA_ERR_ABORTED);
        m_begun = false;
        initAndDispatchProgressEvent(abortEvent);
        if (m_networkState >= LOADING) {
            m_networkState = EMPTY;
            dispatchHTMLEvent(emptiedEvent, false, true);
        }
    }
    
    ExceptionCode ec;
    m_wasPlayingBeforeMovingToPageCache = !paused();
    if (m_wasPlayingBeforeMovingToPageCache)
        pause(ec);
    if (m_movie)
        m_movie->setVisible(false);
}

void HTMLMediaElement::didRestoreFromCache()
{
    ExceptionCode ec;
    if (m_wasPlayingBeforeMovingToPageCache)
        play(ec);
    if (renderer())
        m_movie->setVisible(true);
}

}

#endif
