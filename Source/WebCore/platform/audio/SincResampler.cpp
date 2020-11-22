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
#include <wtf/MathExtras.h>

#if CPU(X86_SSE2)
#include <emmintrin.h>
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

constexpr unsigned defaultRequestFrames { 512 };
constexpr unsigned kernelSize { 32 };
constexpr unsigned numberOfKernelOffsets { 32 };

SincResampler::SincResampler(double scaleFactor, Optional<unsigned> requestFrames)
    : m_scaleFactor(scaleFactor)
    , m_kernelStorage(kernelSize * (numberOfKernelOffsets + 1))
    , m_requestFrames(requestFrames.valueOr(defaultRequestFrames))
    , m_inputBuffer(m_requestFrames + kernelSize) // See input buffer layout above.
    , m_r1(m_inputBuffer.data())
    , m_r2(m_inputBuffer.data() + kernelSize / 2)
{
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

void SincResampler::consumeSource(float* buffer, unsigned numberOfSourceFrames)
{
    ASSERT(m_sourceProvider);
    if (!m_sourceProvider)
        return;

    // Wrap the provided buffer by an AudioBus for use by the source provider.
    if (!m_internalBus || m_internalBus->length() != numberOfSourceFrames)
        m_internalBus = AudioBus::create(1, numberOfSourceFrames, false);

    // FIXME: Find a way to make the following const-correct:
    m_internalBus->setChannelMemory(0, buffer, numberOfSourceFrames);
    
    m_sourceProvider->provideInput(m_internalBus.get(), numberOfSourceFrames);
}

namespace {

// BufferSourceProvider is an AudioSourceProvider wrapping an in-memory buffer.

class BufferSourceProvider : public AudioSourceProvider {
public:
    explicit BufferSourceProvider(const float* source, size_t numberOfSourceFrames)
        : m_source(source)
        , m_sourceFramesAvailable(numberOfSourceFrames)
    {
    }
    
    // Consumes samples from the in-memory buffer.
    void provideInput(AudioBus* bus, size_t framesToProcess) override
    {
        ASSERT(m_source && bus);
        if (!m_source || !bus)
            return;
            
        float* buffer = bus->channel(0)->mutableData();

        // Clamp to number of frames available and zero-pad.
        size_t framesToCopy = std::min(m_sourceFramesAvailable, framesToProcess);
        memcpy(buffer, m_source, sizeof(float) * framesToCopy);

        // Zero-pad if necessary.
        if (framesToCopy < framesToProcess)
            memset(buffer + framesToCopy, 0, sizeof(float) * (framesToProcess - framesToCopy));

        m_sourceFramesAvailable -= framesToCopy;
        m_source += framesToCopy;
    }
    
private:
    const float* m_source;
    size_t m_sourceFramesAvailable;
};

} // namespace

void SincResampler::process(const float* source, float* destination, unsigned numberOfSourceFrames)
{
    // Resample an in-memory buffer using an AudioSourceProvider.
    BufferSourceProvider sourceProvider(source, numberOfSourceFrames);

    unsigned numberOfDestinationFrames = static_cast<unsigned>(numberOfSourceFrames / m_scaleFactor);
    unsigned remaining = numberOfDestinationFrames;
    
    while (remaining) {
        unsigned framesThisTime = std::min(remaining, m_requestFrames);
        process(&sourceProvider, destination, framesThisTime);
        
        destination += framesThisTime;
        remaining -= framesThisTime;
    }
}

void SincResampler::process(AudioSourceProvider* sourceProvider, float* destination, size_t framesToProcess)
{
    ASSERT(sourceProvider);
    if (!sourceProvider)
        return;
    
    m_sourceProvider = sourceProvider;

    unsigned numberOfDestinationFrames = framesToProcess;

    // Step (1)
    // Prime the input buffer at the start of the input stream.
    if (!m_isBufferPrimed) {
        consumeSource(m_r0, m_requestFrames);
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

            // Initialize input pointer based on quantized m_virtualSourceIndex.
            float* inputP = m_r1 + sourceIndexI;

            // We'll compute "convolutions" for the two kernels which straddle m_virtualSourceIndex
            float sum1 = 0;
            float sum2 = 0;

            // Figure out how much to weight each kernel's "convolution".
            double kernelInterpolationFactor = virtualOffsetIndex - offsetIndex;

            // Generate a single output sample. 
            int n = kernelSize;

#define CONVOLVE_ONE_SAMPLE      \
            input = *inputP++;   \
            sum1 += input * *k1; \
            sum2 += input * *k2; \
            ++k1;                \
            ++k2;

            {
                float input;

#if CPU(X86_SSE2)
                // If the sourceP address is not 16-byte aligned, the first several frames (at most three) should be processed seperately.
                while ((reinterpret_cast<uintptr_t>(inputP) & 0x0F) && n) {
                    CONVOLVE_ONE_SAMPLE
                    n--;
                }

                // Now the inputP is aligned and start to apply SSE.
                float* endP = inputP + n - n % 4;
                __m128 mInput;
                __m128 mK1;
                __m128 mK2;
                __m128 mul1;
                __m128 mul2;

                __m128 sums1 = _mm_setzero_ps();
                __m128 sums2 = _mm_setzero_ps();
                bool k1Aligned = !(reinterpret_cast<uintptr_t>(k1) & 0x0F);
                bool k2Aligned = !(reinterpret_cast<uintptr_t>(k2) & 0x0F);

#define LOAD_DATA(l1, l2)                        \
                mInput = _mm_load_ps(inputP);    \
                mK1 = _mm_##l1##_ps(k1);         \
                mK2 = _mm_##l2##_ps(k2);

#define CONVOLVE_4_SAMPLES                       \
                mul1 = _mm_mul_ps(mInput, mK1);  \
                mul2 = _mm_mul_ps(mInput, mK2);  \
                sums1 = _mm_add_ps(sums1, mul1); \
                sums2 = _mm_add_ps(sums2, mul2); \
                inputP += 4;                     \
                k1 += 4;                         \
                k2 += 4;

                if (k1Aligned && k2Aligned) { // both aligned
                    while (inputP < endP) {
                        LOAD_DATA(load, load)
                        CONVOLVE_4_SAMPLES
                    }
                } else if (!k1Aligned && k2Aligned) { // only k2 aligned
                    while (inputP < endP) {
                        LOAD_DATA(loadu, load)
                        CONVOLVE_4_SAMPLES
                    }
                } else if (k1Aligned && !k2Aligned) { // only k1 aligned
                    while (inputP < endP) {
                        LOAD_DATA(load, loadu)
                        CONVOLVE_4_SAMPLES
                    }
                } else { // both non-aligned
                    while (inputP < endP) {
                        LOAD_DATA(loadu, loadu)
                        CONVOLVE_4_SAMPLES
                    }
                }

                // Summarize the SSE results to sum1 and sum2.
                float* groupSumP = reinterpret_cast<float*>(&sums1);
                sum1 += groupSumP[0] + groupSumP[1] + groupSumP[2] + groupSumP[3];
                groupSumP = reinterpret_cast<float*>(&sums2);
                sum2 += groupSumP[0] + groupSumP[1] + groupSumP[2] + groupSumP[3];

                n %= 4;
                while (n) {
                    CONVOLVE_ONE_SAMPLE
                    n--;
                }
#else
                // FIXME: add ARM NEON optimizations for the following. The scalar code-path can probably also be optimized better.
                
                // Optimize size 32 and size 64 kernels by unrolling the while loop.
                // A 20 - 30% speed improvement was measured in some cases by using this approach.
                
                if (n == 32) {
                    CONVOLVE_ONE_SAMPLE // 1
                    CONVOLVE_ONE_SAMPLE // 2
                    CONVOLVE_ONE_SAMPLE // 3
                    CONVOLVE_ONE_SAMPLE // 4
                    CONVOLVE_ONE_SAMPLE // 5
                    CONVOLVE_ONE_SAMPLE // 6
                    CONVOLVE_ONE_SAMPLE // 7
                    CONVOLVE_ONE_SAMPLE // 8
                    CONVOLVE_ONE_SAMPLE // 9
                    CONVOLVE_ONE_SAMPLE // 10
                    CONVOLVE_ONE_SAMPLE // 11
                    CONVOLVE_ONE_SAMPLE // 12
                    CONVOLVE_ONE_SAMPLE // 13
                    CONVOLVE_ONE_SAMPLE // 14
                    CONVOLVE_ONE_SAMPLE // 15
                    CONVOLVE_ONE_SAMPLE // 16
                    CONVOLVE_ONE_SAMPLE // 17
                    CONVOLVE_ONE_SAMPLE // 18
                    CONVOLVE_ONE_SAMPLE // 19
                    CONVOLVE_ONE_SAMPLE // 20
                    CONVOLVE_ONE_SAMPLE // 21
                    CONVOLVE_ONE_SAMPLE // 22
                    CONVOLVE_ONE_SAMPLE // 23
                    CONVOLVE_ONE_SAMPLE // 24
                    CONVOLVE_ONE_SAMPLE // 25
                    CONVOLVE_ONE_SAMPLE // 26
                    CONVOLVE_ONE_SAMPLE // 27
                    CONVOLVE_ONE_SAMPLE // 28
                    CONVOLVE_ONE_SAMPLE // 29
                    CONVOLVE_ONE_SAMPLE // 30
                    CONVOLVE_ONE_SAMPLE // 31
                    CONVOLVE_ONE_SAMPLE // 32
                } else if (n == 64) {
                    CONVOLVE_ONE_SAMPLE // 1
                    CONVOLVE_ONE_SAMPLE // 2
                    CONVOLVE_ONE_SAMPLE // 3
                    CONVOLVE_ONE_SAMPLE // 4
                    CONVOLVE_ONE_SAMPLE // 5
                    CONVOLVE_ONE_SAMPLE // 6
                    CONVOLVE_ONE_SAMPLE // 7
                    CONVOLVE_ONE_SAMPLE // 8
                    CONVOLVE_ONE_SAMPLE // 9
                    CONVOLVE_ONE_SAMPLE // 10
                    CONVOLVE_ONE_SAMPLE // 11
                    CONVOLVE_ONE_SAMPLE // 12
                    CONVOLVE_ONE_SAMPLE // 13
                    CONVOLVE_ONE_SAMPLE // 14
                    CONVOLVE_ONE_SAMPLE // 15
                    CONVOLVE_ONE_SAMPLE // 16
                    CONVOLVE_ONE_SAMPLE // 17
                    CONVOLVE_ONE_SAMPLE // 18
                    CONVOLVE_ONE_SAMPLE // 19
                    CONVOLVE_ONE_SAMPLE // 20
                    CONVOLVE_ONE_SAMPLE // 21
                    CONVOLVE_ONE_SAMPLE // 22
                    CONVOLVE_ONE_SAMPLE // 23
                    CONVOLVE_ONE_SAMPLE // 24
                    CONVOLVE_ONE_SAMPLE // 25
                    CONVOLVE_ONE_SAMPLE // 26
                    CONVOLVE_ONE_SAMPLE // 27
                    CONVOLVE_ONE_SAMPLE // 28
                    CONVOLVE_ONE_SAMPLE // 29
                    CONVOLVE_ONE_SAMPLE // 30
                    CONVOLVE_ONE_SAMPLE // 31
                    CONVOLVE_ONE_SAMPLE // 32
                    CONVOLVE_ONE_SAMPLE // 33
                    CONVOLVE_ONE_SAMPLE // 34
                    CONVOLVE_ONE_SAMPLE // 35
                    CONVOLVE_ONE_SAMPLE // 36
                    CONVOLVE_ONE_SAMPLE // 37
                    CONVOLVE_ONE_SAMPLE // 38
                    CONVOLVE_ONE_SAMPLE // 39
                    CONVOLVE_ONE_SAMPLE // 40
                    CONVOLVE_ONE_SAMPLE // 41
                    CONVOLVE_ONE_SAMPLE // 42
                    CONVOLVE_ONE_SAMPLE // 43
                    CONVOLVE_ONE_SAMPLE // 44
                    CONVOLVE_ONE_SAMPLE // 45
                    CONVOLVE_ONE_SAMPLE // 46
                    CONVOLVE_ONE_SAMPLE // 47
                    CONVOLVE_ONE_SAMPLE // 48
                    CONVOLVE_ONE_SAMPLE // 49
                    CONVOLVE_ONE_SAMPLE // 50
                    CONVOLVE_ONE_SAMPLE // 51
                    CONVOLVE_ONE_SAMPLE // 52
                    CONVOLVE_ONE_SAMPLE // 53
                    CONVOLVE_ONE_SAMPLE // 54
                    CONVOLVE_ONE_SAMPLE // 55
                    CONVOLVE_ONE_SAMPLE // 56
                    CONVOLVE_ONE_SAMPLE // 57
                    CONVOLVE_ONE_SAMPLE // 58
                    CONVOLVE_ONE_SAMPLE // 59
                    CONVOLVE_ONE_SAMPLE // 60
                    CONVOLVE_ONE_SAMPLE // 61
                    CONVOLVE_ONE_SAMPLE // 62
                    CONVOLVE_ONE_SAMPLE // 63
                    CONVOLVE_ONE_SAMPLE // 64
                } else {
                    while (n--) {
                        // Non-optimized using actual while loop.
                        CONVOLVE_ONE_SAMPLE
                    }
                }
#endif
            }

            // Linearly interpolate the two "convolutions".
            double result = (1.0 - kernelInterpolationFactor) * sum1 + kernelInterpolationFactor * sum2;

            *destination++ = result;

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
        consumeSource(m_r0, m_requestFrames);
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
