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

#if USE(GSTREAMER)

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
    memcpy(realData().data(), frame.realData().data(), sizeof(float) * realData().size());
    memcpy(imagData().data(), frame.imagData().data(), sizeof(float) * imagData().size());
}

void FFTFrame::initialize()
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

    float* imagData = m_imagData.data();
    float* realData = m_realData.data();
    for (unsigned i = 0; i < unpackedFFTDataSize(m_FFTSize); ++i) {
        imagData[i] = m_complexData[i].i;
        realData[i] = m_complexData[i].r;
    }
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
    VectorMath::multiplyByScalar(data, 1.0 / m_FFTSize, data, m_FFTSize);
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

#endif // USE(GSTREAMER)
