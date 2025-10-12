/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/Event.h>
#include <WebCore/ExtendableEventInit.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DOMPromise;
template<typename> class ExceptionOr;

class ExtendableEvent : public Event {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ExtendableEvent);
public:
    static Ref<ExtendableEvent> create(const AtomString& type, const ExtendableEventInit& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new ExtendableEvent(EventInterfaceType::ExtendableEvent, type, initializer, isTrusted));
    }

    ~ExtendableEvent();

    ExceptionOr<void> waitUntil(Ref<DOMPromise>&&);
    unsigned pendingPromiseCount() const { return m_pendingPromiseCount; }

    WEBCORE_EXPORT void whenAllExtendLifetimePromisesAreSettled(Function<void(HashSet<Ref<DOMPromise>>&&)>&&);

    virtual bool isBackgroundFetchEvent() const { return false; }
    virtual bool isBackgroundFetchUpdateUIEvent() const { return false; }
    virtual bool isExtendableCookieChangeEvent() const { return false; }
    virtual bool isExtendableMessageEvent() const { return false; }
    virtual bool isFetchEvent() const { return false; }
    virtual bool isInstallEvent() const { return false; }
    virtual bool isNotificationEvent() const { return false; }
    virtual bool isPushEvent() const { return false; }
    virtual bool isPushSubscriptionChangeEvent() const { return false; }

protected:
    WEBCORE_EXPORT ExtendableEvent(enum EventInterfaceType, const AtomString&, const ExtendableEventInit&, IsTrusted);
    ExtendableEvent(enum EventInterfaceType, const AtomString&, CanBubble, IsCancelable);

    void addExtendLifetimePromise(Ref<DOMPromise>&&);
    bool isWaiting() const { return m_isWaiting; }

private:
    unsigned m_pendingPromiseCount { 0 };
    HashSet<Ref<DOMPromise>> m_extendLifetimePromises;
    bool m_isWaiting { true };
    Function<void(HashSet<Ref<DOMPromise>>&&)> m_whenAllExtendLifetimePromisesAreSettledHandler;
};

} // namespace WebCore
