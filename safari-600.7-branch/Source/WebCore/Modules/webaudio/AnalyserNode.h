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

#ifndef AnalyserNode_h
#define AnalyserNode_h

#include "AudioBasicInspectorNode.h"
#include "RealtimeAnalyser.h"
#include <wtf/Forward.h>

namespace WebCore {

class AnalyserNode : public AudioBasicInspectorNode {
public:
    static PassRefPtr<AnalyserNode> create(AudioContext* context, float sampleRate)
    {
        return adoptRef(new AnalyserNode(context, sampleRate));      
    }

    virtual ~AnalyserNode();
    
    // AudioNode
    virtual void process(size_t framesToProcess) override;
    virtual void reset() override;

    // Javascript bindings
    unsigned fftSize() const { return m_analyser.fftSize(); }
    void setFftSize(unsigned size, ExceptionCode&);

    unsigned frequencyBinCount() const { return m_analyser.frequencyBinCount(); }

    void setMinDecibels(double k, ExceptionCode&);
    double minDecibels() const { return m_analyser.minDecibels(); }

    void setMaxDecibels(double k, ExceptionCode&);
    double maxDecibels() const { return m_analyser.maxDecibels(); }

    void setSmoothingTimeConstant(double k, ExceptionCode&);
    double smoothingTimeConstant() const { return m_analyser.smoothingTimeConstant(); }

    void getFloatFrequencyData(JSC::Float32Array* array) { m_analyser.getFloatFrequencyData(array); }
    void getByteFrequencyData(JSC::Uint8Array* array) { m_analyser.getByteFrequencyData(array); }
    void getByteTimeDomainData(JSC::Uint8Array* array) { m_analyser.getByteTimeDomainData(array); }

private:
    virtual double tailTime() const override { return 0; }
    virtual double latencyTime() const override { return 0; }

    AnalyserNode(AudioContext*, float sampleRate);

    RealtimeAnalyser m_analyser;
};

} // namespace WebCore

#endif // AnalyserNode_h
