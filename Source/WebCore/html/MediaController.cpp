/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "MediaController.h"

#if ENABLE(VIDEO)

#include "ContextDestructionObserverInlines.h"
#include "EventNames.h"
#include "ExceptionOr.h"
#include "HTMLMediaElement.h"
#include "TimeRanges.h"
#include <pal/system/Clock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaController);

Ref<MediaController> MediaController::create(ScriptExecutionContext& context)
{
    return adoptRef(*new MediaController(context));
}

MediaController::MediaController(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
    , m_paused(false)
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
    , m_timeupdateTimer(*this, &MediaController::scheduleTimeupdateEvent)
{
}

MediaController::~MediaController() = default;

void MediaController::forEachElement(Function<void(Ref<HTMLMediaElement>&&)>&& func) const
{
    for (auto& element : m_mediaElements) {
        if (RefPtr protectedElement = element.get())
            func(protectedElement.releaseNonNull());
    }
}

bool MediaController::anyElement(Function<bool(Ref<HTMLMediaElement>&&)>&& func) const
{
    for (auto& element : m_mediaElements) {
        RefPtr protectedElement = element.get();
        if (!protectedElement)
            continue;

        if (func(protectedElement.releaseNonNull()))
            return true;
    }
    return false;
}

bool MediaController::everyElement(Function<bool(Ref<HTMLMediaElement>&&)>&& func) const
{
    bool isNonEmpty = false;
    for (auto& element : m_mediaElements) {
        RefPtr protectedElement = element.get();
        if (!protectedElement)
            continue;

        isNonEmpty = true;
        if (!func(protectedElement.releaseNonNull()))
            return false;
    }
    return isNonEmpty;
}

ScriptExecutionContext* MediaController::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
};

void MediaController::addMediaElement(HTMLMediaElement& element)
{
    ASSERT(!m_mediaElements.contains(&element));

    m_mediaElements.append(&element);
    bringElementUpToSpeed(element);
}

void MediaController::removeMediaElement(HTMLMediaElement& element)
{
    ASSERT(m_mediaElements.contains(&element));
    m_mediaElements.removeFirst(&element);
}

Ref<TimeRanges> MediaController::buffered() const
{
    if (m_mediaElements.isEmpty())
        return TimeRanges::create();

    // The buffered attribute must return a new static normalized TimeRanges object that represents
    // the intersection of the ranges of the media resources of the mediagroup elements that the
    // user agent has buffered, at the time the attribute is evaluated.
    Ref<TimeRanges> bufferedRanges = TimeRanges::create(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    forEachElement([&] (auto element) {
        bufferedRanges->intersectWith(element->buffered());
    });
    return bufferedRanges;
}

Ref<TimeRanges> MediaController::seekable() const
{
    if (m_mediaElements.isEmpty())
        return TimeRanges::create();

    // The seekable attribute must return a new static normalized TimeRanges object that represents
    // the intersection of the ranges of the media resources of the mediagroup elements that the
    // user agent is able to seek to, at the time the attribute is evaluated.
    Ref<TimeRanges> seekableRanges = TimeRanges::create(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
    forEachElement([&] (auto element) {
        seekableRanges->intersectWith(element->seekable());
    });
    return seekableRanges;
}

Ref<TimeRanges> MediaController::played()
{
    // The played attribute must return a new static normalized TimeRanges object that represents
    // the union of the ranges of the media resources of the mediagroup elements that the
    // user agent has so far rendered, at the time the attribute is evaluated.
    Ref<TimeRanges> playedRanges = TimeRanges::create();
    forEachElement([&] (auto element) {
        playedRanges->unionWith(element->played());
    });
    return playedRanges;
}

double MediaController::duration() const
{
    // FIXME: Investigate caching the maximum duration and only updating the cached value
    // when the mediagroup elements' durations change.
    double maxDuration = 0;
    forEachElement([&] (auto mediaElement) {
        double duration = mediaElement->duration();
        if (std::isnan(duration))
            return;
        maxDuration = std::max(maxDuration, duration);
    });
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
    m_position = time;
    m_clock->setCurrentTime(time);
    
    // Seek each mediagroup element to the new playback position relative to the media element timeline.
    forEachElement([&] (auto mediaElement) {
        mediaElement->seek(MediaTime::createWithDouble(time));
    });

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
    // mediagroup element in turn,
    forEachElement([&] (auto mediaElement) {
        mediaElement->play();
    });

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

    forEachElement([&] (auto mediaElement) {
        mediaElement->updatePlaybackRate();
    });

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
        return Exception { ExceptionCode::IndexSizeError };

    // The volume attribute, on setting, if the new value is in the range 0.0 to 1.0 inclusive,
    // must set the MediaController's media controller volume multiplier to the new value
    m_volume = level;

    // and queue a task to fire a simple event named volumechange at the MediaController.
    scheduleEvent(eventNames().volumechangeEvent);

    forEachElement([&] (auto mediaElement) {
        mediaElement->updateVolume();
    });

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

    forEachElement([&] (auto mediaElement) {
        mediaElement->updateVolume();
    });
}

static const AtomString& playbackStateWaiting()
{
    static MainThreadNeverDestroyed<const AtomString> waiting("waiting"_s);
    return waiting;
}

static const AtomString& playbackStatePlaying()
{
    static MainThreadNeverDestroyed<const AtomString> playing("playing"_s);
    return playing;
}

static const AtomString& playbackStateEnded()
{
    static MainThreadNeverDestroyed<const AtomString> ended("ended"_s);
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
    auto readyStates = m_mediaElements.map([] (auto& checkedElement) -> std::optional<ReadyState> {
        if (RefPtr mediaElement = checkedElement.get())
            return mediaElement->readyState();
        return std::nullopt;
    });

    // If the MediaController has no mediagroup elements, let new readiness state be 0.
    // Otherwise, let it have the lowest value of the readyState IDL attributes of all of its
    // mediagroup elements.
    ReadyState oldReadyState = m_readyState;
    ReadyState newReadyState = HAVE_NOTHING;
    if (std::ranges::distance(readyStates) > 0)
        newReadyState = std::ranges::min(readyStates).value_or(HAVE_NOTHING);
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
        // If the MediaController has no mediagroup elements
        // Let new playback state be waiting.
        newPlaybackState = WAITING;
    } else if (hasEnded()) {
        // If all of the MediaController's mediagroup elements have ended playback and the media
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
        // all of the MediaController's mediagroup elements have still ended playback, and the
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
    forEachElement([&] (auto mediaElement) {
        mediaElement->updatePlayState();
    });
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
    
    return anyElement([&] (auto element) {
        //  or if any of its mediagroup elements are blocked media elements,
        if (element->isBlocked())
            return true;
        
        // or if any of its mediagroup elements whose autoplaying flag is true still have their
        // paused attribute set to true,
        return element->isAutoplaying() && element->paused();
    }) || everyElement([&] (auto element) {
        // or if all of its mediagroup elements have their paused attribute set to true.
        return element->paused();
    });
}

bool MediaController::hasEnded() const
{
    // If the ... media controller playback rate is positive or zero
    if (m_clock->playRate() < 0)
        return false;

    // [and] all of the MediaController's mediagroup elements have ended playback ... let new
    // playback state be ended.
    return everyElement([] (auto mediaElement) {
        return mediaElement->ended();
    });
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
    return anyElement([] (auto mediaElement) {
        return mediaElement->hasAudio();
    });
}

bool MediaController::hasVideo() const
{
    return anyElement([] (auto mediaElement) {
        return mediaElement->hasVideo();
    });
}

bool MediaController::hasClosedCaptions() const
{
    return anyElement([] (auto mediaElement) {
        return mediaElement->hasClosedCaptions();
    });
}

void MediaController::setClosedCaptionsVisible(bool visible)
{
    m_closedCaptionsVisible = visible;
    forEachElement([visible] (auto mediaElement) {
        mediaElement->setClosedCaptionsVisible(visible);
    });
}

bool MediaController::supportsScanning() const
{
    return everyElement([] (auto mediaElement) {
        return mediaElement->supportsScanning();
    });
}

void MediaController::beginScrubbing()
{
    forEachElement([] (auto mediaElement) {
        mediaElement->beginScrubbing();
    });
    if (m_playbackState == PLAYING)
        m_clock->stop();
}

void MediaController::endScrubbing()
{
    forEachElement([] (auto mediaElement) {
        mediaElement->endScrubbing();
    });
    if (m_playbackState == PLAYING)
        m_clock->start();
}

void MediaController::beginScanning(ScanDirection direction)
{
    forEachElement([direction] (auto mediaElement) {
        mediaElement->beginScanning(direction);
    });
}

void MediaController::endScanning()
{
    forEachElement([] (auto mediaElement) {
        mediaElement->endScanning();
    });
}

bool MediaController::canPlay() const
{
    if (m_paused)
        return true;

    return everyElement([] (auto mediaElement) {
        return mediaElement->canPlay();
    });
}

bool MediaController::isLiveStream() const
{
    return everyElement([] (auto mediaElement) {
        return mediaElement->isLiveStream();
    });
}

bool MediaController::hasCurrentSrc() const
{
    return everyElement([] (auto mediaElement) {
        return mediaElement->hasCurrentSrc();
    });
}

void MediaController::returnToRealtime()
{
    return forEachElement([] (auto mediaElement) {
        mediaElement->returnToRealtime();
    });
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
