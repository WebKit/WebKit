/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WakeLockType.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class DeferredPromise;
class WakeLockManager;

class WakeLockSentinel final : public RefCounted<WakeLockSentinel>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(WakeLockSentinel);
public:
    static Ref<WakeLockSentinel> create(Document& document, WakeLockType type)
    {
        auto sentinel = adoptRef(*new WakeLockSentinel(document, type));
        sentinel->suspendIfNeeded();
        return sentinel;
    }

    using RefCounted::ref;
    using RefCounted::deref;

    bool released() const { return m_wasReleased; }
    WakeLockType type() const { return m_type; }
    void release(Ref<DeferredPromise>&&);
    void release(WakeLockManager&);

private:
    WakeLockSentinel(Document&, WakeLockType);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return WakeLockSentinelEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { RefCounted::ref(); }
    void derefEventTarget() final { RefCounted::deref(); }
    void eventListenersDidChange() final;

    WakeLockType m_type;
    bool m_wasReleased { false };
    bool m_hasReleaseEventListener { false };
};

} // namespace WebCore
