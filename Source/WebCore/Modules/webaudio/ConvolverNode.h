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
#include <wtf/Lock.h>

namespace WebCore {

class AudioBuffer;
class Reverb;
    
class ConvolverNode final : public AudioNode {
    WTF_MAKE_ISO_ALLOCATED(ConvolverNode);
public:
    static Ref<ConvolverNode> create(AudioContextBase& context, float sampleRate)
    {
        return adoptRef(*new ConvolverNode(context, sampleRate));
    }
    
    virtual ~ConvolverNode();
    
    ExceptionOr<void> setBuffer(AudioBuffer*);
    AudioBuffer* buffer();

    bool normalize() const { return m_normalize; }
    void setNormalize(bool normalize) { m_normalize = normalize; }

private:
    ConvolverNode(AudioContextBase&, float sampleRate);

    double tailTime() const final;
    double latencyTime() const final;

    void process(size_t framesToProcess) final;
    void reset() final;
    void initialize() final;
    void uninitialize() final;

    std::unique_ptr<Reverb> m_reverb;
    RefPtr<AudioBuffer> m_buffer;

    // This synchronizes dynamic changes to the convolution impulse response with process().
    mutable Lock m_processMutex;

    // Normalize the impulse response or not.
    bool m_normalize { true };
};

} // namespace WebCore
