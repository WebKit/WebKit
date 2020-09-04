/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "AudioScheduledSourceNode.h"
#include "OscillatorOptions.h"
#include "OscillatorType.h"
#include <wtf/Lock.h>

namespace WebCore {

class PeriodicWave;

// OscillatorNode is an audio generator of periodic waveforms.

class OscillatorNode : public AudioScheduledSourceNode {
    WTF_MAKE_ISO_ALLOCATED(OscillatorNode);
public:
    static ExceptionOr<Ref<OscillatorNode>> create(BaseAudioContext&, const OscillatorOptions& = { });

    virtual ~OscillatorNode();

    const char* activeDOMObjectName() const override { return "OscillatorNode"; }

    OscillatorType type() const { return m_type; }
    ExceptionOr<void> setType(OscillatorType);

    AudioParam* frequency() { return m_frequency.get(); }
    AudioParam* detune() { return m_detune.get(); }

    void setPeriodicWave(PeriodicWave&);

protected:
    explicit OscillatorNode(BaseAudioContext&, const OscillatorOptions& = { });

private:
    void process(size_t framesToProcess) final;
    void reset() final;

    double tailTime() const final { return 0; }
    double latencyTime() const final { return 0; }

    // Returns true if there are sample-accurate timeline parameter changes.
    bool calculateSampleAccuratePhaseIncrements(size_t framesToProcess);

    bool propagatesSilence() const final;

    // One of the waveform types defined in the enum.
    OscillatorType m_type;
    
    // Frequency value in Hertz.
    RefPtr<AudioParam> m_frequency;

    // Detune value (deviating from the frequency) in Cents.
    RefPtr<AudioParam> m_detune;

    bool m_firstRender { true };

    // m_virtualReadIndex is a sample-frame index into our buffer representing the current playback position.
    // Since it's floating-point, it has sub-sample accuracy.
    double m_virtualReadIndex { 0 };

    // This synchronizes process().
    mutable Lock m_processMutex;

    // Stores sample-accurate values calculated according to frequency and detune.
    AudioFloatArray m_phaseIncrements { AudioNode::ProcessingSizeInFrames };
    AudioFloatArray m_detuneValues { AudioNode::ProcessingSizeInFrames };
    
    RefPtr<PeriodicWave> m_periodicWave;
};

String convertEnumerationToString(OscillatorType); // In JSOscillatorNode.cpp

} // namespace WebCore

namespace WTF {

template<> struct LogArgument<WebCore::OscillatorType> {
    static String toString(WebCore::OscillatorType type) { return convertEnumerationToString(type); }
};

} // namespace WTF
