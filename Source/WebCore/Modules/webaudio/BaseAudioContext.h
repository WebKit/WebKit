/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUDIO)
#include "ActiveDOMObject.h"
#include "AudioContextState.h"
#include "AudioDestinationNode.h"
#include "AudioIOCallback.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "OscillatorType.h"
#include "PeriodicWaveConstraints.h"
#include <atomic>
#include <wtf/Forward.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MainThread.h>
#include <wtf/RecursiveLockAdapter.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace WebCore {

class AnalyserNode;
class AsyncAudioDecoder;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioNodeOutput;
class AudioSummingJunction;
class AudioWorklet;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConstantSourceNode;
class ConvolverNode;
class DelayNode;
class Document;
class DynamicsCompressorNode;
class GainNode;
class IIRFilterNode;
class MediaElementAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class ScriptProcessorNode;
class SecurityOrigin;
class StereoPannerNode;
class WaveShaperNode;

struct AudioIOPosition;
struct AudioParamDescriptor;

template<typename IDLType> class DOMPromiseDeferred;

// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism. 

class BaseAudioContext
    : public ActiveDOMObject
    , public ThreadSafeRefCounted<BaseAudioContext>
    , public EventTargetWithInlineData
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(BaseAudioContext);
public:
    virtual ~BaseAudioContext();

    // Reconcile ref/deref which are defined both in ThreadSafeRefCounted and EventTarget.
    using ThreadSafeRefCounted::ref;
    using ThreadSafeRefCounted::deref;

    // This is used for lifetime testing.
    WEBCORE_EXPORT static bool isContextAlive(uint64_t contextID);
    uint64_t contextID() const { return m_contextID; }

    Document* document() const;
    bool isInitialized() const { return m_isInitialized; }
    
    virtual bool isOfflineContext() const = 0;
    virtual AudioDestinationNode& destination() = 0;
    virtual const AudioDestinationNode& destination() const = 0;

    size_t currentSampleFrame() const { return destination().currentSampleFrame(); }
    double currentTime() const { return destination().currentTime(); }
    float sampleRate() const { return destination().sampleRate(); }

    // Asynchronous audio file data decoding.
    void decodeAudioData(Ref<ArrayBuffer>&&, RefPtr<AudioBufferCallback>&&, RefPtr<AudioBufferCallback>&&, std::optional<Ref<DeferredPromise>>&& = std::nullopt);

    AudioListener& listener() { return m_listener; }

    using State = AudioContextState;
    State state() const { return m_state; }
    bool isClosed() const { return m_state == State::Closed; }

    AudioWorklet& audioWorklet() { return m_worklet.get(); }

    bool wouldTaintOrigin(const URL&) const;

    // The AudioNode create methods are called on the main thread (from JavaScript).
    ExceptionOr<Ref<AudioBufferSourceNode>> createBufferSource();
    ExceptionOr<Ref<GainNode>> createGain();
    ExceptionOr<Ref<BiquadFilterNode>> createBiquadFilter();
    ExceptionOr<Ref<WaveShaperNode>> createWaveShaper();
    ExceptionOr<Ref<DelayNode>> createDelay(double maxDelayTime);
    ExceptionOr<Ref<PannerNode>> createPanner();
    ExceptionOr<Ref<ConvolverNode>> createConvolver();
    ExceptionOr<Ref<DynamicsCompressorNode>> createDynamicsCompressor();
    ExceptionOr<Ref<AnalyserNode>> createAnalyser();
    ExceptionOr<Ref<ScriptProcessorNode>> createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels);
    ExceptionOr<Ref<ChannelSplitterNode>> createChannelSplitter(size_t numberOfOutputs);
    ExceptionOr<Ref<ChannelMergerNode>> createChannelMerger(size_t numberOfInputs);
    ExceptionOr<Ref<OscillatorNode>> createOscillator();
    ExceptionOr<Ref<PeriodicWave>> createPeriodicWave(Vector<float>&& real, Vector<float>&& imaginary, const PeriodicWaveConstraints& = { });
    ExceptionOr<Ref<ConstantSourceNode>> createConstantSource();
    ExceptionOr<Ref<StereoPannerNode>> createStereoPanner();
    ExceptionOr<Ref<IIRFilterNode>> createIIRFilter(ScriptExecutionContext&, Vector<double>&& feedforward, Vector<double>&& feedback);
    ExceptionOr<Ref<AudioBuffer>> createBuffer(unsigned numberOfChannels, unsigned length, float sampleRate);

    // Called at the start of each render quantum.
    void handlePreRenderTasks(const AudioIOPosition& outputPosition);

    AudioIOPosition outputPosition();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // We schedule deletion of all marked nodes at the end of each realtime render quantum.
    void markForDeletion(AudioNode&);
    void deleteMarkedNodes();

    void addTailProcessingNode(AudioNode&);
    void removeTailProcessingNode(AudioNode&);

    // AudioContext can pull node(s) at the end of each render quantum even when they are not connected to any downstream nodes.
    // These two methods are called by the nodes who want to add/remove themselves into/from the automatic pull lists.
    void addAutomaticPullNode(AudioNode&);
    void removeAutomaticPullNode(AudioNode&);

    // Called right before handlePostRenderTasks() to handle nodes which need to be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess);

    //
    // Thread Safety and Graph Locking:
    //
    
    void setAudioThread(Thread& thread) { m_audioThread = &thread; } // FIXME: check either not initialized or the same
    bool isAudioThread() const { return m_audioThread == &Thread::current(); }

    // Returns true only after the audio thread has been started and then shutdown.
    bool isAudioThreadFinished() const { return m_isAudioThreadFinished; }

    RecursiveLock& graphLock() { return m_graphLock; }

    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() const { return m_graphLock.isOwner(); }

    // This is considering 32 is large enough for multiple channels audio.
    // It is somewhat arbitrary and could be increased if necessary.
    static constexpr unsigned maxNumberOfChannels = 32;
    
    // In AudioNode::decrementConnectionCount() a tryLock() is used for calling decrementConnectionCountWithLock(), but if it fails keep track here.
    void addDeferredDecrementConnectionCount(AudioNode*);

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*);
    void markAudioNodeOutputDirty(AudioNodeOutput*);

    // Must be called on main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*);

    // EventTarget
    ScriptExecutionContext* scriptExecutionContext() const final;

    virtual void sourceNodeWillBeginPlayback(AudioNode&);
    // When a source node has no more processing to do (has finished playing), then it tells the context to dereference it.
    void sourceNodeDidFinishPlayback(AudioNode&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const override { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const void* nextAudioNodeLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextAudioNodeIdentifier); }
    const void* nextAudioParameterLogIdentifier() { return childLogIdentifier(m_logIdentifier, ++m_nextAudioParameterIdentifier); }
#endif

    void postTask(Function<void()>&&);
    bool isStopped() const { return m_isStopScheduled; }
    const SecurityOrigin* origin() const;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message);

    virtual void lazyInitialize();

    static bool isSupportedSampleRate(float sampleRate);

    PeriodicWave& periodicWave(OscillatorType);

    void addAudioParamDescriptors(const String& processorName, Vector<AudioParamDescriptor>&&);
    const HashMap<String, Vector<AudioParamDescriptor>>& parameterDescriptorMap() const { return m_parameterDescriptorMap; }

protected:
    explicit BaseAudioContext(Document&);

    virtual void uninitialize();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "BaseAudioContext"; }
#endif

    void addReaction(State, DOMPromiseDeferred<void>&&);
    void setState(State);

    void clear();

private:
    void scheduleNodeDeletion();
    void workletIsReady();

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    // In the audio thread at the start of each render cycle, we'll call handleDeferredDecrementConnectionCounts().
    void handleDeferredDecrementConnectionCounts();

    // EventTarget
    EventTargetInterface eventTargetInterface() const final;
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    // ActiveDOMObject API.
    void stop() override;

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    void updateAutomaticPullNodes();
    void updateTailProcessingNodes();
    void finishTailProcessing();
    void disableOutputsForFinishedTailProcessingNodes();

#if !RELEASE_LOG_DISABLED
    Ref<Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextAudioNodeIdentifier { 0 };
    uint64_t m_nextAudioParameterIdentifier { 0 };
#endif

    uint64_t m_contextID;

    Ref<AudioWorklet> m_worklet;

    // Either accessed when the graph lock is held, or on the main thread when the audio thread has finished.
    Vector<AudioConnectionRefPtr<AudioNode>> m_referencedSourceNodes;

    // Accumulate nodes which need to be deleted here.
    // This is copied to m_nodesToDelete at the end of a render cycle in handlePostRenderTasks(), where we're assured of a stable graph
    // state which will have no references to any of the nodes in m_nodesToDelete once the context lock is released
    // (when handlePostRenderTasks() has completed).
    Vector<AudioNode*> m_nodesMarkedForDeletion;

    class TailProcessingNode {
    public:
        TailProcessingNode(AudioNode& node)
            : m_node(&node)
        {
            ASSERT(!node.isTailProcessing());
            node.setIsTailProcessing(true);
        }
        TailProcessingNode(TailProcessingNode&& other)
            : m_node(std::exchange(other.m_node, nullptr))
        { }
        ~TailProcessingNode()
        {
            if (m_node)
                m_node->setIsTailProcessing(false);
        }
        TailProcessingNode& operator=(const TailProcessingNode&) = delete;
        TailProcessingNode& operator=(TailProcessingNode&&) = delete;
        AudioNode* operator->() const { return m_node.get(); }
        bool operator==(const TailProcessingNode& other) const { return m_node == other.m_node; }
        bool operator==(const AudioNode& node) const { return m_node == &node; }
    private:
        RefPtr<AudioNode> m_node;
    };

    // Nodes that are currently processing their tail.
    Vector<TailProcessingNode> m_tailProcessingNodes;

    // Nodes that have finished processing their tail and waiting for their outputs to get disabled on the main thread.
    Vector<TailProcessingNode> m_finishedTailProcessingNodes;

    // They will be scheduled for deletion (on the main thread) at the end of a render cycle (in realtime thread).
    Vector<AudioNode*> m_nodesToDelete;

    // Only accessed when the graph lock is held.
    HashSet<AudioSummingJunction*> m_dirtySummingJunctions;
    HashSet<AudioNodeOutput*> m_dirtyAudioNodeOutputs;

    // For the sake of thread safety, we maintain a seperate Vector of automatic pull nodes for rendering in m_renderingAutomaticPullNodes.
    // It will be copied from m_automaticPullNodes by updateAutomaticPullNodes() at the very start or end of the rendering quantum.
    HashSet<AudioNode*> m_automaticPullNodes;
    Vector<AudioNode*> m_renderingAutomaticPullNodes;
    // Only accessed in the audio thread.
    Vector<AudioNode*> m_deferredBreakConnectionList;
    Vector<Vector<DOMPromiseDeferred<void>>> m_stateReactions;

    Ref<AudioListener> m_listener;

    std::atomic<Thread*> m_audioThread;

    RecursiveLock m_graphLock;

    std::unique_ptr<AsyncAudioDecoder> m_audioDecoder;

    AudioIOPosition m_outputPosition;

    HashMap<String, Vector<AudioParamDescriptor>> m_parameterDescriptorMap;

    // These are cached per audio context for performance reasons. They cannot be
    // static because they rely on the sample rate.
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSine;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSquare;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSawtooth;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveTriangle;

    State m_state { State::Suspended };
    bool m_isDeletionScheduled { false };
    bool m_disableOutputsForTailProcessingScheduled { false };
    bool m_isStopScheduled { false };
    bool m_isInitialized { false };
    bool m_isAudioThreadFinished { false };
    bool m_automaticPullNodesNeedUpdating { false };
    bool m_hasFinishedAudioSourceNodes { false };
};

} // WebCore

#endif // ENABLE(WEB_AUDIO)
