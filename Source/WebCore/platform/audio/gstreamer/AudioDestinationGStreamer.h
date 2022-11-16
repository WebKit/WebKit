/*
 *  Copyright (C) 2011, 2012 Igalia S.L
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

#pragma once

#include "AudioBus.h"
#include "AudioDestination.h"
#include "GRefPtrGStreamer.h"
#include <wtf/Condition.h>
#include <wtf/Forward.h>

namespace WebCore {

class AudioDestinationGStreamer : public AudioDestination {
public:
    AudioDestinationGStreamer(AudioIOCallback&, unsigned long numberOfOutputChannels, float sampleRate);
    virtual ~AudioDestinationGStreamer();

    WEBCORE_EXPORT void start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&&) final;
    WEBCORE_EXPORT void stop(CompletionHandler<void(bool)>&&) final;

    bool isPlaying() override { return m_isPlaying; }
    unsigned framesPerBuffer() const final;

    bool handleMessage(GstMessage*);
    void notifyIsPlaying(bool);

protected:
    virtual void startRendering(CompletionHandler<void(bool)>&&);
    virtual void stopRendering(CompletionHandler<void(bool)>&&);

private:
    void notifyStartupResult(bool);
    void notifyStopResult(bool);

    RefPtr<AudioBus> m_renderBus;

    bool m_isPlaying { false };
    bool m_audioSinkAvailable { false };
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_src;
    CompletionHandler<void(bool)> m_startupCompletionHandler;
    CompletionHandler<void(bool)> m_stopCompletionHandler;
};

} // namespace WebCore
