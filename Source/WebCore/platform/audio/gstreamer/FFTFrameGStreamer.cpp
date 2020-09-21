/*
 *  Copyright (C) 2012 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// FFTFrame implementation using the GStreamer FFT library.

#include "config.h"

#if USE(WEBAUDIO_GSTREAMER)

#include "FFTFrame.h"

#include "VectorMath.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

namespace {

// FIXME: It is unclear what the limits are for gst_fft so we use the same ones as on macOS for now.
const int kMinFFTPow2Size = 2;
const int kMaxFFTPow2Size = 24;

size_t unpackedFFTDataSize(unsigned fftSize)
{
    return fftSize / 2 + 1;
}

} // anonymous namespace

namespace WebCore {

// Normal constructor: allocates for a given fftSize.
FFTFrame::FFTFrame(unsigned fftSize)
    : m_FFTSize(fftSize)
    , m_log2FFTSize(static_cast<unsigned>(log2(fftSize)))
    , m_complexData(makeUniqueArray<GstFFTF32Complex>(unpackedFFTDataSize(m_FFTSize)))
    , m_realData(unpackedFFTDataSize(m_FFTSize))
    , m_imagData(unpackedFFTDataSize(m_FFTSize))
{
    int fftLength = gst_fft_next_fast_length(m_FFTSize);
    m_fft = gst_fft_f32_new(fftLength, FALSE);
    m_inverseFft = gst_fft_f32_new(fftLength, TRUE);
}

// Creates a blank/empty frame (interpolate() must later be called).
FFTFrame::FFTFrame()
    : m_FFTSize(0)
    , m_log2FFTSize(0)
{
    int fftLength = gst_fft_next_fast_length(m_FFTSize);
    m_fft = gst_fft_f32_new(fftLength, FALSE);
    m_inverseFft = gst_fft_f32_new(fftLength, TRUE);
}

// Copy constructor.
FFTFrame::FFTFrame(const FFTFrame& frame)
    : m_FFTSize(frame.m_FFTSize)
    , m_log2FFTSize(frame.m_log2FFTSize)
    , m_complexData(makeUniqueArray<GstFFTF32Complex>(unpackedFFTDataSize(m_FFTSize)))
    , m_realData(unpackedFFTDataSize(frame.m_FFTSize))
    , m_imagData(unpackedFFTDataSize(frame.m_FFTSize))
{
    int fftLength = gst_fft_next_fast_length(m_FFTSize);
    m_fft = gst_fft_f32_new(fftLength, FALSE);
    m_inverseFft = gst_fft_f32_new(fftLength, TRUE);

    // Copy/setup frame data.
    memcpy(realData(), frame.realData(), sizeof(float) * unpackedFFTDataSize(m_FFTSize));
    memcpy(imagData(), frame.imagData(), sizeof(float) * unpackedFFTDataSize(m_FFTSize));
}

void FFTFrame::initialize()
{
}

void FFTFrame::cleanup()
{
}

FFTFrame::~FFTFrame()
{
    if (!m_fft)
        return;

    gst_fft_f32_free(m_fft);
    m_fft = 0;

    gst_fft_f32_free(m_inverseFft);
    m_inverseFft = 0;
}

void FFTFrame::doFFT(const float* data)
{
    gst_fft_f32_fft(m_fft, data, m_complexData.get());
}

void FFTFrame::doInverseFFT(float* data)
{
    //  Merge the real and imaginary vectors to complex vector.
    float* realData = m_realData.data();
    float* imagData = m_imagData.data();

    for (size_t i = 0; i < unpackedFFTDataSize(m_FFTSize); ++i) {
        m_complexData[i].i = imagData[i];
        m_complexData[i].r = realData[i];
    }

    gst_fft_f32_inverse_fft(m_inverseFft, m_complexData.get(), data);

    // Scale so that a forward then inverse FFT yields exactly the original data.
    const float scaleFactor = 1.0 / m_FFTSize;
    VectorMath::vsmul(data, 1, &scaleFactor, data, 1, m_FFTSize);
}

float* FFTFrame::realData() const
{
    return const_cast<float*>(m_realData.data());
}

float* FFTFrame::imagData() const
{
    return const_cast<float*>(m_imagData.data());
}

int FFTFrame::minFFTSize()
{
    return 1 << kMinFFTPow2Size;
}

int FFTFrame::maxFFTSize()
{
    return 1 << kMaxFFTPow2Size;
}

} // namespace WebCore

#endif // USE(WEBAUDIO_GSTREAMER)
