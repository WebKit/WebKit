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
#include "IIRFilterNode.h"

#include "BaseAudioContext.h"
#include "IIRFilter.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(IIRFilterNode);

// Determine if filter is stable based on the feedback coefficients.
// We compute the reflection coefficients for the filter. If, at any
// point, the magnitude of the reflection coefficient is greater than
// or equal to 1, the filter is declared unstable.
//
// Let A(z) be the feedback polynomial given by
//   A[n](z) = 1 + a[1]/z + a[2]/z^2 + ... + a[n]/z^n
//
// The first reflection coefficient k[n] = a[n]. Then, recursively compute
//
//   A[n-1](z) = (A[n](z) - k[n]*A[n](1/z)/z^n)/(1-k[n]^2);
//
// stopping at A[1](z). If at any point |k[n]| >= 1, the filter is unstable.
static bool isFilterStable(const Vector<double>& feedback)
{
    // Make a copy of the feedback coefficients
    Vector<double> coefficients(feedback);
    int order = coefficients.size() - 1;

    // If necessary, normalize filter coefficients so that constant term is 1.
    if (coefficients[0] != 1) {
        for (int m = 1; m <= order; ++m)
            coefficients[m] /= coefficients[0];
        coefficients[0] = 1;
    }

    // Begin recursion, using a work array to hold intermediate results.
    Vector<double> work(order + 1);
    for (int n = order; n >= 1; --n) {
        double k = coefficients[n];

        if (std::fabs(k) >= 1)
            return false;

        // Note that A[n](1/z)/z^n is basically the coefficients of A[n]
        // in reverse order.
        double factor = 1 - k * k;
        for (int m = 0; m <= n; ++m)
            work[m] = (coefficients[m] - k * coefficients[n - m]) / factor;
        coefficients.swap(work);
    }

    return true;
}

ExceptionOr<Ref<IIRFilterNode>> IIRFilterNode::create(ScriptExecutionContext& scriptExecutionContext, BaseAudioContext& context, IIRFilterOptions&& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    if (!options.feedforward.size() || options.feedforward.size() > IIRFilter::maxOrder)
        return Exception { NotSupportedError, "feedforward array must have a length between 1 and 20"_s };

    auto nonZeroValueIndex = options.feedforward.findMatching([](auto& value) { return !!value; });
    if (nonZeroValueIndex == notFound)
        return Exception { InvalidStateError, "feedforward array must contain a non-zero value"_s };

    if (!options.feedback.size() || options.feedback.size() > IIRFilter::maxOrder)
        return Exception { NotSupportedError, "feedback array must have a length between 1 and 20"_s };

    if (!options.feedback[0])
        return Exception { InvalidStateError, "first value of feedback array cannot be zero"_s };

    bool isFilterStable = WebCore::isFilterStable(options.feedback);
    if (!isFilterStable)
        scriptExecutionContext.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "IIRFilter is unstable with provided feedback coefficients"_s);

    auto node = adoptRef(*new IIRFilterNode(context, options.feedforward, options.feedback, isFilterStable));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    return node;
}

IIRFilterNode::IIRFilterNode(BaseAudioContext& context, const Vector<double>& feedforward, const Vector<double>& feedback, bool isFilterStable)
    : AudioBasicProcessorNode(context, NodeTypeIIRFilter)
{
    m_processor = makeUnique<IIRProcessor>(context.sampleRate(), 1, feedforward, feedback, isFilterStable);

    initialize();
}

ExceptionOr<void> IIRFilterNode::getFrequencyResponse(Float32Array& frequencyHz, Float32Array& magResponse, Float32Array& phaseResponse)
{
    auto expectedLength = frequencyHz.length();
    if (magResponse.length() != expectedLength || phaseResponse.length() != expectedLength)
        return Exception { InvalidAccessError, "Arrays must have the same length"_s };

    // Nothing to do if the length is 0.
    if (expectedLength > 0)
        iirProcessor()->getFrequencyResponse(expectedLength, frequencyHz.data(), magResponse.data(), phaseResponse.data());

    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
