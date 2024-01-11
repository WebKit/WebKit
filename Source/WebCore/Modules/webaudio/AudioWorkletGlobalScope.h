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
#include "AudioWorkletThread.h"
#include "MessagePort.h"
#include "WorkletGlobalScope.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/ThreadSafeWeakHashSet.h>

namespace JSC {
class VM;
}

namespace WebCore {

class AudioWorkletProcessorConstructionData;
class AudioWorkletProcessor;
class AudioWorkletThread;
class JSAudioWorkletProcessorConstructor;

struct WorkletParameters;

class AudioWorkletGlobalScope final : public WorkletGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(AudioWorkletGlobalScope);
public:
    static RefPtr<AudioWorkletGlobalScope> tryCreate(AudioWorkletThread&, const WorkletParameters&);
    ~AudioWorkletGlobalScope();

    ExceptionOr<void> registerProcessor(String&& name, Ref<JSAudioWorkletProcessorConstructor>&&);
    RefPtr<AudioWorkletProcessor> createProcessor(const String& name, TransferredMessagePort, Ref<SerializedScriptValue>&& options);
    void processorIsNoLongerNeeded(AudioWorkletProcessor&);
    void visitProcessors(JSC::AbstractSlotVisitor&);

    size_t currentFrame() const { return m_currentFrame; }

    float sampleRate() const { return m_sampleRate; }

    double currentTime() const { return m_sampleRate > 0.0 ? m_currentFrame / static_cast<double>(m_sampleRate) : 0.0; }

    AudioWorkletThread& thread() const;
    void prepareForDestruction() final;

    std::unique_ptr<AudioWorkletProcessorConstructionData> takePendingProcessorConstructionData();

    void handlePreRenderTasks();
    void handlePostRenderTasks(size_t currentFrame);

    FetchOptions::Destination destination() const final { return FetchOptions::Destination::Audioworklet; }

private:
    AudioWorkletGlobalScope(AudioWorkletThread&, Ref<JSC::VM>&&, const WorkletParameters&);

    bool isAudioWorkletGlobalScope() const final { return true; }

    size_t m_currentFrame { 0 };
    const float m_sampleRate;
    MemoryCompactRobinHoodHashMap<String, RefPtr<JSAudioWorkletProcessorConstructor>> m_processorConstructorMap;
    ThreadSafeWeakHashSet<AudioWorkletProcessor> m_processors;
    std::unique_ptr<AudioWorkletProcessorConstructionData> m_pendingProcessorConstructionData;
    std::optional<JSC::VM::DrainMicrotaskDelayScope> m_delayMicrotaskDrainingDuringRendering;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioWorkletGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context)
{
    auto* workletGlobalScope = dynamicDowncast<WebCore::WorkletGlobalScope>(context);
    return workletGlobalScope && workletGlobalScope->isAudioWorkletGlobalScope();
}
static bool isType(const WebCore::WorkletGlobalScope& context) { return context.isAudioWorkletGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_AUDIO)
