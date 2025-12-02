/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AbortSignal.h>
#include <WebCore/ActiveDOMObject.h>
#include <WebCore/EventTarget.h>
#include <WebCore/EventTargetInterfaces.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class KeyboardEvent;

class CloseWatcher final : public RefCounted<CloseWatcher>, public EventTarget, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED(CloseWatcher);
public:
    struct Options {
        RefPtr<AbortSignal> signal;
    };

    static ExceptionOr<Ref<CloseWatcher>> create(ScriptExecutionContext&, const Options&);

    explicit CloseWatcher(Document&);

    bool isActive() const { return m_active; }

    void requestClose();
    bool requestToClose();
    void close();
    void destroy();

    ScriptExecutionContext* scriptExecutionContext() const final;

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

private:
    static Ref<CloseWatcher> establish(Document&);

    void stop() final { destroy(); }
    bool virtualHasPendingActivity() const final;

    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::CloseWatcher; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    void eventListenersDidChange() final;

    bool canBeClosed() const;

    bool m_active { true };
    bool m_isRunningCancelAction { false };
    bool m_hasCancelEventListener { false };
    bool m_hasCloseEventListener { false };
    RefPtr<AbortSignal> m_signal;
    uint32_t m_signalAlgorithm { };
};

} // namespace WebCore
