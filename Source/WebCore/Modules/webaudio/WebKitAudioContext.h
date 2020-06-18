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

#include "ActiveDOMObject.h"
#include "AsyncAudioDecoder.h"
#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioContextState.h"
#include "AudioDestinationNode.h"
#include "EventTarget.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "PlatformMediaSession.h"
#include "ScriptExecutionContext.h"
#include "VisibilityChangeClient.h"
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
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioSummingJunction;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class Document;
class DynamicsCompressorNode;
class GainNode;
class HTMLMediaElement;
class MainThreadGenericEventQueue;
class MediaElementAudioSourceNode;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PeriodicWave;
class ScriptProcessorNode;
class SecurityOrigin;
class WaveShaperNode;
class WebKitAudioPannerNode;

template<typename IDLType> class DOMPromiseDeferred;

// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class WebKitAudioContext
    : public AudioContextBase
    , private PlatformMediaSessionClient
    , private VisibilityChangeClient
{
    WTF_MAKE_ISO_ALLOCATED(WebKitAudioContext);
public:
    // Create an WebKitAudioContext for rendering to the audio hardware.
    static ExceptionOr<Ref<WebKitAudioContext>> create(Document&);

    virtual ~WebKitAudioContext();

    bool isInitialized() const final;

    bool isOfflineContext() const final { return m_isOfflineContext; }

    DocumentIdentifier hostingDocumentIdentifier() const final;

    AudioDestinationNode* destination() { return m_destinationNode.get(); }
    size_t currentSampleFrame() const final { return m_destinationNode ? m_destinationNode->currentSampleFrame() : 0; }
    double currentTime() const final { return m_destinationNode ? m_destinationNode->currentTime() : 0.; }
    float sampleRate() const final { return m_destinationNode ? m_destinationNode->sampleRate() : 0.f; }
    unsigned long activeSourceCount() const { return static_cast<unsigned long>(m_activeSourceCount); }

    void incrementActiveSourceCount() final;
    void decrementActiveSourceCount() final;

    ExceptionOr<Ref<AudioBuffer>> createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);
    ExceptionOr<Ref<AudioBuffer>> createBuffer(ArrayBuffer&, bool mixToMono);

    // Asynchronous audio file data decoding.
    void decodeAudioData(Ref<ArrayBuffer>&&, RefPtr<AudioBufferCallback>&&, RefPtr<AudioBufferCallback>&&);

    AudioListener* listener() { return m_listener.get(); }

    void suspendRendering(DOMPromiseDeferred<void>&&);
    void resumeRendering(DOMPromiseDeferred<void>&&);
    void close(DOMPromiseDeferred<void>&&);

    using State = AudioContextState;
    State state() const;
    bool isClosed() const { return m_state == State::Closed; }

    bool wouldTaintOrigin(const URL&) const;

    // The AudioNode create methods are called on the main thread (from JavaScript).
    ExceptionOr<Ref<AudioBufferSourceNode>> createBufferSource();
#if ENABLE(VIDEO)
    ExceptionOr<Ref<MediaElementAudioSourceNode>> createMediaElementSource(HTMLMediaElement&);
#endif
#if ENABLE(MEDIA_STREAM)
    ExceptionOr<Ref<MediaStreamAudioSourceNode>> createMediaStreamSource(MediaStream&);
    ExceptionOr<Ref<MediaStreamAudioDestinationNode>> createMediaStreamDestination();
#endif
    ExceptionOr<Ref<GainNode>> createGain();
    ExceptionOr<Ref<BiquadFilterNode>> createBiquadFilter();
    ExceptionOr<Ref<WaveShaperNode>> createWaveShaper();
    ExceptionOr<Ref<DelayNode>> createDelay(double maxDelayTime);
    ExceptionOr<Ref<WebKitAudioPannerNode>> createPanner();
    ExceptionOr<Ref<ConvolverNode>> createConvolver();
    ExceptionOr<Ref<DynamicsCompressorNode>> createDynamicsCompressor();
    ExceptionOr<Ref<AnalyserNode>> createAnalyser();
    ExceptionOr<Ref<ScriptProcessorNode>> createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels);
    ExceptionOr<Ref<ChannelSplitterNode>> createChannelSplitter(size_t numberOfOutputs);
    ExceptionOr<Ref<ChannelMergerNode>> createChannelMerger(size_t numberOfInputs);
    ExceptionOr<Ref<OscillatorNode>> createOscillator();
    ExceptionOr<Ref<PeriodicWave>> createPeriodicWave(Float32Array& real, Float32Array& imaginary);

    // When a source node has no more processing to do (has finished playing), then it tells the context to dereference it.
    void notifyNodeFinishedProcessing(AudioNode*) final;

    // Called at the start of each render quantum.
    void handlePreRenderTasks() final;

    // Called at the end of each render quantum.
    void handlePostRenderTasks() final;

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    // We schedule deletion of all marked nodes at the end of each realtime render quantum.
    void markForDeletion(AudioNode&) final;
    void deleteMarkedNodes() final;

    // AudioContext can pull node(s) at the end of each render quantum even when they are not connected to any downstream nodes.
    // These two methods are called by the nodes who want to add/remove themselves into/from the automatic pull lists.
    void addAutomaticPullNode(AudioNode&) final;
    void removeAutomaticPullNode(AudioNode&) final;

    // Called right before handlePostRenderTasks() to handle nodes which need to be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess) final;

    // Keeps track of the number of connections made.
    void incrementConnectionCount() final
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    //
    // Thread Safety and Graph Locking:
    //

    void setAudioThread(Thread& thread) final { m_audioThread = &thread; } // FIXME: check either not initialized or the same
    Thread* audioThread() const { return m_audioThread; }
    bool isAudioThread() const final;

    // Returns true only after the audio thread has been started and then shutdown.
    bool isAudioThreadFinished() final { return m_isAudioThreadFinished; }

    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    void lock(bool& mustReleaseLock) final;

    // Returns true if we own the lock.
    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    bool tryLock(bool& mustReleaseLock) final;

    void unlock() final;

    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() const final;

    // Returns the maximum number of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels; }

    // In AudioNode::deref() a tryLock() is used for calling finishDeref(), but if it fails keep track here.
    void addDeferredFinishDeref(AudioNode*) final;

    // In the audio thread at the start of each render cycle, we'll call handleDeferredFinishDerefs().
    void handleDeferredFinishDerefs();

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*) final;
    void markAudioNodeOutputDirty(AudioNodeOutput*) final;

    // Must be called on main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*) final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final;

    void startRendering();
    void finishedRendering(bool didRendering) final;

    static unsigned s_hardwareContextCount;

    // Restrictions to change default behaviors.
    BehaviorRestrictions behaviorRestrictions() const final { return m_restrictions; }
    void addBehaviorRestriction(BehaviorRestrictions restriction) final { m_restrictions |= restriction; }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) final { m_restrictions &= ~restriction; }

    void isPlayingAudioDidChange() final;

    void nodeWillBeginPlayback() final;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    WTFLogChannel& logChannel() const final;
    const void* nextAudioNodeLogIdentifier() final { return childLogIdentifier(m_logIdentifier, ++m_nextAudioNodeIdentifier); }
    const void* nextAudioParameterLogIdentifier() final { return childLogIdentifier(m_logIdentifier, ++m_nextAudioParameterIdentifier); }
#endif

    void postTask(WTF::Function<void()>&&) final;
    bool isStopped() const final { return m_isStopScheduled; }
    const SecurityOrigin* origin() const final;
    void addConsoleMessage(MessageSource, MessageLevel, const String& message) final;

protected:
    explicit WebKitAudioContext(Document&);
    WebKitAudioContext(Document&, AudioBuffer* renderTarget);

    static bool isSampleRateRangeGood(float sampleRate);
    void clearPendingActivity();
    void makePendingActivity();

private:
    void constructCommon();

    void lazyInitialize();
    void uninitialize();

    bool willBeginPlayback();
    bool willPausePlayback();

    bool userGestureRequiredForAudioStart() const { return !isOfflineContext() && m_restrictions & RequireUserGestureForAudioStartRestriction; }
    bool pageConsentRequiredForAudioStart() const { return !isOfflineContext() && m_restrictions & RequirePageConsentForAudioStartRestriction; }

    void setState(State);

    void clear();

    void scheduleNodeDeletion();

    void mediaCanStart(Document&) override;

    // EventTarget
    void dispatchEvent(Event&) final;

    // MediaProducer
    MediaProducer::MediaStateFlags mediaState() const override;
    void pageMutedStateDidChange() override;

    // The context itself keeps a reference to all source nodes.  The source nodes, then reference all nodes they're connected to.
    // In turn, these nodes reference all nodes they're connected to.  All nodes are ultimately connected to the AudioDestinationNode.
    // When the context dereferences a source node, it will be deactivated from the rendering graph along with all other nodes it is
    // uniquely connected to.  See the AudioNode::ref() and AudioNode::deref() methods for more details.
    void refNode(AudioNode&);
    void derefNode(AudioNode&);

    // ActiveDOMObject API.
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() override;
    const char* activeDOMObjectName() const override;

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    // PlatformMediaSessionClient
    PlatformMediaSession::MediaType mediaType() const override { return PlatformMediaSession::MediaType::WebAudio; }
    PlatformMediaSession::MediaType presentationType() const override { return PlatformMediaSession::MediaType::WebAudio; }
    void mayResumePlayback(bool shouldResume) override;
    void suspendPlayback() override;
    bool canReceiveRemoteControlCommands() const override { return false; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*) override { }
    bool supportsSeeking() const override { return false; }
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const override { return false; }
    bool canProduceAudio() const final { return true; }
    bool isSuspended() const final;

    void visibilityStateChanged() final;

    bool isAudioContext() const final { return false; }
    bool isWebKitAudioContext() const final { return true; }

    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    void addReaction(State, DOMPromiseDeferred<void>&&);
    void updateAutomaticPullNodes();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "WebKitAudioContext"; }

    Ref<Logger> m_logger;
    const void* m_logIdentifier;
    uint64_t m_nextAudioNodeIdentifier { 0 };
    uint64_t m_nextAudioParameterIdentifier { 0 };
#endif

    // Only accessed in the audio thread.
    Vector<AudioNode*> m_finishedNodes;

    // We don't use RefPtr<AudioNode> here because AudioNode has a more complex ref() / deref() implementation
    // with an optional argument for refType.  We need to use the special refType: RefTypeConnection
    // Either accessed when the graph lock is held, or on the main thread when the audio thread has finished.
    Vector<AudioNode*> m_referencedNodes;

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
    Vector<AudioNode*> m_deferredFinishDerefList;
    Vector<Vector<DOMPromiseDeferred<void>>> m_stateReactions;

    std::unique_ptr<PlatformMediaSession> m_mediaSession;
    UniqueRef<MainThreadGenericEventQueue> m_eventQueue;

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

    BehaviorRestrictions m_restrictions { NoRestrictions };

    State m_state { State::Suspended };
    RefPtr<PendingActivity<WebKitAudioContext>> m_pendingActivity;
};

inline AudioContextState WebKitAudioContext::state() const
{
    return m_state;
}

} // WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebKitAudioContext)
    static bool isType(const WebCore::AudioContextBase& context) { return context.isWebKitAudioContext(); }
SPECIALIZE_TYPE_TRAITS_END()
