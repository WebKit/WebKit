/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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
#include "JSDOMPromiseDeferred.h"
#include "OfflineAudioContextOptions.h"
#include "OfflineAudioDestinationNode.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>

namespace WebCore {

class OfflineAudioContext final : public BaseAudioContext {
    WTF_MAKE_ISO_ALLOCATED(OfflineAudioContext);
public:
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, unsigned numberOfChannels, size_t length, float sampleRate);
    
    static ExceptionOr<Ref<OfflineAudioContext>> create(ScriptExecutionContext&, const OfflineAudioContextOptions&);

    void startOfflineRendering(Ref<DeferredPromise>&&);
    void suspendOfflineRendering(double suspendTime, Ref<DeferredPromise>&&);
    void resumeOfflineRendering(Ref<DeferredPromise>&&);

    unsigned length() const;
    bool shouldSuspend() final;

    OfflineAudioDestinationNode* destination() { return static_cast<OfflineAudioDestinationNode*>(BaseAudioContext::destination()); }

    // mustReleaseLock is set to true if we acquired the lock in this method call and caller must unlock(), false if it was previously acquired.
    void offlineLock(bool& mustReleaseLock);

    class OfflineAutoLocker {
    public:
        explicit OfflineAutoLocker(OfflineAudioContext& context)
            : m_context(context)
        {
            m_context.offlineLock(m_mustReleaseLock);
        }

        ~OfflineAutoLocker()
        {
            if (m_mustReleaseLock)
                m_context.unlock();
        }

    private:
        OfflineAudioContext& m_context;
        bool m_mustReleaseLock;
    };

private:
    OfflineAudioContext(Document&, unsigned numberOfChannels, RefPtr<AudioBuffer>&& renderTarget);

    void didFinishOfflineRendering(ExceptionOr<Ref<AudioBuffer>>&&) final;
    void didSuspendRendering(size_t frame) final;
    void uninitialize() final;

    RefPtr<DeferredPromise> m_pendingOfflineRenderingPromise;
    HashMap<unsigned /* frame */, RefPtr<DeferredPromise>, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_suspendRequests;
    bool m_didStartOfflineRendering { false };
};

} // namespace WebCore
