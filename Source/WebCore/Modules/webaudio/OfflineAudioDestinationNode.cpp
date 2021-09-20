/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2020-2021, Apple Inc. All rights reserved.
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

#include "OfflineAudioDestinationNode.h"

#include "AudioBuffer.h"
#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioUtilities.h"
#include "AudioWorklet.h"
#include "AudioWorkletMessagingProxy.h"
#include "HRTFDatabaseLoader.h"
#include "OfflineAudioContext.h"
#include "WorkerRunLoop.h"
#include <algorithm>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/threads/BinarySemaphore.h>
 
namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioDestinationNode);

OfflineAudioDestinationNode::OfflineAudioDestinationNode(OfflineAudioContext& context, unsigned numberOfChannels, float sampleRate, RefPtr<AudioBuffer>&& renderTarget)
    : AudioDestinationNode(context, sampleRate)
    , m_numberOfChannels(numberOfChannels)
    , m_renderTarget(WTFMove(renderTarget))
    , m_renderBus(AudioBus::create(numberOfChannels, AudioUtilities::renderQuantumSize))
    , m_framesToProcess(m_renderTarget ? m_renderTarget->length() : 0)
{
    initializeDefaultNodeOptions(numberOfChannels, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
}

OfflineAudioDestinationNode::~OfflineAudioDestinationNode()
{
    uninitialize();
}

OfflineAudioContext& OfflineAudioDestinationNode::context()
{
    return downcast<OfflineAudioContext>(AudioDestinationNode::context());
}

const OfflineAudioContext& OfflineAudioDestinationNode::context() const
{
    return downcast<OfflineAudioContext>(AudioDestinationNode::context());
}

void OfflineAudioDestinationNode::initialize()
{
    if (isInitialized())
        return;

    AudioNode::initialize();
}

void OfflineAudioDestinationNode::uninitialize()
{
    if (!isInitialized())
        return;

    if (m_startedRendering) {
        if (m_renderThread) {
            m_renderThread->waitForCompletion();
            m_renderThread = nullptr;
        }
        if (auto* workletProxy = context().audioWorklet().proxy()) {
            BinarySemaphore semaphore;
            workletProxy->postTaskForModeToWorkletGlobalScope([&semaphore](ScriptExecutionContext&) mutable {
                semaphore.signal();
            }, WorkerRunLoop::defaultMode());
            semaphore.wait();
        }
    }

    AudioNode::uninitialize();
}

void OfflineAudioDestinationNode::startRendering(CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    ASSERT(m_renderTarget.get());
    if (!m_renderTarget.get())
        return completionHandler(Exception { InvalidStateError, "OfflineAudioContextNode has no rendering buffer"_s });
    
    if (m_startedRendering)
        return completionHandler(Exception { InvalidStateError, "Already started rendering"_s });

    m_startedRendering = true;
    Ref protectedThis { *this };

    auto offThreadRendering = [this, protectedThis = WTFMove(protectedThis)]() mutable {
        auto result = renderOnAudioThread();
        callOnMainThread([this, result, currentSampleFrame = this->currentSampleFrame(), protectedThis = WTFMove(protectedThis)]() mutable {
            context().postTask([this, protectedThis = WTFMove(protectedThis), result, currentSampleFrame]() mutable {
                m_startedRendering = false;
                switch (result) {
                case RenderResult::Failure:
                    context().finishedRendering(false);
                    break;
                case RenderResult::Complete:
                    context().finishedRendering(true);
                    break;
                case RenderResult::Suspended:
                    context().didSuspendRendering(currentSampleFrame);
                    break;
                }
            });
        });
    };

    if (auto* workletProxy = context().audioWorklet().proxy()) {
        workletProxy->postTaskForModeToWorkletGlobalScope([offThreadRendering = WTFMove(offThreadRendering)](ScriptExecutionContext&) mutable {
            offThreadRendering();
        }, WorkerRunLoop::defaultMode());
        return completionHandler(std::nullopt);
    }

    // FIXME: We should probably limit the number of threads we create for offline audio.
    m_renderThread = Thread::create("offline renderer", WTFMove(offThreadRendering), ThreadType::Audio, Thread::QOS::Default);
    completionHandler(std::nullopt);
}

auto OfflineAudioDestinationNode::renderOnAudioThread() -> RenderResult
{
    ASSERT(!isMainThread());
    ASSERT(m_renderBus.get());

    if (!m_renderBus.get())
        return RenderResult::Failure;

    RELEASE_ASSERT(context().isInitialized());

    bool channelsMatch = m_renderBus->numberOfChannels() == m_renderTarget->numberOfChannels();
    ASSERT(channelsMatch);
    if (!channelsMatch)
        return RenderResult::Failure;

    bool isRenderBusAllocated = m_renderBus->length() >= AudioUtilities::renderQuantumSize;
    ASSERT(isRenderBusAllocated);
    if (!isRenderBusAllocated)
        return RenderResult::Failure;

    // Break up the render target into smaller "render quantize" sized pieces.
    // Render until we're finished.
    unsigned numberOfChannels = m_renderTarget->numberOfChannels();

    while (m_framesToProcess > 0) {
        if (context().shouldSuspend())
            return RenderResult::Suspended;

        // Render one render quantum.
        renderQuantum(m_renderBus.get(), AudioUtilities::renderQuantumSize, { });
        
        size_t framesAvailableToCopy = std::min(m_framesToProcess, AudioUtilities::renderQuantumSize);
        
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            const float* source = m_renderBus->channel(channelIndex)->data();
            float* destination = m_renderTarget->channelData(channelIndex)->data();
            memcpy(destination + m_destinationOffset, source, sizeof(float) * framesAvailableToCopy);
        }
        
        m_destinationOffset += framesAvailableToCopy;
        m_framesToProcess -= framesAvailableToCopy;
    }

    return RenderResult::Complete;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
