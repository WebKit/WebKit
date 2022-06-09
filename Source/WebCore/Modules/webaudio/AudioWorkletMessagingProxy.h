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

#pragma once

#if ENABLE(WEB_AUDIO)
#include "WorkerLoaderProxy.h"
#include "WorkletGlobalScopeProxy.h"

namespace WebCore {

class AudioWorklet;
class AudioWorkletThread;
class Document;

class AudioWorkletMessagingProxy : public WorkletGlobalScopeProxy, public WorkerLoaderProxy {
public:
    static Ref<AudioWorkletMessagingProxy> create(AudioWorklet& worklet)
    {
        return adoptRef(*new AudioWorkletMessagingProxy(worklet));
    }

    ~AudioWorkletMessagingProxy();

    // This method is used in the main thread to post task back to the worklet thread.
    bool postTaskForModeToWorkletGlobalScope(ScriptExecutionContext::Task&&, const String& mode) final;

    AudioWorkletThread& workletThread() { return m_workletThread.get(); }

    void postTaskToAudioWorklet(Function<void(AudioWorklet&)>&&);

private:
    explicit AudioWorkletMessagingProxy(AudioWorklet&);

    // WorkerLoaderProxy.
    RefPtr<CacheStorageConnection> createCacheStorageConnection() final;
    RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;
    void postTaskToLoader(ScriptExecutionContext::Task&&) final;
    bool postTaskForModeToWorkerOrWorkletGlobalScope(ScriptExecutionContext::Task&&, const String& mode) final;

    bool isAudioWorkletMessagingProxy() const final { return true; }

    WeakPtr<AudioWorklet> m_worklet;
    Ref<Document> m_document;
    Ref<AudioWorkletThread> m_workletThread;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioWorkletMessagingProxy)
static bool isType(const WebCore::WorkletGlobalScopeProxy& proxy) { return proxy.isAudioWorkletMessagingProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_AUDIO)
