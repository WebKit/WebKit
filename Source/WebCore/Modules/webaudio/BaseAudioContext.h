/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "AsyncAudioDecoder.h"
#include "AudioBus.h"
#include "AudioContextOptions.h"
#include "AudioContextState.h"
#include "AudioDestinationNode.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "OscillatorType.h"
#include "PeriodicWaveConstraints.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/Float32Array.h>
#include <atomic>
#include <wtf/HashSet.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MainThread.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
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
class HTMLMediaElement;
class IIRFilterNode;
class MediaElementAudioSourceNode;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class ScriptExecutionContext;
class ScriptProcessorNode;
class SecurityOrigin;
class StereoPannerNode;
class WaveShaperNode;

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

    Document* document() const;
    bool isInitialized() const;
    
    bool isOfflineContext() const { return m_isOfflineContext; }
    virtual bool isWebKitAudioContext() const { return false; }

    AudioDestinationNode* destination() { return m_destinationNode.get(); }
    size_t currentSampleFrame() const { return m_destinationNode ? m_destinationNode->currentSampleFrame() : 0; }
    double currentTime() const { return m_destinationNode ? m_destinationNode->currentTime() : 0.; }
    float sampleRate() const;
    unsigned long activeSourceCount() const { return static_cast<unsigned long>(m_activeSourceCount); }

    void incrementActiveSourceCount();
    void decrementActiveSourceCount();

    virtual bool shouldSuspend() { return false; }
    
    ExceptionOr<Ref<AudioBuffer>> createBuffer(unsigned numberOfChannels, unsigned length, float sampleRate);

    // Asynchronous audio file data decoding.
    void decodeAudioData(Ref<ArrayBuffer>&&, RefPtr<AudioBufferCallback>&&, RefPtr<AudioBufferCallback>&&, Optional<Ref<DeferredPromise>>&& = WTF::nullopt);

    AudioListener& listener();

    virtual void didSuspendRendering(size_t frame);

    AudioBuffer* renderTarget() const { return m_renderTarget.get(); }

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

    // Called at the start of each render quantum.
    void handlePreRenderTasks(const AudioIOPosition& outputPosition);

    AudioIOPosition outputPosition();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    // We schedule deletion of all marked nodes at the end of each realtime render quantum.
    void markForDeletion(AudioNode&);
    void deleteMarkedNodes();

    // AudioContext can pull node(s) at the end of each render quantum even when they are not connected to any downstream nodes.
    // These two methods are called by the nodes who want to add/remove themselves into/from the automatic pull lists.
    void addAutomaticPullNode(AudioNode&);
    void removeAutomaticPullNode(AudioNode&);

    // Called right before handlePostRenderTasks() to handle nodes which need to be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess);

    // Keeps track of the number of connections made.
    void incrementConnectionCount()
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    //
    // Thread Safety and Graph Locking:
    //
    
    void setAudioThread(Thread& thread) { m_audioThread = &thread; } // FIXME: check either not initialized or the same
    Thread* audioThread() const { return m_audioThread; }
    bool isAudioThread() const;

    // Returns true only after the audio thread has been started and then shutdown.
    bool isAudioThreadFinished() { return m_isAudioThreadFinished; }

    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    void lock(bool& mustReleaseLock);

    // Returns true if we own the lock.
    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    bool tryLock(bool& mustReleaseLock);

    void unlock();

    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() const;

    // Returns the maximum number of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels; }
    
    // In AudioNode::decrementConnectionCount() a tryLock() is used for calling decrementConnectionCountWithLock(), but if it fails keep track here.
    void addDeferredDecrementConnectionCount(AudioNode*);

    // In the audio thread at the start of each render cycle, we'll call handleDeferredDecrementConnectionCounts().
    void handleDeferredDecrementConnectionCounts();

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*);
    void markAudioNodeOutputDirty(AudioNodeOutput*);

    // Must be called on main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    void finishedRendering(bool didRendering);

    static unsigned s_hardwareContextCount;

    void isPlayingAudioDidChange();

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

    void postTask(WTF::Function<void()>&&);
    bool isStopped() const { return m_isStopScheduled; }
    const SecurityOrigin* origin() const;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message);

    class AutoLocker {
    public:
        explicit AutoLocker(BaseAudioContext& context)
            : m_context(context)
        {
            m_context.lock(m_mustReleaseLock);
        }

        ~AutoLocker()
        {
            if (m_mustReleaseLock)
                m_context.unlock();
        }

    private:
        BaseAudioContext& m_context;
        bool m_mustReleaseLock;
    };

    virtual void lazyInitialize();

    static bool isSupportedSampleRate(float sampleRate);

    PeriodicWave& periodicWave(OscillatorType);

    void addAudioParamDescriptors(const String& processorName, Vector<AudioParamDescriptor>&&);
    const HashMap<String, Vector<AudioParamDescriptor>>& parameterDescriptorMap() const { return m_parameterDescriptorMap; }

protected:
    explicit BaseAudioContext(Document&, const AudioContextOptions& = { });
    BaseAudioContext(Document&, unsigned numberOfChannels, float sampleRate, RefPtr<AudioBuffer>&& renderTarget);
    
    void clearPendingActivity();
    void makePendingActivity();

    void lockInternal(bool& mustReleaseLock);

    AudioDestinationNode* destinationNode() const { return m_destinationNode.get(); }

    virtual void uninitialize();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "BaseAudioContext"; }
#endif

    void addReaction(State, DOMPromiseDeferred<void>&&);
    void setState(State);

    virtual void didFinishOfflineRendering(ExceptionOr<Ref<AudioBuffer>>&&) { }

private:
    void clear();

    void scheduleNodeDeletion();
    void workletIsReady();

    // When source nodes begin playing, the BaseAudioContext keeps them alive inside m_referencedSourceNodes.
    // When the nodes stop playing, they get added to m_finishedSourceNodes. After each rendering quantum,
    // we call derefSourceNode() on every node in m_finishedSourceNodes since we no longer need to keep them
    // alive.
    void refSourceNode(AudioNode&);
    void derefSourceNode(AudioNode&);

    // EventTarget
    void dispatchEvent(Event&) final;

    // ActiveDOMObject API.
    void stop() override;
    const char* activeDOMObjectName() const override;

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    void updateAutomaticPullNodes();

#if !RELEASE_LOG_DISABLED
    Ref<Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextAudioNodeIdentifier { 0 };
    uint64_t m_nextAudioParameterIdentifier { 0 };
#endif

    Ref<AudioWorklet> m_worklet;

    // Only accessed in the audio thread.
    Vector<AudioNode*> m_finishedSourceNodes;

    // Either accessed when the graph lock is held, or on the main thread when the audio thread has finished.
    Vector<AudioConnectionRefPtr<AudioNode>> m_referencedSourceNodes;

    // Accumulate nodes which need to be deleted here.
    // This is copied to m_nodesToDelete at the end of a render cycle in handlePostRenderTasks(), where we're assured of a stable graph
    // state which will have no references to any of the nodes in m_nodesToDelete once the context lock is released
    // (when handlePostRenderTasks() has completed).
    Vector<AudioNode*> m_nodesMarkedForDeletion;

    // They will be scheduled for deletion (on the main thread) at the end of a render cycle (in realtime thread).
    Vector<AudioNode*> m_nodesToDelete;

    bool m_isDeletionScheduled { false };
    bool m_isStopScheduled { false };
    bool m_isInitialized { false };
    bool m_isAudioThreadFinished { false };
    bool m_automaticPullNodesNeedUpdating { false };
    bool m_isOfflineContext { false };

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

    RefPtr<AudioBuffer> m_renderTarget;
    RefPtr<AudioDestinationNode> m_destinationNode;
    RefPtr<AudioListener> m_listener;

    unsigned m_connectionCount { 0 };

    // Graph locking.
    Lock m_contextGraphMutex;
    // FIXME: Using volatile seems incorrect.
    // https://bugs.webkit.org/show_bug.cgi?id=180332
    Thread* volatile m_audioThread { nullptr };
    Thread* volatile m_graphOwnerThread { nullptr }; // if the lock is held then this is the thread which owns it, otherwise == nullptr.

    std::unique_ptr<AsyncAudioDecoder> m_audioDecoder;

    // This is considering 32 is large enough for multiple channels audio. 
    // It is somewhat arbitrary and could be increased if necessary.
    enum { MaxNumberOfChannels = 32 };

    // Number of AudioBufferSourceNodes that are active (playing).
    std::atomic<int> m_activeSourceCount { 0 };

    State m_state { State::Suspended };
    RefPtr<PendingActivity<BaseAudioContext>> m_pendingActivity;

    AudioIOPosition m_outputPosition;

    HashMap<String, Vector<AudioParamDescriptor>> m_parameterDescriptorMap;

    // These are cached per audio context for performance reasons. They cannot be
    // static because they rely on the sample rate.
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSine;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSquare;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveSawtooth;
    RefPtr<PeriodicWave> m_cachedPeriodicWaveTriangle;
};

} // WebCore

#endif // ENABLE(WEB_AUDIO)
