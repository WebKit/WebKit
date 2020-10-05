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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioUtilities.h"
#include "VectorMath.h"

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if CPU(X86_SSE2)
#include <emmintrin.h>
#endif

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

#include <algorithm>
#include <math.h>

namespace WebCore {

namespace VectorMath {

#if USE(ACCELERATE)
// On the Mac we use the highly optimized versions in Accelerate.framework

void vsmul(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vsmul(inputVector, inputStride, scale, outputVector, outputStride, numberOfElementsToProcess);
}

void vadd(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vadd(inputVector1, inputStride1, inputVector2, inputStride2, outputVector, outputStride, numberOfElementsToProcess);
}

void vsadd(const float* inputVector, int inputStride, const float* scalar, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vsadd(inputVector, inputStride, scalar, outputVector, outputStride, numberOfElementsToProcess);
}

void vmul(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vmul(inputVector1, inputStride1, inputVector2, inputStride2, outputVector, outputStride, numberOfElementsToProcess);
}

void zvmul(const float* realVector1, const float* imagVector1, const float* realVector2, const float* imag2P, float* realOutputVector, float* imagDestP, size_t numberOfElementsToProcess)
{
    DSPSplitComplex sc1;
    DSPSplitComplex sc2;
    DSPSplitComplex dest;
    sc1.realp = const_cast<float*>(realVector1);
    sc1.imagp = const_cast<float*>(imagVector1);
    sc2.realp = const_cast<float*>(realVector2);
    sc2.imagp = const_cast<float*>(imag2P);
    dest.realp = realOutputVector;
    dest.imagp = imagDestP;
    vDSP_zvmul(&sc1, 1, &sc2, 1, &dest, 1, numberOfElementsToProcess, 1);
}

void vsma(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vsma(inputVector, inputStride, scale, outputVector, outputStride, outputVector, outputStride, numberOfElementsToProcess);
}

void vmaxmgv(const float* inputVector, int inputStride, float* maximumValue, size_t numberOfElementsToProcess)
{
    vDSP_maxmgv(inputVector, inputStride, maximumValue, numberOfElementsToProcess);
}

void vsvesq(const float* inputVector, int inputStride, float* sum, size_t numberOfElementsToProcess)
{
    vDSP_svesq(const_cast<float*>(inputVector), inputStride, sum, numberOfElementsToProcess);
}

void vclip(const float* inputVector, int inputStride, const float* lowThresholdP, const float* highThresholdP, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    vDSP_vclip(const_cast<float*>(inputVector), inputStride, const_cast<float*>(lowThresholdP), const_cast<float*>(highThresholdP), outputVector, outputStride, numberOfElementsToProcess);
}

void linearToDecibels(const float* inputVector, int inputStride, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    float reference = 1;
    vDSP_vdbcon(inputVector, inputStride, &reference, outputVector, outputStride, numberOfElementsToProcess, 1);
}

#else

void vsma(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;

#if CPU(X86_SSE2)
    if ((inputStride == 1) && (outputStride == 1)) {
        float k = *scale;

        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<uintptr_t>(inputVector) & 0x0F) && n) {
            *outputVector += k * *inputVector;
            inputVector++;
            outputVector++;
            n--;
        }

        // Now the inputVector is aligned, use SSE.
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        __m128 pSource;
        __m128 dest;
        __m128 temp;
        __m128 mScale = _mm_set_ps1(k);

        bool destAligned = !(reinterpret_cast<uintptr_t>(outputVector) & 0x0F);

#define SSE2_MULT_ADD(loadInstr, storeInstr)        \
            while (outputVector < endP)                    \
            {                                       \
                pSource = _mm_load_ps(inputVector);     \
                temp = _mm_mul_ps(pSource, mScale); \
                dest = _mm_##loadInstr##_ps(outputVector); \
                dest = _mm_add_ps(dest, temp);      \
                _mm_##storeInstr##_ps(outputVector, dest); \
                inputVector += 4;                       \
                outputVector += 4;                         \
            }

        if (destAligned) 
            SSE2_MULT_ADD(load, store)
        else 
            SSE2_MULT_ADD(loadu, storeu)

        n = tailFrames;
    }
#elif HAVE(ARM_NEON_INTRINSICS)
    if ((inputStride == 1) && (outputStride == 1)) {
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        float32x4_t k = vdupq_n_f32(*scale);
        while (outputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            float32x4_t dest = vld1q_f32(outputVector);

            dest = vmlaq_f32(dest, source, k);
            vst1q_f32(outputVector, dest);

            inputVector += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    while (n) {
        *outputVector += *inputVector * *scale;
        inputVector += inputStride;
        outputVector += outputStride;
        n--;
    }
}

void vsmul(const float* inputVector, int inputStride, const float* scale, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;

#if CPU(X86_SSE2)
    if ((inputStride == 1) && (outputStride == 1)) {
        float k = *scale;

        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<size_t>(inputVector) & 0x0F) && n) {
            *outputVector = k * *inputVector;
            inputVector++;
            outputVector++;
            n--;
        }

        // Now the inputVector address is aligned and start to apply SSE.
        int group = n / 4;
        __m128 mScale = _mm_set_ps1(k);
        __m128* pSource;
        __m128* pDest;
        __m128 dest;


        if (reinterpret_cast<size_t>(outputVector) & 0x0F) {
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(inputVector));
                dest = _mm_mul_ps(*pSource, mScale);
                _mm_storeu_ps(outputVector, dest);

                inputVector += 4;
                outputVector += 4;
            }
        } else {
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(inputVector));
                pDest = reinterpret_cast<__m128*>(outputVector);
                *pDest = _mm_mul_ps(*pSource, mScale);

                inputVector += 4;
                outputVector += 4;
            }
        }

        // Non-SSE handling for remaining frames which is less than 4.
        n %= 4;
        while (n) {
            *outputVector = k * *inputVector;
            inputVector++;
            outputVector++;
            n--;
        }
    } else { // If strides are not 1, rollback to normal algorithm.
#elif HAVE(ARM_NEON_INTRINSICS)
    if ((inputStride == 1) && (outputStride == 1)) {
        float k = *scale;
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        while (outputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            vst1q_f32(outputVector, vmulq_n_f32(source, k));

            inputVector += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    float k = *scale;
    while (n--) {
        *outputVector = k * *inputVector;
        inputVector += inputStride;
        outputVector += outputStride;
    }
#if CPU(X86_SSE2)
    }
#endif
}

void vsadd(const float* inputVector, int inputStride, const float* scalar, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;

#if CPU(X86_SSE2)
    if (inputStride == 1 && outputStride == 1) {
        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<size_t>(inputVector) & 0x0F) && n) {
            *outputVector = *inputVector + *scalar;
            inputVector++;
            outputVector++;
            n--;
        }

        // Now the inputVector address is aligned and start to apply SSE.
        int group = n / 4;
        __m128 mScalar = _mm_set_ps1(*scalar);
        __m128* pSource;
        __m128* pDest;
        __m128 dest;

        bool destAligned = !(reinterpret_cast<size_t>(outputVector) & 0x0F);
        if (destAligned) { // all aligned
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(inputVector));
                pDest = reinterpret_cast<__m128*>(outputVector);
                *pDest = _mm_add_ps(*pSource, mScalar);

                inputVector += 4;
                outputVector += 4;
            }
        } else {
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(inputVector));
                dest = _mm_add_ps(*pSource, mScalar);
                _mm_storeu_ps(outputVector, dest);

                inputVector += 4;
                outputVector += 4;
            }
        }

        // Non-SSE handling for remaining frames which is less than 4.
        n %= 4;
        while (n) {
            *outputVector = *inputVector + *scalar;
            inputVector++;
            outputVector++;
            n--;
        }
    } else { // if strides are not 1, rollback to normal algorithm
#elif HAVE(ARM_NEON_INTRINSICS)
    if (inputStride == 1 && outputStride == 1) {
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;
        float32x4_t scalarVector = vdupq_n_f32(*scalar);

        while (outputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            vst1q_f32(outputVector, vaddq_f32(source, scalarVector));

            inputVector += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    while (n--) {
        *outputVector = *inputVector + *scalar;
        inputVector += inputStride;
        outputVector += outputStride;
    }
#if CPU(X86_SSE2)
    }
#endif
}

void vadd(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;

#if CPU(X86_SSE2)
    if ((inputStride1 ==1) && (inputStride2 == 1) && (outputStride == 1)) {
        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<size_t>(inputVector1) & 0x0F) && n) {
            *outputVector = *inputVector1 + *inputVector2;
            inputVector1++;
            inputVector2++;
            outputVector++;
            n--;
        }

        // Now the inputVector1 address is aligned and start to apply SSE.
        int group = n / 4;
        __m128* pSource1;
        __m128* pSource2;
        __m128* pDest;
        __m128 source2;
        __m128 dest;

        bool source2Aligned = !(reinterpret_cast<size_t>(inputVector2) & 0x0F);
        bool destAligned = !(reinterpret_cast<size_t>(outputVector) & 0x0F);

        if (source2Aligned && destAligned) { // all aligned
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector1));
                pSource2 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector2));
                pDest = reinterpret_cast<__m128*>(outputVector);
                *pDest = _mm_add_ps(*pSource1, *pSource2);

                inputVector1 += 4;
                inputVector2 += 4;
                outputVector += 4;
            }

        } else if (source2Aligned && !destAligned) { // source2 aligned but dest not aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector1));
                pSource2 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector2));
                dest = _mm_add_ps(*pSource1, *pSource2);
                _mm_storeu_ps(outputVector, dest);

                inputVector1 += 4;
                inputVector2 += 4;
                outputVector += 4;
            }

        } else if (!source2Aligned && destAligned) { // source2 not aligned but dest aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector1));
                source2 = _mm_loadu_ps(inputVector2);
                pDest = reinterpret_cast<__m128*>(outputVector);
                *pDest = _mm_add_ps(*pSource1, source2);

                inputVector1 += 4;
                inputVector2 += 4;
                outputVector += 4;
            }
        } else if (!source2Aligned && !destAligned) { // both source2 and dest not aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(inputVector1));
                source2 = _mm_loadu_ps(inputVector2);
                dest = _mm_add_ps(*pSource1, source2);
                _mm_storeu_ps(outputVector, dest);

                inputVector1 += 4;
                inputVector2 += 4;
                outputVector += 4;
            }
        }

        // Non-SSE handling for remaining frames which is less than 4.
        n %= 4;
        while (n) {
            *outputVector = *inputVector1 + *inputVector2;
            inputVector1++;
            inputVector2++;
            outputVector++;
            n--;
        }
    } else { // if strides are not 1, rollback to normal algorithm
#elif HAVE(ARM_NEON_INTRINSICS)
    if ((inputStride1 ==1) && (inputStride2 == 1) && (outputStride == 1)) {
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        while (outputVector < endP) {
            float32x4_t source1 = vld1q_f32(inputVector1);
            float32x4_t source2 = vld1q_f32(inputVector2);
            vst1q_f32(outputVector, vaddq_f32(source1, source2));

            inputVector1 += 4;
            inputVector2 += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    while (n--) {
        *outputVector = *inputVector1 + *inputVector2;
        inputVector1 += inputStride1;
        inputVector2 += inputStride2;
        outputVector += outputStride;
    }
#if CPU(X86_SSE2)
    }
#endif
}

void vmul(const float* inputVector1, int inputStride1, const float* inputVector2, int inputStride2, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{

    int n = numberOfElementsToProcess;

#if CPU(X86_SSE2)
    if ((inputStride1 == 1) && (inputStride2 == 1) && (outputStride == 1)) {
        // If the inputVector1 address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<uintptr_t>(inputVector1) & 0x0F) && n) {
            *outputVector = *inputVector1 * *inputVector2;
            inputVector1++;
            inputVector2++;
            outputVector++;
            n--;
        }

        // Now the inputVector1 address aligned and start to apply SSE.
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;
        __m128 pSource1;
        __m128 pSource2;
        __m128 dest;

        bool source2Aligned = !(reinterpret_cast<uintptr_t>(inputVector2) & 0x0F);
        bool destAligned = !(reinterpret_cast<uintptr_t>(outputVector) & 0x0F);

#define SSE2_MULT(loadInstr, storeInstr)                   \
            while (outputVector < endP)                           \
            {                                              \
                pSource1 = _mm_load_ps(inputVector1);          \
                pSource2 = _mm_##loadInstr##_ps(inputVector2); \
                dest = _mm_mul_ps(pSource1, pSource2);     \
                _mm_##storeInstr##_ps(outputVector, dest);        \
                inputVector1 += 4;                             \
                inputVector2 += 4;                             \
                outputVector += 4;                                \
            }

        if (source2Aligned && destAligned) // Both aligned.
            SSE2_MULT(load, store)
        else if (source2Aligned && !destAligned) // Source2 is aligned but dest not.
            SSE2_MULT(load, storeu)
        else if (!source2Aligned && destAligned) // Dest is aligned but source2 not.
            SSE2_MULT(loadu, store)
        else // Neither aligned.
            SSE2_MULT(loadu, storeu)

        n = tailFrames;
    }
#elif HAVE(ARM_NEON_INTRINSICS)
    if ((inputStride1 ==1) && (inputStride2 == 1) && (outputStride == 1)) {
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        while (outputVector < endP) {
            float32x4_t source1 = vld1q_f32(inputVector1);
            float32x4_t source2 = vld1q_f32(inputVector2);
            vst1q_f32(outputVector, vmulq_f32(source1, source2));

            inputVector1 += 4;
            inputVector2 += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    while (n) {
        *outputVector = *inputVector1 * *inputVector2;
        inputVector1 += inputStride1;
        inputVector2 += inputStride2;
        outputVector += outputStride;
        n--;
    }
}

void zvmul(const float* realVector1, const float* imagVector1, const float* realVector2, const float* imag2P, float* realOutputVector, float* imagDestP, size_t numberOfElementsToProcess)
{
    unsigned i = 0;
#if CPU(X86_SSE2)
    // Only use the SSE optimization in the very common case that all addresses are 16-byte aligned. 
    // Otherwise, fall through to the scalar code below.
    if (!(reinterpret_cast<uintptr_t>(realVector1) & 0x0F)
        && !(reinterpret_cast<uintptr_t>(imagVector1) & 0x0F)
        && !(reinterpret_cast<uintptr_t>(realVector2) & 0x0F)
        && !(reinterpret_cast<uintptr_t>(imag2P) & 0x0F)
        && !(reinterpret_cast<uintptr_t>(realOutputVector) & 0x0F)
        && !(reinterpret_cast<uintptr_t>(imagDestP) & 0x0F)) {
        
        unsigned endSize = numberOfElementsToProcess - numberOfElementsToProcess % 4;
        while (i < endSize) {
            __m128 real1 = _mm_load_ps(realVector1 + i);
            __m128 real2 = _mm_load_ps(realVector2 + i);
            __m128 imag1 = _mm_load_ps(imagVector1 + i);
            __m128 imag2 = _mm_load_ps(imag2P + i);
            __m128 real = _mm_mul_ps(real1, real2);
            real = _mm_sub_ps(real, _mm_mul_ps(imag1, imag2));
            __m128 imag = _mm_mul_ps(real1, imag2);
            imag = _mm_add_ps(imag, _mm_mul_ps(imag1, real2));
            _mm_store_ps(realOutputVector + i, real);
            _mm_store_ps(imagDestP + i, imag);
            i += 4;
        }
    }
#elif HAVE(ARM_NEON_INTRINSICS)
        unsigned endSize = numberOfElementsToProcess - numberOfElementsToProcess % 4;
        while (i < endSize) {
            float32x4_t real1 = vld1q_f32(realVector1 + i);
            float32x4_t real2 = vld1q_f32(realVector2 + i);
            float32x4_t imag1 = vld1q_f32(imagVector1 + i);
            float32x4_t imag2 = vld1q_f32(imag2P + i);

            float32x4_t realResult = vmlsq_f32(vmulq_f32(real1, real2), imag1, imag2);
            float32x4_t imagResult = vmlaq_f32(vmulq_f32(real1, imag2), imag1, real2);

            vst1q_f32(realOutputVector + i, realResult);
            vst1q_f32(imagDestP + i, imagResult);

            i += 4;
        }
#endif
    for (; i < numberOfElementsToProcess; ++i) {
        // Read and compute result before storing them, in case the
        // destination is the same as one of the sources.
        float realResult = realVector1[i] * realVector2[i] - imagVector1[i] * imag2P[i];
        float imagResult = realVector1[i] * imag2P[i] + imagVector1[i] * realVector2[i];

        realOutputVector[i] = realResult;
        imagDestP[i] = imagResult;
    }
}

void vsvesq(const float* inputVector, int inputStride, float* sum, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;
    *sum = 0;

#if CPU(X86_SSE2)
    if (inputStride == 1) {
        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<uintptr_t>(inputVector) & 0x0F) && n) {
            float sample = *inputVector;
            *sum += sample * sample;
            inputVector++;
            n--; 
        } 
 
        // Now the inputVector is aligned, use SSE.
        int tailFrames = n % 4; 
        const float* endP = inputVector + n - tailFrames;
        __m128 source; 
        __m128 mSum = _mm_setzero_ps(); 
 
        while (inputVector < endP) {
            source = _mm_load_ps(inputVector);
            source = _mm_mul_ps(source, source); 
            mSum = _mm_add_ps(mSum, source); 
            inputVector += 4;
        } 
 
        // Summarize the SSE results. 
        const float* groupSumP = reinterpret_cast<float*>(&mSum); 
        *sum += groupSumP[0] + groupSumP[1] + groupSumP[2] + groupSumP[3];
 
        n = tailFrames; 
    } 
#elif HAVE(ARM_NEON_INTRINSICS)
    if (inputStride == 1) {
        int tailFrames = n % 4;
        const float* endP = inputVector + n - tailFrames;

        float32x4_t fourSum = vdupq_n_f32(0);
        while (inputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            fourSum = vmlaq_f32(fourSum, source, source);
            inputVector += 4;
        }
        float32x2_t twoSum = vadd_f32(vget_low_f32(fourSum), vget_high_f32(fourSum));

        float groupSum[2];
        vst1_f32(groupSum, twoSum);
        *sum += groupSum[0] + groupSum[1];

        n = tailFrames;
    }
#endif

    while (n--) {
        float sample = *inputVector;
        *sum += sample * sample;
        inputVector += inputStride;
    }

    ASSERT(*sum);
}

void vmaxmgv(const float* inputVector, int inputStride, float* maximumValue, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;
    float max = 0;

#if CPU(X86_SSE2)
    if (inputStride == 1) {
        // If the inputVector address is not 16-byte aligned, the first several frames (at most three) should be processed separately.
        while ((reinterpret_cast<uintptr_t>(inputVector) & 0x0F) && n) {
            max = std::max(max, fabsf(*inputVector));
            inputVector++;
            n--;
        }

        // Now the inputVector is aligned, use SSE.
        int tailFrames = n % 4;
        const float* endP = inputVector + n - tailFrames;
        __m128 source;
        __m128 mMax = _mm_setzero_ps();
        int mask = 0x7FFFFFFF;
        __m128 mMask = _mm_set1_ps(*reinterpret_cast<float*>(&mask));

        while (inputVector < endP) {
            source = _mm_load_ps(inputVector);
            // Calculate the absolute value by anding source with mask, the sign bit is set to 0.
            source = _mm_and_ps(source, mMask);
            mMax = _mm_max_ps(mMax, source);
            inputVector += 4;
        }

        // Get max from the SSE results.
        const float* groupMaxP = reinterpret_cast<float*>(&mMax);
        max = std::max(max, groupMaxP[0]);
        max = std::max(max, groupMaxP[1]);
        max = std::max(max, groupMaxP[2]);
        max = std::max(max, groupMaxP[3]);

        n = tailFrames;
    }
#elif HAVE(ARM_NEON_INTRINSICS)
    if (inputStride == 1) {
        int tailFrames = n % 4;
        const float* endP = inputVector + n - tailFrames;

        float32x4_t fourMax = vdupq_n_f32(0);
        while (inputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            fourMax = vmaxq_f32(fourMax, vabsq_f32(source));
            inputVector += 4;
        }
        float32x2_t twoMax = vmax_f32(vget_low_f32(fourMax), vget_high_f32(fourMax));

        float groupMax[2];
        vst1_f32(groupMax, twoMax);
        max = std::max(groupMax[0], groupMax[1]);

        n = tailFrames;
    }
#endif

    while (n--) {
        max = std::max(max, fabsf(*inputVector));
        inputVector += inputStride;
    }

    ASSERT(maximumValue);
    *maximumValue = max;
}

void vclip(const float* inputVector, int inputStride, const float* lowThresholdP, const float* highThresholdP, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    int n = numberOfElementsToProcess;
    float lowThreshold = *lowThresholdP;
    float highThreshold = *highThresholdP;

    // FIXME: Optimize for SSE2.
#if HAVE(ARM_NEON_INTRINSICS)
    if ((inputStride == 1) && (outputStride == 1)) {
        int tailFrames = n % 4;
        const float* endP = outputVector + n - tailFrames;

        float32x4_t low = vdupq_n_f32(lowThreshold);
        float32x4_t high = vdupq_n_f32(highThreshold);
        while (outputVector < endP) {
            float32x4_t source = vld1q_f32(inputVector);
            vst1q_f32(outputVector, vmaxq_f32(vminq_f32(source, high), low));
            inputVector += 4;
            outputVector += 4;
        }
        n = tailFrames;
    }
#endif
    while (n--) {
        *outputVector = std::max(std::min(*inputVector, highThreshold), lowThreshold);
        inputVector += inputStride;
        outputVector += outputStride;
    }
}

void linearToDecibels(const float* inputVector, int inputStride, float* outputVector, int outputStride, size_t numberOfElementsToProcess)
{
    for (size_t i = 0; i < numberOfElementsToProcess; ++i, inputVector += inputStride, outputVector += outputStride)
        *outputVector = AudioUtilities::linearToDecibels(*inputVector);
}

#endif // USE(ACCELERATE)

} // namespace VectorMath

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
