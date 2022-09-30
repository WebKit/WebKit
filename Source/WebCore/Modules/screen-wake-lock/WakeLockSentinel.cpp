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

#include "config.h"
#include "WakeLockSentinel.h"

#include "EventNames.h"
#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "WakeLockManager.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WakeLockSentinel);

WakeLockSentinel::WakeLockSentinel(Document& document, WakeLockType type)
    : ActiveDOMObject(&document)
    , m_type(type)
{
}

void WakeLockSentinel::release(Ref<DeferredPromise>&& promise)
{
    if (!m_wasReleased) {
        if (auto* document = downcast<Document>(scriptExecutionContext()))
            Ref { *this }->release(document->wakeLockManager());
    }
    promise->resolve();
}

// https://www.w3.org/TR/screen-wake-lock/#dfn-release-a-wake-lock
void WakeLockSentinel::release(WakeLockManager& manager)
{
    manager.removeWakeLock(*this);

    m_wasReleased = true;

    if (scriptExecutionContext() && !scriptExecutionContext()->activeDOMObjectsAreStopped())
        dispatchEvent(Event::create(eventNames().releaseEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

const char* WakeLockSentinel::activeDOMObjectName() const
{
    return "WakeLockSentinel";
}

// https://www.w3.org/TR/screen-wake-lock/#garbage-collection
bool WakeLockSentinel::virtualHasPendingActivity() const
{
    return m_hasReleaseEventListener && !m_wasReleased;
}

void WakeLockSentinel::eventListenersDidChange()
{
    m_hasReleaseEventListener = hasEventListeners(eventNames().releaseEvent);
}

} // namespace WebCore
