/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#include "MediaStreamTrackPrivate.h"

#include "MediaSourceStates.h"
#include "MediaStreamCapabilities.h"
#include "NotImplemented.h"
#include "UUID.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

PassRefPtr<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(PassRefPtr<MediaStreamSource> source)
{
    return adoptRef(new MediaStreamTrackPrivate(source));
}

MediaStreamTrackPrivate::MediaStreamTrackPrivate(const MediaStreamTrackPrivate& other)
    : RefCounted()
    , m_client(nullptr)
{
    m_ignoreMutations = true;
    m_id = createCanonicalUUIDString();
    setSource(other.source());
    m_readyState = other.readyState();
    m_muted = other.muted();
    m_enabled = other.enabled();
    m_stopped = other.stopped();
    m_ignoreMutations = false;
}

MediaStreamTrackPrivate::MediaStreamTrackPrivate(PassRefPtr<MediaStreamSource> source)
    : m_source(nullptr)
    , m_client(nullptr)
    , m_readyState(MediaStreamSource::New)
    , m_muted(false)
    , m_enabled(true)
    , m_stopped(false)
{
    m_ignoreMutations = true;
    setSource(source);
    m_ignoreMutations = false;
}

MediaStreamTrackPrivate::~MediaStreamTrackPrivate()
{
    if (m_source)
        m_source->removeObserver(this);
}

void MediaStreamTrackPrivate::setSource(PassRefPtr<MediaStreamSource> source)
{
    if (m_source)
        m_source->removeObserver(this);

    m_source = source;

    if (!m_source)
        return;

    setMuted(m_source->muted());
    setReadyState(m_source->readyState());
    if (m_source)
        m_source->addObserver(this);
}

const String& MediaStreamTrackPrivate::id() const
{
    if (!m_id.isEmpty())
        return m_id;

    // The spec says:
    //   Unless a MediaStreamTrack object is created as a part a of special purpose algorithm that
    //   specifies how the track id must be initialized, the user agent must generate a globally
    //   unique identifier string and initialize the object's id attribute to that string.
    if (m_source && m_source->useIDForTrackID())
        return m_source->id();

    m_id = createCanonicalUUIDString();
    return m_id;
}

const String& MediaStreamTrackPrivate::label() const
{
    if (m_source)
        return m_source->name();

    return emptyString();
}

bool MediaStreamTrackPrivate::ended() const
{
    return m_stopped || (m_source && m_source->readyState() == MediaStreamSource::Ended);
}

bool MediaStreamTrackPrivate::muted() const
{
    if (m_stopped || !m_source)
        return false;

    return m_source->muted();
}

void MediaStreamTrackPrivate::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    if (!m_client || m_ignoreMutations)
        return;

    m_client->trackMutedChanged();
}

bool MediaStreamTrackPrivate::readonly() const
{
    if (m_stopped || !m_source)
        return true;

    return m_source->readonly();
}

bool MediaStreamTrackPrivate::remote() const
{
    if (!m_source)
        return false;

    return m_source->remote();
}

void MediaStreamTrackPrivate::setEnabled(bool enabled)
{
    if (m_stopped || m_enabled == enabled)
        return;

    // 4.3.3.1
    // ... after a MediaStreamTrack is disassociated from its track, its enabled attribute still
    // changes value when set; it just doesn't do anything with that new value.
    m_enabled = enabled;

    if (m_source)
        m_source->setEnabled(enabled);

    if (!m_client || m_ignoreMutations)
        return;

    m_client->trackEnabledChanged();
}

void MediaStreamTrackPrivate::stop(StopBehavior stopSource)
{
    if (m_stopped)
        return;

    if (stopSource == StopTrackAndStopSource && m_source)
        m_source->stop();

    setReadyState(MediaStreamSource::Ended);
    m_stopped = true;
}

MediaStreamSource::ReadyState MediaStreamTrackPrivate::readyState() const
{
    if (m_stopped)
        return MediaStreamSource::Ended;

    return m_readyState;
}

void MediaStreamTrackPrivate::setReadyState(MediaStreamSource::ReadyState state)
{
    if (m_readyState == MediaStreamSource::Ended || m_readyState == state)
        return;

    MediaStreamSource::ReadyState oldState = m_readyState;
    m_readyState = state;

    if (!m_client || m_ignoreMutations)
        return;

    if ((m_readyState == MediaStreamSource::Live && oldState == MediaStreamSource::New) || m_readyState == MediaStreamSource::Ended)
        m_client->trackReadyStateChanged();
}

RefPtr<MediaStreamTrackPrivate> MediaStreamTrackPrivate::clone()
{
    return adoptRef(new MediaStreamTrackPrivate(*this));
}

    
RefPtr<MediaConstraints> MediaStreamTrackPrivate::constraints() const
{
    return m_constraints;
}

const MediaStreamSourceStates& MediaStreamTrackPrivate::states() const
{
    if (!m_source) {
        DEPRECATED_DEFINE_STATIC_LOCAL(const MediaStreamSourceStates, noState, ());
        return noState;
    }
    
    return m_source->states();
}

MediaStreamSource::Type MediaStreamTrackPrivate::type() const
{
    if (!m_source)
        return MediaStreamSource::None;

    return m_source->type();
}

RefPtr<MediaStreamSourceCapabilities> MediaStreamTrackPrivate::capabilities() const
{
    if (!m_source)
        return 0;

    return m_source->capabilities();
}

void MediaStreamTrackPrivate::applyConstraints(PassRefPtr<MediaConstraints>)
{
    // FIXME: apply the new constraints to the track
    // https://bugs.webkit.org/show_bug.cgi?id=122428
}

void MediaStreamTrackPrivate::sourceReadyStateChanged()
{
    if (stopped())
        return;
    
    setReadyState(m_source->readyState());
}

void MediaStreamTrackPrivate::sourceMutedChanged()
{
    if (stopped())
        return;
    
    setMuted(m_source->muted());
}

void MediaStreamTrackPrivate::sourceEnabledChanged()
{
    if (stopped())
        return;
    
    setEnabled(m_source->enabled());
}

bool MediaStreamTrackPrivate::observerIsEnabled()
{
    return enabled();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
