/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "DynamicsCompressorOptions.h"
#include <memory>

namespace WebCore {

class DynamicsCompressor;

class DynamicsCompressorNode final : public AudioNode {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(DynamicsCompressorNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DynamicsCompressorNode);
public:
    static ExceptionOr<Ref<DynamicsCompressorNode>> create(BaseAudioContext&, const DynamicsCompressorOptions& = { });

    ~DynamicsCompressorNode();

    // AudioNode
    void process(size_t framesToProcess) final;
    void processOnlyAudioParams(size_t framesToProcess) final;
    void initialize() final;
    void uninitialize() final;

    // Static compression curve parameters.
    AudioParam& threshold() { return m_threshold.get(); }
    AudioParam& knee() { return m_knee.get(); }
    AudioParam& ratio() { return m_ratio.get(); }
    AudioParam& attack() { return m_attack.get(); }
    AudioParam& release() { return m_release.get(); }

    // Amount by which the compressor is currently compressing the signal in decibels.
    float reduction() const { return m_reduction; }

    ExceptionOr<void> setChannelCount(unsigned) final;
    ExceptionOr<void> setChannelCountMode(ChannelCountMode) final;

protected:
    explicit DynamicsCompressorNode(BaseAudioContext&, const DynamicsCompressorOptions& = { });
    void setReduction(float reduction) { m_reduction = reduction; }

private:
    double tailTime() const final;
    double latencyTime() const final;
    bool requiresTailProcessing() const final;

    float noiseInjectionMultiplier() const final { return 0.01; }

    std::unique_ptr<DynamicsCompressor> m_dynamicsCompressor;
    const Ref<AudioParam> m_threshold;
    const Ref<AudioParam> m_knee;
    const Ref<AudioParam> m_ratio;
    const Ref<AudioParam> m_attack;
    const Ref<AudioParam> m_release;
    float m_reduction { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_AUDIONODE(DynamicsCompressorNode, NodeTypeDynamicsCompressor);
