/*
 *  Copyright (C) 2011 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationGStreamer.h"

#include "AudioChannel.h"
#include "AudioSourceProvider.h"
#include "GOwnPtr.h"

namespace WebCore {

PassOwnPtr<AudioDestination> AudioDestination::create(AudioSourceProvider& provider, float sampleRate)
{
    return adoptPtr(new AudioDestinationGStreamer(provider, sampleRate));
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

AudioDestinationGStreamer::AudioDestinationGStreamer(AudioSourceProvider& provider, float sampleRate)
    : m_provider(provider)
    , m_renderBus(2, 128, true)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
}

AudioDestinationGStreamer::~AudioDestinationGStreamer()
{
}

void AudioDestinationGStreamer::start()
{
    m_isPlaying = true;
}

void AudioDestinationGStreamer::stop()
{
    m_isPlaying = false;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
