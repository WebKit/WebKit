/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

namespace WebCore {

MediaStreamSource::MediaStreamSource(const String& id, Type type, const String& name)
    : m_id(id)
    , m_type(type)
    , m_name(name)
    , m_readyState(New)
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
    for (auto observer = m_observers.begin(); observer != m_observers.end(); ++observer)
        (*observer)->sourceReadyStateChanged();

    if (m_readyState == Live) {
        startProducingData();
        return;
    }
    
    // There are no more consumers of this source's data, shut it down as appropriate.
    if (m_readyState == Ended)
        stopProducingData();
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

    if (!m_observers.size())
        stop();
}

void MediaStreamSource::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    if (m_readyState == Ended)
        return;

    for (auto observer = m_observers.begin(); observer != m_observers.end(); ++observer)
        (*observer)->sourceMutedChanged();
}

void MediaStreamSource::setEnabled(bool enabled)
{
    if (!enabled) {
        // Don't disable the source unless all observers are disabled.
        for (auto observer = m_observers.begin(); observer != m_observers.end(); ++observer) {
            if ((*observer)->observerIsEnabled())
                return;
        }
    }

    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (m_readyState == Ended)
        return;

    if (!enabled)
        stopProducingData();
    else
        startProducingData();

    for (auto observer = m_observers.begin(); observer != m_observers.end(); ++observer)
        (*observer)->sourceEnabledChanged();
}

bool MediaStreamSource::readonly() const
{
    return m_readonly;
}

void MediaStreamSource::stop()
{
    // This is called from the track.stop() method, which should "Permanently stop the generation of data
    // for track's source", so go straight to ended. This will notify any other tracks using this source
    // that it is no longer available.
    setReadyState(Ended);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
