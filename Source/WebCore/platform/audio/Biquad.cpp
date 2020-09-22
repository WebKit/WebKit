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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "Biquad.h"

#include "DenormalDisabler.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/MathExtras.h>

#if USE(ACCELERATE)
// Work around a bug where VForce.h forward declares std::complex in a way that's incompatible with libc++ complex.
#define __VFORCE_H
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

#if USE(ACCELERATE)
const int kBufferSize = 1024;
#endif

Biquad::Biquad()
{
#if USE(ACCELERATE)
    // Allocate two samples more for filter history
    m_inputBuffer.allocate(kBufferSize + 2);
    m_outputBuffer.allocate(kBufferSize + 2);
#endif

    // Allocate enough space for the a-rate filter coefficients to handle a
    // rendering quantum of 128 frames.
    m_b0.allocate(MaxFramesToProcess);
    m_b1.allocate(MaxFramesToProcess);
    m_b2.allocate(MaxFramesToProcess);
    m_a1.allocate(MaxFramesToProcess);
    m_a2.allocate(MaxFramesToProcess);

    // Initialize as pass-thru (straight-wire, no filter effect)
    setNormalizedCoefficients(0, 1, 0, 0, 1, 0, 0);

    reset(); // clear filter memory
}

Biquad::~Biquad() = default;

void Biquad::process(const float* sourceP, float* destP, size_t framesToProcess)
{
    if (hasSampleAccurateValues()) {
        size_t n = framesToProcess;

        // Create local copies of member variables
        double x1 = m_x1;
        double x2 = m_x2;
        double y1 = m_y1;
        double y2 = m_y2;

        auto* b0 = m_b0.data();
        auto* b1 = m_b1.data();
        auto* b2 = m_b2.data();
        auto* a1 = m_a1.data();
        auto* a2 = m_a2.data();

        for (size_t k = 0; k < n; ++k) {
            // FIXME: this can be optimized by pipelining the multiply adds...
            float x = *sourceP++;
            float y = b0[k] * x + b1[k] * x1 + b2[k] * x2 - a1[k] * y1 - a2[k] * y2;

            *destP++ = y;

            // Update state variables
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
        }

        // Local variables back to member. Flush denormals here so we
        // don't slow down the inner loop above.
        m_x1 = DenormalDisabler::flushDenormalFloatToZero(x1);
        m_x2 = DenormalDisabler::flushDenormalFloatToZero(x2);
        m_y1 = DenormalDisabler::flushDenormalFloatToZero(y1);
        m_y2 = DenormalDisabler::flushDenormalFloatToZero(y2);

        // There is an assumption here that once we have sample accurate values we
        // can never go back to not having sample accurate values. This is
        // currently true in the way AudioParamTimline is implemented: once an
        // event is inserted, sample accurate processing is always enabled.
        //
        // If so, then we never have to update the state variables for the MACOSX
        // path. The structure of the state variable in these cases aren't well
        // documented so it's not clear how to update them anyway.
        return;
    }

#if USE(ACCELERATE)
    auto* inputP = m_inputBuffer.data();
    auto* outputP = m_outputBuffer.data();

    // Set up filter state. This is needed in case we're switching from
    // filtering with variable coefficients (i.e., with automations) to
    // fixed coefficients (without automations).
    inputP[0] = m_x2;
    inputP[1] = m_x1;
    outputP[0] = m_y2;
    outputP[1] = m_y1;

    // Use vecLib if available
    processFast(sourceP, destP, framesToProcess);

    ASSERT(framesToProcess >= 2);

    // Copy the last inputs and outputs to the filter memory variables.
    // This is needed because the next rendering quantum might be an
    // automation which needs the history to continue correctly. Because
    // sourceP and destP can be the same block of memory, we can't read from
    // sourceP to get the last inputs. Fortunately, processFast has put the
    // last inputs in input[0] and input[1].
    m_x1 = inputP[1];
    m_x2 = inputP[0];
    m_y1 = destP[framesToProcess - 1];
    m_y2 = destP[framesToProcess - 2];
#else
    size_t n = framesToProcess;

    // Create local copies of member variables
    double x1 = m_x1;
    double x2 = m_x2;
    double y1 = m_y1;
    double y2 = m_y2;

    double b0 = m_b0[0];
    double b1 = m_b1[0];
    double b2 = m_b2[0];
    double a1 = m_a1[0];
    double a2 = m_a2[0];

    while (n--) {
        // FIXME: this can be optimized by pipelining the multiply adds...
        float x = *sourceP++;
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        *destP++ = y;

        // Update state variables
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
    }

    // Local variables back to member. Flush denormals here so we
    // don't slow down the inner loop above.
    m_x1 = DenormalDisabler::flushDenormalFloatToZero(x1);
    m_x2 = DenormalDisabler::flushDenormalFloatToZero(x2);
    m_y1 = DenormalDisabler::flushDenormalFloatToZero(y1);
    m_y2 = DenormalDisabler::flushDenormalFloatToZero(y2);
#endif
}

#if USE(ACCELERATE)

// Here we have optimized version using Accelerate.framework

void Biquad::processFast(const float* sourceP, float* destP, size_t framesToProcess)
{
    double filterCoefficients[5];
    filterCoefficients[0] = m_b0[0];
    filterCoefficients[1] = m_b1[0];
    filterCoefficients[2] = m_b2[0];
    filterCoefficients[3] = m_a1[0];
    filterCoefficients[4] = m_a2[0];

    double* inputP = m_inputBuffer.data();
    double* outputP = m_outputBuffer.data();

    double* input2P = inputP + 2;
    double* output2P = outputP + 2;

    // Break up processing into smaller slices (kBufferSize) if necessary.

    size_t n = framesToProcess;

    while (n > 0) {
        int framesThisTime = n < kBufferSize ? n : kBufferSize;

        // Copy input to input buffer
        for (int i = 0; i < framesThisTime; ++i)
            input2P[i] = *sourceP++;

        processSliceFast(inputP, outputP, filterCoefficients, framesThisTime);

        // Copy output buffer to output (converts float -> double).
        for (int i = 0; i < framesThisTime; ++i)
            *destP++ = static_cast<float>(output2P[i]);

        n -= framesThisTime;
    }
}

void Biquad::processSliceFast(double* sourceP, double* destP, double* coefficientsP, size_t framesToProcess)
{
    // Use double-precision for filter stability
    vDSP_deq22D(sourceP, 1, coefficientsP, destP, 1, framesToProcess);

    // Save history. Note that sourceP and destP reference m_inputBuffer and m_outputBuffer respectively.
    // These buffers are allocated (in the constructor) with space for two extra samples so it's OK to access
    // array values two beyond framesToProcess.
    sourceP[0] = sourceP[framesToProcess - 2 + 2];
    sourceP[1] = sourceP[framesToProcess - 1 + 2];
    destP[0] = destP[framesToProcess - 2 + 2];
    destP[1] = destP[framesToProcess - 1 + 2];
}

#endif // USE(ACCELERATE)


void Biquad::reset()
{
#if USE(ACCELERATE)
    // Two extra samples for filter history
    double* inputP = m_inputBuffer.data();
    inputP[0] = 0;
    inputP[1] = 0;

    double* outputP = m_outputBuffer.data();
    outputP[0] = 0;
    outputP[1] = 0;
#endif

    m_x1 = m_x2 = m_y1 = m_y2 = 0;

}

void Biquad::setLowpassParams(size_t index, double cutoff, double resonance)
{
    // Limit cutoff to 0 to 1.
    cutoff = std::max(0.0, std::min(cutoff, 1.0));
    
    if (cutoff == 1) {
        // When cutoff is 1, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for lowpass filter
        resonance = pow(10.0, 0.05 * resonance);

        double theta = piDouble * cutoff;
        double alpha = sin(theta) / (2 * resonance);
        double cosw = cos(theta);
        double beta = (1 - cosw) / 2;

        double b0 = beta;
        double b1 = 2 * beta;
        double b2 = beta;

        double a0 = 1 + alpha;
        double a1 = -2 * cosw;
        double a2 = 1 - alpha;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When cutoff is zero, nothing gets through the filter, so set
        // coefficients up correctly.
        setNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
    }
}

void Biquad::setHighpassParams(size_t index, double cutoff, double resonance)
{
    // Limit cutoff to 0 to 1.
    cutoff = std::max(0.0, std::min(cutoff, 1.0));

    if (cutoff == 1) {
        // The z-transform is 0.
        setNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
    } else if (cutoff > 0) {
        // Compute biquad coefficients for highpass filter
        resonance = pow(10.0, 0.05 * resonance);

        double theta = piDouble * cutoff;
        double alpha = sin(theta) / (2 * resonance);
        double cosw = cos(theta);
        double beta = (1 + cosw) / 2;

        double b0 = beta;
        double b1 = -(2 * beta);
        double b2 = beta;

        double a0 = 1 + alpha;
        double a1 = -2 * cosw;
        double a2 = 1 - alpha;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
      // When cutoff is zero, we need to be careful because the above
      // gives a quadratic divided by the same quadratic, with poles
      // and zeros on the unit circle in the same place. When cutoff
      // is zero, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
}

void Biquad::setNormalizedCoefficients(size_t index, double b0, double b1, double b2, double a0, double a1, double a2)
{
    double a0Inverse = 1 / a0;
    
    m_b0[index] = b0 * a0Inverse;
    m_b1[index] = b1 * a0Inverse;
    m_b2[index] = b2 * a0Inverse;
    m_a1[index] = a1 * a0Inverse;
    m_a2[index] = a2 * a0Inverse;
}

void Biquad::setLowShelfParams(size_t index, double frequency, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = std::max(0.0, std::min(frequency, 1.0));
    
    double A = pow(10.0, dbGain / 40);

    if (frequency == 1) {
        // The z-transform is a constant gain.
        setNormalizedCoefficients(index, A * A, 0, 0, 1, 0, 0);
    } else if (frequency > 0) {
        double w0 = piDouble * frequency;
        double S = 1; // filter slope (1 is max value)
        double alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(w0);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double b0 = A * (aPlusOne - aMinusOne * k + k2);
        double b1 = 2 * A * (aMinusOne - aPlusOne * k);
        double b2 = A * (aPlusOne - aMinusOne * k - k2);
        double a0 = aPlusOne + aMinusOne * k + k2;
        double a1 = -2 * (aMinusOne + aPlusOne * k);
        double a2 = aPlusOne + aMinusOne * k - k2;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When frequency is 0, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
}

void Biquad::setHighShelfParams(size_t index, double frequency, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = std::max(0.0, std::min(frequency, 1.0));

    double A = pow(10.0, dbGain / 40);

    if (frequency == 1) {
        // The z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    } else if (frequency > 0) {
        double w0 = piDouble * frequency;
        double S = 1; // filter slope (1 is max value)
        double alpha = 0.5 * sin(w0) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(w0);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double b0 = A * (aPlusOne + aMinusOne * k + k2);
        double b1 = -2 * A * (aMinusOne + aPlusOne * k);
        double b2 = A * (aPlusOne + aMinusOne * k - k2);
        double a0 = aPlusOne - aMinusOne * k + k2;
        double a1 = 2 * (aMinusOne - aPlusOne * k);
        double a2 = aPlusOne - aMinusOne * k - k2;

        setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
    } else {
        // When frequency = 0, the filter is just a gain, A^2.
        setNormalizedCoefficients(index, A * A, 0, 0, 1, 0, 0);
    }
}

void Biquad::setPeakingParams(size_t index, double frequency, double Q, double dbGain)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = std::max(0.0, std::min(frequency, 1.0));

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    double A = pow(10.0, dbGain / 40);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1 + alpha * A;
            double b1 = -2 * k;
            double b2 = 1 - alpha * A;
            double a0 = 1 + alpha / A;
            double a1 = -2 * k;
            double a2 = 1 - alpha / A;

            setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is A^2, so
            // set the filter that way.
            setNormalizedCoefficients(index, A * A, 0, 0, 1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
}

void Biquad::setAllpassParams(size_t index, double frequency, double Q)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = std::max(0.0, std::min(frequency, 1.0));

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1 - alpha;
            double b1 = -2 * k;
            double b2 = 1 + alpha;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is -1, so
            // set the filter that way.
            setNormalizedCoefficients(index, -1, 0, 0, 1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
}

void Biquad::setNotchParams(size_t index, double frequency, double Q)
{
    // Clip frequencies to between 0 and 1, inclusive.
    frequency = std::max(0.0, std::min(frequency, 1.0));

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        if (Q > 0) {
            double w0 = piDouble * frequency;
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);

            double b0 = 1;
            double b1 = -2 * k;
            double b2 = 1;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is 0, so
            // set the filter that way.
            setNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
        }
    } else {
        // When frequency is 0 or 1, the z-transform is 1.
        setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
    }
}

void Biquad::setBandpassParams(size_t index, double frequency, double Q)
{
    // No negative frequencies allowed.
    frequency = std::max(0.0, frequency);

    // Don't let Q go negative, which causes an unstable filter.
    Q = std::max(0.0, Q);

    if (frequency > 0 && frequency < 1) {
        double w0 = piDouble * frequency;
        if (Q > 0) {
            double alpha = sin(w0) / (2 * Q);
            double k = cos(w0);
    
            double b0 = alpha;
            double b1 = 0;
            double b2 = -alpha;
            double a0 = 1 + alpha;
            double a1 = -2 * k;
            double a2 = 1 - alpha;

            setNormalizedCoefficients(index, b0, b1, b2, a0, a1, a2);
        } else {
            // When Q = 0, the above formulas have problems. If we look at
            // the z-transform, we can see that the limit as Q->0 is 1, so
            // set the filter that way.
            setNormalizedCoefficients(index, 1, 0, 0, 1, 0, 0);
        }
    } else {
        // When the cutoff is zero, the z-transform approaches 0, if Q
        // > 0. When both Q and cutoff are zero, the z-transform is
        // pretty much undefined. What should we do in this case?
        // For now, just make the filter 0. When the cutoff is 1, the
        // z-transform also approaches 0.
        setNormalizedCoefficients(index, 0, 0, 0, 1, 0, 0);
    }
}

void Biquad::getFrequencyResponse(unsigned nFrequencies, const float* frequency, float* magResponse, float* phaseResponse)
{
    // Evaluate the Z-transform of the filter at given normalized
    // frequency from 0 to 1. (1 corresponds to the Nyquist
    // frequency.)
    //
    // The z-transform of the filter is
    //
    // H(z) = (b0 + b1*z^(-1) + b2*z^(-2))/(1 + a1*z^(-1) + a2*z^(-2))
    //
    // Evaluate as
    //
    // b0 + (b1 + b2*z1)*z1
    // --------------------
    // 1 + (a1 + a2*z1)*z1
    //
    // with z1 = 1/z and z = exp(j*pi*frequency). Hence z1 = exp(-j*pi*frequency)

    // Make local copies of the coefficients as a micro-optimization.
    double b0 = m_b0[0];
    double b1 = m_b1[0];
    double b2 = m_b2[0];
    double a1 = m_a1[0];
    double a2 = m_a2[0];
    
    for (unsigned k = 0; k < nFrequencies; ++k) {
        if (frequency[k] < 0 || frequency[k] > 1) {
            // Out-of-bounds frequencies should return NaN.
            magResponse[k] = std::nanf("");
            phaseResponse[k] = std::nanf("");
        } else {
            double omega = -piDouble * frequency[k];
            std::complex<double> z = std::complex<double>(cos(omega), sin(omega));
            std::complex<double> numerator = b0 + (b1 + b2 * z) * z;
            std::complex<double> denominator = std::complex<double>(1, 0) + (a1 + a2 * z) * z;
            std::complex<double> response = numerator / denominator;
            magResponse[k] = static_cast<float>(abs(response));
            phaseResponse[k] = static_cast<float>(atan2(imag(response), real(response)));
        }
    }
}

static double repeatedRootResponse(double n, double c1, double c2, double r, double logEPS)
{
    // The response is h(n) = r^(n-2)*[c1*(n+1)*r^2+c2]. We're looking
    // for n such that |h(n)| = eps. Equivalently, we want a root
    // of the equation log(|h(n)|) - log(eps) = 0 or
    //
    //   (n-2)*log(r) + log(|c1*(n+1)*r^2+c2|) - log(eps)
    //
    // This helps with finding a nuemrical solution because this
    // approximately linearizes the response for large n.

    return (n - 2) * std::log(r) + std::log(std::fabs(c1 * (n + 1) * r * r + c2)) - logEPS;
}


// Regula Falsi root finder, Illinois variant
// (https://en.wikipedia.org/wiki/False_position_method#The_Illinois_algorithm).
//
// This finds a root of the repeated root response where the root is
// assumed to lie between |low| and |high|. The response is given by
// |c1|, |c2|, and |r| as determined by |RepeatedRootResponse|.
// |logEPS| is the log the the maximum allowed amplitude in the
// response.
static double rootFinder(double low, double high, double logEPS, double c1, double c2, double r)
{
    // Desired accuray of the root (in frames). This doesn't need to be
    // super-accurate, so half frame is good enough, and should be less
    // than 1 because the algorithm may prematurely terminate.
    constexpr double accuracyThreshold = 0.5;
    // Max number of iterations to do. If we haven't converged by now,
    // just return whatever we've found.
    constexpr int maxIterations = 10;

    int side = 0;
    double root = 0;
    double fLow = repeatedRootResponse(low, c1, c2, r, logEPS);
    double fHigh = repeatedRootResponse(high, c1, c2, r, logEPS);

    // The function values must be finite and have opposite signs!
    ASSERT(std::isfinite(fLow));
    ASSERT(std::isfinite(fHigh));
    ASSERT(fLow * fHigh <= 0);

    int iteration;
    for (iteration = 0; iteration < maxIterations; ++iteration) {
        root = (fLow * high - fHigh * low) / (fLow - fHigh);
        if (fabs(high - low) < accuracyThreshold * std::fabs(high + low))
            break;
        double fr = repeatedRootResponse(root, c1, c2, r, logEPS);

        ASSERT(std::isfinite(fr));

        if (fr * fHigh > 0) {
            // fr and fHigh have same sign. Copy root to fHigh
            high = root;
            fHigh = fr;
            side = -1;
        } else if (fLow * fr > 0) {
            // fr and fLow have same sign. Copy root to fLow
            low = root;
            fLow = fr;
            if (side == 1)
                fHigh /= 2;
            side = 1;
        } else {
            // fLow * fr looks like zero, so assume we've converged.
            break;
        }
    }

    // Want to know if the max number of iterations is ever exceeded so
    // we can understand why that happened.
    ASSERT(iteration < maxIterations);

    return root;
}

double Biquad::tailFrame(size_t coefIndex, double maxFrame)
{
    // The Biquad filter is given by
    //
    //   H(z) = (b0 + b1/z + b2/z^2)/(1 + a1/z + a2/z^2).
    //
    // To compute the tail time, compute the impulse response, h(n), of
    // H(z), which we can do analytically. From this impulse response,
    // find the value n0 where |h(n)| <= eps for n >= n0.
    //
    // Assume first that the two poles of H(z) are not repeated, say r1
    // and r2. Then, we can compute a partial fraction expansion of
    // H(z):
    //
    //   H(z) = (b0+b1/z+b2/z^2)/[(1-r1/z)*(1-r2/z)]
    //        = b0 + C2/(z-r2) - C1/(z-r1)
    //
    //  where
    //    C2 = (b0*r2^2+b1*r2+b2)/(r2-r1)
    //    C1 = (b0*r1^2+b1*r1+b2)/(r2-r1)
    //
    // Expand H(z) then this in powers of 1/z gives:
    //
    //   H(z) = b0 -(C2/r2+C1/r1) + sum(C2*r2^(i-1)/z^i + C1*r1^(i-1)/z^i)
    //
    // Thus, for n > 1 (we don't care about small n),
    //
    //   h(n) = C2*r2^(n-1) + C1*r1^(n-1)
    //
    // We need to find n0 such that |h(n)| < eps for n > n0.
    //
    // Case 1: r1 and r2 are real and distinct, with |r1|>=|r2|.
    //
    // Then
    //
    //   h(n) = C1*r1^(n-1)*(1 + C2/C1*(r2/r1)^(n-1))
    //
    // so
    //
    //   |h(n)| = |C1|*|r1|^(n-1)*|1+C2/C1*(r2/r1)^(n-1)|
    //          <= |C1|*|r1|^(n-1)*[1 + |C2/C1|*|r2/r1|^(n-1)]
    //          <= |C1|*|r1|^(n-1)*[1 + |C2/C1|]
    //
    // so
    //
    //   |h(n)| = |C1|*|r1|^(n-1)*|1+C2/C1*(r2/r1)^(n-1)|
    //          <= |C1|*|r1|^(n-1)*[1 + |C2/C1|*|r2/r1|^(n-1)]
    //          <= |C1|*|r1|^(n-1)*[1 + |C2/C1|]
    //
    // by using the triangle inequality and the fact that |r2|<=|r1|.
    // And we want |h(n)|<=eps which is true if
    //
    //   |C1|*|r1|^(n-1)*[1 + |C2/C1|] <= eps
    //
    // or
    //
    //   n >= 1 + log(eps/C)/log(|r1|)
    //
    // where C = |C1|*[1+|C2/C1|] = |C1| + |C2|.
    //
    // Case 2: r1 and r2 are complex
    //
    // Thne we can write r1=r*exp(i*p) and r2=r*exp(-i*p). So,
    //
    //   |h(n)| = |C2*r^(n-1)*exp(-i*p*(n-1)) + C1*r^(n-1)*exp(i*p*(n-1))|
    //          = |C1|*r^(n-1)*|1 + C2/C1*exp(-i*p*(n-1))/exp(i*n*(n-1))|
    //          <= |C1|*r^(n-1)*[1 + |C2/C1|]
    //
    // Again, this is easily solved to give
    //
    //   n >= 1 + log(eps/C)/log(r)
    //
    // where C = |C1|*[1+|C2/C1|] = |C1| + |C2|.
    //
    // Case 3: Repeated roots, r1=r2=r.
    //
    // In this case,
    //
    //   H(z) = (b0+b1/z+b2/z^2)/[(1-r/z)^2
    //
    // Expanding this in powers of 1/z gives:
    //
    //   H(z) = C1*sum((i+1)*r^i/z^i) - C2 * sum(r^(i-2)/z^i) + b2/r^2
    //        = b2/r^2 + sum([C1*(i+1)*r^i + C2*r^(i-2)]/z^i)
    // where
    //   C1 = (b0*r^2+b1*r+b2)/r^2
    //   C2 = b1*r+2*b2
    //
    // Thus, the impulse response is
    //
    //   h(n) = C1*(n+1)*r^n + C2*r^(n-2)
    //        = r^(n-2)*[C1*(n+1)*r^2+C2]
    //
    // So
    //
    //   |h(n)| = |r|^(n-2)*|C1*(n+1)*r^2+C2|
    //
    // To find n such that |h(n)| < eps, we need a numerical method in
    // general, so there's no real reason to simplify this or use other
    // approximations. Just solve |h(n)|=eps directly.
    //
    // Thus, for an set of filter coefficients, we can compute the tail
    // time.
    //

    // If the maximum amplitude of the impulse response is less than
    // this, we assume that we've reached the tail of the response.
    // Currently, this means that the impulse is less than 1 bit of a
    // 16-bit PCM value.
    constexpr double maxTailAmplitude = 1 / 32768.0;

    // Find the roots of 1+a1/z+a2/z^2 = 0. Or equivalently,
    // z^2+a1*z+a2 = 0. From the quadratic formula the roots are
    // (-a1+/-sqrt(a1^2-4*a2))/2.

    double a1 = m_a1[coefIndex];
    double a2 = m_a2[coefIndex];
    double b0 = m_b0[coefIndex];
    double b1 = m_b1[coefIndex];
    double b2 = m_b2[coefIndex];

    double discrim = a1 * a1 - 4 * a2;
    if (discrim > 0) {
        // Compute the real roots so that r1 has the largest magnitude.
        double rplus = (-a1 + std::sqrt(discrim)) / 2;
        double rminus = (-a1 - std::sqrt(discrim)) / 2;
        double r1 = std::fabs(rplus) >= std::fabs(rminus) ? rplus : rminus;
        // Use the fact that a2 = r1*r2
        double r2 = a2 / r1;

        double c1 = (b0 * r1 * r1 + b1 * r1 + b2) / (r2 - r1);
        double c2 = (b0 * r2 * r2 + b1 * r2 + b2) / (r2 - r1);

        ASSERT(std::isfinite(r1));
        ASSERT(std::isfinite(r2));
        ASSERT(std::isfinite(c1));
        ASSERT(std::isfinite(c2));

        // It's possible for maxTailAmplitude to be greater than c1 + c2.
        // This may produce a negative tail frame. Just clamp the tail
        // frame to 0.
        double tailFrame = clampTo(1 + std::log(maxTailAmplitude / (std::fabs(c1) + std::fabs(c2))) / std::log(std::fabs(r1)), 0);

        ASSERT(std::isfinite(tailFrame));
        return tailFrame;
    }

    if (discrim < 0) {
        // Two complex roots.
        // One root is -a1/2 + i*sqrt(-discrim)/2.
        double x = -a1 / 2;
        double y = std::sqrt(-discrim) / 2;
        std::complex<double> r1(x, y);
        std::complex<double> r2(x, -y);
        double r = std::hypot(x, y);

        ASSERT(std::isfinite(r));

        double tailFrame = 0;
        // It's possible for r to be 1. (LPF with Q very large can cause this.)
        if (r == 1)
            tailFrame = maxFrame;
        else {
            double c1 = std::abs((b0 * r1 * r1 + b1 * r1 + b2) / (r2 - r1));
            double c2 = std::abs((b0 * r2 * r2 + b1 * r2 + b2) / (r2 - r1));

            ASSERT(std::isfinite(c1));
            ASSERT(std::isfinite(c2));
            tailFrame = 1 + std::log(maxTailAmplitude / (c1 + c2)) / std::log(r);
            if (!c1 && !c2) {
                // If c1 = c2 = 0, then H(z) = b0. Hence, there's no tail
                // because this is just a wire from input to output.
                tailFrame = 0;
            } else {
                // Otherwise, check that the tail has finite length. Not
                // strictly necessary, but we want to know if this ever
                // happens.
                ASSERT(std::isfinite(tailFrame));
            }
        }
        return tailFrame;
    }

    // Repeated roots. This should be pretty rare because all the
    // coefficients need to be just the right values to get a
    // discriminant of exactly zero.
    double r = -a1 / 2;

    double tailFrame = 0;
    if (!r) {
        // Double pole at 0. This just delays the signal by 2 frames,
        // so set the tail frame to 2.
        tailFrame = 2;
    } else {
        double c1 = (b0 * r * r + b1 * r + b2) / (r * r);
        double c2 = b1 * r + 2 * b2;

        ASSERT(std::isfinite(c1));
        ASSERT(std::isfinite(c2));

        // It can happen that c1=c2=0. This basically means that H(z) =
        // constant, which is the limiting case for several of the
        // biquad filters.
        if (!c1 && !c2)
            tailFrame = 0;
        else {
            // The function c*(n+1)*r^n is not monotonic, but it's easy to
            // find the max point since the derivative is
            // c*r^n*(1+(n+1)*log(r)). This has a root at
            // -(1+log(r))/log(r). so we can start our search from that
            // point to maxFrames.

            double low = std::clamp(-(1 + std::log(r)) / std::log(r), 1.0, static_cast<double>(maxFrame - 1));
            double high = maxFrame;

            ASSERT(std::isfinite(low));
            ASSERT(std::isfinite(high));

            tailFrame = rootFinder(low, high, std::log(maxTailAmplitude), c1, c2, r);
        }
    }

    return tailFrame;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
