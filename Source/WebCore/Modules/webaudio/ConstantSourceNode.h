/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioScheduledSourceNode.h"
#include "ConstantSourceOptions.h"
#include <wtf/Lock.h>

namespace WebCore {

class ConstantSourceNode final : public AudioScheduledSourceNode {
    WTF_MAKE_ISO_ALLOCATED(ConstantSourceNode);
public:
    static ExceptionOr<Ref<ConstantSourceNode>> create(BaseAudioContext&, const ConstantSourceOptions& = { });
    
    virtual ~ConstantSourceNode();
    
    AudioParam& offset() { return m_offset.get(); }
    
private:
    void process(size_t framesToProcess) final;

    ConstantSourceNode(BaseAudioContext&, float);
    
    double tailTime() const final { return 0; }
    double latencyTime() const final { return 0; }
    
    // If we are no longer playing, propogate silence ahead to downstream nodes.
    bool propagatesSilence() const final;
    
    const char* activeDOMObjectName() const override { return "ConstantSourceNode"; }
    
    Ref<AudioParam> m_offset;
    
    // Stores sample-accurate values calculated.
    AudioFloatArray m_sampleAccurateValues;
};

} // namespace WebCore
