/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "AudioBasicProcessorNode.h"
#include "BiquadFilterOptions.h"
#include "BiquadProcessor.h"

namespace WebCore {

class AudioParam;

class BiquadFilterNode final : public AudioBasicProcessorNode {
    WTF_MAKE_ISO_ALLOCATED(BiquadFilterNode);
public:
    static ExceptionOr<Ref<BiquadFilterNode>> create(BaseAudioContext& context, const BiquadFilterOptions& = { });

    BiquadFilterType type() const;
    void setType(BiquadFilterType);

    AudioParam& frequency() { return biquadProcessor()->parameter1(); }
    AudioParam& q() { return biquadProcessor()->parameter2(); }
    AudioParam& gain() { return biquadProcessor()->parameter3(); }
    AudioParam& detune() { return biquadProcessor()->parameter4(); }

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    ExceptionOr<void> getFrequencyResponse(const Ref<Float32Array>& frequencyHz, const Ref<Float32Array>& magResponse, const Ref<Float32Array>& phaseResponse);

private:
    explicit BiquadFilterNode(BaseAudioContext&);

    BiquadProcessor* biquadProcessor() { return static_cast<BiquadProcessor*>(processor()); }
};

} // namespace WebCore
