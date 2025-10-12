/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "AudioUtilities.h"
#include "WaveShaperProcessor.h"
#include <JavaScriptCore/Float32Array.h>
#include <algorithm>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>
#include <wtf/ZippedRange.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WaveShaperDSPKernel);

WaveShaperDSPKernel::WaveShaperDSPKernel(WaveShaperProcessor* processor)
    : AudioDSPKernel(processor)
{
    if (processor->oversample() != WaveShaperProcessor::OverSampleNone)
        lazyInitializeOversampling();
}

void WaveShaperDSPKernel::lazyInitializeOversampling()
{
    ASSERT(isMainThread());

    if (!m_tempBuffer) {
        m_tempBuffer = makeUnique<AudioFloatArray>(AudioUtilities::renderQuantumSize * 2);
        m_tempBuffer2 = makeUnique<AudioFloatArray>(AudioUtilities::renderQuantumSize * 4);
        m_upSampler = makeUnique<UpSampler>(AudioUtilities::renderQuantumSize);
        m_downSampler = makeUnique<DownSampler>(AudioUtilities::renderQuantumSize * 2);
        m_upSampler2 = makeUnique<UpSampler>(AudioUtilities::renderQuantumSize * 2);
        m_downSampler2 = makeUnique<DownSampler>(AudioUtilities::renderQuantumSize * 4);
    }
}

void WaveShaperDSPKernel::process(std::span<const float> source, std::span<float> destination)
{
    assertIsHeld(waveShaperProcessor()->processLock());
    switch (waveShaperProcessor()->oversample()) {
    case WaveShaperProcessor::OverSampleNone:
        processCurve(source, destination);
        break;
    case WaveShaperProcessor::OverSample2x:
        processCurve2x(source, destination);
        break;
    case WaveShaperProcessor::OverSample4x:
        processCurve4x(source, destination);
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

void WaveShaperDSPKernel::processCurve(std::span<const float> source, std::span<float> destination)
{
    ASSERT(source.data() && destination.data() && waveShaperProcessor());

    assertIsHeld(waveShaperProcessor()->processLock());
    RefPtr curve = waveShaperProcessor()->curve();
    if (!curve) {
        // Act as "straight wire" pass-through if no curve is set.
        memcpySpan(destination, source);
        return;
    }

    auto curveData = curve->typedMutableSpan();

    if (curveData.empty()) {
        memcpySpan(destination, source);
        return;
    }

    // Apply waveshaping curve.
    for (auto [input, output] : zippedRange(source, destination)) {
        float v = (curveData.size() - 1) * 0.5f * (input + 1);
        if (v < 0)
            output = curveData[0];
        else if (v >= curveData.size() - 1)
            output = curveData.back();
        else {
            float k = std::floor(v);
            float f = v - k;
            unsigned kIndex = k;
            output = (1 - f) * curveData[kIndex] + f * curveData[kIndex + 1];
        }
    }
}

void WaveShaperDSPKernel::processCurve2x(std::span<const float> source, std::span<float> destination)
{
    bool isSafe = source.size() == AudioUtilities::renderQuantumSize;
    ASSERT(isSafe);
    if (!isSafe)
        return;

    auto tempP = m_tempBuffer->span().first(source.size() * 2);

    m_upSampler->process(source, tempP);

    // Process at 2x up-sampled rate.
    processCurve(tempP, tempP);

    m_downSampler->process(tempP, destination);
}

void WaveShaperDSPKernel::processCurve4x(std::span<const float> source, std::span<float> destination)
{
    bool isSafe = source.size() == AudioUtilities::renderQuantumSize;
    ASSERT(isSafe);
    if (!isSafe)
        return;

    auto tempP = m_tempBuffer->span().first(source.size() * 2);
    auto tempP2 = m_tempBuffer2->span().first(source.size() * 4);

    m_upSampler->process(source, tempP);
    m_upSampler2->process(tempP, tempP2);

    // Process at 4x up-sampled rate.
    processCurve(tempP2, tempP2);

    m_downSampler2->process(tempP2, tempP);
    m_downSampler->process(tempP, destination);
}

void WaveShaperDSPKernel::reset()
{
    if (m_upSampler) {
        m_upSampler->reset();
        m_downSampler->reset();
        m_upSampler2->reset();
        m_downSampler2->reset();
    }
}

double WaveShaperDSPKernel::latencyTime() const
{
    if (!waveShaperProcessor()->processLock().tryLock())
        return std::numeric_limits<double>::infinity();

    Locker locker { AdoptLock, waveShaperProcessor()->processLock() };

    size_t latencyFrames = 0;
    switch (waveShaperProcessor()->oversample()) {
    case WaveShaperProcessor::OverSampleNone:
        break;
    case WaveShaperProcessor::OverSample2x:
        latencyFrames += m_upSampler->latencyFrames();
        latencyFrames += m_downSampler->latencyFrames();
        break;
    case WaveShaperProcessor::OverSample4x:
        {
            // Account for first stage upsampling.
            latencyFrames += m_upSampler->latencyFrames();
            latencyFrames += m_downSampler->latencyFrames();

            // Account for second stage upsampling.
            // and divide by 2 to get back down to the regular sample-rate.
            size_t latencyFrames2 = (m_upSampler2->latencyFrames() + m_downSampler2->latencyFrames()) / 2;
            latencyFrames += latencyFrames2;
            break;
        }
    default:
        ASSERT_NOT_REACHED();
    }

    return static_cast<double>(latencyFrames) / sampleRate();
}

bool WaveShaperDSPKernel::requiresTailProcessing() const
{
    // Always return true even if the tail time and latency might both be zero.
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
