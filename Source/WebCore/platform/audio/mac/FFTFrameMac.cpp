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

// Mac OS X - specific FFTFrame implementation

#include "config.h"

#if ENABLE(WEB_AUDIO)

#if OS(DARWIN)

#include "FFTFrame.h"

#include "VectorMath.h"
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WebCore {

constexpr unsigned kMinFFTPow2Size = 2;
constexpr unsigned kMaxFFTPow2Size = 24;

static Lock fftSetupsLock;

static Vector<FFTSetup>& fftSetups()
{
    ASSERT(fftSetupsLock.isHeld());
    static NeverDestroyed<Vector<FFTSetup>> fftSetups(kMaxFFTPow2Size, nullptr);
    return fftSetups;
}

// Normal constructor: allocates for a given fftSize
FFTFrame::FFTFrame(unsigned fftSize)
    : m_realData(fftSize)
    , m_imagData(fftSize)
{
    m_FFTSize = fftSize;
    m_log2FFTSize = static_cast<unsigned>(log2(fftSize));

    // We only allow power of two
    ASSERT(1UL << m_log2FFTSize == m_FFTSize);

    // Lazily create and share fftSetup with other frames
    m_FFTSetup = fftSetupForSize(fftSize);

    // Setup frame data
    m_frame.realp = m_realData.data();
    m_frame.imagp = m_imagData.data();
}

// Creates a blank/empty frame (interpolate() must later be called)
FFTFrame::FFTFrame()
{
    // Later will be set to correct values when interpolate() is called
    m_frame.realp = 0;
    m_frame.imagp = 0;

    m_FFTSize = 0;
    m_log2FFTSize = 0;
}

// Copy constructor
FFTFrame::FFTFrame(const FFTFrame& frame)
    : m_FFTSize(frame.m_FFTSize)
    , m_log2FFTSize(frame.m_log2FFTSize)
    , m_FFTSetup(frame.m_FFTSetup)
    , m_realData(frame.m_FFTSize)
    , m_imagData(frame.m_FFTSize)
{
    // Setup frame data
    m_frame.realp = m_realData.data();
    m_frame.imagp = m_imagData.data();

    // Copy/setup frame data
    memcpy(realData().data(), frame.m_frame.realp, sizeof(float) * realData().size());
    memcpy(imagData().data(), frame.m_frame.imagp, sizeof(float) * imagData().size());
}

FFTFrame::~FFTFrame() = default;

void FFTFrame::doFFT(const float* data)
{
    unsigned halfSize = m_FFTSize / 2;
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(data), 2, &m_frame, 1, halfSize);
    vDSP_fft_zrip(m_FFTSetup, &m_frame, 1, m_log2FFTSize, FFT_FORWARD);

    RELEASE_ASSERT(realData().size() >= halfSize);
    RELEASE_ASSERT(imagData().size() >= halfSize);

    // To provide the best possible execution speeds, the vDSP library's functions don't always adhere strictly
    // to textbook formulas for Fourier transforms, and must be scaled accordingly.
    // (See https://developer.apple.com/library/archive/documentation/Performance/Conceptual/vDSP_Programming_Guide/UsingFourierTransforms/UsingFourierTransforms.html#//apple_ref/doc/uid/TP40005147-CH3-SW5)
    // In the case of a Real forward Transform like above: RFimp = RFmath * 2 so we need to divide the output
    // by 2 to get the correct value.
    VectorMath::multiplyByScalar(realData().data(), 0.5, realData().data(), halfSize);
    VectorMath::multiplyByScalar(imagData().data(), 0.5, imagData().data(), halfSize);
}

void FFTFrame::doInverseFFT(float* data)
{
    vDSP_fft_zrip(m_FFTSetup, &m_frame, 1, m_log2FFTSize, FFT_INVERSE);
    vDSP_ztoc(&m_frame, 1, (DSPComplex*)data, 2, m_FFTSize / 2);

    // Do final scaling so that x == IFFT(FFT(x))
    VectorMath::multiplyByScalar(data, 1.0f / m_FFTSize, data, m_FFTSize);
}

FFTSetup FFTFrame::fftSetupForSize(unsigned fftSize)
{
    auto pow2size = static_cast<size_t>(log2(fftSize));
    ASSERT(pow2size < kMaxFFTPow2Size);

    auto locker = holdLock(fftSetupsLock);
    auto& fftSetup = fftSetups().at(pow2size);
    if (!fftSetup)
        fftSetup = vDSP_create_fftsetup(pow2size, FFT_RADIX2);

    return fftSetup;
}

int FFTFrame::minFFTSize()
{
    return 1 << kMinFFTPow2Size;
}

int FFTFrame::maxFFTSize()
{
    return 1 << kMaxFFTPow2Size;
}

void FFTFrame::initialize()
{
}

} // namespace WebCore

#endif // #if OS(DARWIN)

#endif // ENABLE(WEB_AUDIO)
