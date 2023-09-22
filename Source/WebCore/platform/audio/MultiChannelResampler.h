/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MultiChannelResampler_h
#define MultiChannelResampler_h

#include "AudioArray.h"
#include <memory>
#include <wtf/Function.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioBus;
class SincResampler;

class MultiChannelResampler final {
    WTF_MAKE_FAST_ALLOCATED;
public:   
    // requestFrames constrols the size of the buffer in frames when provideInput is called.
    MultiChannelResampler(double scaleFactor, unsigned numberOfChannels, unsigned requestFrames, Function<void(AudioBus*, size_t framesToProcess)>&& provideInput);
    ~MultiChannelResampler();

    void process(AudioBus* destination, size_t framesToProcess);

private:
    void provideInputForChannel(std::span<float> buffer, size_t framesToProcess, unsigned channelIndex);

    // FIXME: the mac port can have a more highly optimized implementation based on CoreAudio
    // instead of SincResampler. For now the default implementation will be used on all ports.
    // https://bugs.webkit.org/show_bug.cgi?id=75118
    
    // Each channel will be resampled using a high-quality SincResampler.
    Vector<std::unique_ptr<SincResampler>> m_kernels;
    
    unsigned m_numberOfChannels;
    size_t m_outputFramesReady { 0 };
    Function<void(AudioBus*, size_t framesToProcess)> m_provideInput;
    RefPtr<AudioBus> m_multiChannelBus;
    Vector<std::unique_ptr<AudioFloatArray>> m_channelsMemory;
};

} // namespace WebCore

#endif // MultiChannelResampler_h
