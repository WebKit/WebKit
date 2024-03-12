/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#pragma once

#include "BaseAudioContext.h"
#include "JSDOMPromiseDeferredForward.h"
#include "OfflineAudioDestinationNode.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

struct OfflineAudioContextOptions;

class OfflineAudioContext final : public BaseAudioContext {
    WTF_MAKE_ISO_ALLOCATED(OfflineAudioContext);
public:
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, const OfflineAudioContextOptions&);
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, unsigned numberOfChannels, unsigned length, float sampleRate);
    void startRendering(Ref<DeferredPromise>&&);
    void suspendRendering(double suspendTime, Ref<DeferredPromise>&&);
    void resumeRendering(Ref<DeferredPromise>&&);
    void finishedRendering(bool didRendering);
    void didSuspendRendering(size_t frame);

    unsigned length() const { return m_length; }
    bool shouldSuspend();

    OfflineAudioDestinationNode& destination() final { return m_destinationNode.get(); }
    const OfflineAudioDestinationNode& destination() const final { return m_destinationNode.get(); }

private:
    OfflineAudioContext(Document&, const OfflineAudioContextOptions&);

    void lazyInitialize() final;
    void increaseNoiseMultiplierIfNeeded();

    AudioBuffer* renderTarget() const { return destination().renderTarget(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void settleRenderingPromise(ExceptionOr<Ref<AudioBuffer>>&&);
    void uninitialize() final;
    bool isOfflineContext() const final { return true; }

    UniqueRef<OfflineAudioDestinationNode> m_destinationNode;
    RefPtr<DeferredPromise> m_pendingRenderingPromise;
    HashMap<unsigned /* frame */, RefPtr<DeferredPromise>, IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_suspendRequests;
    unsigned m_length;
    bool m_didStartRendering { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::OfflineAudioContext)
    static bool isType(const WebCore::BaseAudioContext& context) { return context.isOfflineContext(); }
SPECIALIZE_TYPE_TRAITS_END()
