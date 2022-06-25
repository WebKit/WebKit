/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "JSAudioNode.h"

#include "AnalyserNode.h"
#include "AudioBufferSourceNode.h"
#include "AudioDestinationNode.h"
#include "AudioNode.h"
#include "AudioWorkletNode.h"
#include "BiquadFilterNode.h"
#include "ChannelMergerNode.h"
#include "ChannelSplitterNode.h"
#include "ConstantSourceNode.h"
#include "ConvolverNode.h"
#include "DelayNode.h"
#include "DynamicsCompressorNode.h"
#include "GainNode.h"
#include "IIRFilterNode.h"
#include "JSAnalyserNode.h"
#include "JSAudioBufferSourceNode.h"
#include "JSAudioDestinationNode.h"
#include "JSAudioWorkletNode.h"
#include "JSBiquadFilterNode.h"
#include "JSChannelMergerNode.h"
#include "JSChannelSplitterNode.h"
#include "JSConstantSourceNode.h"
#include "JSConvolverNode.h"
#include "JSDelayNode.h"
#include "JSDynamicsCompressorNode.h"
#include "JSGainNode.h"
#include "JSIIRFilterNode.h"
#include "JSMediaElementAudioSourceNode.h"
#include "JSMediaStreamAudioDestinationNode.h"
#include "JSMediaStreamAudioSourceNode.h"
#include "JSOscillatorNode.h"
#include "JSPannerNode.h"
#include "JSScriptProcessorNode.h"
#include "JSStereoPannerNode.h"
#include "JSWaveShaperNode.h"
#include "MediaElementAudioSourceNode.h"
#include "MediaStreamAudioDestinationNode.h"
#include "MediaStreamAudioSourceNode.h"
#include "OscillatorNode.h"
#include "PannerNode.h"
#include "ScriptProcessorNode.h"
#include "StereoPannerNode.h"
#include "WaveShaperNode.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<AudioNode>&& node)
{
    switch (node->nodeType()) {
    case AudioNode::NodeTypeDestination:
        return createWrapper<AudioDestinationNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeOscillator:
        return createWrapper<OscillatorNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeAudioBufferSource:
        return createWrapper<AudioBufferSourceNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeMediaElementAudioSource:
#if ENABLE(VIDEO)
        return createWrapper<MediaElementAudioSourceNode>(globalObject, WTFMove(node));
#else
        return createWrapper<AudioNode>(globalObject, WTFMove(node));
#endif
#if ENABLE(MEDIA_STREAM)
    case AudioNode::NodeTypeMediaStreamAudioDestination:
        return createWrapper<MediaStreamAudioDestinationNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeMediaStreamAudioSource:
        return createWrapper<MediaStreamAudioSourceNode>(globalObject, WTFMove(node));
#else
    case AudioNode::NodeTypeMediaStreamAudioDestination:
    case AudioNode::NodeTypeMediaStreamAudioSource:
        return createWrapper<AudioNode>(globalObject, WTFMove(node));
#endif
    case AudioNode::NodeTypeJavaScript:
        return createWrapper<ScriptProcessorNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeBiquadFilter:
        return createWrapper<BiquadFilterNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypePanner:
        return createWrapper<PannerNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeConvolver:
        return createWrapper<ConvolverNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeDelay:
        return createWrapper<DelayNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeGain:
        return createWrapper<GainNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeChannelSplitter:
        return createWrapper<ChannelSplitterNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeChannelMerger:
        return createWrapper<ChannelMergerNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeAnalyser:
        return createWrapper<AnalyserNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeDynamicsCompressor:
        return createWrapper<DynamicsCompressorNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeWaveShaper:
        return createWrapper<WaveShaperNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeConstant:
        return createWrapper<ConstantSourceNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeStereoPanner:
        return createWrapper<StereoPannerNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeIIRFilter:
        return createWrapper<IIRFilterNode>(globalObject, WTFMove(node));
    case AudioNode::NodeTypeWorklet:
        return createWrapper<AudioWorkletNode>(globalObject, WTFMove(node));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, AudioNode& node)
{
    return wrap(lexicalGlobalObject, globalObject, node);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
