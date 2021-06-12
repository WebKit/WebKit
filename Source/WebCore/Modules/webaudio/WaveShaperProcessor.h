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

#include "AudioDSPKernel.h"
#include "AudioDSPKernelProcessor.h"
#include "AudioNode.h"
#include <JavaScriptCore/Float32Array.h>
#include <memory>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>

namespace WebCore {

// WaveShaperProcessor is an AudioDSPKernelProcessor which uses WaveShaperDSPKernel objects to implement non-linear distortion effects.

class WaveShaperProcessor final : public AudioDSPKernelProcessor {
public:
    enum OverSampleType {
        OverSampleNone,
        OverSample2x,
        OverSample4x
    };

    WaveShaperProcessor(float sampleRate, size_t numberOfChannels);

    virtual ~WaveShaperProcessor();

    std::unique_ptr<AudioDSPKernel> createKernel() final;

    void process(const AudioBus* source, AudioBus* destination, size_t framesToProcess) final;

    void setCurveForBindings(Float32Array*);
    Float32Array* curveForBindings() WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_curve.get(); } // Doesn't grab the lock, only safe to call on the main thread.
    Float32Array* curve() const WTF_REQUIRES_LOCK(m_processLock) { return m_curve.get(); }

    void setOversampleForBindings(OverSampleType);
    OverSampleType oversampleForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_oversample; } // Doesn't grab the lock, only safe to call on the main thread.
    OverSampleType oversample() const WTF_REQUIRES_LOCK(m_processLock) { return m_oversample; }

    Lock& processLock() const WTF_RETURNS_LOCK(m_processLock) { return m_processLock; }

private:
    // m_curve represents the non-linear shaping curve.
    RefPtr<Float32Array> m_curve WTF_GUARDED_BY_LOCK(m_processLock);

    OverSampleType m_oversample WTF_GUARDED_BY_LOCK(m_processLock) { OverSampleNone };

    // This synchronizes process() with setCurve().
    mutable Lock m_processLock;
};

} // namespace WebCore
