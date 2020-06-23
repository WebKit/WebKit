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
    
const size_t renderQuantumSize = 128;    

OfflineAudioDestinationNode::OfflineAudioDestinationNode(BaseAudioContext& context, AudioBuffer* renderTarget)
    : AudioDestinationNode(context, renderTarget->sampleRate())
    , m_renderTarget(renderTarget)
    , m_startedRendering(false)
{
    m_renderBus = AudioBus::create(renderTarget->numberOfChannels(), renderQuantumSize);
}

OfflineAudioDestinationNode::~OfflineAudioDestinationNode()
{
    uninitialize();
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

void OfflineAudioDestinationNode::startRendering()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    ASSERT(m_renderTarget.get());
    if (!m_renderTarget.get())
        return;
    
    if (m_startedRendering)
        return;

    m_startedRendering = true;
    ref();
    // FIXME: Should we call lazyInitialize here?
    // FIXME: We should probably limit the number of threads we create for offline audio.
    m_renderThread = Thread::create("offline renderer", [this] {
        bool didRender = offlineRender();
        callOnMainThread([this, didRender] {
            context().finishedRendering(didRender);
            deref();
        });
    }, ThreadType::Audio);
}

bool OfflineAudioDestinationNode::offlineRender()
{
    ASSERT(!isMainThread());
    ASSERT(m_renderBus.get());
    if (!m_renderBus.get())
        return false;

    bool isAudioContextInitialized = context().isInitialized();
    // FIXME: We used to assert that isAudioContextInitialized is true here.
    // But it's trivially false in imported/w3c/web-platform-tests/webaudio/the-audio-api/the-offlineaudiocontext-interface/current-time-block-size.html 
    if (!isAudioContextInitialized)
        return false;

    bool channelsMatch = m_renderBus->numberOfChannels() == m_renderTarget->numberOfChannels();
    ASSERT(channelsMatch);
    if (!channelsMatch)
        return false;

    bool isRenderBusAllocated = m_renderBus->length() >= renderQuantumSize;
    ASSERT(isRenderBusAllocated);
    if (!isRenderBusAllocated)
        return false;

    // Break up the render target into smaller "render quantize" sized pieces.
    // Render until we're finished.
    size_t framesToProcess = m_renderTarget->length();
    unsigned numberOfChannels = m_renderTarget->numberOfChannels();

    unsigned n = 0;
    while (framesToProcess > 0) {
        // Render one render quantum.
        render(0, m_renderBus.get(), renderQuantumSize);
        
        size_t framesAvailableToCopy = std::min(framesToProcess, renderQuantumSize);
        
        for (unsigned channelIndex = 0; channelIndex < numberOfChannels; ++channelIndex) {
            const float* source = m_renderBus->channel(channelIndex)->data();
            float* destination = m_renderTarget->channelData(channelIndex)->data();
            memcpy(destination + n, source, sizeof(float) * framesAvailableToCopy);
        }
        
        n += framesAvailableToCopy;
        framesToProcess -= framesAvailableToCopy;
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
