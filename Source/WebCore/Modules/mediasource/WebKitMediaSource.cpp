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
#include "WebKitMediaSource.h"

#if ENABLE(MEDIA_SOURCE)

#include "ContentType.h"
#include "ExceptionCodePlaceholder.h"
#include "MIMETypeRegistry.h"
#include "MediaSourceRegistry.h"
#include "SourceBufferPrivate.h"
#include "TimeRanges.h"
#include <runtime/Uint8Array.h>

namespace WebCore {

PassRefPtr<WebKitMediaSource> WebKitMediaSource::create(ScriptExecutionContext* context)
{
    RefPtr<WebKitMediaSource> mediaSource(adoptRef(new WebKitMediaSource(context)));
    mediaSource->suspendIfNeeded();
    return mediaSource.release();
}

WebKitMediaSource::WebKitMediaSource(ScriptExecutionContext* context)
    : MediaSourceBase(context)
    , m_asyncEventQueue(*this)
    , m_sourceBuffers(WebKitSourceBufferList::create(scriptExecutionContext()))
    , m_activeSourceBuffers(WebKitSourceBufferList::create(scriptExecutionContext()))
{
}

WebKitSourceBufferList* WebKitMediaSource::sourceBuffers()
{
    return m_sourceBuffers.get();
}

WebKitSourceBufferList* WebKitMediaSource::activeSourceBuffers()
{
    // FIXME(91649): support track selection
    return m_activeSourceBuffers.get();
}

WebKitSourceBuffer* WebKitMediaSource::addSourceBuffer(const String& type, ExceptionCode& ec)
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
    Vector<String> codecs = contentType.codecs();
    OwnPtr<SourceBufferPrivate> sourceBufferPrivate = createSourceBufferPrivate(contentType.type(), codecs, ec);
    if (!sourceBufferPrivate)
        return 0;

    RefPtr<WebKitSourceBuffer> buffer = WebKitSourceBuffer::create(sourceBufferPrivate.release(), this);
    // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer);
    m_activeSourceBuffers->add(buffer);
    // 7. Return the new object to the caller.
    return buffer.get();
}

void WebKitMediaSource::removeSourceBuffer(WebKitSourceBuffer* buffer, ExceptionCode& ec)
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
    if (isClosed() || !m_sourceBuffers->length()) {
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

    // 4. Remove track information from audioTracks, videoTracks, and textTracks for all tracks
    // associated with sourceBuffer and fire a simple event named change on the modified lists.
    // FIXME(91649): support track selection

    // 5. If sourceBuffer is in activeSourceBuffers, then remove it from that list and fire a
    // removesourcebuffer event on that object.
    // FIXME(91649): support track selection
}

void WebKitMediaSource::onReadyStateChange(const AtomicString& oldState, const AtomicString& newState)
{
    if (isClosed()) {
        m_sourceBuffers->clear();
        m_activeSourceBuffers->clear();
        scheduleEvent(eventNames().webkitsourcecloseEvent);
        return;
    }

    if (oldState == openKeyword() && newState == endedKeyword()) {
        scheduleEvent(eventNames().webkitsourceendedEvent);
        return;
    }

    if (isOpen()) {
        scheduleEvent(eventNames().webkitsourceopenEvent);
        return;
    }
}

Vector<RefPtr<TimeRanges> > WebKitMediaSource::activeRanges() const
{
    Vector<RefPtr<TimeRanges> > activeRanges(m_activeSourceBuffers->length());
    for (size_t i = 0; i < m_activeSourceBuffers->length(); ++i)
        activeRanges[i] = m_activeSourceBuffers->item(i)->buffered(ASSERT_NO_EXCEPTION);

    return activeRanges;
}

bool WebKitMediaSource::isTypeSupported(const String& type)
{
    // Section 2.1 isTypeSupported() method steps.
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
    return MIMETypeRegistry::isSupportedMediaSourceMIMEType(contentType.type(), codecs);
}

EventTargetInterface WebKitMediaSource::eventTargetInterface() const
{
    return WebKitMediaSourceEventTargetInterfaceType;
}

} // namespace WebCore

#endif
