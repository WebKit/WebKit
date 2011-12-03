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

#include "VectorMath.h"

#if OS(DARWIN)
#include <Accelerate/Accelerate.h>
#endif

#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace WebCore {

namespace VectorMath {

#if OS(DARWIN)
// On the Mac we use the highly optimized versions in Accelerate.framework
// In 32-bit mode (__ppc__ or __i386__) <Accelerate/Accelerate.h> includes <vecLib/vDSP_translate.h> which defines macros of the same name as
// our namespaced function names, so we must handle this case differently. Other architectures (64bit, ARM, etc.) do not include this header file.

void vsmul(const float* sourceP, int sourceStride, const float* scale, float* destP, int destStride, size_t framesToProcess)
{
#if defined(__ppc__) || defined(__i386__)
    ::vsmul(sourceP, sourceStride, scale, destP, destStride, framesToProcess);
#else
    vDSP_vsmul(sourceP, sourceStride, scale, destP, destStride, framesToProcess);
#endif
}

void vadd(const float* source1P, int sourceStride1, const float* source2P, int sourceStride2, float* destP, int destStride, size_t framesToProcess)
{
#if defined(__ppc__) || defined(__i386__)
    ::vadd(source1P, sourceStride1, source2P, sourceStride2, destP, destStride, framesToProcess);
#else
    vDSP_vadd(source1P, sourceStride1, source2P, sourceStride2, destP, destStride, framesToProcess);
#endif
}

#else

void vsmul(const float* sourceP, int sourceStride, const float* scale, float* destP, int destStride, size_t framesToProcess)
{
#ifdef __SSE2__
    if ((sourceStride == 1) && (destStride == 1)) {
        
        int n = framesToProcess;
        float k = *scale;

        // If the sourceP address is not 16-byte aligned, the first several frames (at most three) should be processed seperately.
        while ((reinterpret_cast<size_t>(sourceP) & 0x0F) && n) {
            *destP = k * *sourceP;
            sourceP++;
            destP++;
            n--;
        }

        // Now the sourceP address is aligned and start to apply SSE.
        int group = n / 4;
        __m128 mScale = _mm_set_ps1(k);
        __m128* pSource;
        __m128* pDest;
        __m128 dest;


        if (reinterpret_cast<size_t>(destP) & 0x0F) {
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(sourceP));
                dest = _mm_mul_ps(*pSource, mScale);
                _mm_storeu_ps(destP, dest);

                sourceP += 4;
                destP += 4;
            }
        } else {
            while (group--) {
                pSource = reinterpret_cast<__m128*>(const_cast<float*>(sourceP));
                pDest = reinterpret_cast<__m128*>(destP);
                *pDest = _mm_mul_ps(*pSource, mScale);

                sourceP += 4;
                destP += 4;
            }
        }

        // Non-SSE handling for remaining frames which is less than 4.
        n %= 4;
        while (n) {
            *destP = k * *sourceP;
            sourceP++;
            destP++;
            n--;
        }
    } else { // If strides are not 1, rollback to normal algorithm.
#endif
    int n = framesToProcess;
    float k = *scale;
    while (n--) {
        *destP = k * *sourceP;
        sourceP += sourceStride;
        destP += destStride;
    }
#ifdef __SSE2__
    }
#endif
}

void vadd(const float* source1P, int sourceStride1, const float* source2P, int sourceStride2, float* destP, int destStride, size_t framesToProcess)
{
#ifdef __SSE2__
    if ((sourceStride1 ==1) && (sourceStride2 == 1) && (destStride == 1)) {

        int n = framesToProcess;

        // If the sourceP address is not 16-byte aligned, the first several frames (at most three) should be processed seperately.
        while ((reinterpret_cast<size_t>(source1P) & 0x0F) && n) {
            *destP = *source1P + *source2P;
            source1P++;
            source2P++;
            destP++;
            n--;
        }

        // Now the source1P address is aligned and start to apply SSE.
        int group = n / 4;
        __m128* pSource1;
        __m128* pSource2;
        __m128* pDest;
        __m128 source2;
        __m128 dest;

        bool source2Aligned = !(reinterpret_cast<size_t>(source2P) & 0x0F);
        bool destAligned = !(reinterpret_cast<size_t>(destP) & 0x0F);

        if (source2Aligned && destAligned) { // all aligned
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(source1P));
                pSource2 = reinterpret_cast<__m128*>(const_cast<float*>(source2P));
                pDest = reinterpret_cast<__m128*>(destP);
                *pDest = _mm_add_ps(*pSource1, *pSource2);

                source1P += 4;
                source2P += 4;
                destP += 4;
            }

        } else if (source2Aligned && !destAligned) { // source2 aligned but dest not aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(source1P));
                pSource2 = reinterpret_cast<__m128*>(const_cast<float*>(source2P));
                dest = _mm_add_ps(*pSource1, *pSource2);
                _mm_storeu_ps(destP, dest);

                source1P += 4;
                source2P += 4;
                destP += 4;
            }

        } else if (!source2Aligned && destAligned) { // source2 not aligned but dest aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(source1P));
                source2 = _mm_loadu_ps(source2P);
                pDest = reinterpret_cast<__m128*>(destP);
                *pDest = _mm_add_ps(*pSource1, source2);

                source1P += 4;
                source2P += 4;
                destP += 4;
            }
        } else if (!source2Aligned && !destAligned) { // both source2 and dest not aligned 
            while (group--) {
                pSource1 = reinterpret_cast<__m128*>(const_cast<float*>(source1P));
                source2 = _mm_loadu_ps(source2P);
                dest = _mm_add_ps(*pSource1, source2);
                _mm_storeu_ps(destP, dest);

                source1P += 4;
                source2P += 4;
                destP += 4;
            }
        }

        // Non-SSE handling for remaining frames which is less than 4.
        n %= 4;
        while (n) {
            *destP = *source1P + *source2P;
            source1P++;
            source2P++;
            destP++;
            n--;
        }
    } else { // if strides are not 1, rollback to normal algorithm
#endif
    int n = framesToProcess;
    while (n--) {
        *destP = *source1P + *source2P;
        source1P += sourceStride1;
        source2P += sourceStride2;
        destP += destStride;
    }
#ifdef __SSE2__
    }
#endif
}

#endif // OS(DARWIN)

} // namespace VectorMath

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
