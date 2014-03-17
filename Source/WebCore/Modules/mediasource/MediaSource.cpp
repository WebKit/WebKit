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
#include "MediaSource.h"

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrack.h"
#include "AudioTrackList.h"
#include "ContentType.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "GenericEventQueue.h"
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaError.h"
#include "MediaPlayer.h"
#include "MediaSourceRegistry.h"
#include "SourceBufferPrivate.h"
#include "TextTrack.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrack.h"
#include "VideoTrackList.h"
#include <runtime/Uint8Array.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

URLRegistry* MediaSource::s_registry = 0;

void MediaSource::setRegistry(URLRegistry* registry)
{
    ASSERT(!s_registry);
    s_registry = registry;
}

PassRefPtr<MediaSource> MediaSource::create(ScriptExecutionContext& context)
{
    RefPtr<MediaSource> mediaSource(adoptRef(new MediaSource(context)));
    mediaSource->suspendIfNeeded();
    return mediaSource.release();
}

MediaSource::MediaSource(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_mediaElement(0)
    , m_readyState(closedKeyword())
    , m_asyncEventQueue(*this)
{
    LOG(Media, "MediaSource::MediaSource %p", this);
    m_sourceBuffers = SourceBufferList::create(scriptExecutionContext());
    m_activeSourceBuffers = SourceBufferList::create(scriptExecutionContext());
}

MediaSource::~MediaSource()
{
    LOG(Media, "MediaSource::~MediaSource %p", this);
    ASSERT(isClosed());
}

const AtomicString& MediaSource::openKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, open, ("open", AtomicString::ConstructFromLiteral));
    return open;
}

const AtomicString& MediaSource::closedKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, closed, ("closed", AtomicString::ConstructFromLiteral));
    return closed;
}

const AtomicString& MediaSource::endedKeyword()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, ended, ("ended", AtomicString::ConstructFromLiteral));
    return ended;
}

void MediaSource::setPrivateAndOpen(PassRef<MediaSourcePrivate> mediaSourcePrivate)
{
    ASSERT(!m_private);
    ASSERT(m_mediaElement);
    m_private = std::move(mediaSourcePrivate);
    setReadyState(openKeyword());
}

void MediaSource::addedToRegistry()
{
    setPendingActivity(this);
}

void MediaSource::removedFromRegistry()
{
    unsetPendingActivity(this);
}

double MediaSource::duration() const
{
    return isClosed() ? std::numeric_limits<float>::quiet_NaN() : m_private->duration();
}

double MediaSource::currentTime() const
{
    return m_mediaElement ? m_mediaElement->currentTime() : 0;
}

std::unique_ptr<PlatformTimeRanges> MediaSource::buffered() const
{
    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    Vector<RefPtr<TimeRanges>> ranges = activeRanges();

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (ranges.isEmpty())
        return PlatformTimeRanges::create();

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
        return PlatformTimeRanges::create();

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
        intersectionRanges->intersectWith(*sourceRanges);
    }

    return PlatformTimeRanges::create(intersectionRanges->ranges());
}

void MediaSource::monitorSourceBuffers()
{
    // 2.4.4 SourceBuffer Monitoring
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#buffer-monitoring

    // Note, the behavior if activeSourceBuffers is empty is undefined.
    if (!m_activeSourceBuffers) {
        m_private->setReadyState(MediaPlayer::HaveNothing);
        return;
    }

    // ↳ If buffered for all objects in activeSourceBuffers do not contain TimeRanges for the current
    // playback position:
    auto begin = m_activeSourceBuffers->begin();
    auto end = m_activeSourceBuffers->end();
    if (std::all_of(begin, end, [](RefPtr<SourceBuffer>& sourceBuffer) {
        return !sourceBuffer->hasCurrentTime();
    })) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 2. If this is the first transition to HAVE_METADATA, then queue a task to fire a simple event
        // named loadedmetadata at the media element.
        m_private->setReadyState(MediaPlayer::HaveMetadata);

        // 3. Abort these steps.
        return;
    }

    // ↳ If buffered for all objects in activeSourceBuffers contain TimeRanges that include the current
    // playback position and enough data to ensure uninterrupted playback:
    if (std::all_of(begin, end, [](RefPtr<SourceBuffer>& sourceBuffer) {
        return sourceBuffer->hasFutureTime() && sourceBuffer->canPlayThrough();
    })) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_ENOUGH_DATA.
        // 2. Queue a task to fire a simple event named canplaythrough at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setReadyState(MediaPlayer::HaveEnoughData);

        // 4. Abort these steps.
        return;
    }

    // ↳ If buffered for at least one object in activeSourceBuffers contains a TimeRange that includes
    // the current playback position but not enough data to ensure uninterrupted playback:
    if (std::any_of(begin, end, [](RefPtr<SourceBuffer>& sourceBuffer) {
        return sourceBuffer->hasFutureTime();
    })) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_FUTURE_DATA.
        // 2. If the previous value of HTMLMediaElement.readyState was less than HAVE_FUTURE_DATA, then queue a task to fire a simple event named canplay at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setReadyState(MediaPlayer::HaveFutureData);

        // 4. Abort these steps.
        return;
    }

    // ↳ If buffered for at least one object in activeSourceBuffers contains a TimeRange that ends
    // at the current playback position and does not have a range covering the time immediately
    // after the current position:
    // NOTE: Logically, !(all objects do not contain currentTime) == (some objects contain current time)

    // 1. Set the HTMLMediaElement.readyState attribute to HAVE_CURRENT_DATA.
    // 2. If this is the first transition to HAVE_CURRENT_DATA, then queue a task to fire a simple
    // event named loadeddata at the media element.
    // 3. Playback is suspended at this point since the media element doesn't have enough data to
    // advance the media timeline.
    m_private->setReadyState(MediaPlayer::HaveCurrentData);
    
    // 4. Abort these steps.
}

void MediaSource::setDuration(double duration, ExceptionCode& ec)
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


void MediaSource::setReadyState(const AtomicString& state)
{
    ASSERT(state == openKeyword() || state == closedKeyword() || state == endedKeyword());

    AtomicString oldState = readyState();
    LOG(Media, "MediaSource::setReadyState() %p : %s -> %s", this, oldState.string().ascii().data(), state.string().ascii().data());

    if (state == closedKeyword()) {
        m_private.clear();
        m_mediaElement = 0;
    }

    if (oldState == state)
        return;

    m_readyState = state;

    onReadyStateChange(oldState, state);
}

static bool SourceBufferIsUpdating(RefPtr<SourceBuffer>& sourceBuffer)
{
    return sourceBuffer->updating();
}

void MediaSource::endOfStream(const AtomicString& error, ExceptionCode& ec)
{
    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-endOfStream-void-EndOfStreamError-error
    // 1. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 2. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (std::any_of(m_sourceBuffers->begin(), m_sourceBuffers->end(), SourceBufferIsUpdating)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 3. Run the end of stream algorithm with the error parameter set to error.
    streamEndedWithError(error, ec);
}

void MediaSource::streamEndedWithError(const AtomicString& error, ExceptionCode& ec)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, network, ("network", AtomicString::ConstructFromLiteral));
    DEPRECATED_DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode", AtomicString::ConstructFromLiteral));

    // 2.4.7 https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#end-of-stream-algorithm
    // 1. Change the readyState attribute value to "ended".
    // 2. Queue a task to fire a simple event named sourceended at the MediaSource.
    setReadyState(endedKeyword());

    // 3.
    if (error.isEmpty()) {
        // ↳ If error is not set, is null, or is an empty string
        // 1. Run the duration change algorithm with new duration set to the highest end timestamp
        // across all SourceBuffer objects in sourceBuffers.
        MediaTime maxEndTimestamp;
        for (auto it = m_sourceBuffers->begin(), end = m_sourceBuffers->end(); it != end; ++it)
            maxEndTimestamp = std::max((*it)->highestPresentationEndTimestamp(), maxEndTimestamp);
        m_private->setDuration(maxEndTimestamp.toDouble());

        // 2. Notify the media element that it now has all of the media data.
        m_private->markEndOfStream(MediaSourcePrivate::EosNoError);
    } else if (error == network) {
        // ↳ If error is set to "network"
        ASSERT(m_mediaElement);
        if (m_mediaElement->readyState() == HTMLMediaElement::HAVE_NOTHING) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data cannot be fetched at all, due to network errors, causing
            //    the user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            m_mediaElement->mediaLoadingFailed(MediaPlayer::NetworkError);
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the "If the connection is interrupted after some media data has been received, causing the
            //    user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            m_mediaElement->mediaLoadingFailedFatally(MediaPlayer::NetworkError);
        }
    } else if (error == decode) {
        // ↳ If error is set to "decode"
        ASSERT(m_mediaElement);
        if (m_mediaElement->readyState() == HTMLMediaElement::HAVE_NOTHING) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data can be fetched but is found by inspection to be in an unsupported
            //    format, or can otherwise not be rendered at all" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            m_mediaElement->mediaLoadingFailed(MediaPlayer::FormatError);
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the media data is corrupted steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            m_mediaElement->mediaLoadingFailedFatally(MediaPlayer::DecodeError);
        }
    } else {
        // ↳ Otherwise
        //   Throw an INVALID_ACCESS_ERR exception.
        ec = INVALID_ACCESS_ERR;
    }
}

SourceBuffer* MediaSource::addSourceBuffer(const String& type, ExceptionCode& ec)
{
    LOG(Media, "MediaSource::addSourceBuffer(%s) %p", type.ascii().data(), this);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // 1. If type is null or an empty then throw an INVALID_ACCESS_ERR exception and
    // abort these steps.
    if (type.isNull() || type.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return nullptr;
    }

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NOT_SUPPORTED_ERR exception and abort these steps.
    if (!isTypeSupported(type)) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    // 4. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    RefPtr<SourceBufferPrivate> sourceBufferPrivate = createSourceBufferPrivate(contentType, ec);

    if (!sourceBufferPrivate) {
        ASSERT(ec == NOT_SUPPORTED_ERR || ec == QUOTA_EXCEEDED_ERR);
        // 2. If type contains a MIME type that is not supported ..., then throw a NOT_SUPPORTED_ERR exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QUOTA_EXCEEDED_ERR exception and abort these steps
        return nullptr;
    }

    RefPtr<SourceBuffer> buffer = SourceBuffer::create(sourceBufferPrivate.releaseNonNull(), this);
    // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer);

    if (buffer->active())
        m_activeSourceBuffers->add(buffer);
    // 7. Return the new object to the caller.
    return buffer.get();
}

void MediaSource::removeSourceBuffer(SourceBuffer* buffer, ExceptionCode& ec)
{
    LOG(Media, "MediaSource::removeSourceBuffer() %p", this);
    RefPtr<SourceBuffer> protect(buffer);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer
    // 1. If sourceBuffer is null then throw an INVALID_ACCESS_ERR exception and
    // abort these steps.
    if (!buffer) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 2. If sourceBuffer specifies an object that is not in sourceBuffers then
    // throw a NOT_FOUND_ERR exception and abort these steps.
    if (!m_sourceBuffers->length() || !m_sourceBuffers->contains(buffer)) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    buffer->abortIfUpdating();

    // 4. Let SourceBuffer audioTracks list equal the AudioTrackList object returned by sourceBuffer.audioTracks.
    RefPtr<AudioTrackList> audioTracks = buffer->audioTracks();

    // 5. If the SourceBuffer audioTracks list is not empty, then run the following steps:
    if (audioTracks->length()) {
        // 5.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object returned by the audioTracks
        // attribute on the HTMLMediaElement.
        // 5.2 Let the removed enabled audio track flag equal false.
        bool removedEnabledAudioTrack = false;

        // 5.3 For each AudioTrack object in the SourceBuffer audioTracks list, run the following steps:
        while (audioTracks->length()) {
            AudioTrack* track = audioTracks->lastItem();

            // 5.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
            track->setSourceBuffer(0);

            // 5.3.2 If the enabled attribute on the AudioTrack object is true, then set the removed enabled
            // audio track flag to true.
            if (track->enabled())
                removedEnabledAudioTrack = true;

            // 5.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks list.
            // 5.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement audioTracks list.
            if (mediaElement())
                mediaElement()->removeAudioTrack(track);

            // 5.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks list.
            // 5.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the SourceBuffer audioTracks list.
            audioTracks->remove(track);
        }

        // 5.4 If the removed enabled audio track flag equals true, then queue a task to fire a simple event
        // named change at the HTMLMediaElement audioTracks list.
        if (removedEnabledAudioTrack)
            mediaElement()->audioTracks()->scheduleChangeEvent();
    }

    // 6. Let SourceBuffer videoTracks list equal the VideoTrackList object returned by sourceBuffer.videoTracks.
    RefPtr<VideoTrackList> videoTracks = buffer->videoTracks();

    // 7. If the SourceBuffer videoTracks list is not empty, then run the following steps:
    if (videoTracks->length()) {
        // 7.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object returned by the videoTracks
        // attribute on the HTMLMediaElement.
        // 7.2 Let the removed selected video track flag equal false.
        bool removedSelectedVideoTrack = false;

        // 7.3 For each VideoTrack object in the SourceBuffer videoTracks list, run the following steps:
        while (videoTracks->length()) {
            VideoTrack* track = videoTracks->lastItem();

            // 7.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
            track->setSourceBuffer(0);

            // 7.3.2 If the selected attribute on the VideoTrack object is true, then set the removed selected
            // video track flag to true.
            if (track->selected())
                removedSelectedVideoTrack = true;

            // 7.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks list.
            // 7.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement videoTracks list.
            if (mediaElement())
                mediaElement()->removeVideoTrack(track);

            // 7.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks list.
            // 7.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the SourceBuffer videoTracks list.
            videoTracks->remove(track);
        }

        // 7.4 If the removed selected video track flag equals true, then queue a task to fire a simple event
        // named change at the HTMLMediaElement videoTracks list.
        if (removedSelectedVideoTrack)
            mediaElement()->videoTracks()->scheduleChangeEvent();
    }

    // 8. Let SourceBuffer textTracks list equal the TextTrackList object returned by sourceBuffer.textTracks.
    RefPtr<TextTrackList> textTracks = buffer->textTracks();

    // 9. If the SourceBuffer textTracks list is not empty, then run the following steps:
    if (textTracks->length()) {
        // 9.1 Let HTMLMediaElement textTracks list equal the TextTrackList object returned by the textTracks
        // attribute on the HTMLMediaElement.
        // 9.2 Let the removed enabled text track flag equal false.
        bool removedEnabledTextTrack = false;

        // 9.3 For each TextTrack object in the SourceBuffer textTracks list, run the following steps:
        while (textTracks->length()) {
            TextTrack* track = textTracks->lastItem();

            // 9.3.1 Set the sourceBuffer attribute on the TextTrack object to null.
            track->setSourceBuffer(0);

            // 9.3.2 If the mode attribute on the TextTrack object is set to "showing" or "hidden", then
            // set the removed enabled text track flag to true.
            if (track->mode() == TextTrack::showingKeyword() || track->mode() == TextTrack::hiddenKeyword())
                removedEnabledTextTrack = true;

            // 9.3.3 Remove the TextTrack object from the HTMLMediaElement textTracks list.
            // 9.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement textTracks list.
            if (mediaElement())
                mediaElement()->removeTextTrack(track);

            // 9.3.5 Remove the TextTrack object from the SourceBuffer textTracks list.
            // 9.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
            // cancelable, and that uses the TrackEvent interface, at the SourceBuffer textTracks list.
            textTracks->remove(track);
        }
        
        // 9.4 If the removed enabled text track flag equals true, then queue a task to fire a simple event
        // named change at the HTMLMediaElement textTracks list.
        if (removedEnabledTextTrack)
            mediaElement()->textTracks()->scheduleChangeEvent();
    }
    
    
    // 10. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from activeSourceBuffers ...
    m_activeSourceBuffers->remove(buffer);
    
    // 11. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    m_sourceBuffers->remove(buffer);
    
    // 12. Destroy all resources for sourceBuffer.
    buffer->removedFromMediaSource();
}

bool MediaSource::isTypeSupported(const String& type)
{
    LOG(Media, "MediaSource::isTypeSupported(%s)", type.ascii().data());

    // Section 2.2 isTypeSupported() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
    // 1. If type is an empty string, then return false.
    if (type.isNull() || type.isEmpty())
        return false;

    ContentType contentType(type);
    String codecs = contentType.parameter("codecs");

    // 2. If type does not contain a valid MIME type string, then return false.
    if (contentType.type().isEmpty() || codecs.isEmpty())
        return false;

    // 3. If type contains a media type or media subtype that the MediaSource does not support, then return false.
    // 4. If type contains at a codec that the MediaSource does not support, then return false.
    // 5. If the MediaSource does not support the specified combination of media type, media subtype, and codecs then return false.
    // 6. Return true.
    MediaEngineSupportParameters parameters;
    parameters.type = contentType.type();
    parameters.codecs = codecs;
    parameters.isMediaSource = true;
    return MediaPlayer::supportsType(parameters, 0) != MediaPlayer::IsNotSupported;
}

bool MediaSource::isOpen() const
{
    return readyState() == openKeyword();
}

bool MediaSource::isClosed() const
{
    return readyState() == closedKeyword();
}

void MediaSource::close()
{
    setReadyState(closedKeyword());
}

void MediaSource::sourceBufferDidChangeAcitveState(SourceBuffer* sourceBuffer, bool active)
{
    if (active && !m_activeSourceBuffers->contains(sourceBuffer))
        m_activeSourceBuffers->add(sourceBuffer);
    else if (!active && m_activeSourceBuffers->contains(sourceBuffer))
        m_activeSourceBuffers->remove(sourceBuffer);
}

bool MediaSource::attachToElement(HTMLMediaElement* element)
{
    if (m_mediaElement)
        return false;

    ASSERT(isClosed());

    m_mediaElement = element;
    return true;
}

void MediaSource::openIfInEndedState()
{
    if (m_readyState != endedKeyword())
        return;

    setReadyState(openKeyword());
    m_private->unmarkEndOfStream();
}

bool MediaSource::hasPendingActivity() const
{
    return m_private || m_asyncEventQueue.hasPendingEvents()
        || ActiveDOMObject::hasPendingActivity();
}

void MediaSource::stop()
{
    m_asyncEventQueue.close();
    if (!isClosed())
        setReadyState(closedKeyword());
    m_private.clear();
}

void MediaSource::onReadyStateChange(const AtomicString& oldState, const AtomicString& newState)
{
    if (isOpen()) {
        scheduleEvent(eventNames().sourceopenEvent);
        return;
    }

    if (oldState == openKeyword() && newState == endedKeyword()) {
        scheduleEvent(eventNames().sourceendedEvent);
        return;
    }

    ASSERT(isClosed());

    m_activeSourceBuffers->clear();

    // Clear SourceBuffer references to this object.
    for (unsigned long i = 0, length =  m_sourceBuffers->length(); i < length; ++i)
        m_sourceBuffers->item(i)->removedFromMediaSource();
    m_sourceBuffers->clear();
    
    scheduleEvent(eventNames().sourcecloseEvent);
}

Vector<RefPtr<TimeRanges>> MediaSource::activeRanges() const
{
    Vector<RefPtr<TimeRanges>> activeRanges(m_activeSourceBuffers->length());
    for (size_t i = 0, length = m_activeSourceBuffers->length(); i < length; ++i)
        activeRanges[i] = m_activeSourceBuffers->item(i)->buffered(ASSERT_NO_EXCEPTION);

    return activeRanges;
}

RefPtr<SourceBufferPrivate> MediaSource::createSourceBufferPrivate(const ContentType& type, ExceptionCode& ec)
{
    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    switch (m_private->addSourceBuffer(type, sourceBufferPrivate)) {
    case MediaSourcePrivate::Ok: {
        return sourceBufferPrivate;
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

void MediaSource::scheduleEvent(const AtomicString& eventName)
{
    RefPtr<Event> event = Event::create(eventName, false, false);
    event->setTarget(this);

    m_asyncEventQueue.enqueueEvent(event.release());
}

ScriptExecutionContext* MediaSource::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetInterface MediaSource::eventTargetInterface() const
{
    return MediaSourceEventTargetInterfaceType;
}

URLRegistry& MediaSource::registry() const
{
    return MediaSourceRegistry::registry();
}

}

#endif
