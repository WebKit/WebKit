/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO)

#include "AudioBasicProcessorNode.h"
#include "OverSampleType.h"
#include "WaveShaperOptions.h"
#include "WaveShaperProcessor.h"
#include <wtf/Forward.h>

namespace WebCore {

class WaveShaperNode final : public AudioBasicProcessorNode {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WaveShaperNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WaveShaperNode);
public:
    static ExceptionOr<Ref<WaveShaperNode>> create(BaseAudioContext&, const WaveShaperOptions& = { });

    // setCurve() is called on the main thread.
    ExceptionOr<void> setCurveForBindings(RefPtr<Float32Array>&&);
    RefPtr<Float32Array> curveForBindings();

    void setOversampleForBindings(OverSampleType);
    OverSampleType oversampleForBindings() const;

    double latency() const { return latencyTime(); }

private:    
    explicit WaveShaperNode(BaseAudioContext&);

    bool propagatesSilence() const final;

    WaveShaperProcessor* waveShaperProcessor() { return downcast<WaveShaperProcessor>(processor()); }
    const WaveShaperProcessor* waveShaperProcessor() const { return downcast<WaveShaperProcessor>(processor()); }
};

String convertEnumerationToString(WebCore::OverSampleType); // in JSOverSampleType.cpp

} // namespace WebCore

namespace WTF {
    
template<> struct LogArgument<WebCore::OverSampleType> {
    static String toString(WebCore::OverSampleType type) { return convertEnumerationToString(type); }
};
    
} // namespace WTF

SPECIALIZE_TYPE_TRAITS_AUDIONODE(WaveShaperNode, NodeTypeWaveShaper);

#endif // ENABLE(WEB_AUDIO)
