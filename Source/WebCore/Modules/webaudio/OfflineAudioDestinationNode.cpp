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

#if ENABLE(WEB_AUDIO)

#include "OfflineAudioDestinationNode.h"

#include "AudioBus.h"
#include "AudioContext.h"
#include "HRTFDatabaseLoader.h"
#include <algorithm>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
 
namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(OfflineAudioDestinationNode);
    
const size_t OfflineAudioDestinationNode::renderQuantumSize = 128;

OfflineAudioDestinationNode::OfflineAudioDestinationNode(BaseAudioContext& context, unsigned numberOfChannels, RefPtr<AudioBuffer>&& renderTarget)
    : AudioDestinationNode(context)
    , m_numberOfChannels(numberOfChannels)
    , m_renderTarget(WTFMove(renderTarget))
    , m_framesToProcess(m_renderTarget ? m_renderTarget->length() : 0)
{
    m_renderBus = AudioBus::create(numberOfChannels, renderQuantumSize);
    initializeDefaultNodeOptions(numberOfChannels, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
}

OfflineAudioDestinationNode::~OfflineAudioDestinationNode()
{
    uninitialize();
}

unsigned OfflineAudioDestinationNode::maxChannelCount() const
{
    return m_numberOfChannels;
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

    if (m_renderThread) {
        m_renderThread->waitForCompletion();
        m_renderThread = nullptr;
    }

    AudioNode::uninitialize();
}

ExceptionOr<void> OfflineAudioDestinationNode::startRendering()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    ASSERT(m_renderTarget.get());
    if (!m_renderTarget.get())
        return Exception { InvalidStateError };
    
    if (m_startedRendering)
        return Exception { InvalidStateError, "Already started rendering"_s };

    m_startedRendering = true;
    auto protectedThis = makeRef(*this);

    // FIXME: Should we call lazyInitialize here?
    // FIXME: We should probably limit the number of threads we create for offline audio.
    m_renderThread = Thread::create("offline renderer", [this, protectedThis = WTFMove(protectedThis)]() mutable {
        auto result = offlineRender();
        callOnMainThread([this, result, currentSampleFrame = m_currentSampleFrame, protectedThis = WTFMove(protectedThis)] {
            m_startedRendering = false;
            switch (result) {
            case OfflineRenderResult::Failure:
                context().finishedRendering(false);
                break;
            case OfflineRenderResult::Complete:
                context().finishedRendering(true);
                break;
            case OfflineRenderResult::Suspended:
                context().didSuspendRendering(currentSampleFrame);
                break;
            }
        });
    }, ThreadType::Audio);
    return { };
}

auto OfflineAudioDestinationNode::offlineRender() -> OfflineRenderResult
{
    ASSERT(!isMainThread());
    ASSERT(m_renderBus.get());

    if (!m_renderBus.get())
        return OfflineRenderResult::Failure;

    RELEASE_ASSERT(context().isInitialized());

    bool channelsMatch = m_renderBus->numberOfChannels() == m_renderTarget->numberOfChannels();
    ASSERT(channelsMatch);
    if (!channelsMatch)
        return OfflineRenderResult::Failure;

    bool isRenderBusAllocated = m_renderBus->length() >= renderQuantumSize;
    ASSERT(isRenderBusAllocated);
    if (!isRenderBusAllocated)
        return OfflineRenderResult::Failure;

    // Break up the render target into smaller "render quantize" sized pieces.
    // Render until we're finished.
    unsigned numberOfChannels = m_renderTarget->numberOfChannels();

    while (m_framesToProcess > 0) {
        if (context().shouldSuspend())
            return OfflineRenderResult::Suspended;

        // Render one render quantum.
        render(0, m_renderBus.get(), renderQuantumSize, { });
        
        size_t framesAvailableToCopy = std::min(m_framesToProcess, renderQuantumSize);
        
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            const float* source = m_renderBus->channel(channelIndex)->data();
            float* destination = m_renderTarget->channelData(channelIndex)->data();
            memcpy(destination + m_destinationOffset, source, sizeof(float) * framesAvailableToCopy);
        }
        
        m_destinationOffset += framesAvailableToCopy;
        m_framesToProcess -= framesAvailableToCopy;
    }

    return OfflineRenderResult::Complete;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
