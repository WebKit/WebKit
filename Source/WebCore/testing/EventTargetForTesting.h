/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "MessageClientForTesting.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class MessageTargetForTesting;

class EventTargetForTesting final
    : public ActiveDOMObject
    , public EventTarget
    , public RefCounted<EventTargetForTesting>
    , private MessageClientForTesting {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(EventTargetForTesting);
public:
    static Ref<EventTargetForTesting> create(ScriptExecutionContext&, MessageTargetForTesting&);
    virtual ~EventTargetForTesting();

    // MessageClientForTesting, ActiveDOMObject
    void ref() const final { return RefCounted::ref(); }
    void deref() const final { return RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

private:
    EventTargetForTesting(ScriptExecutionContext&, MessageTargetForTesting&);

    // ActiveDOMObject.
    void stop() final { }
    void suspend(ReasonForSuspension) final { }
    bool virtualHasPendingActivity() const final { return !!m_messageTarget; }

    // EventTarget
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::EventTarget; }
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { return RefCounted::ref(); }
    void derefEventTarget() final { return RefCounted::deref(); }

    // MessageClientForTesting
    void sendInternalMessage(const MessageForTesting&) final;

    WeakPtr<MessageTargetForTesting> m_messageTarget;
};

}
