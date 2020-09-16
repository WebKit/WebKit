/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "PeriodicWave.h"

#include "BaseAudioContext.h"
#include "FFTFrame.h"
#include "VectorMath.h"
#include <algorithm>

// The number of bands per octave. Each octave will have this many entries in the wave tables.
constexpr unsigned NumberOfOctaveBands = 3;

// The max length of a periodic wave. This must be a power of two greater than
// or equal to 2048 and must be supported by the FFT routines.
constexpr unsigned MaxPeriodicWaveSize = 16384;

constexpr float CentsPerRange = 1200 / NumberOfOctaveBands;

namespace WebCore {
    
using namespace VectorMath;

Ref<PeriodicWave> PeriodicWave::create(float sampleRate, Float32Array& real, Float32Array& imaginary)
{
    ASSERT(real.length() == imaginary.length());

    auto waveTable = adoptRef(*new PeriodicWave(sampleRate));
    waveTable->createBandLimitedTables(real.data(), imaginary.data(), real.length());
    return waveTable;
}

ExceptionOr<Ref<PeriodicWave>> PeriodicWave::create(BaseAudioContext& context, PeriodicWaveOptions&& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    Vector<float> real;
    Vector<float> imag;
    
    if (options.real && options.imag) {
        if (options.real->size() != options.imag->size())
            return Exception { IndexSizeError, "real and imag have different lengths"_s };
        if (options.real->size() < 2)
            return Exception { IndexSizeError, "real's length cannot be less than 2"_s };
        if (options.imag->size() < 2)
            return Exception { IndexSizeError, "imag's length cannot be less than 2"_s };
        real = WTFMove(*options.real);
        imag = WTFMove(*options.imag);
    } else if (options.real) {
        if (options.real->size() < 2)
            return Exception { IndexSizeError, "real's length cannot be less than 2"_s };
        real = WTFMove(*options.real);
        imag.fill(0, real.size());
    } else if (options.imag) {
        if (options.imag->size() < 2)
            return Exception { IndexSizeError, "imag's length cannot be less than 2"_s };
        imag = WTFMove(*options.imag);
        real.fill(0, imag.size());
    } else {
        real.fill(0, 2);
        imag.fill(0, 2);
        imag[1] = 1;
    }

    real[0] = 0;
    imag[0] = 0;
    
    auto waveTable = adoptRef(*new PeriodicWave(context.sampleRate()));
    waveTable->createBandLimitedTables(real.data(), imag.data(), real.size(), options.disableNormalization ? ShouldDisableNormalization::Yes : ShouldDisableNormalization::No);
    return waveTable;
}

Ref<PeriodicWave> PeriodicWave::createSine(float sampleRate)
{
    Ref<PeriodicWave> waveTable = adoptRef(*new PeriodicWave(sampleRate));
    waveTable->generateBasicWaveform(Type::Sine);
    return waveTable;
}

Ref<PeriodicWave> PeriodicWave::createSquare(float sampleRate)
{
    Ref<PeriodicWave> waveTable = adoptRef(*new PeriodicWave(sampleRate));
    waveTable->generateBasicWaveform(Type::Square);
    return waveTable;
}

Ref<PeriodicWave> PeriodicWave::createSawtooth(float sampleRate)
{
    Ref<PeriodicWave> waveTable = adoptRef(*new PeriodicWave(sampleRate));
    waveTable->generateBasicWaveform(Type::Sawtooth);
    return waveTable;
}

Ref<PeriodicWave> PeriodicWave::createTriangle(float sampleRate)
{
    Ref<PeriodicWave> waveTable = adoptRef(*new PeriodicWave(sampleRate));
    waveTable->generateBasicWaveform(Type::Triangle);
    return waveTable;
}

PeriodicWave::PeriodicWave(float sampleRate)
    : m_sampleRate(sampleRate)
    , m_numberOfRanges(0.5 + NumberOfOctaveBands * log2f(periodicWaveSize()))
    , m_centsPerRange(CentsPerRange)
{
    float nyquist = 0.5 * m_sampleRate;
    m_lowestFundamentalFrequency = nyquist / maxNumberOfPartials();
    m_rateScale = periodicWaveSize() / m_sampleRate;
}

void PeriodicWave::waveDataForFundamentalFrequency(float fundamentalFrequency, float* &lowerWaveData, float* &higherWaveData, float& tableInterpolationFactor)
{
    // Negative frequencies are allowed, in which case we alias to the positive frequency.
    fundamentalFrequency = fabsf(fundamentalFrequency);

    // Calculate the pitch range.
    float ratio = fundamentalFrequency > 0 ? fundamentalFrequency / m_lowestFundamentalFrequency : 0.5;
    float centsAboveLowestFrequency = log2f(ratio) * 1200;

    // Add one to round-up to the next range just in time to truncate partials before aliasing occurs.
    float pitchRange = 1 + centsAboveLowestFrequency / m_centsPerRange;

    pitchRange = std::max(pitchRange, 0.0f);
    pitchRange = std::min(pitchRange, static_cast<float>(m_numberOfRanges - 1));

    // The words "lower" and "higher" refer to the table data having the lower and higher numbers of partials.
    // It's a little confusing since the range index gets larger the more partials we cull out.
    // So the lower table data will have a larger range index.
    unsigned rangeIndex1 = static_cast<unsigned>(pitchRange);
    unsigned rangeIndex2 = rangeIndex1 < m_numberOfRanges - 1 ? rangeIndex1 + 1 : rangeIndex1;

    lowerWaveData = m_bandLimitedTables[rangeIndex2]->data();
    higherWaveData = m_bandLimitedTables[rangeIndex1]->data();
    
    // Ranges from 0 -> 1 to interpolate between lower -> higher.
    tableInterpolationFactor = pitchRange - rangeIndex1;
}

unsigned PeriodicWave::maxNumberOfPartials() const
{
    return periodicWaveSize() / 2;
}

unsigned PeriodicWave::numberOfPartialsForRange(unsigned rangeIndex) const
{
    // Number of cents below nyquist where we cull partials.
    float centsToCull = rangeIndex * m_centsPerRange;

    // A value from 0 -> 1 representing what fraction of the partials to keep.
    float cullingScale = pow(2, -centsToCull / 1200);

    // The very top range will have all the partials culled.
    unsigned numberOfPartials = cullingScale * maxNumberOfPartials();

    return numberOfPartials;
}

// Convert into time-domain wave tables.
// One table is created for each range for non-aliasing playback at different playback rates.
// Thus, higher ranges have more high-frequency partials culled out.
void PeriodicWave::createBandLimitedTables(const float* realData, const float* imagData, unsigned numberOfComponents, ShouldDisableNormalization disableNormalization)
{
    float normalizationScale = 1;

    unsigned fftSize = periodicWaveSize();
    unsigned halfSize = fftSize / 2;
    unsigned i;
    
    numberOfComponents = std::min(numberOfComponents, halfSize);

    m_bandLimitedTables.reserveCapacity(m_numberOfRanges);

    for (unsigned rangeIndex = 0; rangeIndex < m_numberOfRanges; ++rangeIndex) {
        // This FFTFrame is used to cull partials (represented by frequency bins).
        FFTFrame frame(fftSize);
        float* realP = frame.realData();
        float* imagP = frame.imagData();

        // Copy from loaded frequency data and scale.
        float scale = fftSize;
        vsmul(realData, 1, &scale, realP, 1, numberOfComponents);
        scale = -scale;
        vsmul(imagData, 1, &scale, imagP, 1, numberOfComponents);

        // Find the starting bin where we should start culling.
        // We need to clear out the highest frequencies to band-limit the waveform.
        unsigned numberOfPartials = numberOfPartialsForRange(rangeIndex);

        // If fewer components were provided than 1/2 FFT size, then clear the
        // remaining bins. We also need to cull the aliasing partials for this
        // pitch range.
        for (i = std::min(numberOfComponents, numberOfPartials + 1); i < halfSize; ++i) {
            realP[i] = 0;
            imagP[i] = 0;
        }

        // Clear packed-nyquist and any DC-offset.
        realP[0] = 0;
        imagP[0] = 0;

        // Create the band-limited table.
        unsigned waveSize = periodicWaveSize();
        m_bandLimitedTables.append(makeUnique<AudioFloatArray>(waveSize));

        // Apply an inverse FFT to generate the time-domain table data.
        float* data = m_bandLimitedTables[rangeIndex]->data();
        frame.doInverseFFT(data);

        // For the first range (which has the highest power), calculate its peak value then compute normalization scale.
        if (disableNormalization == ShouldDisableNormalization::No) {
            if (!rangeIndex) {
                float maxValue;
                vmaxmgv(data, 1, &maxValue, fftSize);

                if (maxValue)
                    normalizationScale = 1.0f / maxValue;
            }
        }

        // Apply normalization scale.
        vsmul(data, 1, &normalizationScale, data, 1, fftSize);
    }
}

void PeriodicWave::generateBasicWaveform(Type shape)
{
    unsigned fftSize = periodicWaveSize();
    unsigned halfSize = fftSize / 2;

    AudioFloatArray real(halfSize);
    AudioFloatArray imag(halfSize);
    float* realP = real.data();
    float* imagP = imag.data();

    // Clear DC and Nyquist.
    realP[0] = 0;
    imagP[0] = 0;

    for (unsigned n = 1; n < halfSize; ++n) {
        float piFactor = 2 / (n * piFloat);

        // All waveforms are odd functions with a positive slope at time 0. Hence
        // the coefficients for cos() are always 0.

        // Fourier coefficients according to standard definition:
        // b = 1/pi*integrate(f(x)*sin(n*x), x, -pi, pi)
        //   = 2/pi*integrate(f(x)*sin(n*x), x, 0, pi)
        // since f(x) is an odd function.

        float b; // Coefficient for sin().

        // Calculate Fourier coefficients depending on the shape.
        // Note that the overall scaling (magnitude) of the waveforms is normalized in createBandLimitedTables().
        switch (shape) {
        case Type::Sine:
            // Standard sine wave function.
            b = (n == 1) ? 1 : 0;
            break;
        case Type::Square:
            // Square-shaped waveform with the first half its maximum value and the
            // second half its minimum value.
            //
            // See http://mathworld.wolfram.com/FourierSeriesSquareWave.html
            //
            // b[n] = 2/n/pi*(1-(-1)^n)
            //      = 4/n/pi for n odd and 0 otherwise.
            //      = 2*(2/(n*pi)) for n odd
            b = (n & 1) ? 2 * piFactor : 0;
            break;
        case Type::Sawtooth:
            // Sawtooth-shaped waveform with the first half ramping from zero to
            // maximum and the second half from minimum to zero.
            //
            // b[n] = -2*(-1)^n/pi/n
            //      = (2/(n*pi))*(-1)^(n+1)
            b = piFactor * ((n & 1) ? 1 : -1);
            break;
        case Type::Triangle:
            // Triangle-shaped waveform going from 0 at time 0 to 1 at time pi/2 and
            // back to 0 at time pi.
            //
            // See http://mathworld.wolfram.com/FourierSeriesTriangleWave.html
            //
            // b[n] = 8*sin(pi*k/2)/(pi*k)^2
            //      = 8/pi^2/n^2*(-1)^((n-1)/2) for n odd and 0 otherwise
            //      = 2*(2/(n*pi))^2 * (-1)^((n-1)/2)
            if (n & 1)
                b = 2 * (piFactor * piFactor) * ((((n - 1) >> 1) & 1) ? -1 : 1);
            else
                b = 0;
            break;
        }

        realP[n] = 0;
        imagP[n] = b;
    }

    createBandLimitedTables(realP, imagP, halfSize);
}

unsigned PeriodicWave::periodicWaveSize() const
{
    // Choose an appropriate wave size for the given sample rate. This allows us
    // to use shorter FFTs when possible to limit the complexity. The breakpoints
    // here are somewhat arbitrary, but we want sample rates around 44.1 kHz or so
    // to have a size of 4096 to preserve backward compatibility.
    if (m_sampleRate <= 24000)
        return 2048;

    if (m_sampleRate <= 88200)
        return 4096;

    return MaxPeriodicWaveSize;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
