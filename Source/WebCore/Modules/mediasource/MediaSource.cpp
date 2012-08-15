/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "ContentType.h"
#include "Event.h"
#include "TimeRanges.h"
#include <wtf/Uint8Array.h>

namespace WebCore {

PassRefPtr<MediaSource> MediaSource::create(ScriptExecutionContext* context)
{
    return adoptRef(new MediaSource(context));
}

MediaSource::MediaSource(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
    , m_readyState(closedKeyword())
    , m_player(0)
{
    m_sourceBuffers = SourceBufferList::create(scriptExecutionContext());
    m_activeSourceBuffers = SourceBufferList::create(scriptExecutionContext());
}

SourceBufferList* MediaSource::sourceBuffers()
{
    return m_sourceBuffers.get();
}

SourceBufferList* MediaSource::activeSourceBuffers()
{
    // FIXME(91649): support track selection
    return m_activeSourceBuffers.get();
}

SourceBuffer* MediaSource::addSourceBuffer(const String& type, ExceptionCode& ec)
{
    // 3.1 http://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#dom-addsourcebuffer
    // 1. If type is null or an empty then throw an INVALID_ACCESS_ERR exception and
    // abort these steps.
    if (type.isNull() || type.isEmpty()) {
        ec = INVALID_ACCESS_ERR;
        return 0;
    }

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NOT_SUPPORTED_ERR exception and abort these steps.
    ContentType contentType(type);
    Vector<String> codecs = contentType.codecs();

    if (!codecs.size()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    
    // 4. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!m_player || m_readyState != openKeyword()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    // 5. Create a new SourceBuffer object and associated resources.
    String id = m_sourceBuffers->generateUniqueId();
    if (id == emptyString()) {
        ec = QUOTA_EXCEEDED_ERR;
        return 0;
    }

    RefPtr<SourceBuffer> buffer = SourceBuffer::create(id, this);

    switch (m_player->sourceAddId(buffer->id(), contentType.type(), codecs)) {
    case MediaPlayer::Ok:
        // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
        m_sourceBuffers->add(buffer);
        m_activeSourceBuffers->add(buffer);
        // 7. Return the new object to the caller.
        return buffer.get();
    case MediaPlayer::NotSupported:
        // 2 (cont). If type contains a MIME type ... that is not supported with the types 
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NOT_SUPPORTED_ERR exception and abort these steps.
        ec = NOT_SUPPORTED_ERR;
        return 0;
    case MediaPlayer::ReachedIdLimit:
        // 3 (cont). If the user agent can't handle any more SourceBuffer objects then throw 
        // a QUOTA_EXCEEDED_ERR exception and abort these steps.
        ec = QUOTA_EXCEEDED_ERR;
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void MediaSource::removeSourceBuffer(SourceBuffer* buffer, ExceptionCode& ec)
{
    // 3.1 http://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#dom-removesourcebuffer
    // 1. If sourceBuffer is null then throw an INVALID_ACCESS_ERR exception and
    // abort these steps.
    if (!buffer) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 2. If sourceBuffers is empty then throw an INVALID_STATE_ERR exception and
    // abort these steps.
    if (!m_player || !m_sourceBuffers->length()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    // 3. If sourceBuffer specifies an object that is not in sourceBuffers then
    // throw a NOT_FOUND_ERR exception and abort these steps.
    // 6. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    if (!m_sourceBuffers->remove(buffer)) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // 7. Destroy all resources for sourceBuffer.
    m_activeSourceBuffers->remove(buffer);
    m_player->sourceRemoveId(buffer->id());

    // 4. Remove track information from audioTracks, videoTracks, and textTracks for all tracks 
    // associated with sourceBuffer and fire a simple event named change on the modified lists.
    // FIXME(91649): support track selection

    // 5. If sourceBuffer is in activeSourceBuffers, then remove it from that list and fire a
    // removesourcebuffer event on that object.
    // FIXME(91649): support track selection
}

const String& MediaSource::readyState() const
{
    return m_readyState;
}

void MediaSource::setReadyState(const String& state)
{
    ASSERT(state == openKeyword() || state == closedKeyword() || state == endedKeyword());
    if (m_readyState == state)
        return;

    String oldState = m_readyState;
    m_readyState = state;

    if (m_readyState == closedKeyword()) {
        m_sourceBuffers->clear();
        m_activeSourceBuffers->clear();
        m_player = 0;
        dispatchEvent(Event::create(eventNames().webkitsourcecloseEvent, false, false));
        return;
    }
    
    if (oldState == openKeyword() && m_readyState == endedKeyword()) {
        dispatchEvent(Event::create(eventNames().webkitsourceendedEvent, false, false));
        return;
    }

    if (m_readyState == openKeyword()) {
        dispatchEvent(Event::create(eventNames().webkitsourceopenEvent, false, false));
        return;
    }
}

void MediaSource::endOfStream(const String& error, ExceptionCode& ec)
{
    // 3.1 http://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#dom-endofstream
    // 1. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!m_player || m_readyState != openKeyword()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaPlayer::EndOfStreamStatus eosStatus = MediaPlayer::EosNoError;

    if (error.isNull() || error.isEmpty())
        eosStatus = MediaPlayer::EosNoError;
    else if (error == "network")
        eosStatus = MediaPlayer::EosNetworkError;
    else if (error == "decode")
        eosStatus = MediaPlayer::EosDecodeError;
    else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 2. Change the readyState attribute value to "ended".
    setReadyState(endedKeyword());
    m_player->sourceEndOfStream(eosStatus);
}

PassRefPtr<TimeRanges> MediaSource::buffered(const String& id, ExceptionCode& ec) const
{
    if (!m_player || m_readyState == closedKeyword()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_player->sourceBuffered(id);
}

void MediaSource::append(const String& id, PassRefPtr<Uint8Array> data, ExceptionCode& ec)
{
    if (!data.get()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (!m_player || m_readyState != openKeyword()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!data->length())
        return;

    if (!m_player->sourceAppend(id, data->data(), data->length())) {
        ec = SYNTAX_ERR;
        return;
    }
}

void MediaSource::abort(const String& id, ExceptionCode& ec)
{
    if (!m_player || m_readyState != openKeyword()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!m_player->sourceAbort(id))
        ASSERT_NOT_REACHED();
}

bool MediaSource::setTimestampOffset(const String& id, double offset, ExceptionCode& ec)
{
    if (!m_player || m_readyState != openKeyword()) {
        ec = INVALID_STATE_ERR;
        return false;
    }

    if (!m_player->sourceSetTimestampOffset(id, offset)) {
        ec = INVALID_STATE_ERR;
        return false;
    }
    return true;
}

const AtomicString& MediaSource::interfaceName() const
{
    return eventNames().interfaceForMediaSource;
}

ScriptExecutionContext* MediaSource::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

EventTargetData* MediaSource::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MediaSource::ensureEventTargetData()
{
    return &m_eventTargetData;
}

} // namespace WebCore

#endif
