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

#ifndef FFTFrame_h
#define FFTFrame_h

#include "AudioArray.h"

#if USE(WEBAUDIO_GSTREAMER)
#include <glib.h>
G_BEGIN_DECLS
#include <gst/fft/gstfftf32.h>
G_END_DECLS
#endif // USE(WEBAUDIO_GSTREAMER)

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#include <memory>
#include <wtf/Forward.h>
#include <wtf/UniqueArray.h>

namespace WebCore {

// Defines the interface for an "FFT frame", an object which is able to perform a forward
// and reverse FFT, internally storing the resultant frequency-domain data.

class FFTFrame {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // The constructors, destructor, and methods up to the CROSS-PLATFORM section have platform-dependent implementations.

    FFTFrame(unsigned fftSize);
    FFTFrame(); // creates a blank/empty frame for later use with createInterpolatedFrame()
    FFTFrame(const FFTFrame& frame);
    ~FFTFrame();

    static void initialize();
    static void cleanup();
    void doFFT(const float* data);
    void doInverseFFT(float* data);
    void multiply(const FFTFrame& frame); // multiplies ourself with frame : effectively operator*=()

    float* realData() const;
    float* imagData() const;

    void print(); // for debugging

    // CROSS-PLATFORM
    // The remaining public methods have cross-platform implementations:

    // Interpolates from frame1 -> frame2 as x goes from 0.0 -> 1.0
    static std::unique_ptr<FFTFrame> createInterpolatedFrame(const FFTFrame& frame1, const FFTFrame& frame2, double x);

    void doPaddedFFT(const float* data, size_t dataSize); // zero-padding with dataSize <= fftSize
    double extractAverageGroupDelay();
    void addConstantGroupDelay(double sampleFrameDelay);

    unsigned fftSize() const { return m_FFTSize; }
    unsigned log2FFTSize() const { return m_log2FFTSize; }

private:
    unsigned m_FFTSize;
    unsigned m_log2FFTSize;

    void interpolateFrequencyComponents(const FFTFrame& frame1, const FFTFrame& frame2, double x);

#if USE(ACCELERATE)
    DSPSplitComplex& dspSplitComplex() { return m_frame; }
    DSPSplitComplex dspSplitComplex() const { return m_frame; }

    static FFTSetup fftSetupForSize(unsigned fftSize);

    static FFTSetup* fftSetups;

    FFTSetup m_FFTSetup;

    DSPSplitComplex m_frame;
    AudioFloatArray m_realData;
    AudioFloatArray m_imagData;
#endif

#if USE(WEBAUDIO_GSTREAMER)
    GstFFTF32* m_fft;
    GstFFTF32* m_inverseFft;
    UniqueArray<GstFFTF32Complex> m_complexData;
    AudioFloatArray m_realData;
    AudioFloatArray m_imagData;
#endif // USE(WEBAUDIO_GSTREAMER)
};

} // namespace WebCore

#endif // FFTFrame_h
