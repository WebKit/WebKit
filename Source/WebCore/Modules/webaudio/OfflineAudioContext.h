/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "ExceptionCode.h"
#include "JSDOMPromiseDeferredForward.h"
#include "OfflineAudioDestinationNode.h"
#include <optional>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Ref.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

struct OfflineAudioContextOptions;

class AudioBuffer;

class OfflineAudioContext final : public BaseAudioContext {
    WTF_MAKE_TZONE_ALLOCATED(OfflineAudioContext);
public:
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, const OfflineAudioContextOptions&);
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, unsigned numberOfChannels, unsigned length, float sampleRate);
    ~OfflineAudioContext();

    void startRendering(std::optional<unsigned> chunkSize, Ref<DeferredPromise>&&);
    void close(Ref<DeferredPromise>&&);
    void suspendRendering(double suspendTime, Ref<DeferredPromise>&&);
    void resumeRendering(Ref<DeferredPromise>&&);

    void didRenderChunk();
    void didFailRender();
    void didStopRender();
    void didSuspendRendering(size_t frame);

    bool closeRequested() const { return m_closeRequested; }
    void setIsRendering(bool isRendering) { m_isRendering = isRendering; }
    unsigned length() const { return m_length; }
    bool shouldSuspend();

    OfflineAudioDestinationNode& destination() final { return m_destinationNode.get(); }
    const OfflineAudioDestinationNode& destination() const final { return m_destinationNode.get(); }

private:
    struct PendingRender {
        Ref<DeferredPromise> promise;
        Ref<AudioBuffer> buffer;
    };

    OfflineAudioContext(Document&, const OfflineAudioContextOptions&);

    std::optional<size_t> renderLengthForChunk(std::optional<unsigned> chunkSize) const;
    void stampNoise(AudioBuffer&);
    void renderFront();
    void completeRendering(Ref<AudioBuffer>&&);
    void finishClose();
    void rejectPendingRenders(ExceptionCode, const String&);
    void rejectPendingSuspends();

    // ActiveDOMObject
    NODELETE bool virtualHasPendingActivity() const final;

    void uninitialize() final;
    bool isOfflineContext() const final { return true; }

    const UniqueRef<OfflineAudioDestinationNode> m_destinationNode;
    Deque<PendingRender, 0> m_pendingRenders;
    HashMap<unsigned /* frame */, Ref<DeferredPromise>> m_suspendRequests;
    RefPtr<DeferredPromise> m_closePromise;
    unsigned m_length;
    size_t m_committedFrames { 0 };
    bool m_didStartRendering { false };
    bool m_isRendering { false };
    bool m_chunkInProgress { false };
    bool m_closeRequested { false };
    bool m_didFinishClose { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::OfflineAudioContext)
    static bool isType(const WebCore::BaseAudioContext& context) { return context.isOfflineContext(); }
SPECIALIZE_TYPE_TRAITS_END()
