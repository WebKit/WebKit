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
#include "AudioWorklet.h"

#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletMessagingProxy.h"
#include "AudioWorkletNode.h"
#include "AudioWorkletProcessor.h"
#include "BaseAudioContext.h"
#include "WorkerRunLoop.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorklet);

Ref<AudioWorklet> AudioWorklet::create(BaseAudioContext& audioContext)
{
    auto worklet = adoptRef(*new AudioWorklet(audioContext));
    worklet->suspendIfNeeded();
    return worklet;
}

AudioWorklet::AudioWorklet(BaseAudioContext& audioContext)
    : Worklet(*audioContext.document())
    , m_audioContext(audioContext)
{
}

Vector<Ref<WorkletGlobalScopeProxy>> AudioWorklet::createGlobalScopes()
{
    // WebAudio uses a single global scope.
    return { AudioWorkletMessagingProxy::create(*this) };
}

AudioWorkletMessagingProxy* AudioWorklet::proxy() const
{
    auto& proxies = this->proxies();
    if (proxies.isEmpty())
        return nullptr;
    return downcast<AudioWorkletMessagingProxy>(proxies.first().ptr());
}

BaseAudioContext* AudioWorklet::audioContext() const
{
    return m_audioContext.get();
}

void AudioWorklet::createProcessor(const String& name, TransferredMessagePort port, Ref<SerializedScriptValue>&& options, AudioWorkletNode& node)
{
    RefPtr proxy = this->proxy();
    ASSERT(proxy);
    if (!proxy)
        return;

    proxy->postTaskForModeToWorkletGlobalScope([name = name.isolatedCopy(), port, options = WTFMove(options), node = Ref { node }](ScriptExecutionContext& context) mutable {
        node->setProcessor(downcast<AudioWorkletGlobalScope>(context).createProcessor(name, port, WTFMove(options)));
        callOnMainThread([node = WTFMove(node)] { });
    }, WorkerRunLoop::defaultMode());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
