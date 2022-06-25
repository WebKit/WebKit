/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef Reverb_h
#define Reverb_h

#include "ReverbConvolver.h"
#include <wtf/Vector.h>

namespace WebCore {

class AudioBus;
    
// Multi-channel convolution reverb with channel matrixing - one or more ReverbConvolver objects are used internally.

class Reverb final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum { MaxFrameSize = 256 };

    // renderSliceSize is a rendering hint, so the FFTs can be optimized to not all occur at the same time (very bad when rendering on a real-time thread).
    Reverb(AudioBus* impulseResponseBuffer, size_t renderSliceSize, size_t maxFFTSize, bool useBackgroundThreads, bool normalize);

    void process(const AudioBus* sourceBus, AudioBus* destinationBus, size_t framesToProcess);
    void reset();

    size_t impulseResponseLength() const { return m_impulseResponseLength; }
    size_t latencyFrames() const;

private:
    void initialize(AudioBus* impulseResponseBuffer, size_t renderSliceSize, size_t maxFFTSize, bool useBackgroundThreads, float scale);

    size_t m_impulseResponseLength;

    // The actual number of channels in the response. This can be less
    // than the number of ReverbConvolver's in |m_convolvers|.
    unsigned m_numberOfResponseChannels { 0 };
    Vector<std::unique_ptr<ReverbConvolver>> m_convolvers;

    // For "True" stereo processing
    RefPtr<AudioBus> m_tempBuffer;
};

} // namespace WebCore

#endif // Reverb_h
