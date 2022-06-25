/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Panner_h
#define Panner_h

#include <memory>

namespace WebCore {

class AudioBus;
class HRTFDatabaseLoader;

enum class PanningModelType {
    Equalpower,
    HRTF
};

// Abstract base class for panning a mono or stereo source.

class Panner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<Panner> create(PanningModelType, float sampleRate, HRTFDatabaseLoader*);

    virtual ~Panner() { };

    PanningModelType panningModel() const { return m_panningModel; }

    virtual void pan(double azimuth, double elevation, const AudioBus* inputBus, AudioBus* outputBus, size_t framesToProcess) = 0;
    virtual void panWithSampleAccurateValues(double* azimuth, double* elevation, const AudioBus* inputBus, AudioBus* outputBus, size_t framesToProcess) = 0;
    virtual void reset() = 0;

    virtual double tailTime() const = 0;
    virtual double latencyTime() const = 0;
    virtual bool requiresTailProcessing() const = 0;

protected:
    Panner(PanningModelType model)
        : m_panningModel(model)
    {
    }

    PanningModelType m_panningModel;
};

} // namespace WebCore

#endif // Panner_h
