/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "AudioBus.h"
#include "ChannelCountMode.h"
#include "ChannelInterpretation.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include <wtf/Forward.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Variant.h>

#define DEBUG_AUDIONODE_REFERENCES 0

namespace WebCore {

class AudioNodeInput;
struct AudioNodeOptions;
class AudioNodeOutput;
class AudioParam;
class BaseAudioContext;
class WebKitAudioContext;

// An AudioNode is the basic building block for handling audio within an AudioContext.
// It may be an audio source, an intermediate processing module, or an audio destination.
// Each AudioNode can have inputs and/or outputs. An AudioSourceNode has no inputs and a single output.
// An AudioDestinationNode has one input and no outputs and represents the final destination to the audio hardware.
// Most processing nodes such as filters will have one input and one output, although multiple inputs and outputs are possible.

class AudioNode
    : public EventTargetWithInlineData
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_NONCOPYABLE(AudioNode);
    WTF_MAKE_ISO_ALLOCATED(AudioNode);
public:
    enum NodeType {
        NodeTypeDestination,
        NodeTypeOscillator,
        NodeTypeAudioBufferSource,
        NodeTypeMediaElementAudioSource,
        NodeTypeMediaStreamAudioDestination,
        NodeTypeMediaStreamAudioSource,
        NodeTypeJavaScript,
        NodeTypeBiquadFilter,
        NodeTypePanner,
        NodeTypeConvolver,
        NodeTypeDelay,
        NodeTypeGain,
        NodeTypeChannelSplitter,
        NodeTypeChannelMerger,
        NodeTypeAnalyser,
        NodeTypeDynamicsCompressor,
        NodeTypeWaveShaper,
        NodeTypeConstant,
        NodeTypeStereoPanner,
        NodeTypeIIRFilter,
        NodeTypeLast = NodeTypeIIRFilter
    };

    AudioNode(BaseAudioContext&, NodeType);
    virtual ~AudioNode();

    BaseAudioContext& context() { return m_context.get(); }
    const BaseAudioContext& context() const { return m_context.get(); }

    Variant<RefPtr<BaseAudioContext>, RefPtr<WebKitAudioContext>> contextForBindings() const;

    NodeType nodeType() const { return m_nodeType; }

    // Can be called from main thread or context's audio thread.
    void ref();
    void deref();
    void incrementConnectionCount();
    void decrementConnectionCount();

    // Can be called from main thread or context's audio thread.  It must be called while the context's graph lock is held.
    void decrementConnectionCountWithLock();
    virtual void didBecomeMarkedForDeletion() { }

    // The AudioNodeInput(s) (if any) will already have their input data available when process() is called.
    // Subclasses will take this input data and put the results in the AudioBus(s) of its AudioNodeOutput(s) (if any).
    // Called from context's audio thread.
    virtual void process(size_t framesToProcess) = 0;

    // Like process(), but only causes the automations to process; the
    // normal processing of the node is bypassed. By default, we assume
    // no AudioParams need to be updated.
    virtual void processOnlyAudioParams(size_t) { }

    // No significant resources should be allocated until initialize() is called.
    // Processing may not occur until a node is initialized.
    virtual void initialize();
    virtual void uninitialize();

    bool isInitialized() const { return m_isInitialized; }
    void lazyInitialize();

    unsigned numberOfInputs() const { return m_inputs.size(); }
    unsigned numberOfOutputs() const { return m_outputs.size(); }

    AudioNodeInput* input(unsigned);
    AudioNodeOutput* output(unsigned);

    // Called from main thread by corresponding JavaScript methods.
    ExceptionOr<void> connect(AudioNode&, unsigned outputIndex, unsigned inputIndex);
    ExceptionOr<void> connect(AudioParam&, unsigned outputIndex);

    void disconnect();
    ExceptionOr<void> disconnect(unsigned output);
    ExceptionOr<void> disconnect(AudioNode& destinationNode);
    ExceptionOr<void> disconnect(AudioNode& destinationNode, unsigned output);
    ExceptionOr<void> disconnect(AudioNode& destinationNode, unsigned output, unsigned input);
    ExceptionOr<void> disconnect(AudioParam& destinationParam);
    ExceptionOr<void> disconnect(AudioParam& destinationParam, unsigned output);

    virtual float sampleRate() const;

    // processIfNecessary() is called by our output(s) when the rendering graph needs this AudioNode to process.
    // This method ensures that the AudioNode will only process once per rendering time quantum even if it's called repeatedly.
    // This handles the case of "fanout" where an output is connected to multiple AudioNode inputs.
    // Called from context's audio thread.
    void processIfNecessary(size_t framesToProcess);

    // Called when a new connection has been made to one of our inputs or the connection number of channels has changed.
    // This potentially gives us enough information to perform a lazy initialization or, if necessary, a re-initialization.
    // Called from main thread.
    virtual void checkNumberOfChannelsForInput(AudioNodeInput*);

#if DEBUG_AUDIONODE_REFERENCES
    static void printNodeCounts();
#endif

    bool isMarkedForDeletion() const { return m_isMarkedForDeletion; }

    // tailTime() is the length of time (not counting latency time) where non-zero output may occur after continuous silent input.
    virtual double tailTime() const = 0;
    // latencyTime() is the length of time it takes for non-zero output to appear after non-zero input is provided. This only applies to
    // processing delay which is an artifact of the processing algorithm chosen and is *not* part of the intrinsic desired effect. For 
    // example, a "delay" effect is expected to delay the signal, and thus would not be considered latency.
    virtual double latencyTime() const = 0;

    // True if the node has a tail time or latency time that requires
    // special tail processing to behave properly. Ideally, this can be
    // checked using tailTime and latencyTime, but these aren't
    // available on the main thread, and the tail processing check can
    // happen on the main thread.
    virtual bool requiresTailProcessing() const = 0;

    // propagatesSilence() should return true if the node will generate silent output when given silent input. By default, AudioNode
    // will take tailTime() and latencyTime() into account when determining whether the node will propagate silence.
    virtual bool propagatesSilence() const;
    bool inputsAreSilent();
    void silenceOutputs();

    void enableOutputsIfNecessary();
    void disableOutputsIfNecessary();
    void disableOutputs();

    unsigned channelCount() const { return m_channelCount; }
    virtual ExceptionOr<void> setChannelCount(unsigned);

    ChannelCountMode channelCountMode() const { return m_channelCountMode; }
    virtual ExceptionOr<void> setChannelCountMode(ChannelCountMode);

    ChannelInterpretation channelInterpretation() const { return m_channelInterpretation; }
    virtual ExceptionOr<void> setChannelInterpretation(ChannelInterpretation);

protected:
    // Inputs and outputs must be created before the AudioNode is initialized.
    void addInput();
    void addOutput(unsigned numberOfChannels);

    void markNodeForDeletionIfNecessary();
    void derefWithLock();

    struct DefaultAudioNodeOptions {
        unsigned channelCount;
        ChannelCountMode channelCountMode;
        ChannelInterpretation channelInterpretation;
    };

    ExceptionOr<void> handleAudioNodeOptions(const AudioNodeOptions&, const DefaultAudioNodeOptions&);
    
    // Called by processIfNecessary() to cause all parts of the rendering graph connected to us to process.
    // Each rendering quantum, the audio data for each of the AudioNode's inputs will be available after this method is called.
    // Called from context's audio thread.
    virtual void pullInputs(size_t framesToProcess);

    // Force all inputs to take any channel interpretation changes into account.
    void updateChannelsForInputs();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "AudioNode"; }
    WTFLogChannel& logChannel() const final;
#endif

    void initializeDefaultNodeOptions(unsigned count, ChannelCountMode, ChannelInterpretation);

    virtual void updatePullStatus() { }

private:
    // EventTarget
    EventTargetInterface eventTargetInterface() const override;
    ScriptExecutionContext* scriptExecutionContext() const final;

    volatile bool m_isInitialized { false };
    NodeType m_nodeType;
    Ref<BaseAudioContext> m_context;

    Vector<std::unique_ptr<AudioNodeInput>> m_inputs;
    Vector<std::unique_ptr<AudioNodeOutput>> m_outputs;

    double m_lastProcessingTime { -1 };
    double m_lastNonSilentTime { -1 };

    // Ref-counting
    // start out with normal refCount == 1 (like WTF::RefCounted class).
    std::atomic<int> m_normalRefCount { 1 };
    std::atomic<int> m_connectionRefCount { 0 };
    
    bool m_isMarkedForDeletion { false };
    bool m_isDisabled { false };

#if DEBUG_AUDIONODE_REFERENCES
    static bool s_isNodeCountInitialized;
    static int s_nodeCount[NodeTypeLast + 1];
#endif

    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

#if !RELEASE_LOG_DISABLED
    mutable Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    unsigned m_channelCount { 2 };
    ChannelCountMode m_channelCountMode { ChannelCountMode::Max };
    ChannelInterpretation m_channelInterpretation { ChannelInterpretation::Speakers };
};

template<typename T> struct AudioNodeConnectionRefDerefTraits {
    static ALWAYS_INLINE void refIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->incrementConnectionCount();
    }

    static ALWAYS_INLINE void derefIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->decrementConnectionCount();
    }
};

template<typename T>
using AudioConnectionRefPtr = RefPtr<T, DumbPtrTraits<T>, AudioNodeConnectionRefDerefTraits<T>>;

String convertEnumerationToString(AudioNode::NodeType);

} // namespace WebCore

namespace WTF {
    
template<> struct LogArgument<WebCore::AudioNode::NodeType> {
    static String toString(WebCore::AudioNode::NodeType type) { return convertEnumerationToString(type); }
};

} // namespace WTF
