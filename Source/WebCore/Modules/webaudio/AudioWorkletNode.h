/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUDIO)

#include "ActiveDOMObject.h"
#include "AudioNode.h"
#include <wtf/Lock.h>
#include <wtf/RobinHoodHashMap.h>

namespace JSC {
class JSGlobalObject;
} // namespace JSC

namespace WebCore {

class AudioParamMap;
class AudioWorkletProcessor;
class MessagePort;

struct AudioParamDescriptor;
struct AudioWorkletNodeOptions;

template<typename> class AudioArray;
typedef AudioArray<float> AudioFloatArray;

class AudioWorkletNode : public AudioNode, public ActiveDOMObject {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(AudioWorkletNode);
public:
    static ExceptionOr<Ref<AudioWorkletNode>> create(JSC::JSGlobalObject&, BaseAudioContext&, String&& name, AudioWorkletNodeOptions&&);
    ~AudioWorkletNode();

    // ActiveDOMObject.
    void ref() const final { AudioNode::ref(); }
    void deref() const final { AudioNode::deref(); }

    AudioParamMap& parameters() { return m_parameters.get(); }
    MessagePort& port() { return m_port.get(); }

    void setProcessor(RefPtr<AudioWorkletProcessor>&&);
    void initializeAudioParameters(const Vector<AudioParamDescriptor>&, const std::optional<Vector<KeyValuePair<String, double>>>& paramValues);

private:
    AudioWorkletNode(BaseAudioContext&, const String& name, AudioWorkletNodeOptions&&, Ref<MessagePort>&&);

    enum class ProcessorError { ConstructorError, ProcessError };
    void fireProcessorErrorOnMainThread(ProcessorError);
    void didFinishProcessingOnRenderingThread(bool threwException);

    // AudioNode.
    void process(size_t framesToProcess) final;
    double tailTime() const final { return m_tailTime; }
    double latencyTime() const final { return 0; }
    bool requiresTailProcessing() const final { return true; }
    void checkNumberOfChannelsForInput(AudioNodeInput*) final;
    void updatePullStatus() final;

    // ActiveDOMObject.
    bool virtualHasPendingActivity() const final;

    String m_name;
    Ref<AudioParamMap> m_parameters;
    Ref<MessagePort> m_port;
    Lock m_processLock;
    RefPtr<AudioWorkletProcessor> m_processor; // Should only be used on the rendering thread.
    MemoryCompactLookupOnlyRobinHoodHashMap<String, std::unique_ptr<AudioFloatArray>> m_paramValuesMap;
    Thread* m_workletThread { nullptr };

    // Keeps the reference of AudioBus objects from AudioNodeInput and AudioNodeOutput in order
    // to pass them to AudioWorkletProcessor.
    Vector<RefPtr<AudioBus>> m_inputs;
    Vector<Ref<AudioBus>> m_outputs;

    double m_tailTime { std::numeric_limits<double>::infinity() };
    bool m_wasOutputChannelCountGiven { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
