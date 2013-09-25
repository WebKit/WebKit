/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSourceBase.h"

#if ENABLE(MEDIA_SOURCE)

#include "Event.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "GenericEventQueue.h"
#include "Logging.h"
#include "MediaSourceRegistry.h"
#include "SourceBufferPrivate.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

MediaSourceBase::MediaSourceBase(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_readyState(closedKeyword())
    , m_asyncEventQueue(*this)
    , m_attached(false)
{
}

MediaSourceBase::~MediaSourceBase()
{
}

const AtomicString& MediaSourceBase::openKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, open, ("open", AtomicString::ConstructFromLiteral));
    return open;
}

const AtomicString& MediaSourceBase::closedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, closed, ("closed", AtomicString::ConstructFromLiteral));
    return closed;
}

const AtomicString& MediaSourceBase::endedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, ended, ("ended", AtomicString::ConstructFromLiteral));
    return ended;
}

void MediaSourceBase::setPrivateAndOpen(PassOwnPtr<MediaSourcePrivate> mediaSourcePrivate)
{
    ASSERT(mediaSourcePrivate);
    ASSERT(!m_private);
    ASSERT(m_attached);
    m_private = mediaSourcePrivate;
    setReadyState(openKeyword());
}

void MediaSourceBase::addedToRegistry()
{
    setPendingActivity(this);
}

void MediaSourceBase::removedFromRegistry()
{
    unsetPendingActivity(this);
}

double MediaSourceBase::duration() const
{
    return isClosed() ? std::numeric_limits<float>::quiet_NaN() : m_private->duration();
}

PassRefPtr<TimeRanges> MediaSourceBase::buffered() const
{
    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    Vector<RefPtr<TimeRanges> > ranges = activeRanges();

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (ranges.isEmpty())
        return TimeRanges::create();

    // 2. Let active ranges be the ranges returned by buffered for each SourceBuffer object in activeSourceBuffers.
    // 3. Let highest end time be the largest range end time in the active ranges.
    double highestEndTime = -1;
    for (size_t i = 0; i < ranges.size(); ++i) {
        unsigned length = ranges[i]->length();
        if (length)
            highestEndTime = std::max(highestEndTime, ranges[i]->end(length - 1, ASSERT_NO_EXCEPTION));
    }

    // Return an empty range if all ranges are empty.
    if (highestEndTime < 0)
        return TimeRanges::create();

    // 4. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    RefPtr<TimeRanges> intersectionRanges = TimeRanges::create(0, highestEndTime);

    // 5. For each SourceBuffer object in activeSourceBuffers run the following steps:
    bool ended = readyState() == endedKeyword();
    for (size_t i = 0; i < ranges.size(); ++i) {
        // 5.1 Let source ranges equal the ranges returned by the buffered attribute on the current SourceBuffer.
        TimeRanges* sourceRanges = ranges[i].get();

        // 5.2 If readyState is "ended", then set the end time on the last range in source ranges to highest end time.
        if (ended && sourceRanges->length())
            sourceRanges->add(sourceRanges->start(sourceRanges->length() - 1, ASSERT_NO_EXCEPTION), highestEndTime);

        // 5.3 Let new intersection ranges equal the the intersection between the intersection ranges and the source ranges.
        // 5.4 Replace the ranges in intersection ranges with the new intersection ranges.
        intersectionRanges->intersectWith(sourceRanges);
    }

    return intersectionRanges.release();
}

void MediaSourceBase::setDuration(double duration, ExceptionCode& ec)
{
    if (duration < 0.0 || std::isnan(duration)) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    m_private->setDuration(duration);
}


void MediaSourceBase::setReadyState(const AtomicString& state)
{
    ASSERT(state == openKeyword() || state == closedKeyword() || state == endedKeyword());

    AtomicString oldState = readyState();
    LOG(Media, "MediaSourceBase::setReadyState() %p : %s -> %s", this, oldState.string().ascii().data(), state.string().ascii().data());

    if (state == closedKeyword()) {
        m_private.clear();
        m_attached = false;
    }

    if (oldState == state)
        return;

    m_readyState = state;

    onReadyStateChange(oldState, state);
}

void MediaSourceBase::endOfStream(const AtomicString& error, ExceptionCode& ec)
{
    DEFINE_STATIC_LOCAL(const AtomicString, network, ("network", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode", AtomicString::ConstructFromLiteral));

    // 3.1 http://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#dom-endofstream
    // 1. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaSourcePrivate::EndOfStreamStatus eosStatus = MediaSourcePrivate::EosNoError;

    if (error.isNull() || error.isEmpty())
        eosStatus = MediaSourcePrivate::EosNoError;
    else if (error == network)
        eosStatus = MediaSourcePrivate::EosNetworkError;
    else if (error == decode)
        eosStatus = MediaSourcePrivate::EosDecodeError;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 2. Change the readyState attribute value to "ended".
    setReadyState(endedKeyword());
    m_private->markEndOfStream(eosStatus);
}

bool MediaSourceBase::isOpen() const
{
    return readyState() == openKeyword();
}

bool MediaSourceBase::isClosed() const
{
    return readyState() == closedKeyword();
}

void MediaSourceBase::close()
{
    setReadyState(closedKeyword());
}

bool MediaSourceBase::attachToElement()
{
    if (m_attached)
        return false;

    ASSERT(isClosed());

    m_attached = true;
    return true;
}

void MediaSourceBase::openIfInEndedState()
{
    if (m_readyState != endedKeyword())
        return;

    setReadyState(openKeyword());
    m_private->unmarkEndOfStream();
}

bool MediaSourceBase::hasPendingActivity() const
{
    return m_private || m_asyncEventQueue.hasPendingEvents()
        || ActiveDOMObject::hasPendingActivity();
}

void MediaSourceBase::stop()
{
    m_asyncEventQueue.close();
    if (!isClosed())
        setReadyState(closedKeyword());
    m_private.clear();
}

PassOwnPtr<SourceBufferPrivate> MediaSourceBase::createSourceBufferPrivate(const String& type, const MediaSourcePrivate::CodecsArray& codecs, ExceptionCode& ec)
{
    OwnPtr<SourceBufferPrivate> sourceBufferPrivate;
    switch (m_private->addSourceBuffer(type, codecs, &sourceBufferPrivate)) {
    case MediaSourcePrivate::Ok: {
        return sourceBufferPrivate.release();
    }
    case MediaSourcePrivate::NotSupported:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NOT_SUPPORTED_ERR exception and abort these steps.
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    case MediaSourcePrivate::ReachedIdLimit:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QUOTA_EXCEEDED_ERR exception and abort these steps.
        ec = QUOTA_EXCEEDED_ERR;
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void MediaSourceBase::scheduleEvent(const AtomicString& eventName)
{
    RefPtr<Event> event = Event::create(eventName, false, false);
    event->setTarget(this);

    m_asyncEventQueue.enqueueEvent(event.release());
}

ScriptExecutionContext* MediaSourceBase::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

URLRegistry& MediaSourceBase::registry() const
{
    return MediaSourceRegistry::registry();
}

}

#endif
