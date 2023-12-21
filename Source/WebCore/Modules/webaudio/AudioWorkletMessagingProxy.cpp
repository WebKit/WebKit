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

#include "config.h"
#if ENABLE(WEB_AUDIO)
#include "AudioWorkletMessagingProxy.h"

#include "AudioWorklet.h"
#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletThread.h"
#include "BaseAudioContext.h"
#include "CacheStorageConnection.h"
#include "Document.h"
#include "LocalFrame.h"
#include "Page.h"
#include "Settings.h"
#include "WebRTCProvider.h"
#include "WorkletParameters.h"
#include "WorkletPendingTasks.h"

namespace WebCore {

static WorkletParameters generateWorkletParameters(AudioWorklet& worklet)
{
    RefPtr document = worklet.document();
    auto jsRuntimeFlags = document->settings().javaScriptRuntimeFlags();
    RELEASE_ASSERT(document->sessionID());

    return {
        document->url(),
        jsRuntimeFlags,
        worklet.audioContext() ? worklet.audioContext()->sampleRate() : 0.0f,
        worklet.identifier(),
        *document->sessionID(),
        document->settingsValues(),
        document->referrerPolicy(),
        worklet.audioContext() ? !worklet.audioContext()->isOfflineContext() : false,
        document->noiseInjectionHashSalt()
    };
}

AudioWorkletMessagingProxy::AudioWorkletMessagingProxy(AudioWorklet& worklet)
    : m_worklet(worklet)
    , m_document(*worklet.document())
    , m_workletThread(AudioWorkletThread::create(*this, generateWorkletParameters(worklet)))
{
    ASSERT(isMainThread());

    m_workletThread->start();
}

AudioWorkletMessagingProxy::~AudioWorkletMessagingProxy()
{
    m_workletThread->stop();
    m_workletThread->clearProxies();
}

bool AudioWorkletMessagingProxy::postTaskForModeToWorkletGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    m_workletThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

RefPtr<CacheStorageConnection> AudioWorkletMessagingProxy::createCacheStorageConnection()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<RTCDataChannelRemoteHandlerConnection> AudioWorkletMessagingProxy::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    if (!m_document->page())
        return nullptr;
    return m_document->page()->webRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

ScriptExecutionContextIdentifier AudioWorkletMessagingProxy::loaderContextIdentifier() const
{
    return m_document->identifier();
}

void AudioWorkletMessagingProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    m_document->postTask(WTFMove(task));
}

void AudioWorkletMessagingProxy::postTaskToAudioWorklet(Function<void(AudioWorklet&)>&& task)
{
    m_document->postTask([this, protectedThis = Ref { *this }, task = WTFMove(task)](ScriptExecutionContext&) {
        if (m_worklet)
            task(*m_worklet);
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
