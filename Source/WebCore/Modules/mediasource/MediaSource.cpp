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
#include "ExceptionCodePlaceholder.h"
#include "GenericEventQueue.h"
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
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

namespace WebCore {

PassRefPtr<MediaSource> MediaSource::create(ScriptExecutionContext& context)
{
    RefPtr<MediaSource> mediaSource(adoptRef(new MediaSource(context)));
    mediaSource->suspendIfNeeded();
    return mediaSource.release();
}

MediaSource::MediaSource(ScriptExecutionContext& context)
    : MediaSourceBase(context)
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

SourceBuffer* MediaSource::addSourceBuffer(const String& type, ExceptionCode& ec)
{
    LOG(Media, "MediaSource::addSourceBuffer(%s) %p", type.ascii().data(), this);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // 1. If type is null or an empty then throw an INVALID_ACCESS_ERR exception and
    // abort these steps.
    if (type.isNull() || type.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NOT_SUPPORTED_ERR exception and abort these steps.
    if (!isTypeSupported(type)) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    // 4. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    RefPtr<SourceBufferPrivate> sourceBufferPrivate = createSourceBufferPrivate(contentType, ec);

    if (!sourceBufferPrivate) {
        ASSERT(ec == NOT_SUPPORTED_ERR || ec == QUOTA_EXCEEDED_ERR);
        // 2. If type contains a MIME type that is not supported ..., then throw a NOT_SUPPORTED_ERR exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QUOTA_EXCEEDED_ERR exception and abort these steps
        return 0;
    }

    RefPtr<SourceBuffer> buffer = SourceBuffer::create(sourceBufferPrivate.releaseNonNull(), this);
    // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer);
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
    for (unsigned long i = 0; i < m_sourceBuffers->length(); ++i)
        m_sourceBuffers->item(i)->removedFromMediaSource();
    m_sourceBuffers->clear();

    scheduleEvent(eventNames().sourcecloseEvent);
}

Vector<RefPtr<TimeRanges>> MediaSource::activeRanges() const
{
    Vector<RefPtr<TimeRanges>> activeRanges(m_activeSourceBuffers->length());
    for (size_t i = 0; i < m_activeSourceBuffers->length(); ++i)
        activeRanges[i] = m_activeSourceBuffers->item(i)->buffered(ASSERT_NO_EXCEPTION);

    return activeRanges;
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

EventTargetInterface MediaSource::eventTargetInterface() const
{
    return MediaSourceEventTargetInterfaceType;
}

void MediaSource::sourceBufferDidChangeAcitveState(SourceBuffer* sourceBuffer, bool active)
{
    if (active && !m_activeSourceBuffers->contains(sourceBuffer))
        m_activeSourceBuffers->add(sourceBuffer);
    else if (!active && m_activeSourceBuffers->contains(sourceBuffer))
        m_activeSourceBuffers->remove(sourceBuffer);
}

class SourceBufferBufferedDoesNotContainTime {
public:
    SourceBufferBufferedDoesNotContainTime(double time) : m_time(time) { }
    bool operator()(RefPtr<SourceBuffer> sourceBuffer)
    {
        return !sourceBuffer->buffered()->contain(m_time);
    }

    double m_time;
};

class SourceBufferBufferedHasEnough {
public:
    SourceBufferBufferedHasEnough(double time, double duration) : m_time(time), m_duration(duration) { }
    bool operator()(RefPtr<SourceBuffer> sourceBuffer)
    {
        size_t rangePos = sourceBuffer->buffered()->find(m_time);
        if (rangePos == notFound)
            return false;

        double endTime = sourceBuffer->buffered()->end(rangePos, IGNORE_EXCEPTION);
        return m_duration - endTime < 1;
    }

    double m_time;
    double m_duration;
};

class SourceBufferBufferedHasFuture {
public:
    SourceBufferBufferedHasFuture(double time) : m_time(time) { }
    bool operator()(RefPtr<SourceBuffer> sourceBuffer)
    {
        size_t rangePos = sourceBuffer->buffered()->find(m_time);
        if (rangePos == notFound)
            return false;

        double endTime = sourceBuffer->buffered()->end(rangePos, IGNORE_EXCEPTION);
        return endTime - m_time > 1;
    }

    double m_time;
};

void MediaSource::monitorSourceBuffers()
{
    double currentTime = mediaElement()->currentTime();

    // 2.4.4 SourceBuffer Monitoring
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#buffer-monitoring
    // ↳ If buffered for all objects in activeSourceBuffers do not contain TimeRanges for the current
    // playback position:
    auto begin = m_activeSourceBuffers->begin();
    auto end = m_activeSourceBuffers->end();
    if (std::all_of(begin, end, SourceBufferBufferedDoesNotContainTime(currentTime))) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 2. If this is the first transition to HAVE_METADATA, then queue a task to fire a simple event
        // named loadedmetadata at the media element.
        m_private->setReadyState(MediaPlayer::HaveMetadata);

        // 3. Abort these steps.
        return;
    }

    // ↳ If buffered for all objects in activeSourceBuffers contain TimeRanges that include the current
    // playback position and enough data to ensure uninterrupted playback:
    if (std::all_of(begin, end, SourceBufferBufferedHasEnough(currentTime, mediaElement()->duration()))) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_ENOUGH_DATA.
        // 2. Queue a task to fire a simple event named canplaythrough at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setReadyState(MediaPlayer::HaveEnoughData);

        // 4. Abort these steps.
        return;
    }

    // ↳ If buffered for at least one object in activeSourceBuffers contains a TimeRange that includes
    // the current playback position but not enough data to ensure uninterrupted playback:
    if (std::any_of(begin, end, SourceBufferBufferedHasFuture(currentTime))) {
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



} // namespace WebCore

#endif
