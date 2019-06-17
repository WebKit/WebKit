/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO)
#include "MediaController.h"

#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "TimeRanges.h"
#include <pal/system/Clock.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaController);

Ref<MediaController> MediaController::create(ScriptExecutionContext& context)
{
    return adoptRef(*new MediaController(context));
}

MediaController::MediaController(ScriptExecutionContext& context)
    : m_paused(false)
    , m_defaultPlaybackRate(1)
    , m_volume(1)
    , m_position(MediaPlayer::invalidTime())
    , m_muted(false)
    , m_readyState(HAVE_NOTHING)
    , m_playbackState(WAITING)
    , m_asyncEventTimer(*this, &MediaController::asyncEventTimerFired)
    , m_clearPositionTimer(*this, &MediaController::clearPositionTimerFired)
    , m_closedCaptionsVisible(false)
    , m_clock(PAL::Clock::create())
    , m_scriptExecutionContext(context)
    , m_timeupdateTimer(*this, &MediaController::scheduleTimeupdateEvent)
{
}

MediaController::~MediaController() = default;

void MediaController::addMediaElement(HTMLMediaElement& element)
{
    ASSERT(!m_mediaElements.contains(&element));

    m_mediaElements.append(&element);
    bringElementUpToSpeed(element);
}

void MediaController::removeMediaElement(HTMLMediaElement& element)
{
    ASSERT(m_mediaElements.contains(&element));
    m_mediaElements.remove(m_mediaElements.find(&element));
}

bool MediaController::containsMediaElement(HTMLMediaElement& element) const
{
    return m_mediaElements.contains(&element);
}

Ref<TimeRanges> MediaController::buffered() const
{
    if (m_mediaElements.isEmpty())
        return TimeRanges::create();

    // The buffered attribute must return a new static normalized TimeRanges object that represents 
    // the intersection of the ranges of the media resources of the slaved media elements that the 
    // user agent has buffered, at the time the attribute is evaluated.
    Ref<TimeRanges> bufferedRanges = m_mediaElements.first()->buffered();
    for (size_t index = 1; index < m_mediaElements.size(); ++index)
        bufferedRanges->intersectWith(m_mediaElements[index]->buffered());
    return bufferedRanges;
}

Ref<TimeRanges> MediaController::seekable() const
{
    if (m_mediaElements.isEmpty())
        return TimeRanges::create();

    // The seekable attribute must return a new static normalized TimeRanges object that represents
    // the intersection of the ranges of the media resources of the slaved media elements that the
    // user agent is able to seek to, at the time the attribute is evaluated.
    Ref<TimeRanges> seekableRanges = m_mediaElements.first()->seekable();
    for (size_t index = 1; index < m_mediaElements.size(); ++index)
        seekableRanges->intersectWith(m_mediaElements[index]->seekable());
    return seekableRanges;
}

Ref<TimeRanges> MediaController::played()
{
    if (m_mediaElements.isEmpty())
        return TimeRanges::create();

    // The played attribute must return a new static normalized TimeRanges object that represents 
    // the union of the ranges of the media resources of the slaved media elements that the 
    // user agent has so far rendered, at the time the attribute is evaluated.
    Ref<TimeRanges> playedRanges = m_mediaElements.first()->played();
    for (size_t index = 1; index < m_mediaElements.size(); ++index)
        playedRanges->unionWith(m_mediaElements[index]->played());
    return playedRanges;
}

double MediaController::duration() const
{
    // FIXME: Investigate caching the maximum duration and only updating the cached value
    // when the slaved media elements' durations change.
    double maxDuration = 0;
    for (auto& mediaElement : m_mediaElements) {
        double duration = mediaElement->duration();
        if (std::isnan(duration))
            continue;
        maxDuration = std::max(maxDuration, duration);
    }
    return maxDuration;
}

double MediaController::currentTime() const
{
    if (m_mediaElements.isEmpty())
        return 0;

    if (m_position == MediaPlayer::invalidTime()) {
        // Some clocks may return times outside the range of [0..duration].
        m_position = std::max<double>(0, std::min(duration(), m_clock->currentTime()));
        m_clearPositionTimer.startOneShot(0_s);
    }

    return m_position;
}

void MediaController::setCurrentTime(double time)
{
    // When the user agent is to seek the media controller to a particular new playback position, 
    // it must follow these steps:
    // If the new playback position is less than zero, then set it to zero.
    time = std::max(0.0, time);
    
    // If the new playback position is greater than the media controller duration, then set it 
    // to the media controller duration.
    time = std::min(time, duration());
    
    // Set the media controller position to the new playback position.
    m_clock->setCurrentTime(time);
    
    // Seek each slaved media element to the new playback position relative to the media element timeline.
    for (auto& mediaElement : m_mediaElements)
        mediaElement->seek(MediaTime::createWithDouble(time));

    scheduleTimeupdateEvent();
    m_resetCurrentTimeInNextPlay = false;
}

void MediaController::unpause()
{
    // When the unpause() method is invoked, if the MediaController is a paused media controller,
    if (!m_paused)
        return;
    // the user agent must change the MediaController into a playing media controller,
    m_paused = false;
    // queue a task to fire a simple event named play at the MediaController,
    scheduleEvent(eventNames().playEvent);
    // and then report the controller state of the MediaController.
    reportControllerState();
}

void MediaController::play()
{
    // When the play() method is invoked, the user agent must invoke the play method of each
    // slaved media element in turn,
    for (auto& mediaElement : m_mediaElements)
        mediaElement->play();

    // and then invoke the unpause method of the MediaController.
    unpause();
}

void MediaController::pause()
{
    // When the pause() method is invoked, if the MediaController is a playing media controller,
    if (m_paused)
        return;

    // then the user agent must change the MediaController into a paused media controller,
    m_paused = true;
    // queue a task to fire a simple event named pause at the MediaController,
    scheduleEvent(eventNames().pauseEvent);
    // and then report the controller state of the MediaController.
    reportControllerState();
}

void MediaController::setDefaultPlaybackRate(double rate)
{
    if (m_defaultPlaybackRate == rate)
        return;

    // The defaultPlaybackRate attribute, on setting, must set the MediaController's media controller
    // default playback rate to the new value,
    m_defaultPlaybackRate = rate;

    // then queue a task to fire a simple event named ratechange at the MediaController.
    scheduleEvent(eventNames().ratechangeEvent);
}

double MediaController::playbackRate() const
{
    return m_clock->playRate();
}

void MediaController::setPlaybackRate(double rate)
{
    if (m_clock->playRate() == rate)
        return;

    // The playbackRate attribute, on setting, must set the MediaController's media controller 
    // playback rate to the new value,
    m_clock->setPlayRate(rate);

    for (auto& mediaElement : m_mediaElements)
        mediaElement->updatePlaybackRate();

    // then queue a task to fire a simple event named ratechange at the MediaController.
    scheduleEvent(eventNames().ratechangeEvent);
}

ExceptionOr<void> MediaController::setVolume(double level)
{
    if (m_volume == level)
        return { };

    // If the new value is outside the range 0.0 to 1.0 inclusive, then, on setting, an 
    // IndexSizeError exception must be raised instead.
    if (!(level >= 0 && level <= 1))
        return Exception { IndexSizeError };

    // The volume attribute, on setting, if the new value is in the range 0.0 to 1.0 inclusive,
    // must set the MediaController's media controller volume multiplier to the new value
    m_volume = level;

    // and queue a task to fire a simple event named volumechange at the MediaController.
    scheduleEvent(eventNames().volumechangeEvent);

    for (auto& mediaElement : m_mediaElements)
        mediaElement->updateVolume();

    return { };
}

void MediaController::setMuted(bool flag)
{
    if (m_muted == flag)
        return;

    // The muted attribute, on setting, must set the MediaController's media controller mute override
    // to the new value
    m_muted = flag;

    // and queue a task to fire a simple event named volumechange at the MediaController.
    scheduleEvent(eventNames().volumechangeEvent);

    for (auto& mediaElement : m_mediaElements)
        mediaElement->updateVolume();
}

static const AtomString& playbackStateWaiting()
{
    static NeverDestroyed<AtomString> waiting("waiting", AtomString::ConstructFromLiteral);
    return waiting;
}

static const AtomString& playbackStatePlaying()
{
    static NeverDestroyed<AtomString> playing("playing", AtomString::ConstructFromLiteral);
    return playing;
}

static const AtomString& playbackStateEnded()
{
    static NeverDestroyed<AtomString> ended("ended", AtomString::ConstructFromLiteral);
    return ended;
}

const AtomString& MediaController::playbackState() const
{
    switch (m_playbackState) {
    case WAITING:
        return playbackStateWaiting();
    case PLAYING:
        return playbackStatePlaying();
    case ENDED:
        return playbackStateEnded();
    default:
        ASSERT_NOT_REACHED();
        return nullAtom();
    }
}

void MediaController::reportControllerState()
{
    updateReadyState();
    updatePlaybackState();
}

static AtomString eventNameForReadyState(MediaControllerInterface::ReadyState state)
{
    switch (state) {
    case MediaControllerInterface::HAVE_NOTHING:
        return eventNames().emptiedEvent;
    case MediaControllerInterface::HAVE_METADATA:
        return eventNames().loadedmetadataEvent;
    case MediaControllerInterface::HAVE_CURRENT_DATA:
        return eventNames().loadeddataEvent;
    case MediaControllerInterface::HAVE_FUTURE_DATA:
        return eventNames().canplayEvent;
    case MediaControllerInterface::HAVE_ENOUGH_DATA:
        return eventNames().canplaythroughEvent;
    default:
        ASSERT_NOT_REACHED();
        return nullAtom();
    }
}

void MediaController::updateReadyState()
{
    ReadyState oldReadyState = m_readyState;
    ReadyState newReadyState;
    
    if (m_mediaElements.isEmpty()) {
        // If the MediaController has no slaved media elements, let new readiness state be 0.
        newReadyState = HAVE_NOTHING;
    } else {
        // Otherwise, let it have the lowest value of the readyState IDL attributes of all of its
        // slaved media elements.
        newReadyState = m_mediaElements.first()->readyState();
        for (size_t index = 1; index < m_mediaElements.size(); ++index)
            newReadyState = std::min(newReadyState, m_mediaElements[index]->readyState());
    }

    if (newReadyState == oldReadyState) 
        return;

    // If the MediaController's most recently reported readiness state is greater than new readiness 
    // state then queue a task to fire a simple event at the MediaController object, whose name is the
    // event name corresponding to the value of new readiness state given in the table below. [omitted]
    if (oldReadyState > newReadyState) {
        scheduleEvent(eventNameForReadyState(newReadyState));
        return;
    }

    // If the MediaController's most recently reported readiness state is less than the new readiness
    // state, then run these substeps:
    // 1. Let next state be the MediaController's most recently reported readiness state.
    ReadyState nextState = oldReadyState;
    do {
        // 2. Loop: Increment next state by one.
        nextState = static_cast<ReadyState>(nextState + 1);
        // 3. Queue a task to fire a simple event at the MediaController object, whose name is the
        // event name corresponding to the value of next state given in the table below. [omitted]
        scheduleEvent(eventNameForReadyState(nextState));        
        // If next state is less than new readiness state, then return to the step labeled loop
    } while (nextState < newReadyState);

    // Let the MediaController's most recently reported readiness state be new readiness state.
    m_readyState = newReadyState;
}

void MediaController::updatePlaybackState()
{
    PlaybackState oldPlaybackState = m_playbackState;
    PlaybackState newPlaybackState;

    // Initialize new playback state by setting it to the state given for the first matching 
    // condition from the following list:
    if (m_mediaElements.isEmpty()) {
        // If the MediaController has no slaved media elements
        // Let new playback state be waiting.
        newPlaybackState = WAITING;
    } else if (hasEnded()) {
        // If all of the MediaController's slaved media elements have ended playback and the media
        // controller playback rate is positive or zero
        // Let new playback state be ended.
        newPlaybackState = ENDED;
    } else if (isBlocked()) {
        // If the MediaController is a blocked media controller
        // Let new playback state be waiting.
        newPlaybackState = WAITING;
    } else {
        // Otherwise
        // Let new playback state be playing.
        newPlaybackState = PLAYING;
    }

    // If the MediaController's most recently reported playback state is not equal to new playback state
    if (newPlaybackState == oldPlaybackState)
        return;

    // and the new playback state is ended,
    if (newPlaybackState == ENDED) {
        // then queue a task that, if the MediaController object is a playing media controller, and 
        // all of the MediaController's slaved media elements have still ended playback, and the 
        // media controller playback rate is still positive or zero, 
        if (!m_paused && hasEnded()) {
            // changes the MediaController object to a paused media controller
            m_paused = true;

            // and then fires a simple event named pause at the MediaController object.
            scheduleEvent(eventNames().pauseEvent);
        }
    }

    // If the MediaController's most recently reported playback state is not equal to new playback state
    // then queue a task to fire a simple event at the MediaController object, whose name is playing 
    // if new playback state is playing, ended if new playback state is ended, and waiting otherwise.
    AtomString eventName;
    switch (newPlaybackState) {
    case WAITING:
        eventName = eventNames().waitingEvent;
        m_clock->stop();
        m_timeupdateTimer.stop();
        break;
    case ENDED:
        eventName = eventNames().endedEvent;
        m_resetCurrentTimeInNextPlay = true;
        m_clock->stop();
        m_timeupdateTimer.stop();
        break;
    case PLAYING:
        if (m_resetCurrentTimeInNextPlay) {
            m_resetCurrentTimeInNextPlay = false;
            m_clock->setCurrentTime(0);
        }
        eventName = eventNames().playingEvent;
        m_clock->start();
        startTimeupdateTimer();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    scheduleEvent(eventName);

    // Let the MediaController's most recently reported playback state be new playback state.
    m_playbackState = newPlaybackState;

    updateMediaElements();
}

void MediaController::updateMediaElements()
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->updatePlayState();
}

void MediaController::bringElementUpToSpeed(HTMLMediaElement& element)
{
    ASSERT(m_mediaElements.contains(&element));

    // When the user agent is to bring a media element up to speed with its new media controller,
    // it must seek that media element to the MediaController's media controller position relative
    // to the media element's timeline.
    element.seekInternal(MediaTime::createWithDouble(currentTime()));
}

bool MediaController::isBlocked() const
{
    // A MediaController is a blocked media controller if the MediaController is a paused media 
    // controller,
    if (m_paused)
        return true;
    
    if (m_mediaElements.isEmpty())
        return false;
    
    bool allPaused = true;
    for (auto& element : m_mediaElements) {
        //  or if any of its slaved media elements are blocked media elements,
        if (element->isBlocked())
            return true;
        
        // or if any of its slaved media elements whose autoplaying flag is true still have their 
        // paused attribute set to true,
        if (element->isAutoplaying() && element->paused())
            return true;
        
        if (!element->paused())
            allPaused = false;
    }
    
    // or if all of its slaved media elements have their paused attribute set to true.
    return allPaused;
}

bool MediaController::hasEnded() const
{
    // If the ... media controller playback rate is positive or zero
    if (m_clock->playRate() < 0)
        return false;

    // [and] all of the MediaController's slaved media elements have ended playback ... let new
    // playback state be ended.
    if (m_mediaElements.isEmpty())
        return false;
    
    bool allHaveEnded = true;
    for (auto& mediaElement : m_mediaElements) {
        if (!mediaElement->ended())
            allHaveEnded = false;
    }
    return allHaveEnded;
}

void MediaController::scheduleEvent(const AtomString& eventName)
{
    m_pendingEvents.append(Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::Yes));
    if (!m_asyncEventTimer.isActive())
        m_asyncEventTimer.startOneShot(0_s);
}

void MediaController::asyncEventTimerFired()
{
    Vector<Ref<Event>> pendingEvents;

    m_pendingEvents.swap(pendingEvents);
    for (auto& pendingEvent : pendingEvents)
        dispatchEvent(pendingEvent);
}

void MediaController::clearPositionTimerFired()
{
    m_position = MediaPlayer::invalidTime();
}

bool MediaController::hasAudio() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (mediaElement->hasAudio())
            return true;
    }
    return false;
}

bool MediaController::hasVideo() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (mediaElement->hasVideo())
            return true;
    }
    return false;
}

bool MediaController::hasClosedCaptions() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (mediaElement->hasClosedCaptions())
            return true;
    }
    return false;
}

void MediaController::setClosedCaptionsVisible(bool visible)
{
    m_closedCaptionsVisible = visible;
    for (auto& mediaElement : m_mediaElements)
        mediaElement->setClosedCaptionsVisible(visible);
}

bool MediaController::supportsScanning() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (!mediaElement->supportsScanning())
            return false;
    }
    return true;
}

void MediaController::beginScrubbing()
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->beginScrubbing();
    if (m_playbackState == PLAYING)
        m_clock->stop();
}

void MediaController::endScrubbing()
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->endScrubbing();
    if (m_playbackState == PLAYING)
        m_clock->start();
}

void MediaController::beginScanning(ScanDirection direction)
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->beginScanning(direction);
}

void MediaController::endScanning()
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->endScanning();
}

bool MediaController::canPlay() const
{
    if (m_paused)
        return true;

    for (auto& mediaElement : m_mediaElements) {
        if (!mediaElement->canPlay())
            return false;
    }
    return true;
}

bool MediaController::isLiveStream() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (!mediaElement->isLiveStream())
            return false;
    }
    return true;
}

bool MediaController::hasCurrentSrc() const
{
    for (auto& mediaElement : m_mediaElements) {
        if (!mediaElement->hasCurrentSrc())
            return false;
    }
    return true;
}

void MediaController::returnToRealtime()
{
    for (auto& mediaElement : m_mediaElements)
        mediaElement->returnToRealtime();
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const Seconds maxTimeupdateEventFrequency { 250_ms };

void MediaController::startTimeupdateTimer()
{
    if (m_timeupdateTimer.isActive())
        return;

    m_timeupdateTimer.startRepeating(maxTimeupdateEventFrequency);
}

void MediaController::scheduleTimeupdateEvent()
{
    MonotonicTime now = MonotonicTime::now();
    Seconds timedelta = now - m_previousTimeupdateTime;

    if (timedelta < maxTimeupdateEventFrequency)
        return;

    scheduleEvent(eventNames().timeupdateEvent);
    m_previousTimeupdateTime = now;
}

} // namespace WebCore

#endif
