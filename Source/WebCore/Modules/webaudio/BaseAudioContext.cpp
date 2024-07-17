/*
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "AudioContext.h"

#include "AnalyserNode.h"
#include "AsyncAudioDecoder.h"
#include "AudioBuffer.h"
#include "AudioBufferCallback.h"
#include "AudioBufferOptions.h"
#include "AudioBufferSourceNode.h"
#include "AudioDestination.h"
#include "AudioListener.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioParamDescriptor.h"
#include "AudioSession.h"
#include "AudioWorklet.h"
#include "BiquadFilterNode.h"
#include "ChannelMergerNode.h"
#include "ChannelMergerOptions.h"
#include "ChannelSplitterNode.h"
#include "ChannelSplitterOptions.h"
#include "ConstantSourceNode.h"
#include "ConstantSourceOptions.h"
#include "ConvolverNode.h"
#include "DelayNode.h"
#include "DelayOptions.h"
#include "DocumentInlines.h"
#include "DynamicsCompressorNode.h"
#include "EventNames.h"
#include "FFTFrame.h"
#include "FrameLoader.h"
#include "GainNode.h"
#include "HRTFDatabaseLoader.h"
#include "HRTFPanner.h"
#include "IIRFilterNode.h"
#include "IIRFilterOptions.h"
#include "JSAudioBuffer.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "NetworkingContext.h"
#include "OriginAccessPatterns.h"
#include "OscillatorNode.h"
#include "Page.h"
#include "PannerNode.h"
#include "PeriodicWave.h"
#include "PeriodicWaveOptions.h"
#include "PlatformMediaSessionManager.h"
#include "ScriptController.h"
#include "ScriptProcessorNode.h"
#include "StereoPannerNode.h"
#include "StereoPannerOptions.h"
#include "WaveShaperNode.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/Atomics.h>
#include <wtf/MainThread.h>
#include <wtf/NativePromise.h>
#include <wtf/Ref.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

#if USE(GSTREAMER)
#include "GStreamerCommon.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(BaseAudioContext);

bool BaseAudioContext::isSupportedSampleRate(float sampleRate)
{
    return sampleRate >= 3000 && sampleRate <= 384000;
}

static uint64_t generateContextID()
{
    ASSERT(isMainThread());
    static uint64_t contextIDSeed = 0;
    return ++contextIDSeed;
}

static HashSet<uint64_t>& liveAudioContexts()
{
    ASSERT(isMainThread());
    static NeverDestroyed<HashSet<uint64_t>> contexts;
    return contexts;
}

BaseAudioContext::BaseAudioContext(Document& document)
    : ActiveDOMObject(document)
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
    , m_contextID(generateContextID())
    , m_worklet(AudioWorklet::create(*this))
    , m_listener(AudioListener::create(*this))
    , m_noiseInjectionPolicy(document.noiseInjectionPolicy())
{
    liveAudioContexts().add(m_contextID);

    FFTFrame::initialize();
}

BaseAudioContext::~BaseAudioContext()
{
    liveAudioContexts().remove(m_contextID);
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: BaseAudioContext::~AudioContext()\n", this);
#endif
    ASSERT(m_nodesToDelete.isEmpty());
    ASSERT(m_referencedSourceNodes.isEmpty());
    ASSERT(m_automaticPullNodes.isEmpty());
    if (m_automaticPullNodesNeedUpdating)
        m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());
    ASSERT(m_renderingAutomaticPullNodes.isEmpty());
    // FIXME: Can we assert that m_deferredBreakConnectionList is empty?
}

bool BaseAudioContext::isContextAlive(uint64_t contextID)
{
    return liveAudioContexts().contains(contextID);
}

void BaseAudioContext::lazyInitialize()
{
    if (isStopped() || isClosed())
        return;

    if (m_isInitialized)
        return;

    // Don't allow the context to initialize a second time after it's already been explicitly uninitialized.
    ASSERT(!m_isAudioThreadFinished);
    if (m_isAudioThreadFinished)
        return;

    destination().initialize();

    m_isInitialized = true;
}

void BaseAudioContext::clear()
{
    Ref protectedThis { *this };

    // Audio thread is dead. Nobody will schedule node deletion action. Let's do it ourselves.
    do {
        m_nodesToDelete = std::exchange(m_nodesMarkedForDeletion, { });
        deleteMarkedNodes();
    } while (!m_nodesToDelete.isEmpty());
}

void BaseAudioContext::uninitialize()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());

    if (!m_isInitialized)
        return;

    // This stops the audio thread and all audio rendering.
    destination().uninitialize();

    // Don't allow the context to initialize a second time after it's already been explicitly uninitialized.
    m_isAudioThreadFinished = true;

    finishTailProcessing();

    {
        Locker locker { graphLock() };
        // This should have been called from handlePostRenderTasks() at the end of rendering.
        // However, in case of lock contention, the tryLock() call could have failed in handlePostRenderTasks(),
        // leaving nodes in m_referencedSourceNodes. Now that the audio thread is gone, make sure we deref those nodes
        // before the BaseAudioContext gets destroyed.
        derefFinishedSourceNodes();
    }

    // Get rid of the sources which may still be playing.
    derefUnfinishedSourceNodes();

    m_isInitialized = false;
}

void BaseAudioContext::addReaction(State state, DOMPromiseDeferred<void>&& promise)
{
    size_t stateIndex = static_cast<size_t>(state);
    if (stateIndex >= m_stateReactions.size())
        m_stateReactions.grow(stateIndex + 1);

    m_stateReactions[stateIndex].append(WTFMove(promise));
}

void BaseAudioContext::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventNames().statechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
        PlatformMediaSessionManager::updateNowPlayingInfoIfNecessary();
    }

    size_t stateIndex = static_cast<size_t>(state);
    if (stateIndex >= m_stateReactions.size())
        return;

    Vector<DOMPromiseDeferred<void>> reactions;
    m_stateReactions[stateIndex].swap(reactions);

    for (auto& promise : reactions)
        promise.resolve();
}

void BaseAudioContext::stop()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);
    
    // Usually ScriptExecutionContext calls stop twice.
    if (m_isStopScheduled)
        return;

    Ref protectedThis { *this };

    m_isStopScheduled = true;

    ASSERT(document());
    document()->updateIsPlayingMedia();

    uninitialize();
    clear();
}

Document* BaseAudioContext::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

bool BaseAudioContext::wouldTaintOrigin(const URL& url) const
{
    if (url.protocolIsData())
        return false;

    if (RefPtr document = this->document())
        return !document->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton());

    return false;
}

ExceptionOr<Ref<AudioBuffer>> BaseAudioContext::createBuffer(unsigned numberOfChannels, unsigned length, float sampleRate)
{
    return AudioBuffer::create(AudioBufferOptions {numberOfChannels, length, sampleRate});
}

void BaseAudioContext::decodeAudioData(Ref<ArrayBuffer>&& audioData, RefPtr<AudioBufferCallback>&& successCallback, RefPtr<AudioBufferCallback>&& errorCallback, Ref<DeferredPromise>&& promise)
{
    if (!document() || !document()->isFullyActive()) {
        promise->reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s });
        return;
    }

    if (!m_audioDecoder)
        m_audioDecoder = makeUnique<AsyncAudioDecoder>();

    audioData->pin();

    auto p = m_audioDecoder->decodeAsync(audioData.copyRef(), sampleRate());
    p->whenSettled(RunLoop::current(), [this, audioData = WTFMove(audioData), activity = makePendingActivity(*this), successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), promise = WTFMove(promise)] (DecodingTaskPromise::Result&& result) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::InternalAsyncTask, [audioData = WTFMove(audioData), successCallback = WTFMove(successCallback), errorCallback = WTFMove(errorCallback), promise = WTFMove(promise), result = WTFMove(result)]() mutable {

            audioData->unpin();

            if (!result) {
                promise->reject(WTFMove(result.error()));
                if (errorCallback)
                    errorCallback->handleEvent(nullptr);
                return;
            }
            auto audioBuffer = WTFMove(result.value());
            promise->resolve<IDLInterface<AudioBuffer>>(audioBuffer.get());
            if (successCallback)
                successCallback->handleEvent(audioBuffer.ptr());
        });
    });
}

ExceptionOr<Ref<AudioBufferSourceNode>> BaseAudioContext::createBufferSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    return AudioBufferSourceNode::create(*this);
}

ExceptionOr<Ref<ScriptProcessorNode>> BaseAudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());

    // W3C Editor's Draft 06 June 2017
    //  https://webaudio.github.io/web-audio-api/#widl-BaseAudioContext-createScriptProcessor-ScriptProcessorNode-unsigned-long-bufferSize-unsigned-long-numberOfInputChannels-unsigned-long-numberOfOutputChannels

    // The bufferSize parameter determines the buffer size in units of sample-frames. If it's not passed in,
    // or if the value is 0, then the implementation will choose the best buffer size for the given environment,
    // which will be constant power of 2 throughout the lifetime of the node. ... If the value of this parameter
    // is not one of the allowed power-of-2 values listed above, an IndexSizeError must be thrown.
    switch (bufferSize) {
    case 0:
#if USE(AUDIO_SESSION)
        // Pick a value between 256 (2^8) and 16384 (2^14), based on the buffer size of the current AudioSession:
        bufferSize = 1 << std::max<size_t>(8, std::min<size_t>(14, std::log2(AudioSession::sharedSession().bufferSize())));
#else
        bufferSize = 2048;
#endif
        break;
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
        break;
    default:
        return Exception { ExceptionCode::IndexSizeError, "Unsupported buffer size for ScriptProcessorNode"_s };
    }

    // An IndexSizeError exception must be thrown if bufferSize or numberOfInputChannels or numberOfOutputChannels
    // are outside the valid range. It is invalid for both numberOfInputChannels and numberOfOutputChannels to be zero.
    // In this case an IndexSizeError must be thrown.

    if (!numberOfInputChannels && !numberOfOutputChannels)
        return Exception { ExceptionCode::IndexSizeError, "numberOfInputChannels and numberOfOutputChannels cannot both be 0"_s };

    // This parameter [numberOfInputChannels] determines the number of channels for this node's input. Values of
    // up to 32 must be supported. A NotSupportedError must be thrown if the number of channels is not supported.

    if (numberOfInputChannels > maxNumberOfChannels)
        return Exception { ExceptionCode::IndexSizeError, "numberOfInputChannels exceeds maximum number of channels"_s };

    // This parameter [numberOfOutputChannels] determines the number of channels for this node's output. Values of
    // up to 32 must be supported. A NotSupportedError must be thrown if the number of channels is not supported.

    if (numberOfOutputChannels > maxNumberOfChannels)
        return Exception { ExceptionCode::IndexSizeError, "numberOfOutputChannels exceeds maximum number of channels"_s };

    return ScriptProcessorNode::create(*this, bufferSize, numberOfInputChannels, numberOfOutputChannels);
}

ExceptionOr<Ref<BiquadFilterNode>> BaseAudioContext::createBiquadFilter()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return BiquadFilterNode::create(*this);
}

ExceptionOr<Ref<WaveShaperNode>> BaseAudioContext::createWaveShaper()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return WaveShaperNode::create(*this);
}

ExceptionOr<Ref<PannerNode>> BaseAudioContext::createPanner()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return PannerNode::create(*this);
}

ExceptionOr<Ref<ConvolverNode>> BaseAudioContext::createConvolver()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return ConvolverNode::create(*this);
}

ExceptionOr<Ref<DynamicsCompressorNode>> BaseAudioContext::createDynamicsCompressor()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return DynamicsCompressorNode::create(*this);
}

ExceptionOr<Ref<AnalyserNode>> BaseAudioContext::createAnalyser()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return AnalyserNode::create(*this);
}

ExceptionOr<Ref<GainNode>> BaseAudioContext::createGain()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return GainNode::create(*this);
}

ExceptionOr<Ref<DelayNode>> BaseAudioContext::createDelay(double maxDelayTime)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    DelayOptions options;
    options.maxDelayTime = maxDelayTime;
    return DelayNode::create(*this, options);
}

ExceptionOr<Ref<ChannelSplitterNode>> BaseAudioContext::createChannelSplitter(size_t numberOfOutputs)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());

    ChannelSplitterOptions options;
    options.numberOfOutputs = numberOfOutputs;
    return ChannelSplitterNode::create(*this, options);
}

ExceptionOr<Ref<ChannelMergerNode>> BaseAudioContext::createChannelMerger(size_t numberOfInputs)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());

    ChannelMergerOptions options;
    options.numberOfInputs = numberOfInputs;
    return ChannelMergerNode::create(*this, options);
}

ExceptionOr<Ref<OscillatorNode>> BaseAudioContext::createOscillator()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return OscillatorNode::create(*this);
}

ExceptionOr<Ref<PeriodicWave>> BaseAudioContext::createPeriodicWave(Vector<float>&& real, Vector<float>&& imaginary, const PeriodicWaveConstraints& constraints)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    
    PeriodicWaveOptions options;
    options.real = WTFMove(real);
    options.imag = WTFMove(imaginary);
    options.disableNormalization = constraints.disableNormalization;
    return PeriodicWave::create(*this, WTFMove(options));
}

ExceptionOr<Ref<ConstantSourceNode>> BaseAudioContext::createConstantSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return ConstantSourceNode::create(*this);
}

ExceptionOr<Ref<StereoPannerNode>> BaseAudioContext::createStereoPanner()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
    ASSERT(isMainThread());
    return StereoPannerNode::create(*this);
}

ExceptionOr<Ref<IIRFilterNode>> BaseAudioContext::createIIRFilter(ScriptExecutionContext& scriptExecutionContext, Vector<double>&& feedforward, Vector<double>&& feedback)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    IIRFilterOptions options;
    options.feedforward = WTFMove(feedforward);
    options.feedback = WTFMove(feedback);
    return IIRFilterNode::create(scriptExecutionContext, *this, WTFMove(options));
}

void BaseAudioContext::derefFinishedSourceNodes()
{
    ASSERT(isGraphOwner());
    ASSERT(isAudioThread() || isAudioThreadFinished());

    if (!m_hasFinishedAudioSourceNodes)
        return;

    m_referencedSourceNodes.removeAllMatching([](auto& node) { return node->isFinishedSourceNode(); });
    m_hasFinishedAudioSourceNodes = false;
}

void BaseAudioContext::derefUnfinishedSourceNodes()
{
    ASSERT(isMainThread() && isAudioThreadFinished());
    m_referencedSourceNodes.clear();
}

void BaseAudioContext::addDeferredDecrementConnectionCount(AudioNode* node)
{
    ASSERT(isAudioThread());
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_deferredBreakConnectionList.append(node);
}

void BaseAudioContext::handlePreRenderTasks(const AudioIOPosition& outputPosition)
{
    ASSERT(isAudioThread());

    // At the beginning of every render quantum, try to update the internal rendering graph state (from main thread changes).
    // It's OK if the tryLock() fails, we'll just take slightly longer to pick up the changes.
    if (auto locker = Locker<RecursiveLock>::tryLock(graphLock())) {
        // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
        handleDirtyAudioSummingJunctions();
        handleDirtyAudioNodeOutputs();

        updateAutomaticPullNodes();
        m_outputPosition = outputPosition;

        m_listener->updateDirtyState();
    }
}

AudioIOPosition BaseAudioContext::outputPosition()
{
    ASSERT(isMainThread());
    Locker locker { graphLock() };
    return m_outputPosition;
}

void BaseAudioContext::handlePostRenderTasks()
{
    ASSERT(isAudioThread());

    // Must use a tryLock() here too. Don't worry, the lock will very rarely be contended and this method is called frequently.
    // The worst that can happen is that there will be some nodes which will take slightly longer than usual to be deleted or removed
    // from the render graph (in which case they'll render silence).
    auto locker = Locker<RecursiveLock>::tryLock(graphLock());
    if (!locker)
        return;

    // Take care of finishing any derefs where the tryLock() failed previously.
    handleDeferredDecrementConnectionCounts();

    // Dynamically clean up nodes which are no longer needed.
    derefFinishedSourceNodes();

    // Don't delete in the real-time thread. Let the main thread do it.
    // Ref-counted objects held by certain AudioNodes may not be thread-safe.
    scheduleNodeDeletion();

    // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
    handleDirtyAudioSummingJunctions();
    handleDirtyAudioNodeOutputs();

    updateAutomaticPullNodes();
    updateTailProcessingNodes();
}

void BaseAudioContext::handleDeferredDecrementConnectionCounts()
{
    ASSERT(isAudioThread() && isGraphOwner());
    for (auto& node : m_deferredBreakConnectionList)
        node->decrementConnectionCountWithLock();
    
    m_deferredBreakConnectionList.clear();
}

void BaseAudioContext::addTailProcessingNode(AudioNode& node)
{
    ASSERT(isGraphOwner());
    if (node.isTailProcessing()) {
        ASSERT(m_tailProcessingNodes.contains(node) || m_finishedTailProcessingNodes.contains(node));
        return;
    }

    // Ideally we'd find a way to avoid this vector append since we try to avoid potential heap allocations
    // on the audio thread for performance reasons.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    ASSERT(!m_tailProcessingNodes.contains(node));
    m_tailProcessingNodes.append(node);
}

void BaseAudioContext::removeTailProcessingNode(AudioNode& node)
{
    ASSERT(isGraphOwner());
    ASSERT(node.isTailProcessing());

    if (m_tailProcessingNodes.removeFirst(node))
        return;

    // Remove the node from finished tail processing nodes so we don't end up disabling its outputs later on the main thread.
    ASSERT(m_finishedTailProcessingNodes.contains(node));
    m_finishedTailProcessingNodes.removeFirst(node);
}

void BaseAudioContext::updateTailProcessingNodes()
{
    ASSERT(isAudioThread());
    ASSERT(isGraphOwner());
    // Go backwards as the current node may be removed from m_tailProcessingNodes as we iterate.
    // We are on the audio thread so we want to avoid allocations as much as possible.
    for (auto i = m_tailProcessingNodes.size(); i > 0; --i) {
        auto& node = m_tailProcessingNodes[i - 1];
        if (!node->propagatesSilence())
            continue; // Node is not done processing its tail.

        // Ideally we'd find a way to avoid this vector append since we try to avoid potential heap allocations
        // on the audio thread for performance reasons.
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

        // Disabling of outputs should happen on the main thread we add the node to m_finishedTailProcessingNodes
        // for disableOutputsForFinishedTailProcessingNodes() to process later on the main thread.
        ASSERT(!m_finishedTailProcessingNodes.contains(node));
        m_finishedTailProcessingNodes.append(WTFMove(node));
        m_tailProcessingNodes.remove(i - 1);
    }

    if (m_finishedTailProcessingNodes.isEmpty() || m_disableOutputsForTailProcessingScheduled)
        return;

    m_disableOutputsForTailProcessingScheduled = true;

    // We try to avoid heap allocations on the audio thread but there is no way to do a main thread dispatch
    // without one.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThread([this, protectedThis = Ref { *this }]() mutable {
        Locker locker { graphLock() };
        disableOutputsForFinishedTailProcessingNodes();
        m_disableOutputsForTailProcessingScheduled = false;
    });
}

void BaseAudioContext::disableOutputsForFinishedTailProcessingNodes()
{
    ASSERT(isMainThread());
    ASSERT(isGraphOwner());
    for (auto& finishedTailProcessingNode : std::exchange(m_finishedTailProcessingNodes, { }))
        finishedTailProcessingNode->disableOutputs();
}

void BaseAudioContext::finishTailProcessing()
{
    ASSERT(isMainThread());
    Locker locker { graphLock() };

    // disableOutputs() can cause new nodes to start tail processing so we need to loop until both vectors are empty.
    while (!m_tailProcessingNodes.isEmpty() || !m_finishedTailProcessingNodes.isEmpty()) {
        for (auto& tailProcessingNode : std::exchange(m_tailProcessingNodes, { }))
            tailProcessingNode->disableOutputs();
        disableOutputsForFinishedTailProcessingNodes();
    }
}

void BaseAudioContext::markForDeletion(AudioNode& node)
{
    ASSERT(isGraphOwner());
    ASSERT_WITH_MESSAGE(node.nodeType() != AudioNode::NodeTypeDestination, "Destination node is owned by the BaseAudioContext");

    if (isAudioThreadFinished())
        m_nodesToDelete.append(&node);
    else {
        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        m_nodesMarkedForDeletion.append(&node);
    }

    // This is probably the best time for us to remove the node from automatic pull list,
    // since all connections are gone and we hold the graph lock. Then when handlePostRenderTasks()
    // gets a chance to schedule the deletion work, updateAutomaticPullNodes() also gets a chance to
    // modify m_renderingAutomaticPullNodes.
    removeAutomaticPullNode(node);
}

void BaseAudioContext::unmarkForDeletion(AudioNode& node)
{
    ASSERT(isGraphOwner());
    ASSERT_WITH_MESSAGE(node.nodeType() != AudioNode::NodeTypeDestination, "Destination node is owned by the BaseAudioContext");

    m_nodesToDelete.removeFirst(&node);
    m_nodesMarkedForDeletion.removeFirst(&node);
}

void BaseAudioContext::scheduleNodeDeletion()
{
    bool isGood = m_isInitialized && isGraphOwner();
    ASSERT(isGood);
    if (!isGood)
        return;

    // Make sure to call deleteMarkedNodes() on main thread.    
    if (!m_nodesMarkedForDeletion.isEmpty() && !m_isDeletionScheduled) {
        ASSERT(m_nodesToDelete.isEmpty());
        m_nodesToDelete = std::exchange(m_nodesMarkedForDeletion, { });

        m_isDeletionScheduled = true;

        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        callOnMainThread([protectedThis = Ref { *this }]() mutable {
            protectedThis->deleteMarkedNodes();
        });
    }
}

void BaseAudioContext::deleteMarkedNodes()
{
    ASSERT(isMainThread());

    // Protect this object from being deleted before we release the lock.
    Ref protectedThis { *this };

    Locker locker { graphLock() };

    while (m_nodesToDelete.size()) {
        AudioNode* node = m_nodesToDelete.takeLast();

        // Before deleting the node, clear out any AudioNodeInputs from m_dirtySummingJunctions.
        unsigned numberOfInputs = node->numberOfInputs();
        for (unsigned i = 0; i < numberOfInputs; ++i)
            m_dirtySummingJunctions.remove(node->input(i));

        // Before deleting the node, clear out any AudioNodeOutputs from m_dirtyAudioNodeOutputs.
        unsigned numberOfOutputs = node->numberOfOutputs();
        for (unsigned i = 0; i < numberOfOutputs; ++i)
            m_dirtyAudioNodeOutputs.remove(node->output(i));

        ASSERT_WITH_MESSAGE(node->nodeType() != AudioNode::NodeTypeDestination, "Destination node is owned by the BaseAudioContext");

        // Finally, delete it.
        delete node;
    }
    m_isDeletionScheduled = false;
}

void BaseAudioContext::markSummingJunctionDirty(AudioSummingJunction* summingJunction)
{
    ASSERT(isGraphOwner());
    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_dirtySummingJunctions.add(summingJunction);
}

void BaseAudioContext::removeMarkedSummingJunction(AudioSummingJunction* summingJunction)
{
    ASSERT(isMainThread());
    Locker locker { graphLock() };
    m_dirtySummingJunctions.remove(summingJunction);
}

enum EventTargetInterfaceType BaseAudioContext::eventTargetInterface() const
{
    return EventTargetInterfaceType::BaseAudioContext;
}

void BaseAudioContext::markAudioNodeOutputDirty(AudioNodeOutput* output)
{
    ASSERT(isGraphOwner());
    m_dirtyAudioNodeOutputs.add(output);
}

void BaseAudioContext::handleDirtyAudioSummingJunctions()
{
    ASSERT(isGraphOwner());

    for (auto& junction : m_dirtySummingJunctions)
        junction->updateRenderingState();

    m_dirtySummingJunctions.clear();
}

void BaseAudioContext::handleDirtyAudioNodeOutputs()
{
    ASSERT(isGraphOwner());

    for (auto& output : m_dirtyAudioNodeOutputs)
        output->updateRenderingState();

    m_dirtyAudioNodeOutputs.clear();
}

void BaseAudioContext::addAutomaticPullNode(AudioNode& node)
{
    ASSERT(isGraphOwner());

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    if (m_automaticPullNodes.add(&node).isNewEntry)
        m_automaticPullNodesNeedUpdating = true;
}

void BaseAudioContext::removeAutomaticPullNode(AudioNode& node)
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodes.remove(&node))
        m_automaticPullNodesNeedUpdating = true;
}

void BaseAudioContext::updateAutomaticPullNodes()
{
    ASSERT(isGraphOwner());

    if (!m_automaticPullNodesNeedUpdating)
        return;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    // Copy from m_automaticPullNodes to m_renderingAutomaticPullNodes.
    m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());

    unsigned i = 0;
    for (auto& output : m_automaticPullNodes)
        m_renderingAutomaticPullNodes[i++] = output;

    m_automaticPullNodesNeedUpdating = false;
}

void BaseAudioContext::processAutomaticPullNodes(size_t framesToProcess)
{
    ASSERT(isAudioThread());

    for (auto& node : m_renderingAutomaticPullNodes)
        node->processIfNecessary(framesToProcess);
}

ScriptExecutionContext* BaseAudioContext::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void BaseAudioContext::postTask(Function<void()>&& task)
{
    ASSERT(isMainThread());
    if (!m_isStopScheduled)
        queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, WTFMove(task));
}

const SecurityOrigin* BaseAudioContext::origin() const
{
    RefPtr context = scriptExecutionContext();
    return context ? context->securityOrigin() : nullptr;
}

void BaseAudioContext::addConsoleMessage(MessageSource source, MessageLevel level, const String& message)
{
    if (RefPtr context = scriptExecutionContext())
        context->addConsoleMessage(source, level, message);
}

PeriodicWave& BaseAudioContext::periodicWave(OscillatorType type)
{
    switch (type) {
    case OscillatorType::Square:
        if (!m_cachedPeriodicWaveSquare)
            m_cachedPeriodicWaveSquare = PeriodicWave::createSquare(sampleRate());
        return *m_cachedPeriodicWaveSquare;
    case OscillatorType::Sawtooth:
        if (!m_cachedPeriodicWaveSawtooth)
            m_cachedPeriodicWaveSawtooth = PeriodicWave::createSawtooth(sampleRate());
        return *m_cachedPeriodicWaveSawtooth;
    case OscillatorType::Triangle:
        if (!m_cachedPeriodicWaveTriangle)
            m_cachedPeriodicWaveTriangle = PeriodicWave::createTriangle(sampleRate());
        return *m_cachedPeriodicWaveTriangle;
    case OscillatorType::Custom:
        RELEASE_ASSERT_NOT_REACHED();
    case OscillatorType::Sine:
        if (!m_cachedPeriodicWaveSine)
            m_cachedPeriodicWaveSine = PeriodicWave::createSine(sampleRate());
        return *m_cachedPeriodicWaveSine;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void BaseAudioContext::addAudioParamDescriptors(const String& processorName, Vector<AudioParamDescriptor>&& descriptors)
{
    ASSERT(!m_parameterDescriptorMap.contains(processorName));
    bool wasEmpty = m_parameterDescriptorMap.isEmpty();
    m_parameterDescriptorMap.add(processorName, WTFMove(descriptors));
    if (wasEmpty)
        workletIsReady();
}

void BaseAudioContext::sourceNodeWillBeginPlayback(AudioNode& node)
{
    ASSERT(isMainThread());
    Locker locker { graphLock() };

    ASSERT(!m_referencedSourceNodes.contains(&node));
    // Reference source node to keep it alive and playing even if its JS wrapper gets garbage collected.
    m_referencedSourceNodes.append(&node);
}

void BaseAudioContext::sourceNodeDidFinishPlayback(AudioNode& node)
{
    ASSERT(isAudioThread());

    node.setIsFinishedSourceNode();
    m_hasFinishedAudioSourceNodes = true;
}

void BaseAudioContext::workletIsReady()
{
    ASSERT(isMainThread());

    // If we're already rendering when the worklet becomes ready, we need to restart
    // rendering in order to switch to the audio worklet thread.
    destination().restartRendering();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& BaseAudioContext::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
