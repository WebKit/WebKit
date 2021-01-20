/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "AudioArray.h"
#include <JavaScriptCore/Float32Array.h>
#include <JavaScriptCore/Uint8Array.h>
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class AudioBus;
class FFTFrame;

class RealtimeAnalyser {
    WTF_MAKE_NONCOPYABLE(RealtimeAnalyser);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RealtimeAnalyser();
    virtual ~RealtimeAnalyser();

    size_t fftSize() const { return m_fftSize; }
    bool setFftSize(size_t);

    unsigned frequencyBinCount() const { return m_fftSize / 2; }

    void setMinDecibels(double k) { m_minDecibels = k; }
    double minDecibels() const { return m_minDecibels; }

    void setMaxDecibels(double k) { m_maxDecibels = k; }
    double maxDecibels() const { return m_maxDecibels; }

    void setSmoothingTimeConstant(double k) { m_smoothingTimeConstant = k; }
    double smoothingTimeConstant() const { return m_smoothingTimeConstant; }

    void getFloatFrequencyData(JSC::Float32Array&);
    void getByteFrequencyData(JSC::Uint8Array&);
    void getFloatTimeDomainData(JSC::Float32Array&);
    void getByteTimeDomainData(JSC::Uint8Array&);

    // The audio thread writes input data here.
    void writeInput(AudioBus*, size_t framesToProcess);

    static constexpr double DefaultSmoothingTimeConstant { 0.8 };
    static constexpr double DefaultMinDecibels { -100 };
    static constexpr double DefaultMaxDecibels { -30 };

    // All FFT implementations are expected to handle power-of-two sizes MinFFTSize <= size <= MaxFFTSize.
    static constexpr unsigned DefaultFFTSize { 2048 };
    static constexpr unsigned MinFFTSize { 32 };
    static constexpr unsigned MaxFFTSize { 32768 };
    static constexpr unsigned InputBufferSize { MaxFFTSize * 2 };

private:
    // The audio thread writes the input audio here.
    AudioFloatArray m_inputBuffer;
    unsigned m_writeIndex { 0 };

    // AudioBus used for downmixing input audio before copying it to m_inputBuffer.
    RefPtr<AudioBus> m_downmixBus;
    
    size_t m_fftSize { DefaultFFTSize };
    std::unique_ptr<FFTFrame> m_analysisFrame;
    void doFFTAnalysisIfNecessary();
    
    // doFFTAnalysisIfNecessary() stores the floating-point magnitude analysis data here.
    AudioFloatArray m_magnitudeBuffer { DefaultFFTSize / 2 };
    AudioFloatArray& magnitudeBuffer() { return m_magnitudeBuffer; }

    // A value between 0 and 1 which averages the previous version of m_magnitudeBuffer with the current analysis magnitude data.
    double m_smoothingTimeConstant { DefaultSmoothingTimeConstant };

    // The range used when converting when using getByteFrequencyData(). 
    double m_minDecibels { DefaultMinDecibels };
    double m_maxDecibels { DefaultMaxDecibels };

    // We should only do the FFT analysis once per render quantum.
    bool m_shouldDoFFTAnalysis { true };
};

} // namespace WebCore
