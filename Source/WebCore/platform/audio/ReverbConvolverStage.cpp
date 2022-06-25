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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "ReverbConvolverStage.h"

#include "DirectConvolver.h"
#include "FFTConvolver.h"
#include "FFTFrame.h"
#include "VectorMath.h"
#include "ReverbAccumulationBuffer.h"
#include "ReverbConvolver.h"
#include "ReverbInputBuffer.h"

namespace WebCore {

ReverbConvolverStage::ReverbConvolverStage(const float* impulseResponse, size_t, size_t reverbTotalLatency, size_t stageOffset, size_t stageLength,
    size_t fftSize, size_t renderPhase, size_t renderSliceSize, ReverbAccumulationBuffer* accumulationBuffer, float scale, bool directMode)
    : m_accumulationBuffer(accumulationBuffer)
    , m_directMode(directMode)
{
    ASSERT(impulseResponse);
    ASSERT(accumulationBuffer);

    if (!m_directMode) {
        m_fftKernel = makeUnique<FFTFrame>(fftSize);
        m_fftKernel->doPaddedFFT(impulseResponse + stageOffset, stageLength);
        // Account for the normalization (if any) of the convolver. By linearity,
        // we can scale the FFT by the factor instead of the input. We do it this
        // way so we don't need to create a temporary for the scaled result before
        // computing the FFT.
        if (scale != 1)
            m_fftKernel->scaleFFT(scale);
        m_fftConvolver = makeUnique<FFTConvolver>(fftSize);
    } else {
        ASSERT(!stageOffset);
        ASSERT(stageLength <= fftSize / 2);

        m_directKernel = makeUnique<AudioFloatArray>(fftSize / 2);
        m_directKernel->copyToRange(impulseResponse, 0, stageLength);
        // Account for the normalization (if any) of the convolver node.
        if (scale != 1)
            VectorMath::multiplyByScalar(m_directKernel->data(), scale, m_directKernel->data(), stageLength);
        m_directConvolver = makeUnique<DirectConvolver>(renderSliceSize);
    }
    m_temporaryBuffer.resize(renderSliceSize);

    // The convolution stage at offset stageOffset needs to have a corresponding delay to cancel out the offset.
    size_t totalDelay = stageOffset + reverbTotalLatency;

    // But, the FFT convolution itself incurs fftSize / 2 latency, so subtract this out...
    size_t halfSize = fftSize / 2;
    if (!m_directMode) {
        ASSERT(totalDelay >= halfSize);
        if (totalDelay >= halfSize)
            totalDelay -= halfSize;
    }

    // We divide up the total delay, into pre and post delay sections so that we can schedule at exactly the moment when the FFT will happen.
    // This is coordinated with the other stages, so they don't all do their FFTs at the same time...
    int maxPreDelayLength = std::min(halfSize, totalDelay);
    m_preDelayLength = totalDelay > 0 ? renderPhase % maxPreDelayLength : 0;
    if (m_preDelayLength > totalDelay)
        m_preDelayLength = 0;

    m_postDelayLength = totalDelay - m_preDelayLength;
    m_preReadWriteIndex = 0;
    m_framesProcessed = 0; // total frames processed so far

    size_t delayBufferSize = m_preDelayLength < fftSize ? fftSize : m_preDelayLength;
    delayBufferSize = delayBufferSize < renderSliceSize ? renderSliceSize : delayBufferSize;
    m_preDelayBuffer.resize(delayBufferSize);
}

ReverbConvolverStage::~ReverbConvolverStage() = default;

void ReverbConvolverStage::processInBackground(ReverbConvolver* convolver, size_t framesToProcess)
{
    ReverbInputBuffer* inputBuffer = convolver->inputBuffer();
    float* source = inputBuffer->directReadFrom(&m_inputReadIndex, framesToProcess);
    process(source, framesToProcess);
}

void ReverbConvolverStage::process(const float* source, size_t framesToProcess)
{
    ASSERT(source);
    if (!source)
        return;
    
    // Deal with pre-delay stream : note special handling of zero delay.

    const float* preDelayedSource;
    float* preDelayedDestination;
    float* temporaryBuffer;
    bool isTemporaryBufferSafe = false;
    if (m_preDelayLength > 0) {
        // Handles both the read case (call to process() ) and the write case (memcpy() )
        bool isPreDelaySafe = m_preReadWriteIndex + framesToProcess <= m_preDelayBuffer.size();
        ASSERT(isPreDelaySafe);
        if (!isPreDelaySafe)
            return;

        isTemporaryBufferSafe = framesToProcess <= m_temporaryBuffer.size();

        preDelayedDestination = m_preDelayBuffer.data() + m_preReadWriteIndex;
        preDelayedSource = preDelayedDestination;
        temporaryBuffer = m_temporaryBuffer.data();        
    } else {
        // Zero delay
        preDelayedDestination = 0;
        preDelayedSource = source;
        temporaryBuffer = m_preDelayBuffer.data();
        
        isTemporaryBufferSafe = framesToProcess <= m_preDelayBuffer.size();
    }
    
    ASSERT(isTemporaryBufferSafe);
    if (!isTemporaryBufferSafe)
        return;

    if (m_framesProcessed < m_preDelayLength) {
        // For the first m_preDelayLength frames don't process the convolver, instead simply buffer in the pre-delay.
        // But while buffering the pre-delay, we still need to update our index.
        m_accumulationBuffer->updateReadIndex(&m_accumulationReadIndex, framesToProcess);
    } else {
        // Now, run the convolution (into the delay buffer).
        // An expensive FFT will happen every fftSize / 2 frames.
        // We process in-place here...
        if (!m_directMode)
            m_fftConvolver->process(m_fftKernel.get(), preDelayedSource, temporaryBuffer, framesToProcess);
        else
            m_directConvolver->process(m_directKernel.get(), preDelayedSource, temporaryBuffer, framesToProcess);

        // Now accumulate into reverb's accumulation buffer.
        m_accumulationBuffer->accumulate(temporaryBuffer, framesToProcess, &m_accumulationReadIndex, m_postDelayLength);
    }

    // Finally copy input to pre-delay.
    if (m_preDelayLength > 0) {
        memcpy(preDelayedDestination, source, sizeof(float) * framesToProcess);
        m_preReadWriteIndex += framesToProcess;

        ASSERT(m_preReadWriteIndex <= m_preDelayLength);
        if (m_preReadWriteIndex >= m_preDelayLength)
            m_preReadWriteIndex = 0;
    }

    m_framesProcessed += framesToProcess;
}

void ReverbConvolverStage::reset()
{
    if (!m_directMode)
        m_fftConvolver->reset();
    else
        m_directConvolver->reset();
    m_preDelayBuffer.zero();
    m_accumulationReadIndex = 0;
    m_inputReadIndex = 0;
    m_framesProcessed = 0;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
