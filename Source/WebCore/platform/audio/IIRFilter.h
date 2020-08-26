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

#pragma once

#include <wtf/Vector.h>

namespace WebCore {

class IIRFilter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr size_t maxOrder { 20 };
    IIRFilter(const Vector<double>& feedforward, const Vector<double>& feedback);

    void reset();

    void process(const float* source, float* destination, size_t framesToProcess);
    void getFrequencyResponse(unsigned length, const float* frequency, float* magResponse, float* phaseResponse);
    double tailTime(double sampleRate, bool isFilterStable);

    const Vector<double>& feedforward() const { return m_feedforward; }
    const Vector<double>& feedback() const { return m_feedback; }

private:
    // Filter memory
    //
    // For simplicity, we assume |m_xBuffer| and |m_yBuffer| have the same length,
    // and the length is a power of two. Since the number of coefficients has a
    // fixed upper length, the size of xBuffer and yBuffer is fixed. |m_xBuffer|
    // holds the old input values and |m_yBuffer| holds the old output values
    // needed to compute the new output value.
    //
    // m_yBuffer[m_bufferIndex] holds the most recent output value, say, y[n].
    // Then m_yBuffer[m_bufferIndex - k] is y[n - k]. Similarly for m_xBuffer.
    //
    // To minimize roundoff, these arrays are double's instead of floats.
    Vector<double> m_xBuffer;
    Vector<double> m_yBuffer;

    // Index into the xBuffer and yBuffer arrays where the most current x and y
    // values should be stored. xBuffer[bufferIndex] corresponds to x[n], the
    // current x input value and yBuffer[bufferIndex] is where y[n], the current
    // output value.
    size_t m_bufferIndex { 0 };

    // Those Vectors are owned by the IIRProcess, which owns the IIRFilters via
    // the IIRDSPKernels. This is a memory optimization to avoid having copies
    // of these vectors in each IIRDSPKernel / IIRFilter.
    const Vector<double>& m_feedforward;
    const Vector<double>& m_feedback;
};

} // namespace WebCore
