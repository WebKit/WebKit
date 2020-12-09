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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "SincResampler.h"

#include "AudioBus.h"
#include "AudioUtilities.h"
#include <wtf/MathExtras.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#elif CPU(X86_SSE2)
#include <xmmintrin.h>
#elif HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

// Initial input buffer layout, dividing into regions r0 to r4 (note: r0, r3
// and r4 will move after the first load):
//
// |----------------|----------------------------------------------------------------|----------------|
//
//                                              m_requestFrames
//                   <-------------------------------------------------------------------------------->
//                                           r0 (during first load)
//
//   kernelSize / 2   kernelSize / 2                                 kernelSize / 2     kernelSize / 2 
// <---------------> <--------------->                              <---------------> <--------------->
//         r1                r2                                             r3                r4
// 
//                             m_blockSize == r4 - r2
//                   <--------------------------------------->
//
//                                                  m_requestFrames
//                                    <------------------ ... ----------------->
//                                               r0 (during second load)
//
// On the second request r0 slides to the right by kernelSize / 2 and r3, r4
// and m_blockSize are reinitialized via step (3) in the algorithm below.
//
// These new regions remain constant until a Flush() occurs. While complicated,
// this allows us to reduce jitter by always requesting the same amount from the
// provided callback.

// The Algorithm:
//
// 1) Allocate input_buffer of size: m_requestFrames + kernelSize; this ensures
//    there's enough room to read m_requestFrames from the callback into region
//    r0 (which will move between the first and subsequent passes).
//
// 2) Let r1, r2 each represent half the kernel centered around r0:
//
//        r0 = m_inputBuffer + kernelSize / 2
//        r1 = m_inputBuffer
//        r2 = r0
//
//    r0 is always m_requestFrames in size. r1, r2 are kernelSize / 2 in
//    size. r1 must be zero initialized to avoid convolution with garbage (see
//    step (5) for why).
//
// 3) Let r3, r4 each represent half the kernel right aligned with the end of
//    r0 and choose m_blockSize as the distance in frames between r4 and r2:
//
//        r3 = r0 + m_requestFrames - kernelSize
//        r4 = r0 + m_requestFrames - kernelSize / 2
//        m_blockSize = r4 - r2 = m_requestFrames - kernelSize / 2
//
// 4) Consume m_requestFrames frames into r0.
//
// 5) Position kernel centered at start of r2 and generate output frames until
//    the kernel is centered at the start of r4 or we've finished generating
//    all the output frames.
//
// 6) Wrap left over data from the r3 to r1 and r4 to r2.
//
// 7) If we're on the second load, in order to avoid overwriting the frames we
//    just wrapped from r4 we need to slide r0 to the right by the size of
//    r4, which is kernelSize / 2:
//
//        r0 = r0 + kernelSize / 2 = m_inputBuffer + kernelSize
//
//    r3, r4, and m_blockSize then need to be reinitialized, so goto (3).
//
// 8) Else, if we're not on the second load, goto (4).
//
// note: we're glossing over how the sub-sample handling works with m_virtualSourceIndex, etc.

namespace WebCore {

constexpr unsigned kernelSize { 32 };
constexpr unsigned numberOfKernelOffsets { 32 };
constexpr unsigned kernelStorageSize { kernelSize * (numberOfKernelOffsets + 1) };

static size_t calculateChunkSize(unsigned blockSize, double scaleFactor)
{
    return blockSize / scaleFactor;
}

SincResampler::SincResampler(double scaleFactor, unsigned requestFrames, Function<void(float* buffer, size_t framesToProcess)>&& provideInput)
    : m_scaleFactor(scaleFactor)
    , m_kernelStorage(kernelStorageSize)
    , m_requestFrames(requestFrames)
    , m_provideInput(WTFMove(provideInput))
    , m_inputBuffer(m_requestFrames + kernelSize) // See input buffer layout above.
    , m_r1(m_inputBuffer.data())
    , m_r2(m_inputBuffer.data() + kernelSize / 2)
{
    ASSERT(m_provideInput);
    ASSERT(m_requestFrames > 0);
    updateRegions(false);
    ASSERT(m_blockSize > kernelSize);
    initializeKernel();
}

void SincResampler::updateRegions(bool isSecondLoad)
{
    // Setup various region pointers in the buffer (see diagram above). If we're
    // on the second load we need to slide m_r0 to the right by kernelSize / 2.
    m_r0 = m_inputBuffer.data() + (isSecondLoad ? kernelSize : kernelSize / 2);
    m_r3 = m_r0 + m_requestFrames - kernelSize;
    m_r4 = m_r0 + m_requestFrames - kernelSize / 2;
    m_blockSize = m_r4 - m_r2;
    m_chunkSize = calculateChunkSize(m_blockSize, m_scaleFactor);

    // m_r1 at the beginning of the buffer.
    ASSERT(m_r1 == m_inputBuffer.data());
    // m_r1 left of m_r2, m_r4 left of m_r3 and size correct.
    ASSERT((m_r2 - m_r1) == (m_r4 - m_r3));
    // m_r2 left of r3.
    ASSERT(m_r2 <= m_r3);
}

void SincResampler::initializeKernel()
{
    // Blackman window parameters.
    double alpha = 0.16;
    double a0 = 0.5 * (1.0 - alpha);
    double a1 = 0.5;
    double a2 = 0.5 * alpha;

    // sincScaleFactor is basically the normalized cutoff frequency of the low-pass filter.
    double sincScaleFactor = m_scaleFactor > 1.0 ? 1.0 / m_scaleFactor : 1.0;

    // The sinc function is an idealized brick-wall filter, but since we're windowing it the
    // transition from pass to stop does not happen right away. So we should adjust the
    // lowpass filter cutoff slightly downward to avoid some aliasing at the very high-end.
    // FIXME: this value is empirical and to be more exact should vary depending on kernelSize.
    sincScaleFactor *= 0.9;

    int n = kernelSize;
    int halfSize = n / 2;

    // Generates a set of windowed sinc() kernels.
    // We generate a range of sub-sample offsets from 0.0 to 1.0.
    for (unsigned offsetIndex = 0; offsetIndex <= numberOfKernelOffsets; ++offsetIndex) {
        double subsampleOffset = static_cast<double>(offsetIndex) / numberOfKernelOffsets;

        for (int i = 0; i < n; ++i) {
            // Compute the sinc() with offset.
            double s = sincScaleFactor * piDouble * (i - halfSize - subsampleOffset);
            double sinc = !s ? 1.0 : sin(s) / s;
            sinc *= sincScaleFactor;

            // Compute Blackman window, matching the offset of the sinc().
            double x = (i - subsampleOffset) / n;
            double window = a0 - a1 * cos(2.0 * piDouble * x) + a2 * cos(4.0 * piDouble * x);

            // Window the sinc() function and store at the correct offset.
            m_kernelStorage[i + offsetIndex * kernelSize] = sinc * window;
        }
    }
}

void SincResampler::processBuffer(const float* source, float* destination, unsigned numberOfSourceFrames, double scaleFactor)
{
    SincResampler resampler(scaleFactor, AudioUtilities::renderQuantumSize, [source, numberOfSourceFrames](float* buffer, size_t framesToProcess) mutable {
        // Clamp to number of frames available and zero-pad.
        size_t framesToCopy = std::min<size_t>(numberOfSourceFrames, framesToProcess);
        memcpy(buffer, source, sizeof(float) * framesToCopy);

        // Zero-pad if necessary.
        if (framesToCopy < framesToProcess)
            memset(buffer + framesToCopy, 0, sizeof(float) * (framesToProcess - framesToCopy));

        numberOfSourceFrames -= framesToCopy;
        source += framesToCopy;
    });

    unsigned numberOfDestinationFrames = static_cast<unsigned>(numberOfSourceFrames / scaleFactor);
    unsigned remaining = numberOfDestinationFrames;

    while (remaining) {
        unsigned framesThisTime = std::min<unsigned>(remaining, AudioUtilities::renderQuantumSize);
        resampler.process(destination, framesThisTime);
        
        destination += framesThisTime;
        remaining -= framesThisTime;
    }
}

void SincResampler::process(float* destination, size_t framesToProcess)
{
    unsigned numberOfDestinationFrames = framesToProcess;

    // Step (1)
    // Prime the input buffer at the start of the input stream.
    if (!m_isBufferPrimed) {
        m_provideInput(m_r0, m_requestFrames);
        m_isBufferPrimed = true;
    }
    
    // Step (2)

    while (numberOfDestinationFrames) {
        while (m_virtualSourceIndex < m_blockSize) {
            // m_virtualSourceIndex lies in between two kernel offsets so figure out what they are.
            int sourceIndexI = static_cast<int>(m_virtualSourceIndex);
            double subsampleRemainder = m_virtualSourceIndex - sourceIndexI;

            double virtualOffsetIndex = subsampleRemainder * numberOfKernelOffsets;
            int offsetIndex = static_cast<int>(virtualOffsetIndex);
            
            float* k1 = m_kernelStorage.data() + offsetIndex * kernelSize;
            float* k2 = k1 + kernelSize;

            // Ensure |k1|, |k2| are 16-byte aligned for SIMD usage. Should always be true so long as kernelSize is a multiple of 16.
            ASSERT(!(reinterpret_cast<uintptr_t>(k1) & 0x0F));
            ASSERT(!(reinterpret_cast<uintptr_t>(k2) & 0x0F));

            // Initialize input pointer based on quantized m_virtualSourceIndex.
            float* inputP = m_r1 + sourceIndexI;

            // Figure out how much to weight each kernel's "convolution".
            double kernelInterpolationFactor = virtualOffsetIndex - offsetIndex;

            *destination++ = convolve(inputP, k1, k2, kernelInterpolationFactor);

            // Advance the virtual index.
            m_virtualSourceIndex += m_scaleFactor;

            --numberOfDestinationFrames;
            if (!numberOfDestinationFrames)
                return;
        }

        // Wrap back around to the start.
        ASSERT(m_virtualSourceIndex >= m_blockSize);
        m_virtualSourceIndex -= m_blockSize;

        // Step (3) Copy r3 to r1.
        // This wraps the last input frames back to the start of the buffer.
        memcpy(m_r1, m_r3, sizeof(float) * kernelSize);

        // Step (4) -- Reinitialize regions if necessary.
        if (m_r0 == m_r2)
            updateRegions(true);

        // Step (5)
        // Refresh the buffer with more input.
        m_provideInput(m_r0, m_requestFrames);
    }
}

float SincResampler::convolve(const float* inputP, const float* k1, const float* k2, float kernelInterpolationFactor)
{
#if USE(ACCELERATE)
    float sum1;
    float sum2;
    vDSP_dotpr(inputP, 1, k1, 1, &sum1, kernelSize);
    vDSP_dotpr(inputP, 1, k2, 1, &sum2, kernelSize);

    // Linearly interpolate the two "convolutions".
    return (1.0f - kernelInterpolationFactor) * sum1 + kernelInterpolationFactor * sum2;
#elif CPU(X86_SSE2)
    __m128 m_input;
    __m128 m_sums1 = _mm_setzero_ps();
    __m128 m_sums2 = _mm_setzero_ps();

    // Based on |inputP| alignment, we need to use loadu or load.
    if (reinterpret_cast<uintptr_t>(inputP) & 0x0F) {
        for (unsigned i = 0; i < kernelSize; i += 4) {
            m_input = _mm_loadu_ps(inputP + i);
            m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
            m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
        }
    } else {
        for (unsigned i = 0; i < kernelSize; i += 4) {
            m_input = _mm_load_ps(inputP + i);
            m_sums1 = _mm_add_ps(m_sums1, _mm_mul_ps(m_input, _mm_load_ps(k1 + i)));
            m_sums2 = _mm_add_ps(m_sums2, _mm_mul_ps(m_input, _mm_load_ps(k2 + i)));
        }
    }

    // Linearly interpolate the two "convolutions".
    m_sums1 = _mm_mul_ps(m_sums1, _mm_set_ps1(1.0f - kernelInterpolationFactor));
    m_sums2 = _mm_mul_ps(m_sums2, _mm_set_ps1(kernelInterpolationFactor));
    m_sums1 = _mm_add_ps(m_sums1, m_sums2);

    // Sum components together.
    float result;
    m_sums2 = _mm_add_ps(_mm_movehl_ps(m_sums1, m_sums1), m_sums1);
    _mm_store_ss(&result, _mm_add_ss(m_sums2, _mm_shuffle_ps(m_sums2, m_sums2, 1)));

    return result;
#elif HAVE(ARM_NEON_INTRINSICS)
    float32x4_t m_input;
    float32x4_t m_sums1 = vmovq_n_f32(0);
    float32x4_t m_sums2 = vmovq_n_f32(0);

    const float* upper = inputP + kernelSize;
    for (; inputP < upper; ) {
        m_input = vld1q_f32(inputP);
        inputP += 4;
        m_sums1 = vmlaq_f32(m_sums1, m_input, vld1q_f32(k1));
        k1 += 4;
        m_sums2 = vmlaq_f32(m_sums2, m_input, vld1q_f32(k2));
        k2 += 4;
    }

    // Linearly interpolate the two "convolutions".
    m_sums1 = vmlaq_f32(vmulq_f32(m_sums1, vmovq_n_f32(1.0 - kernelInterpolationFactor)), m_sums2, vmovq_n_f32(kernelInterpolationFactor));

    // Sum components together.
    float32x2_t m_half = vadd_f32(vget_high_f32(m_sums1), vget_low_f32(m_sums1));
    return vget_lane_f32(vpadd_f32(m_half, m_half), 0);
#else
    float sum1 = 0;
    float sum2 = 0;

    // Generate a single output sample.
    int n = kernelSize;
    while (n--) {
        sum1 += *inputP * *k1++;
        sum2 += *inputP++ * *k2++;
    }

    // Linearly interpolate the two "convolutions".
    return (1.0f - kernelInterpolationFactor) * sum1 + kernelInterpolationFactor * sum2;
#endif
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
