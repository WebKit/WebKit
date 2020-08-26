/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
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
#include "IIRFilter.h"

#include "AudioArray.h"
#include "VectorMath.h"
#include <algorithm>
#include <complex>
#include <wtf/MathExtras.h>

namespace WebCore {

constexpr unsigned iirRenderingQuantum = 128;

// The length of the memory buffers for the IIR filter. This MUST be a power of
// two and must be greater than the possible length of the filter coefficients.
constexpr int bufferLength = 32;
static_assert(bufferLength >= IIRFilter::maxOrder + 1, "Internal IIR buffer length must be greater than maximum IIR Filter order.");

static std::complex<double> evaluatePolynomial(const double* coefficients, std::complex<double> z, int order)
{
    // Use Horner's method to evaluate the polynomial P(z) = sum(coef[k]*z^k, k, 0, order);
    std::complex<double> result = 0;
    for (int k = order; k >= 0; --k)
        result = result * z + std::complex<double>(coefficients[k]);
    return result;
}

IIRFilter::IIRFilter(const Vector<double>& feedforward, const Vector<double>& feedback)
    : m_feedforward(feedforward)
    , m_feedback(feedback)
{
    m_xBuffer.fill(0, bufferLength);
    m_yBuffer.fill(0, bufferLength);
}

void IIRFilter::reset()
{
    m_xBuffer.fill(0);
    m_yBuffer.fill(0);
    m_bufferIndex = 0;
}

void IIRFilter::process(const float* source, float* destination, size_t framesToProcess)
{
    // Compute
    //
    //   y[n] = sum(b[k] * x[n - k], k = 0, M) - sum(a[k] * y[n - k], k = 1, N)
    //
    // where b[k] are the feedforward coefficients and a[k] are the feedback
    // coefficients of the filter.
    // This is a Direct Form I implementation of an IIR Filter. Should we
    // consider doing a different implementation such as Transposed Direct Form
    // II?
    const double* feedback = m_feedback.data();
    const double* feedforward = m_feedforward.data();

    ASSERT(feedback);
    ASSERT(feedforward);

    // Sanity check to see if the feedback coefficients have been scaled appropriately. It must be EXACTLY 1!
    ASSERT(feedback[0] == 1);

    int feedbackLength = m_feedback.size();
    int feedforwardLength = m_feedforward.size();
    int minLength = std::min(feedbackLength, feedforwardLength);

    double* xBuffer = m_xBuffer.data();
    double* yBuffer = m_yBuffer.data();

    for (size_t n = 0; n < framesToProcess; ++n) {
        // To help minimize roundoff, we compute using double's, even though the
        // filter coefficients only have single precision values.
        double yn = feedforward[0] * source[n];

        // Run both the feedforward and feedback terms together, when possible.
        for (int k = 1; k < minLength; ++k) {
            int m = (m_bufferIndex - k) & (bufferLength - 1);
            yn += feedforward[k] * xBuffer[m];
            yn -= feedback[k] * yBuffer[m];
        }

        // Handle any remaining feedforward or feedback terms.
        for (int k = minLength; k < feedforwardLength; ++k)
            yn += feedforward[k] * xBuffer[(m_bufferIndex - k) & (bufferLength - 1)];
        for (int k = minLength; k < feedbackLength; ++k)
            yn -= feedback[k] * yBuffer[(m_bufferIndex - k) & (bufferLength - 1)];

        // Save the current input and output values in the memory buffers for the
        // next output.
        m_xBuffer[m_bufferIndex] = source[n];
        m_yBuffer[m_bufferIndex] = yn;

        m_bufferIndex = (m_bufferIndex + 1) & (bufferLength - 1);

        destination[n] = yn;
    }
}

void IIRFilter::getFrequencyResponse(unsigned length, const float* frequency, float* magResponse, float* phaseResponse)
{
    // Evaluate the z-transform of the filter at the given normalized frequencies
    // from 0 to 1. (One corresponds to the Nyquist frequency.)
    //
    // The z-tranform of the filter is
    //
    // H(z) = sum(b[k]*z^(-k), k, 0, M) / sum(a[k]*z^(-k), k, 0, N);
    //
    // The desired frequency response is H(exp(j*omega)), where omega is in [0,
    // 1).
    //
    // Let P(x) = sum(c[k]*x^k, k, 0, P) be a polynomial of order P. Then each of
    // the sums in H(z) is equivalent to evaluating a polynomial at the point
    // 1/z.

    for (unsigned k = 0; k < length; ++k) {
        if (frequency[k] < 0 || frequency[k] > 1) {
            // Out-of-bounds frequencies should return NaN.
            magResponse[k] = std::nanf("");
            phaseResponse[k] = std::nanf("");
        } else {
            // zRecip = 1/z = exp(-j*frequency)
            double omega = -piDouble * frequency[k];
            auto zRecip = std::complex<double>(cos(omega), sin(omega));

            auto numerator = evaluatePolynomial(m_feedforward.data(), zRecip, m_feedforward.size() - 1);
            auto denominator = evaluatePolynomial(m_feedback.data(), zRecip, m_feedback.size() - 1);
            auto response = numerator / denominator;
            magResponse[k] = static_cast<float>(abs(response));
            phaseResponse[k] = static_cast<float>(atan2(imag(response), real(response)));
        }
    }
}

double IIRFilter::tailTime(double sampleRate, bool isFilterStable)
{
    // The maximum tail time. This is somewhat arbitrary, but we're assuming that
    // no one is going to expect the IIRFilter to produce an output after this
    // much time after the inputs have stopped.
    constexpr double maxTailTime = 10;

    // If the maximum amplitude of the impulse response is less than this, we
    // assume that we've reached the tail of the response. Currently, this means
    // that the impulse is less than 1 bit of a 16-bit PCM value.
    constexpr float maxTailAmplitude = 1 / 32768.0;

    // If filter is not stable, just return max tail. Since the filter is not
    // stable, the impulse response won't converge to zero, so we don't need to
    // find the impulse response to find the actual tail time.
    if (!isFilterStable)
        return maxTailTime;

    // How to compute the tail time? We're going to filter an impulse
    // for |maxTailTime| seconds, in blocks of kRenderQuantumFrames at
    // a time. The maximum magnitude of this block is saved. After all
    // of the samples have been computed, find the last block with a
    // maximum magnitude greater than |kMaxTaileAmplitude|. That block
    // index + 1 will be the tail time. We don't need to be
    // super-accurate in computing the tail time since we process on
    // blocks, block accuracy is good enough, and the value just needs
    // to be larger than the "real" tail time, so we don't prematurely
    // zero out the output of the node.

    // Number of render quanta needed to reach the max tail time.
    int numberOfBlocks = std::ceil(sampleRate * maxTailTime / iirRenderingQuantum);

    // Input and output buffers for filtering.
    AudioFloatArray input(iirRenderingQuantum);
    AudioFloatArray output(iirRenderingQuantum);

    // Array to hold the max magnitudes
    AudioFloatArray magnitudes(numberOfBlocks);

    // Create the impulse input signal.
    input[0] = 1;

    // Process the first block and get the max magnitude of the output.
    process(input.data(), output.data(), iirRenderingQuantum);
    VectorMath::vmaxmgv(output.data(), 1, &magnitudes[0], iirRenderingQuantum);

    // Process the rest of the signal, getting the max magnitude of the
    // output for each block.
    input[0] = 0;

    for (int k = 1; k < numberOfBlocks; ++k) {
        process(input.data(), output.data(), iirRenderingQuantum);
        VectorMath::vmaxmgv(output.data(), 1, &magnitudes[k], iirRenderingQuantum);
    }

    // Done computing the impulse response; reset the state so the actual node
    // starts in the expected initial state.
    reset();

    // Find the last block with amplitude greater than the threshold.
    int index = numberOfBlocks - 1;
    for (int k = index; k >= 0; --k) {
        if (magnitudes[k] > maxTailAmplitude) {
            index = k;
            break;
        }
    }

    // The magnitude first become lower than the threshold at the next block.
    // Compute the corresponding time value value; that's the tail time.
    return (index + 1) * iirRenderingQuantum / sampleRate;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
