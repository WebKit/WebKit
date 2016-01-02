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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"

#include "AnalyserNode.h"
#include "AsyncAudioDecoder.h"
#include "AudioBuffer.h"
#include "AudioBufferCallback.h"
#include "AudioBufferSourceNode.h"
#include "AudioListener.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "BiquadFilterNode.h"
#include "ChannelMergerNode.h"
#include "ChannelSplitterNode.h"
#include "ConvolverNode.h"
#include "DefaultAudioDestinationNode.h"
#include "DelayNode.h"
#include "Document.h"
#include "DynamicsCompressorNode.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FFTFrame.h"
#include "GainNode.h"
#include "GenericEventQueue.h"
#include "HRTFDatabaseLoader.h"
#include "HRTFPanner.h"
#include "JSDOMPromise.h"
#include "OfflineAudioCompletionEvent.h"
#include "OfflineAudioDestinationNode.h"
#include "OscillatorNode.h"
#include "Page.h"
#include "PannerNode.h"
#include "PeriodicWave.h"
#include "ScriptController.h"
#include "ScriptProcessorNode.h"
#include "WaveShaperNode.h"
#include <inspector/ScriptCallStack.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(MEDIA_STREAM)
#include "MediaStream.h"
#include "MediaStreamAudioDestinationNode.h"
#include "MediaStreamAudioSource.h"
#include "MediaStreamAudioSourceNode.h"
#endif

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#include "MediaElementAudioSourceNode.h"
#endif

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

#if USE(GSTREAMER)
#include "GStreamerUtilities.h"
#endif

#if PLATFORM(IOS)
#include "ScriptController.h"
#include "Settings.h"
#endif

#include <runtime/ArrayBuffer.h>
#include <wtf/Atomics.h>
#include <wtf/MainThread.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

// FIXME: check the proper way to reference an undefined thread ID
const int UndefinedThreadIdentifier = 0xffffffff;

const unsigned MaxPeriodicWaveLength = 4096;

namespace WebCore {
    
bool AudioContext::isSampleRateRangeGood(float sampleRate)
{
    // FIXME: It would be nice if the minimum sample-rate could be less than 44.1KHz,
    // but that will require some fixes in HRTFPanner::fftSizeForSampleRate(), and some testing there.
    return sampleRate >= 44100 && sampleRate <= 96000;
}

// Don't allow more than this number of simultaneous AudioContexts talking to hardware.
const unsigned MaxHardwareContexts = 4;
unsigned AudioContext::s_hardwareContextCount = 0;
    
RefPtr<AudioContext> AudioContext::create(Document& document, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);

    ASSERT(isMainThread());
    if (s_hardwareContextCount >= MaxHardwareContexts)
        return nullptr;

    RefPtr<AudioContext> audioContext(adoptRef(new AudioContext(document)));
    audioContext->suspendIfNeeded();
    return audioContext;
}

// Constructor for rendering to the audio hardware.
AudioContext::AudioContext(Document& document)
    : ActiveDOMObject(&document)
    , m_mediaSession(PlatformMediaSession::create(*this))
    , m_eventQueue(std::make_unique<GenericEventQueue>(*this))
    , m_graphOwnerThread(UndefinedThreadIdentifier)
{
    constructCommon();

    m_destinationNode = DefaultAudioDestinationNode::create(this);

    // Initialize the destination node's muted state to match the page's current muted state.
    pageMutedStateDidChange();
}

// Constructor for offline (non-realtime) rendering.
AudioContext::AudioContext(Document& document, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate)
    : ActiveDOMObject(&document)
    , m_isOfflineContext(true)
    , m_mediaSession(PlatformMediaSession::create(*this))
    , m_eventQueue(std::make_unique<GenericEventQueue>(*this))
    , m_graphOwnerThread(UndefinedThreadIdentifier)
{
    constructCommon();

    // Create a new destination for offline rendering.
    m_renderTarget = AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate);
    m_destinationNode = OfflineAudioDestinationNode::create(this, m_renderTarget.get());
}

void AudioContext::constructCommon()
{
    // According to spec AudioContext must die only after page navigate.
    // Lets mark it as ActiveDOMObject with pending activity and unmark it in clear method.
    setPendingActivity(this);

#if USE(GSTREAMER)
    initializeGStreamer();
#endif

    FFTFrame::initialize();
    
    m_listener = AudioListener::create();

#if PLATFORM(IOS)
    if (!document()->settings() || document()->settings()->requiresUserGestureForMediaPlayback())
        addBehaviorRestriction(RequireUserGestureForAudioStartRestriction);
    else
        m_restrictions = NoRestrictions;
#endif

#if PLATFORM(COCOA)
    addBehaviorRestriction(RequirePageConsentForAudioStartRestriction);
#endif

    m_mediaSession->setCanProduceAudio(true);
}

AudioContext::~AudioContext()
{
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: AudioContext::~AudioContext()\n", this);
#endif
    ASSERT(!m_isInitialized);
    ASSERT(m_isStopScheduled);
    ASSERT(m_nodesToDelete.isEmpty());
    ASSERT(m_referencedNodes.isEmpty());
    ASSERT(m_finishedNodes.isEmpty()); // FIXME (bug 105870): This assertion fails on tests sometimes.
    ASSERT(m_automaticPullNodes.isEmpty());
    if (m_automaticPullNodesNeedUpdating)
        m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());
    ASSERT(m_renderingAutomaticPullNodes.isEmpty());
    // FIXME: Can we assert that m_deferredFinishDerefList is empty?
}

void AudioContext::lazyInitialize()
{
    if (m_isInitialized)
        return;

    // Don't allow the context to initialize a second time after it's already been explicitly uninitialized.
    ASSERT(!m_isAudioThreadFinished);
    if (m_isAudioThreadFinished)
        return;

    if (m_destinationNode.get()) {
        m_destinationNode->initialize();

        if (!isOfflineContext()) {
            document()->addAudioProducer(this);

            // This starts the audio thread. The destination node's provideInput() method will now be called repeatedly to render audio.
            // Each time provideInput() is called, a portion of the audio stream is rendered. Let's call this time period a "render quantum".
            // NOTE: for now default AudioContext does not need an explicit startRendering() call from JavaScript.
            // We may want to consider requiring it for symmetry with OfflineAudioContext.
            startRendering();
            ++s_hardwareContextCount;
        }
    }
    m_isInitialized = true;
}

void AudioContext::clear()
{
    // We have to release our reference to the destination node before the context will ever be deleted since the destination node holds a reference to the context.
    if (m_destinationNode)
        m_destinationNode = nullptr;

    // Audio thread is dead. Nobody will schedule node deletion action. Let's do it ourselves.
    do {
        deleteMarkedNodes();
        m_nodesToDelete.appendVector(m_nodesMarkedForDeletion);
        m_nodesMarkedForDeletion.clear();
    } while (m_nodesToDelete.size());

    // It was set in constructCommon.
    unsetPendingActivity(this);
}

void AudioContext::uninitialize()
{
    ASSERT(isMainThread());

    if (!m_isInitialized)
        return;

    // This stops the audio thread and all audio rendering.
    m_destinationNode->uninitialize();

    // Don't allow the context to initialize a second time after it's already been explicitly uninitialized.
    m_isAudioThreadFinished = true;

    if (!isOfflineContext()) {
        document()->removeAudioProducer(this);

        ASSERT(s_hardwareContextCount);
        --s_hardwareContextCount;

        // Offline contexts move to 'Closed' state when dispatching the completion event.
        setState(State::Closed);
    }

    // Get rid of the sources which may still be playing.
    derefUnfinishedSourceNodes();

    m_isInitialized = false;
}

bool AudioContext::isInitialized() const
{
    return m_isInitialized;
}

void AudioContext::addReaction(State state, Promise&& promise)
{
    size_t stateIndex = static_cast<size_t>(state);
    if (stateIndex >= m_stateReactions.size())
        m_stateReactions.resize(stateIndex + 1);

    m_stateReactions[stateIndex].append(WTFMove(promise));
}

void AudioContext::setState(State state)
{
    if (m_state == state)
        return;

    m_state = state;
    m_eventQueue->enqueueEvent(Event::create(eventNames().statechangeEvent, true, false));

    size_t stateIndex = static_cast<size_t>(state);
    if (stateIndex >= m_stateReactions.size())
        return;

    Vector<Promise> reactions;
    m_stateReactions[stateIndex].swap(reactions);

    for (auto& promise : reactions)
        promise.resolve(nullptr);
}

const AtomicString& AudioContext::state() const
{
    static NeverDestroyed<AtomicString> suspended("suspended");
    static NeverDestroyed<AtomicString> running("running");
    static NeverDestroyed<AtomicString> interrupted("interrupted");
    static NeverDestroyed<AtomicString> closed("closed");

    switch (m_state) {
    case State::Suspended:
        return suspended;
    case State::Running:
        return running;
    case State::Interrupted:
        return interrupted;
    case State::Closed:
        return closed;
    }

    ASSERT_NOT_REACHED();
    return suspended;
}

void AudioContext::stop()
{
    ASSERT(isMainThread());

    // Usually ScriptExecutionContext calls stop twice.
    if (m_isStopScheduled)
        return;
    m_isStopScheduled = true;

    document()->updateIsPlayingMedia();

    m_eventQueue->close();

    // Don't call uninitialize() immediately here because the ScriptExecutionContext is in the middle
    // of dealing with all of its ActiveDOMObjects at this point. uninitialize() can de-reference other
    // ActiveDOMObjects so let's schedule uninitialize() to be called later.
    // FIXME: see if there's a more direct way to handle this issue.
    // FIXME: This sounds very wrong. The whole idea of stop() is that it stops everything, and if we
    // schedule some observable work for later, the work likely happens at an inappropriate time.
    callOnMainThread([this] {
        uninitialize();
        clear();
    });
}

bool AudioContext::canSuspendForDocumentSuspension() const
{
    // FIXME: We should be able to suspend while rendering as well with some more code.
    return m_state == State::Suspended || m_state == State::Closed;
}

const char* AudioContext::activeDOMObjectName() const
{
    return "AudioContext";
}

Document* AudioContext::document() const
{
    ASSERT(m_scriptExecutionContext);
    return downcast<Document>(m_scriptExecutionContext);
}

const Document* AudioContext::hostingDocument() const
{
    return downcast<Document>(m_scriptExecutionContext);
}

PassRefPtr<AudioBuffer> AudioContext::createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionCode& ec)
{
    RefPtr<AudioBuffer> audioBuffer = AudioBuffer::create(numberOfChannels, numberOfFrames, sampleRate);
    if (!audioBuffer.get()) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    return audioBuffer;
}

PassRefPtr<AudioBuffer> AudioContext::createBuffer(ArrayBuffer* arrayBuffer, bool mixToMono, ExceptionCode& ec)
{
    ASSERT(arrayBuffer);
    if (!arrayBuffer) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    RefPtr<AudioBuffer> audioBuffer = AudioBuffer::createFromAudioFileData(arrayBuffer->data(), arrayBuffer->byteLength(), mixToMono, sampleRate());
    if (!audioBuffer.get()) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    return audioBuffer;
}

void AudioContext::decodeAudioData(ArrayBuffer* audioData, PassRefPtr<AudioBufferCallback> successCallback, PassRefPtr<AudioBufferCallback> errorCallback, ExceptionCode& ec)
{
    if (!audioData) {
        ec = SYNTAX_ERR;
        return;
    }
    m_audioDecoder.decodeAsync(audioData, sampleRate(), successCallback, errorCallback);
}

PassRefPtr<AudioBufferSourceNode> AudioContext::createBufferSource()
{
    ASSERT(isMainThread());
    lazyInitialize();
    RefPtr<AudioBufferSourceNode> node = AudioBufferSourceNode::create(this, m_destinationNode->sampleRate());

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls AudioContext::notifyNodeFinishedProcessing().
    refNode(node.get());

    return node;
}

#if ENABLE(VIDEO)
PassRefPtr<MediaElementAudioSourceNode> AudioContext::createMediaElementSource(HTMLMediaElement* mediaElement, ExceptionCode& ec)
{
    ASSERT(mediaElement);
    if (!mediaElement) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }
        
    ASSERT(isMainThread());
    lazyInitialize();
    
    // First check if this media element already has a source node.
    if (mediaElement->audioSourceNode()) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }
        
    RefPtr<MediaElementAudioSourceNode> node = MediaElementAudioSourceNode::create(this, mediaElement);

    mediaElement->setAudioSourceNode(node.get());

    refNode(node.get()); // context keeps reference until node is disconnected
    return node;
}
#endif

#if ENABLE(MEDIA_STREAM)
PassRefPtr<MediaStreamAudioSourceNode> AudioContext::createMediaStreamSource(MediaStream* mediaStream, ExceptionCode& ec)
{
    ASSERT(isMainThread());

    ASSERT(mediaStream);
    if (!mediaStream) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    auto audioTracks = mediaStream->getAudioTracks();
    if (audioTracks.isEmpty()) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    MediaStreamTrack* providerTrack = nullptr;
    for (auto& track : audioTracks) {
        if (track->audioSourceProvider()) {
            providerTrack = track.get();
            break;
        }
    }

    if (!providerTrack) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    lazyInitialize();

    auto node = MediaStreamAudioSourceNode::create(*this, *mediaStream, *providerTrack);
    node->setFormat(2, sampleRate());

    refNode(&node.get()); // context keeps reference until node is disconnected
    return &node.get();
}

PassRefPtr<MediaStreamAudioDestinationNode> AudioContext::createMediaStreamDestination()
{
    // FIXME: Add support for an optional argument which specifies the number of channels.
    // FIXME: The default should probably be stereo instead of mono.
    return MediaStreamAudioDestinationNode::create(this, 1);
}

#endif

PassRefPtr<ScriptProcessorNode> AudioContext::createScriptProcessor(size_t bufferSize, ExceptionCode& ec)
{
    // Set number of input/output channels to stereo by default.
    return createScriptProcessor(bufferSize, 2, 2, ec);
}

PassRefPtr<ScriptProcessorNode> AudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionCode& ec)
{
    // Set number of output channels to stereo by default.
    return createScriptProcessor(bufferSize, numberOfInputChannels, 2, ec);
}

PassRefPtr<ScriptProcessorNode> AudioContext::createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionCode& ec)
{
    ASSERT(isMainThread());
    lazyInitialize();
    RefPtr<ScriptProcessorNode> node = ScriptProcessorNode::create(this, m_destinationNode->sampleRate(), bufferSize, numberOfInputChannels, numberOfOutputChannels);

    if (!node.get()) {
        ec = INDEX_SIZE_ERR;
        return nullptr;
    }

    refNode(node.get()); // context keeps reference until we stop making javascript rendering callbacks
    return node;
}

PassRefPtr<BiquadFilterNode> AudioContext::createBiquadFilter()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return BiquadFilterNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<WaveShaperNode> AudioContext::createWaveShaper()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return WaveShaperNode::create(this);
}

PassRefPtr<PannerNode> AudioContext::createPanner()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return PannerNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<ConvolverNode> AudioContext::createConvolver()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return ConvolverNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<DynamicsCompressorNode> AudioContext::createDynamicsCompressor()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return DynamicsCompressorNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<AnalyserNode> AudioContext::createAnalyser()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return AnalyserNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<GainNode> AudioContext::createGain()
{
    ASSERT(isMainThread());
    lazyInitialize();
    return GainNode::create(this, m_destinationNode->sampleRate());
}

PassRefPtr<DelayNode> AudioContext::createDelay(ExceptionCode& ec)
{
    const double defaultMaxDelayTime = 1;
    return createDelay(defaultMaxDelayTime, ec);
}

PassRefPtr<DelayNode> AudioContext::createDelay(double maxDelayTime, ExceptionCode& ec)
{
    ASSERT(isMainThread());
    lazyInitialize();
    RefPtr<DelayNode> node = DelayNode::create(this, m_destinationNode->sampleRate(), maxDelayTime, ec);
    if (ec)
        return nullptr;
    return node;
}

PassRefPtr<ChannelSplitterNode> AudioContext::createChannelSplitter(ExceptionCode& ec)
{
    const unsigned ChannelSplitterDefaultNumberOfOutputs = 6;
    return createChannelSplitter(ChannelSplitterDefaultNumberOfOutputs, ec);
}

PassRefPtr<ChannelSplitterNode> AudioContext::createChannelSplitter(size_t numberOfOutputs, ExceptionCode& ec)
{
    ASSERT(isMainThread());
    lazyInitialize();

    RefPtr<ChannelSplitterNode> node = ChannelSplitterNode::create(this, m_destinationNode->sampleRate(), numberOfOutputs);

    if (!node.get()) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    return node;
}

PassRefPtr<ChannelMergerNode> AudioContext::createChannelMerger(ExceptionCode& ec)
{
    const unsigned ChannelMergerDefaultNumberOfInputs = 6;
    return createChannelMerger(ChannelMergerDefaultNumberOfInputs, ec);
}

PassRefPtr<ChannelMergerNode> AudioContext::createChannelMerger(size_t numberOfInputs, ExceptionCode& ec)
{
    ASSERT(isMainThread());
    lazyInitialize();

    RefPtr<ChannelMergerNode> node = ChannelMergerNode::create(this, m_destinationNode->sampleRate(), numberOfInputs);

    if (!node.get()) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    return node;
}

PassRefPtr<OscillatorNode> AudioContext::createOscillator()
{
    ASSERT(isMainThread());
    lazyInitialize();

    RefPtr<OscillatorNode> node = OscillatorNode::create(this, m_destinationNode->sampleRate());

    // Because this is an AudioScheduledSourceNode, the context keeps a reference until it has finished playing.
    // When this happens, AudioScheduledSourceNode::finish() calls AudioContext::notifyNodeFinishedProcessing().
    refNode(node.get());

    return node;
}

PassRefPtr<PeriodicWave> AudioContext::createPeriodicWave(Float32Array* real, Float32Array* imag, ExceptionCode& ec)
{
    ASSERT(isMainThread());
    
    if (!real || !imag || (real->length() != imag->length() || (real->length() > MaxPeriodicWaveLength) || (real->length() <= 0))) {
        ec = SYNTAX_ERR;
        return nullptr;
    }
    
    lazyInitialize();
    return PeriodicWave::create(sampleRate(), real, imag);
}

void AudioContext::notifyNodeFinishedProcessing(AudioNode* node)
{
    ASSERT(isAudioThread());
    m_finishedNodes.append(node);
}

void AudioContext::derefFinishedSourceNodes()
{
    ASSERT(isGraphOwner());
    ASSERT(isAudioThread() || isAudioThreadFinished());
    for (auto& node : m_finishedNodes)
        derefNode(node);

    m_finishedNodes.clear();
}

void AudioContext::refNode(AudioNode* node)
{
    ASSERT(isMainThread());
    AutoLocker locker(*this);
    
    node->ref(AudioNode::RefTypeConnection);
    m_referencedNodes.append(node);
}

void AudioContext::derefNode(AudioNode* node)
{
    ASSERT(isGraphOwner());
    
    node->deref(AudioNode::RefTypeConnection);

    ASSERT(m_referencedNodes.contains(node));
    m_referencedNodes.removeFirst(node);
}

void AudioContext::derefUnfinishedSourceNodes()
{
    ASSERT(isMainThread() && isAudioThreadFinished());
    for (auto& node : m_referencedNodes)
        node->deref(AudioNode::RefTypeConnection);

    m_referencedNodes.clear();
}

void AudioContext::lock(bool& mustReleaseLock)
{
    // Don't allow regular lock in real-time audio thread.
    ASSERT(isMainThread());

    ThreadIdentifier thisThread = currentThread();

    if (thisThread == m_graphOwnerThread) {
        // We already have the lock.
        mustReleaseLock = false;
    } else {
        // Acquire the lock.
        m_contextGraphMutex.lock();
        m_graphOwnerThread = thisThread;
        mustReleaseLock = true;
    }
}

bool AudioContext::tryLock(bool& mustReleaseLock)
{
    ThreadIdentifier thisThread = currentThread();
    bool isAudioThread = thisThread == audioThread();

    // Try to catch cases of using try lock on main thread - it should use regular lock.
    ASSERT(isAudioThread || isAudioThreadFinished());
    
    if (!isAudioThread) {
        // In release build treat tryLock() as lock() (since above ASSERT(isAudioThread) never fires) - this is the best we can do.
        lock(mustReleaseLock);
        return true;
    }
    
    bool hasLock;
    
    if (thisThread == m_graphOwnerThread) {
        // Thread already has the lock.
        hasLock = true;
        mustReleaseLock = false;
    } else {
        // Don't already have the lock - try to acquire it.
        hasLock = m_contextGraphMutex.tryLock();
        
        if (hasLock)
            m_graphOwnerThread = thisThread;

        mustReleaseLock = hasLock;
    }
    
    return hasLock;
}

void AudioContext::unlock()
{
    ASSERT(currentThread() == m_graphOwnerThread);

    m_graphOwnerThread = UndefinedThreadIdentifier;
    m_contextGraphMutex.unlock();
}

bool AudioContext::isAudioThread() const
{
    return currentThread() == m_audioThread;
}

bool AudioContext::isGraphOwner() const
{
    return currentThread() == m_graphOwnerThread;
}

void AudioContext::addDeferredFinishDeref(AudioNode* node)
{
    ASSERT(isAudioThread());
    m_deferredFinishDerefList.append(node);
}

void AudioContext::handlePreRenderTasks()
{
    ASSERT(isAudioThread());

    // At the beginning of every render quantum, try to update the internal rendering graph state (from main thread changes).
    // It's OK if the tryLock() fails, we'll just take slightly longer to pick up the changes.
    bool mustReleaseLock;
    if (tryLock(mustReleaseLock)) {
        // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
        handleDirtyAudioSummingJunctions();
        handleDirtyAudioNodeOutputs();

        updateAutomaticPullNodes();

        if (mustReleaseLock)
            unlock();
    }
}

void AudioContext::handlePostRenderTasks()
{
    ASSERT(isAudioThread());

    // Must use a tryLock() here too. Don't worry, the lock will very rarely be contended and this method is called frequently.
    // The worst that can happen is that there will be some nodes which will take slightly longer than usual to be deleted or removed
    // from the render graph (in which case they'll render silence).
    bool mustReleaseLock;
    if (tryLock(mustReleaseLock)) {
        // Take care of finishing any derefs where the tryLock() failed previously.
        handleDeferredFinishDerefs();

        // Dynamically clean up nodes which are no longer needed.
        derefFinishedSourceNodes();

        // Don't delete in the real-time thread. Let the main thread do it.
        // Ref-counted objects held by certain AudioNodes may not be thread-safe.
        scheduleNodeDeletion();

        // Fixup the state of any dirty AudioSummingJunctions and AudioNodeOutputs.
        handleDirtyAudioSummingJunctions();
        handleDirtyAudioNodeOutputs();

        updateAutomaticPullNodes();

        if (mustReleaseLock)
            unlock();
    }
}

void AudioContext::handleDeferredFinishDerefs()
{
    ASSERT(isAudioThread() && isGraphOwner());
    for (auto& node : m_deferredFinishDerefList)
        node->finishDeref(AudioNode::RefTypeConnection);
    
    m_deferredFinishDerefList.clear();
}

void AudioContext::markForDeletion(AudioNode* node)
{
    ASSERT(isGraphOwner());

    if (isAudioThreadFinished())
        m_nodesToDelete.append(node);
    else
        m_nodesMarkedForDeletion.append(node);

    // This is probably the best time for us to remove the node from automatic pull list,
    // since all connections are gone and we hold the graph lock. Then when handlePostRenderTasks()
    // gets a chance to schedule the deletion work, updateAutomaticPullNodes() also gets a chance to
    // modify m_renderingAutomaticPullNodes.
    removeAutomaticPullNode(node);
}

void AudioContext::scheduleNodeDeletion()
{
    bool isGood = m_isInitialized && isGraphOwner();
    ASSERT(isGood);
    if (!isGood)
        return;

    // Make sure to call deleteMarkedNodes() on main thread.    
    if (m_nodesMarkedForDeletion.size() && !m_isDeletionScheduled) {
        m_nodesToDelete.appendVector(m_nodesMarkedForDeletion);
        m_nodesMarkedForDeletion.clear();

        m_isDeletionScheduled = true;

        RefPtr<AudioContext> strongThis(this);
        callOnMainThread([strongThis] {
            strongThis->deleteMarkedNodes();
        });
    }
}

void AudioContext::deleteMarkedNodes()
{
    ASSERT(isMainThread());

    // Protect this object from being deleted before we release the mutex locked by AutoLocker.
    Ref<AudioContext> protect(*this);
    {
        AutoLocker locker(*this);

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

            // Finally, delete it.
            delete node;
        }
        m_isDeletionScheduled = false;
    }
}

void AudioContext::markSummingJunctionDirty(AudioSummingJunction* summingJunction)
{
    ASSERT(isGraphOwner());    
    m_dirtySummingJunctions.add(summingJunction);
}

void AudioContext::removeMarkedSummingJunction(AudioSummingJunction* summingJunction)
{
    ASSERT(isMainThread());
    AutoLocker locker(*this);
    m_dirtySummingJunctions.remove(summingJunction);
}

void AudioContext::markAudioNodeOutputDirty(AudioNodeOutput* output)
{
    ASSERT(isGraphOwner());    
    m_dirtyAudioNodeOutputs.add(output);
}

void AudioContext::handleDirtyAudioSummingJunctions()
{
    ASSERT(isGraphOwner());    

    for (auto& junction : m_dirtySummingJunctions)
        junction->updateRenderingState();

    m_dirtySummingJunctions.clear();
}

void AudioContext::handleDirtyAudioNodeOutputs()
{
    ASSERT(isGraphOwner());    

    for (auto& output : m_dirtyAudioNodeOutputs)
        output->updateRenderingState();

    m_dirtyAudioNodeOutputs.clear();
}

void AudioContext::addAutomaticPullNode(AudioNode* node)
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodes.add(node).isNewEntry)
        m_automaticPullNodesNeedUpdating = true;
}

void AudioContext::removeAutomaticPullNode(AudioNode* node)
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodes.remove(node))
        m_automaticPullNodesNeedUpdating = true;
}

void AudioContext::updateAutomaticPullNodes()
{
    ASSERT(isGraphOwner());

    if (m_automaticPullNodesNeedUpdating) {
        // Copy from m_automaticPullNodes to m_renderingAutomaticPullNodes.
        m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());

        unsigned i = 0;
        for (auto& output : m_automaticPullNodes)
            m_renderingAutomaticPullNodes[i++] = output;

        m_automaticPullNodesNeedUpdating = false;
    }
}

void AudioContext::processAutomaticPullNodes(size_t framesToProcess)
{
    ASSERT(isAudioThread());

    for (auto& node : m_renderingAutomaticPullNodes)
        node->processIfNecessary(framesToProcess);
}

ScriptExecutionContext* AudioContext::scriptExecutionContext() const
{
    return m_isStopScheduled ? 0 : ActiveDOMObject::scriptExecutionContext();
}

void AudioContext::nodeWillBeginPlayback()
{
    // Called by scheduled AudioNodes when clients schedule their start times.
    // Prior to the introduction of suspend(), resume(), and stop(), starting
    // a scheduled AudioNode would remove the user-gesture restriction, if present,
    // and would thus unmute the context. Now that AudioContext stays in the
    // "suspended" state if a user-gesture restriction is present, starting a
    // schedule AudioNode should set the state to "running", but only if the
    // user-gesture restriction is set.
    if (userGestureRequiredForAudioStart())
        startRendering();
}

bool AudioContext::willBeginPlayback()
{
    if (userGestureRequiredForAudioStart()) {
        if (!ScriptController::processingUserGestureForMedia())
            return false;
        removeBehaviorRestriction(AudioContext::RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        Page* page = document()->page();
        if (page && !page->canStartMedia()) {
            document()->addMediaCanStartListener(this);
            return false;
        }
        removeBehaviorRestriction(AudioContext::RequirePageConsentForAudioStartRestriction);
    }

    return m_mediaSession->clientWillBeginPlayback();
}

bool AudioContext::willPausePlayback()
{
    if (userGestureRequiredForAudioStart()) {
        if (!ScriptController::processingUserGestureForMedia())
            return false;
        removeBehaviorRestriction(AudioContext::RequireUserGestureForAudioStartRestriction);
    }

    if (pageConsentRequiredForAudioStart()) {
        Page* page = document()->page();
        if (page && !page->canStartMedia()) {
            document()->addMediaCanStartListener(this);
            return false;
        }
        removeBehaviorRestriction(AudioContext::RequirePageConsentForAudioStartRestriction);
    }
    
    return m_mediaSession->clientWillPausePlayback();
}

void AudioContext::startRendering()
{
    if (!willBeginPlayback())
        return;

    destination()->startRendering();
    setState(State::Running);
}

void AudioContext::mediaCanStart()
{
    removeBehaviorRestriction(AudioContext::RequirePageConsentForAudioStartRestriction);
}

MediaProducer::MediaStateFlags AudioContext::mediaState() const
{
    if (!m_isStopScheduled && m_destinationNode && m_destinationNode->isPlayingAudio())
        return MediaProducer::IsPlayingAudio;

    return MediaProducer::IsNotPlaying;
}

void AudioContext::pageMutedStateDidChange()
{
    if (m_destinationNode && document()->page())
        m_destinationNode->setMuted(document()->page()->isMuted());
}

void AudioContext::isPlayingAudioDidChange()
{
    // Make sure to call Document::updateIsPlayingMedia() on the main thread, since
    // we could be on the audio I/O thread here and the call into WebCore could block.
    RefPtr<AudioContext> strongThis(this);
    callOnMainThread([strongThis] {
        if (strongThis->document())
            strongThis->document()->updateIsPlayingMedia();
    });
}

void AudioContext::fireCompletionEvent()
{
    ASSERT(isMainThread());
    if (!isMainThread())
        return;
        
    AudioBuffer* renderedBuffer = m_renderTarget.get();
    setState(State::Closed);

    ASSERT(renderedBuffer);
    if (!renderedBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (scriptExecutionContext()) {
        // Call the offline rendering completion event listener.
        m_eventQueue->enqueueEvent(OfflineAudioCompletionEvent::create(renderedBuffer));
    }
}

void AudioContext::incrementActiveSourceCount()
{
    ++m_activeSourceCount;
}

void AudioContext::decrementActiveSourceCount()
{
    --m_activeSourceCount;
}

void AudioContext::suspend(Promise&& promise)
{
    if (isOfflineContext()) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    if (m_state == State::Suspended) {
        promise.resolve(nullptr);
        return;
    }

    if (m_state == State::Closed || m_state == State::Interrupted || !m_destinationNode) {
        promise.reject(0);
        return;
    }

    addReaction(State::Suspended, WTFMove(promise));

    if (!willPausePlayback())
        return;

    lazyInitialize();

    RefPtr<AudioContext> strongThis(this);
    m_destinationNode->suspend([strongThis] {
        strongThis->setState(State::Suspended);
    });
}

void AudioContext::resume(Promise&& promise)
{
    if (isOfflineContext()) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    if (m_state == State::Running) {
        promise.resolve(nullptr);
        return;
    }

    if (m_state == State::Closed || !m_destinationNode) {
        promise.reject(0);
        return;
    }

    addReaction(State::Running, WTFMove(promise));

    if (!willBeginPlayback())
        return;

    lazyInitialize();

    RefPtr<AudioContext> strongThis(this);
    m_destinationNode->resume([strongThis] {
        strongThis->setState(State::Running);
    });
}

void AudioContext::close(Promise&& promise)
{
    if (isOfflineContext()) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    if (m_state == State::Closed || !m_destinationNode) {
        promise.resolve(nullptr);
        return;
    }

    addReaction(State::Closed, WTFMove(promise));

    lazyInitialize();

    RefPtr<AudioContext> strongThis(this);
    m_destinationNode->close([strongThis] {
        strongThis->setState(State::Closed);
        strongThis->uninitialize();
    });
}


void AudioContext::suspendPlayback()
{
    if (!m_destinationNode || m_state == State::Closed)
        return;

    if (m_state == State::Suspended) {
        if (m_mediaSession->state() == PlatformMediaSession::Interrupted)
            setState(State::Interrupted);
        return;
    }

    lazyInitialize();

    RefPtr<AudioContext> strongThis(this);
    m_destinationNode->suspend([strongThis] {
        bool interrupted = strongThis->m_mediaSession->state() == PlatformMediaSession::Interrupted;
        strongThis->setState(interrupted ? State::Interrupted : State::Suspended);
    });
}

void AudioContext::mayResumePlayback(bool shouldResume)
{
    if (!m_destinationNode || m_state == State::Closed || m_state == State::Running)
        return;

    if (!shouldResume) {
        setState(State::Suspended);
        return;
    }

    if (!willBeginPlayback())
        return;

    lazyInitialize();

    RefPtr<AudioContext> strongThis(this);
    m_destinationNode->resume([strongThis] {
        strongThis->setState(State::Running);
    });
}


} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
