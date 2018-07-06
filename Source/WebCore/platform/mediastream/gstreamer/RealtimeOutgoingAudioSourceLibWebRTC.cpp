/*
 * Copyright (C) 2017 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if USE(LIBWEBRTC) && USE(GSTREAMER)
#include "RealtimeOutgoingAudioSourceLibWebRTC.h"

namespace WebCore {

RealtimeOutgoingAudioSourceLibWebRTC::RealtimeOutgoingAudioSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&& audioSource)
    : RealtimeOutgoingAudioSource(WTFMove(audioSource))
{
}

RealtimeOutgoingAudioSourceLibWebRTC::~RealtimeOutgoingAudioSourceLibWebRTC()
{
}

Ref<RealtimeOutgoingAudioSource> RealtimeOutgoingAudioSource::create(Ref<MediaStreamTrackPrivate>&& audioSource)
{
    return RealtimeOutgoingAudioSourceLibWebRTC::create(WTFMove(audioSource));
}

void RealtimeOutgoingAudioSourceLibWebRTC::audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&,
    size_t /* sampleCount */)
{
}

void RealtimeOutgoingAudioSourceLibWebRTC::pullAudioData()
{
}

bool RealtimeOutgoingAudioSourceLibWebRTC::isReachingBufferedAudioDataHighLimit()
{
    return false;
}

bool RealtimeOutgoingAudioSourceLibWebRTC::isReachingBufferedAudioDataLowLimit()
{
    return false;
}

bool RealtimeOutgoingAudioSourceLibWebRTC::hasBufferedEnoughData()
{
    return false;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
