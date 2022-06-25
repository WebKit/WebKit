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

#include "AudioNode.h"
#include "AudioParam.h"
#include "GainOptions.h"
#include <wtf/Threading.h>

namespace WebCore {

class AudioContext;

// GainNode is an AudioNode with one input and one output which applies a gain (volume) change to the audio signal.
// De-zippering (smoothing) is applied when the gain value is changed dynamically.

class GainNode final : public AudioNode {
    WTF_MAKE_ISO_ALLOCATED(GainNode);
public:
    static ExceptionOr<Ref<GainNode>> create(BaseAudioContext& context, const GainOptions& = { });

    // AudioNode
    void process(size_t framesToProcess) override;
    void processOnlyAudioParams(size_t framesToProcess) final;

    // Called in the main thread when the number of channels for the input may have changed.
    void checkNumberOfChannelsForInput(AudioNodeInput*) override;

    // JavaScript interface
    AudioParam& gain() { return m_gain.get(); }

private:
    double tailTime() const override { return 0; }
    double latencyTime() const override { return 0; }
    bool requiresTailProcessing() const final { return false; }

    explicit GainNode(BaseAudioContext&);

    AudioFloatArray m_sampleAccurateGainValues;
    Ref<AudioParam> m_gain;
};

} // namespace WebCore
