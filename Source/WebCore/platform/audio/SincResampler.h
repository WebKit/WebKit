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

#ifndef SincResampler_h
#define SincResampler_h

#include "AudioArray.h"
#include "AudioSourceProvider.h"
#include <wtf/RefPtr.h>

namespace WebCore {

// SincResampler is a high-quality sample-rate converter.

class SincResampler final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // scaleFactor == sourceSampleRate / destinationSampleRate
    // requestFrames controls the size in frames of the buffer requested by each provideInput() call.
    SincResampler(double scaleFactor, Optional<unsigned> requestFrames = WTF::nullopt);
    
    // Processes numberOfSourceFrames from source to produce numberOfSourceFrames / scaleFactor frames in destination.
    void process(const float* source, float* destination, unsigned numberOfSourceFrames);

    // Process with input source callback function for streaming applications.
    void process(AudioSourceProvider*, float* destination, size_t framesToProcess);

protected:
    void initializeKernel();
    void consumeSource(float* buffer, unsigned numberOfSourceFrames);
    void updateRegions(bool isSecondLoad);
    
    double m_scaleFactor;

    // m_kernelStorage has m_numberOfKernelOffsets kernels back-to-back, each of size m_kernelSize.
    // The kernel offsets are sub-sample shifts of a windowed sinc() shifted from 0.0 to 1.0 sample.
    AudioFloatArray m_kernelStorage;
    
    // m_virtualSourceIndex is an index on the source input buffer with sub-sample precision.
    // It must be double precision to avoid drift.
    double m_virtualSourceIndex { 0 };
    
    // This is the number of destination frames we generate per processing pass on the buffer.
    unsigned m_requestFrames;

    // The number of source frames processed per pass.
    unsigned m_blockSize { 0 };

    // Source is copied into this buffer for each processing pass.
    AudioFloatArray m_inputBuffer;

    // Pointers to the various regions inside |m_inputBuffer|. See the diagram at
    // the top of the .cpp file for more information.
    float* m_r0 { nullptr };
    float* const m_r1 { nullptr };
    float* const m_r2 { nullptr };
    float* m_r3 { nullptr };
    float* m_r4 { nullptr };

    const float* m_source { nullptr };
    unsigned m_sourceFramesAvailable { 0 };
    
    // m_sourceProvider is used to provide the audio input stream to the resampler.
    AudioSourceProvider* m_sourceProvider { nullptr };

    // The buffer is primed once at the very beginning of processing.
    bool m_isBufferPrimed { false };

    RefPtr<AudioBus> m_internalBus;
};

} // namespace WebCore

#endif // SincResampler_h
