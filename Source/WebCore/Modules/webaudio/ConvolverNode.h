/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "ConvolverOptions.h"
#include <wtf/Lock.h>

namespace WebCore {

class AudioBuffer;
class Reverb;
    
class ConvolverNode final : public AudioNode {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ConvolverNode);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ConvolverNode);
public:
    static ExceptionOr<Ref<ConvolverNode>> create(BaseAudioContext&, ConvolverOptions&& = { });
    
    virtual ~ConvolverNode();
    
    ExceptionOr<void> setBufferForBindings(RefPtr<AudioBuffer>&&);
    AudioBuffer* bufferForBindings(); // Only safe to call on the main thread.

    bool normalizeForBindings() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS { ASSERT(isMainThread()); return m_normalize; }
    void setNormalizeForBindings(bool);

    ExceptionOr<void> setChannelCount(unsigned) final;
    ExceptionOr<void> setChannelCountMode(ChannelCountMode) final;

private:
    explicit ConvolverNode(BaseAudioContext&);

    double tailTime() const final;
    double latencyTime() const final;
    bool requiresTailProcessing() const final;

    void process(size_t framesToProcess) final;
    void checkNumberOfChannelsForInput(AudioNodeInput*) final;

    std::unique_ptr<Reverb> m_reverb WTF_GUARDED_BY_LOCK(m_processLock); // Only modified on the main thread but accessed on the audio thread.
    RefPtr<AudioBuffer> m_buffer WTF_GUARDED_BY_LOCK(m_processLock); // Only modified on the main thread but accessed on the audio thread.

    // This synchronizes dynamic changes to the convolution impulse response with process().
    mutable Lock m_processLock;

    // Normalize the impulse response or not.
    bool m_normalize { true }; // Only used on the main thread.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_AUDIONODE(ConvolverNode, NodeTypeConvolver);
