/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamSource.h"

#include "MediaStreamCenter.h"
#include "MediaStreamSourceCapabilities.h"
#include "UUID.h"

#include <wtf/PassOwnPtr.h>

namespace WebCore {

MediaStreamSource::MediaStreamSource(const String& id, Type type, const String& name)
    : m_id(id)
    , m_type(type)
    , m_name(name)
    , m_readyState(New)
    , m_stream(0)
    , m_enabled(true)
    , m_muted(false)
    , m_readonly(false)
    , m_remote(false)
{
    if (!m_id.isEmpty())
        return;
    
    m_id = createCanonicalUUIDString();
}

void MediaStreamSource::reset()
{
    m_readyState = New;
    m_stream = 0;
    m_enabled = true;
    m_muted = false;
    m_readonly = false;
    m_remote = false;
}

void MediaStreamSource::setReadyState(ReadyState readyState)
{
    if (m_readyState == Ended || m_readyState == readyState)
        return;

    m_readyState = readyState;
    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i)
        (*i)->sourceStateChanged();
}

void MediaStreamSource::addObserver(MediaStreamSource::Observer* observer)
{
    m_observers.append(observer);
}

void MediaStreamSource::removeObserver(MediaStreamSource::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);
}

void MediaStreamSource::setStream(MediaStreamDescriptor* stream)
{
    // FIXME: A source should not need to know about its stream(s). This will be fixed as a part of
    // https://bugs.webkit.org/show_bug.cgi?id=121954
    m_stream = stream;
}

MediaConstraints* MediaStreamSource::constraints() const
{
    // FIXME: While this returns 
    // https://bugs.webkit.org/show_bug.cgi?id=122428
    return m_constraints.get();
}
    
void MediaStreamSource::setConstraints(PassRefPtr<MediaConstraints> constraints)
{
    m_constraints = constraints;
}

void MediaStreamSource::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    if (m_readyState == Ended)
        return;

    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i)
        (*i)->sourceMutedChanged();
}

void MediaStreamSource::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    
    m_enabled = enabled;

    if (m_readyState == Ended)
        return;
    
    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i)
        (*i)->sourceEnabledChanged();
}

bool MediaStreamSource::readonly() const
{
    // http://www.w3.org/TR/mediacapture-streams/#widl-MediaStreamTrack-_readonly
    // If the track (audio or video) is backed by a read-only source such as a file, or the track source 
    // is a local microphone or camera, but is shared so that this track cannot modify any of the source's
    // settings, the readonly attribute must return the value true. Otherwise, it must return the value false.
    return m_readonly;
}

void MediaStreamSource::stop()
{
    for (Vector<Observer*>::iterator i = m_observers.begin(); i != m_observers.end(); ++i) {
        if (!(*i)->stopped())
            return;
    }

    // There are no more consumers of this source's data, shut it down as appropriate.
    setReadyState(Ended);

    // http://www.w3.org/TR/mediacapture-streams/#widl-MediaStreamTrack-stop-void
    // If the data is being generated from a live source (e.g., a microphone or camera), then the user 
    // agent should remove any active "on-air" indicator for that source. If the data is being 
    // generated from a prerecorded source (e.g. a video file), any remaining content in the file is ignored.
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
