/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "WaveShaperDSPKernel.h"

#include "WaveShaperProcessor.h"
#include <algorithm>

using namespace std;

namespace WebCore {

void WaveShaperDSPKernel::process(const float* source, float* destination, size_t framesToProcess)
{
    ASSERT(source && destination && waveShaperProcessor());

    Float32Array* curve = waveShaperProcessor()->curve();
    if (!curve) {
        // Act as "straight wire" pass-through if no curve is set.
        memcpy(destination, source, sizeof(float) * framesToProcess);
        return;
    }

    float* curveData = curve->data();
    int curveLength = curve->length();

    ASSERT(curveData);

    if (!curveData || !curveLength) {
        memcpy(destination, source, sizeof(float) * framesToProcess);
        return;
    }

    // Apply waveshaping curve.
    for (unsigned i = 0; i < framesToProcess; ++i) {
        const float input = source[i];

        // Calculate an index based on input -1 -> +1 with 0 being at the center of the curve data.
        int index = (curveLength * (input + 1)) / 2;

        // Clip index to the input range of the curve.
        // This takes care of input outside of nominal range -1 -> +1
        index = max(index, 0);
        index = min(index, curveLength - 1);
        destination[i] = curveData[index];
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
