/*
 *  Copyright (C) 2011 Philippe Normand <pnormand@igalia.com>
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

#ifndef AudioDestinationGStreamer_h
#define AudioDestinationGStreamer_h

#include "AudioBus.h"
#include "AudioDestination.h"

typedef struct _GstElement GstElement;
typedef struct _GstPad GstPad;

namespace WebCore {

class AudioDestinationGStreamer : public AudioDestination {
public:
    AudioDestinationGStreamer(AudioSourceProvider&, float sampleRate);
    virtual ~AudioDestinationGStreamer();

    virtual void start();
    virtual void stop();

    bool isPlaying() { return m_isPlaying; }
    float sampleRate() const { return m_sampleRate; }
    AudioSourceProvider& sourceProvider() const { return m_provider; }

    void finishBuildingPipelineAfterWavParserPadReady(GstPad*);

private:
    AudioSourceProvider& m_provider;
    AudioBus m_renderBus;

    float m_sampleRate;
    bool m_isPlaying;
    bool m_wavParserAvailable;
    bool m_audioSinkAvailable;
    GstElement* m_pipeline;
};

} // namespace WebCore

#endif // AudioDestinationGStreamer_h
