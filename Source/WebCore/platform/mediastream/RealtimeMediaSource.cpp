/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
#include "RealtimeMediaSource.h"

#include "RealtimeMediaSourceCapabilities.h"
#include "UUID.h"

namespace WebCore {

RealtimeMediaSource::RealtimeMediaSource(const String& id, Type type, const String& name)
    : m_id(id)
    , m_type(type)
    , m_name(name)
{
    // FIXME(147205): Need to implement fitness score for constraints

    if (m_id.isEmpty())
        m_id = createCanonicalUUIDString();
    m_persistentID = m_id;
}

void RealtimeMediaSource::reset()
{
    m_stopped = false;
    m_muted = false;
    m_readonly = false;
    m_remote = false;
}

void RealtimeMediaSource::addObserver(RealtimeMediaSource::Observer* observer)
{
    m_observers.append(observer);
}

void RealtimeMediaSource::removeObserver(RealtimeMediaSource::Observer* observer)
{
    size_t pos = m_observers.find(observer);
    if (pos != notFound)
        m_observers.remove(pos);

    if (!m_observers.size())
        stop();
}

void RealtimeMediaSource::setMuted(bool muted)
{
    if (m_muted == muted)
        return;

    m_muted = muted;

    if (stopped())
        return;

    for (auto& observer : m_observers)
        observer->sourceMutedChanged();
}

void RealtimeMediaSource::settingsDidChange()
{
    for (auto& observer : m_observers)
        observer->sourceSettingsChanged();
}

void RealtimeMediaSource::mediaDataUpdated(MediaSample& mediaSample)
{
    for (auto& observer : m_observers)
        observer->sourceHasMoreMediaData(mediaSample);
}

bool RealtimeMediaSource::readonly() const
{
    return m_readonly;
}

void RealtimeMediaSource::stop(Observer* callingObserver)
{
    if (stopped())
        return;

    m_stopped = true;

    for (auto* observer : m_observers) {
        if (observer != callingObserver)
            observer->sourceStopped();
    }

    stopProducingData();
}

void RealtimeMediaSource::requestStop(Observer* callingObserver)
{
    if (stopped())
        return;

    for (auto* observer : m_observers) {
        if (observer->preventSourceFromStopping())
            return;
    }
    stop(callingObserver);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
