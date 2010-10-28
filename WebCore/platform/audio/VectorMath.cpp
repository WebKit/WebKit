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

namespace WebCore {

namespace VectorMath {

#if OS(DARWIN)
// On the Mac we use the highly optimized versions in Accelerate.framework

void vsmul(const float* sourceP, int sourceStride, const float* scale, float* destP, int destStride, size_t framesToProcess)
{
    vDSP_vsmul(sourceP, sourceStride, scale, destP, destStride, framesToProcess);
}

void vadd(const float* source1P, int sourceStride1, const float* source2P, int sourceStride2, float* destP, int destStride, size_t framesToProcess)
{
    vDSP_vadd(source1P, sourceStride1, source2P, sourceStride2, destP, destStride, framesToProcess);
}

#else

void vsmul(const float* sourceP, int sourceStride, const float* scale, float* destP, int destStride, size_t framesToProcess)
{
    // FIXME: optimize for SSE
    int n = framesToProcess;
    float k = *scale;
    while (n--) {
        *destP = k * *sourceP;
        sourceP += sourceStride;
        destP += destStride;
    }
}

void vadd(const float* source1P, int sourceStride1, const float* source2P, int sourceStride2, float* destP, int destStride, size_t framesToProcess)
{
    // FIXME: optimize for SSE
    int n = framesToProcess;
    while (n--) {
        *destP = *source1P + *source2P;
        source1P += sourceStride1;
        source2P += sourceStride2;
        destP += destStride;
    }
}

#endif // OS(DARWIN)

} // namespace VectorMath

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
