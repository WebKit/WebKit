/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "FFTFrame.h"

#include "VectorMath.h"
#include <cmath>
#include <public/FFTFrame.h>
#include <public/Platform.h>

namespace WebCore {

static inline void scalePlanarData(float* data, const unsigned size, const float scale)
{
    VectorMath::vsmul(data, 1, &scale, data, 1, size);
}

FFTFrame::FFTFrame(unsigned fftSize)
    : m_FFTSize(fftSize)
    , m_log2FFTSize(static_cast<unsigned>(log2(fftSize)))
{
    m_fftFrame = adoptPtr(Nix::Platform::current()->createFFTFrame(fftSize));
}

FFTFrame::FFTFrame(const FFTFrame& frame)
    : m_FFTSize(frame.m_FFTSize)
    , m_log2FFTSize(frame.m_log2FFTSize)
{
    m_fftFrame = adoptPtr(frame.m_fftFrame->copy());
}

FFTFrame::~FFTFrame()
{
}

void FFTFrame::multiply(const FFTFrame& frame)
{
    float* realP1 = realData();
    float* imagP1 = imagData();
    const float* realP2 = frame.realData();
    const float* imagP2 = frame.imagData();

    const size_t size = m_fftFrame->frequencyDomainSampleCount();
    VectorMath::zvmul(realP1, imagP1, realP2, imagP2, realP1, imagP1, size);

    // Scale accounts the peculiar scaling of vecLib on the Mac.
    // This ensures the right scaling all the way back to inverse FFT.
    // FIXME: if we change the scaling on the Mac then this scale
    // factor will need to change too.
    const float scale = 0.5;
    scalePlanarData(realP1, size, scale);
    scalePlanarData(imagP1, size, scale);
}

// Provides time domain samples in argument "data". Frame will store the transformed data.
void FFTFrame::doFFT(const float* data)
{
    m_fftFrame->doFFT(data);
    // Scale the frequency domain data to match vecLib's scale factor
    // on the Mac. FIXME: if we change the definition of FFTFrame to
    // eliminate this scale factor then this code will need to change.
    const float scale = 2;
    const size_t size = m_fftFrame->frequencyDomainSampleCount();
    scalePlanarData(realData(), size, scale);
    scalePlanarData(imagData(), size, scale);
}

// Calculates inverse transform from the stored data, putting the results in argument 'data'.
void FFTFrame::doInverseFFT(float* data)
{
    m_fftFrame->doInverseFFT(data);

    // Scale so that a forward then inverse FFT yields exactly the original data.
    const float scale = 1.0 / (2 * m_FFTSize);
    for (unsigned i = 0; i < m_FFTSize; ++i)
        data[i] = scale * data[i];
}

void FFTFrame::initialize()
{
}

void FFTFrame::cleanup()
{
}

float* FFTFrame::realData() const
{
    return m_fftFrame->realData();
}

float* FFTFrame::imagData() const
{
    return m_fftFrame->imagData();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
