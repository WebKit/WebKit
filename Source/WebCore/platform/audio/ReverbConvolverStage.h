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

#ifndef ReverbConvolverStage_h
#define ReverbConvolverStage_h

#include "AudioArray.h"
#include <memory>

namespace WebCore {

class FFTFrame;
class ReverbAccumulationBuffer;
class ReverbConvolver;
class FFTConvolver;
class DirectConvolver;
    
// A ReverbConvolverStage represents the convolution associated with a sub-section of a large impulse response.
// It incorporates a delay line to account for the offset of the sub-section within the larger impulse response.
class ReverbConvolverStage final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // renderPhase is useful to know so that we can manipulate the pre versus post delay so that stages will perform
    // their heavy work (FFT processing) on different slices to balance the load in a real-time thread.
    ReverbConvolverStage(const float* impulseResponse, size_t responseLength, size_t reverbTotalLatency, size_t stageOffset, size_t stageLength, size_t fftSize, size_t renderPhase, size_t renderSliceSize, ReverbAccumulationBuffer*, float scale, bool directMode = false);
    ~ReverbConvolverStage();

    // WARNING: framesToProcess must be such that it evenly divides the delay buffer size (stage_offset).
    void process(const float* source, size_t framesToProcess);

    void processInBackground(ReverbConvolver* convolver, size_t framesToProcess);

    void reset();

    // Useful for background processing
    int inputReadIndex() const { return m_inputReadIndex; }

private:
    std::unique_ptr<FFTFrame> m_fftKernel;
    std::unique_ptr<FFTConvolver> m_fftConvolver;

    AudioFloatArray m_preDelayBuffer;

    ReverbAccumulationBuffer* m_accumulationBuffer;
    int m_accumulationReadIndex { 0 };
    int m_inputReadIndex { 0 };

    size_t m_preDelayLength;
    size_t m_postDelayLength;
    size_t m_preReadWriteIndex;
    size_t m_framesProcessed;

    AudioFloatArray m_temporaryBuffer;

    bool m_directMode;
    std::unique_ptr<AudioFloatArray> m_directKernel;
    std::unique_ptr<DirectConvolver> m_directConvolver;
};

} // namespace WebCore

#endif // ReverbConvolverStage_h
