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

#include "config.h"
#include "BidiSessionAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "BidiEventNames.h"
#include "BidiScriptAgent.h"
#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProcessor.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include <wtf/HashSet.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>

namespace WebKit {

using namespace Inspector;

static bool shouldReplayRealmCreatedEvents(const HashSet<AtomString>& events)
{
    return events.contains(AtomString { BidiEventNames::Script::RealmCreated });
}

static bool subscriptionMatchesScope(const BidiEventSubscription& subscription, const HashSet<String>& browsingContexts)
{
    if (subscription.isGlobal())
        return true;

    for (const auto& browsingContext : browsingContexts) {
        if (subscription.browsingContextIDs.contains(browsingContext))
            return true;
    }

    return false;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiSessionAgent);

BidiSessionAgent::BidiSessionAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_sessionDomainDispatcher(BidiSessionBackendDispatcher::create(backendDispatcher, this))
{
}

BidiSessionAgent::~BidiSessionAgent() = default;

void BidiSessionAgent::subscribe(Ref<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, RefPtr<JSON::Array>&& userContexts, Inspector::CommandCallback<Inspector::Protocol::BidiSession::SubscriptionID>&& callback)
{
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371

    auto subscriptionID = WTF::createVersion4UUIDString();

    HashSet<AtomString> atomEventNames;
    for (const auto& event : events.get()) {
        auto eventName = event->asString();

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(eventName.isEmpty(), InvalidParameter, "Event name must be a valid string."_s);
        atomEventNames.add(AtomString { eventName });
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(contexts && userContexts && (contexts->length() > 0) && (userContexts->length() > 0), InvalidParameter, "Contexts and user contexts cannot be used together."_s);

    // FIXME: Support by-userContext subscriptions
    // https://webkit.org/b/309502
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(userContexts && userContexts->length(), NotImplemented, "Subscriptions by userContext are not supported yet."_s);

    HashSet<BrowsingContext> browsingContextIDs;
    if (contexts) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!contexts->length(), InvalidParameter, "At least one browsing context must be provided."_s);

        RefPtr session = m_session.get();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

        for (const auto& context : *contexts) {
            auto browsingContext = context->asString();
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(browsingContext.isEmpty(), InvalidParameter, "Browsing context must be a valid non-empty string."_s);

            [[maybe_unused]] auto handles = session->extractBrowsingContextHandles(browsingContext);
            ASYNC_FAIL_IF_UNEXPECTED_RESULT(handles);

            browsingContextIDs.add(browsingContext);
        }
    }

    for (const auto& event : atomEventNames) {
        auto addResult = m_eventSubscriptionCounts.add(event, 1);
        if (!addResult.isNewEntry)
            addResult.iterator->value++;
    }

    LOG(Automation, "BidiSessionAgent::subscribe: adding subscriptionID=%s, events=%s",
        subscriptionID.utf8().data(),
        events->toJSONString().utf8().data());
    m_eventSubscriptions.add(subscriptionID, BidiEventSubscription { subscriptionID, WTF::move(atomEventNames), WTF::move(browsingContextIDs), { } });

    if (shouldReplayRealmCreatedEvents(m_eventSubscriptions.get(subscriptionID).events)) {
        RefPtr session = m_session.get();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);
        session->bidiProcessor().scriptAgent().emitEventsForActiveRealms(m_eventSubscriptions.get(subscriptionID).browsingContextIDs);
    }

    callback({ subscriptionID });
}

void BidiSessionAgent::unsubscribeByEventName(RefPtr<JSON::Array>&& events, Inspector::CommandCallback<void>&& callback)
{
IGNORE_GCC_WARNINGS_BEGIN("format-overflow")
    LOG(Automation, "BidiSessionAgent::unsubscribeByEventName: events=%s",
        events ? events->toJSONString().utf8().data() : "null");
IGNORE_GCC_WARNINGS_END

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!events || !events->length(), InvalidParameter, "At least one event name must be provided."_s);

    HashMap<String, BidiEventSubscription> subscriptionsToKeep;
    HashSet<String> matchedEvents;
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371
    HashSet<AtomString> eventNames;
    for (const auto& event : *events) {
        auto eventName = event->asString();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(eventName.isEmpty(), InvalidParameter, "Event name must be a valid non-empty string."_s);
        eventNames.add(AtomString { eventName });
    }

    for (const auto& subscription : m_eventSubscriptions) {
        auto commonEventNames = eventNames.intersectionWith(subscription.value.events);
        if (!commonEventNames.size()) {
            subscriptionsToKeep.add(subscription.value.id, subscription.value);
            continue;
        }

        auto currentSubscriptionEventNames = subscription.value.events;
        for (const auto& eventName : commonEventNames) {
            matchedEvents.add(eventName);
            currentSubscriptionEventNames.remove(eventName);
        }
        if (!currentSubscriptionEventNames.isEmpty()) {
            auto clonedSubscription = subscription.value;
            clonedSubscription.events = currentSubscriptionEventNames;
            subscriptionsToKeep.add(clonedSubscription.id, WTF::move(clonedSubscription));
        }
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(matchedEvents.size() != eventNames.size(), InvalidParameter, "Some events are not subscribed to."_s);

    // Only modify the actual subscriptions after we validated all requested events.
    m_eventSubscriptions = WTF::move(subscriptionsToKeep);
    m_eventSubscriptionCounts.clear();
    for (const auto& subscription : m_eventSubscriptions.values()) {
        for (const auto& event : subscription.events) {
            auto addResult = m_eventSubscriptionCounts.add(event, 1);
            if (!addResult.isNewEntry)
                addResult.iterator->value++;
        }
    }

    callback({ });
}

void BidiSessionAgent::unsubscribe(RefPtr<JSON::Array>&& subscriptions, RefPtr<JSON::Array>&& events, Inspector::CommandCallback<void>&& callback)
{
IGNORE_GCC_WARNINGS_BEGIN("format-overflow")
    LOG(Automation, "BidiSessionAgent::unsubscribe: subscriptions=%s, events=%s",
        subscriptions ? subscriptions->toJSONString().utf8().data() : "null",
        events ? events->toJSONString().utf8().data() : "null");
IGNORE_GCC_WARNINGS_END

    if (!subscriptions) {
        unsubscribeByEventName(WTF::move(events), WTF::move(callback));
        return;
    }

    Vector<String> subscriptionIDs;
    for (auto& subscription : *subscriptions) {
        auto subscriptionID = subscription->asString();
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!subscriptionID, InvalidParameter, "Subscription ID must be a valid string."_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!m_eventSubscriptions.contains(subscriptionID), InvalidParameter, "At least one subscription ID is unknown."_s);
        subscriptionIDs.append(subscriptionID);
    }

    for (const auto& id : subscriptionIDs) {
        const auto& subscription = m_eventSubscriptions.get(id);

        for (const auto& event : subscription.events) {
            auto findResult = m_eventSubscriptionCounts.find(event);
            if (findResult != m_eventSubscriptionCounts.end()) {
                if (!(--findResult->value))
                    m_eventSubscriptionCounts.remove(event);
            }
        }
        m_eventSubscriptions.remove(id);
    }

    callback({ });
}

bool BidiSessionAgent::eventIsEnabled(const String& eventName, const HashSet<String>& browsingContexts)
{
    // https://w3c.github.io/webdriver-bidi/#event-is-enabled
    AtomString atomEventName { eventName };

    if (!m_eventSubscriptionCounts.contains(atomEventName))
        return false;

    for (const auto& subscription : m_eventSubscriptions) {
        if (subscription.value.events.contains(atomEventName) && subscriptionMatchesScope(subscription.value, browsingContexts))
            return true;
    }

    return false;
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
