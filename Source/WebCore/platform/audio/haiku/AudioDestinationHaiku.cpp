/*
 *  Copyright (C) 2014 Haiku, Inc.
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

#include "AudioDestinationHaiku.h"

#include "Logging.h"

namespace WebCore {

std::unique_ptr<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    // FIXME: Add support for multi-channel (> stereo) output.
    if (numberOfOutputChannels != 2)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    return std::make_unique<AudioDestinationHaiku>(callback, sampleRate);
}

float AudioDestination::hardwareSampleRate()
{
    // FIXME get the actual sample rate from the media kit
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    return 0;
}


static const unsigned framesToPull = 128;
AudioDestinationHaiku::AudioDestinationHaiku(AudioIOCallback& callback, float sampleRate)
    : m_callback(callback)
    , m_renderBus(AudioBus::create(2, framesToPull, false))
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    // FIXME implement
}


AudioDestinationHaiku::~AudioDestinationHaiku()
{
}


void AudioDestinationHaiku::start()
{
    // FIXME implement
}

void AudioDestinationHaiku::stop()
{
    // FIXME implement
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
