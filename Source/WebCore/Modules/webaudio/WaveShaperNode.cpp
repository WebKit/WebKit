/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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
#include "WaveShaperNode.h"

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WaveShaperNode);

ExceptionOr<Ref<WaveShaperNode>> WaveShaperNode::create(BaseAudioContext& context, const WaveShaperOptions& options)
{
    RefPtr<Float32Array> curve;
    if (options.curve) {
        curve = Float32Array::tryCreate(options.curve->data(), options.curve->size());
        if (!curve)
            return Exception { InvalidStateError, "Invalid curve parameter"_s };
    }

    auto node = adoptRef(*new WaveShaperNode(context));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    if (curve) {
        result = node->setCurveForBindings(WTFMove(curve));
        if (result.hasException())
            return result.releaseException();
    }

    node->setOversampleForBindings(options.oversample);

    return node;
}

WaveShaperNode::WaveShaperNode(BaseAudioContext& context)
    : AudioBasicProcessorNode(context, NodeTypeWaveShaper)
{
    m_processor = makeUnique<WaveShaperProcessor>(context.sampleRate(), 1);

    initialize();
}

ExceptionOr<void> WaveShaperNode::setCurveForBindings(RefPtr<Float32Array>&& curve)
{
    ASSERT(isMainThread()); 
    DEBUG_LOG(LOGIDENTIFIER);
    if (curve && curve->length() < 2)
        return Exception { InvalidStateError, "Length of curve array cannot be less than 2"_s };

    if (curve) {
        // The specification states that we should maintain an internal copy of the curve so that
        // subsequent modifications of the contents of the array have no effect.
        auto clonedCurve = Float32Array::create(curve->data(), curve->length());
        curve = WTFMove(clonedCurve);
    }

    waveShaperProcessor()->setCurveForBindings(curve.get());
    return { };
}

Float32Array* WaveShaperNode::curveForBindings()
{
    ASSERT(isMainThread());
    return waveShaperProcessor()->curveForBindings();
}

static inline WaveShaperProcessor::OverSampleType processorType(OverSampleType type)
{
    switch (type) {
    case OverSampleType::None:
        return WaveShaperProcessor::OverSampleNone;
    case OverSampleType::_2x:
        return WaveShaperProcessor::OverSample2x;
    case OverSampleType::_4x:
        return WaveShaperProcessor::OverSample4x;
    }
    ASSERT_NOT_REACHED();
    return WaveShaperProcessor::OverSampleNone;
}

void WaveShaperNode::setOversampleForBindings(OverSampleType type)
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER, type);

    // Synchronize with any graph changes or changes to channel configuration.
    Locker contextLocker { context().graphLock() };
    waveShaperProcessor()->setOversampleForBindings(processorType(type));
}

auto WaveShaperNode::oversampleForBindings() const -> OverSampleType
{
    ASSERT(isMainThread());
    switch (waveShaperProcessor()->oversampleForBindings()) {
    case WaveShaperProcessor::OverSampleNone:
        return OverSampleType::None;
    case WaveShaperProcessor::OverSample2x:
        return OverSampleType::_2x;
    case WaveShaperProcessor::OverSample4x:
        return OverSampleType::_4x;
    }
    ASSERT_NOT_REACHED();
    return OverSampleType::None;
}

bool WaveShaperNode::propagatesSilence() const
{
    if (!waveShaperProcessor()->processLock().tryLock())
        return false;

    Locker locker { AdoptLock, waveShaperProcessor()->processLock() };
    auto curve = waveShaperProcessor()->curve();
    return !curve || !curve->length();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
