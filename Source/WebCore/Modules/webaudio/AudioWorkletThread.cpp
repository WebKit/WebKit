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
#include "AudioWorkletThread.h"

#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletMessagingProxy.h"
#include <wtf/Threading.h>

namespace WebCore {

AudioWorkletThread::AudioWorkletThread(AudioWorkletMessagingProxy& messagingProxy, const WorkletParameters& parameters)
    : WorkerOrWorkletThread(parameters.identifier.isolatedCopy())
    , m_messagingProxy(messagingProxy)
    , m_parameters(parameters.isolatedCopy())
{
}

AudioWorkletThread::~AudioWorkletThread() = default;

RefPtr<WorkerOrWorkletGlobalScope> AudioWorkletThread::createGlobalScope()
{
    return AudioWorkletGlobalScope::tryCreate(*this, m_parameters);
}

WorkerLoaderProxy& AudioWorkletThread::workerLoaderProxy()
{
    return m_messagingProxy;
}

WorkerDebuggerProxy* AudioWorkletThread::workerDebuggerProxy() const
{
    // FIXME: Add debugging support for AudioWorklets.
    return nullptr;
}

Ref<Thread> AudioWorkletThread::createThread()
{
    return Thread::create("WebCore: AudioWorklet", [this] {
        workerOrWorkletThread();
    }, ThreadType::Audio, m_parameters.isAudioContextRealTime ? Thread::QOS::UserInteractive : Thread::QOS::Default);
}

AudioWorkletGlobalScope* AudioWorkletThread::globalScope() const
{
    return downcast<AudioWorkletGlobalScope>(WorkerOrWorkletThread::globalScope());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
