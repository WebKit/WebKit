/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2020-2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioNode.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

class AudioBus;
struct AudioIOPosition;

class AudioDestinationNode : public AudioNode {
    WTF_MAKE_ISO_ALLOCATED(AudioDestinationNode);
public:
    ~AudioDestinationNode();
    
    // AudioNode   
    void process(size_t) final { } // we're pulled by hardware so this is never called

    float sampleRate() const final { return m_sampleRate; }

    size_t currentSampleFrame() const { return m_currentSampleFrame; }
    double currentTime() const { return currentSampleFrame() / static_cast<double>(sampleRate()); }

    virtual unsigned maxChannelCount() const = 0;

    // Enable local/live input for the specified device.
    virtual void enableInput(const String& inputDeviceId) = 0;

    virtual void startRendering(CompletionHandler<void(std::optional<Exception>&&)>&&) = 0;
    virtual void restartRendering() { }

    // AudioDestinationNodes are owned by the BaseAudioContext so we forward the refcounting to its BaseAudioContext.
    void ref() final;
    void deref() final;

protected:
    AudioDestinationNode(BaseAudioContext&, float sampleRate);

    double tailTime() const final { return 0; }
    double latencyTime() const final { return 0; }

    void renderQuantum(AudioBus* destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition);

private:
    // Counts the number of sample-frames processed by the destination.
    std::atomic<size_t> m_currentSampleFrame { 0 };
    float m_sampleRate;
};

} // namespace WebCore
