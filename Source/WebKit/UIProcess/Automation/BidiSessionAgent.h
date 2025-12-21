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

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "WebDriverBidiBackendDispatchers.h"
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebKit {

class WebAutomationSession;

using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;
using Subscription = Inspector::Protocol::BidiSession::Subscription;
using UserContext = Inspector::Protocol::BidiBrowser::UserContext;

struct BidiEventSubscription {
    Subscription id;
    HashSet<AtomString> events;
    HashSet<BrowsingContext> browsingContextIDs;
    HashSet<UserContext> userContextIDs;

    bool isGlobal() const
    {
        return browsingContextIDs.isEmpty() && userContextIDs.isEmpty();
    }

    String asString() const;
};

class BidiSessionAgent final : public Inspector::BidiSessionBackendDispatcherHandler {
    WTF_MAKE_TZONE_ALLOCATED(BidiSessionAgent);
public:
    BidiSessionAgent(WebAutomationSession&, Inspector::BackendDispatcher&);
    ~BidiSessionAgent() override;

    void subscribe(Ref<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, RefPtr<JSON::Array>&& userContexts, Inspector::CommandCallback<Inspector::Protocol::BidiSession::Subscription>&&) override;
    void unsubscribe(RefPtr<JSON::Array>&& subscriptions, RefPtr<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, Inspector::CommandCallback<void>&&) override;

    bool eventIsEnabled(const String& eventName, const HashSet<String>& browsingContexts);

private:

    void unsubscribeByEventName(RefPtr<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, Inspector::CommandCallback<void>&&);

    WeakPtr<WebAutomationSession> m_session;
    Ref<Inspector::BidiSessionBackendDispatcher> m_sessionDomainDispatcher;

    // https://w3c.github.io/webdriver-bidi/#events
    HashMap<AtomString, unsigned> m_eventSubscriptionCounts;
    HashMap<Subscription, BidiEventSubscription> m_eventSubscriptions;
};

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
