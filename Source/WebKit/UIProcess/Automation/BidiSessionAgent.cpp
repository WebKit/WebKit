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

#include "Logging.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include <wtf/HashSet.h>
#include <wtf/UUID.h>

namespace WebKit {

using namespace Inspector;


WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiSessionAgent);

BidiSessionAgent::BidiSessionAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_sessionDomainDispatcher(BidiSessionBackendDispatcher::create(backendDispatcher, this))
{
}

BidiSessionAgent::~BidiSessionAgent() = default;

void BidiSessionAgent::subscribe(Ref<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, RefPtr<JSON::Array>&& userContexts, Inspector::CommandCallback<Inspector::Protocol::BidiSession::Subscription>&& callback)
{
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371

    auto subscriptionID = WTF::createVersion4UUIDString();

    HashSet<AtomString> atomEventNames;
    for (const auto& event : events.get()) {
        auto eventName = event->asString();
        if (eventName.isEmpty())
            return callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "Event name must be a valid string."_s)));
        atomEventNames.add(AtomString { eventName });
    }

    if (contexts && userContexts && (contexts->length() > 0) && (userContexts->length() > 0))
        return callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "Contexts and user contexts cannot be used together."_s)));

    for (const auto& event : atomEventNames) {
        auto addResult = m_eventSubscriptionCounts.add(event, 1);
        if (!addResult.isNewEntry)
            addResult.iterator->value++;
    }

    // FIXME: Support by-context subscriptions.
    // https://bugs.webkit.org/show_bug.cgi?id=282981
    // Also: spec says we need to convert into top-level browsing contexts.
    if (contexts && contexts->length())
        LOG(Automation, "BidiSessionAgent::subscribe: Ignoring contexts parameter, as by-context subscriptions are not supported yet.");

    if (userContexts && userContexts->length())
        LOG(Automation, "BidiSessionAgent::subscribe: Ignoring userContexts parameter, as by-user-context subscriptions are not supported yet.");

    LOG(Automation, "BidiSessionAgent::subscribe: adding subscriptionID=%s, events=%s",
        subscriptionID.utf8().data(),
        events->toJSONString().utf8().data());
    m_eventSubscriptions.add(subscriptionID, BidiEventSubscription { subscriptionID, WTF::move(atomEventNames), { }, { } });

    callback({ subscriptionID });
}

void BidiSessionAgent::unsubscribeByEventName(RefPtr<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, Inspector::CommandCallback<void>&& callback)
{
    RELEASE_LOG(Automation, "BidiSessionAgent::unsubscribeByEventName: events=%s, contexts=%s",
        events ? events->toJSONString().utf8().data() : "null",
        contexts ? contexts->toJSONString().utf8().data() : "null");

    if (!events || !events->length()) {
        callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "At least one event name must be provided."_s)));
        return;
    }

    // FIXME: Support by-context subscriptions.
    // https://bugs.webkit.org/show_bug.cgi?id=282981
    // Maybe we'll don't need to support this at all at unsubscribe(), as this parameter is deprecated in the spec.
    if (contexts && contexts->length())
        LOG(Automation, "BidiSessionAgent::unsubscribe: Ignoring contexts parameter, as by-context subscriptions are not supported yet.");

    HashMap<String, BidiEventSubscription> subscriptionsToKeep;
    HashSet<String> matchedEvents;
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371
    HashSet<AtomString> eventNames;
    for (const auto& event : *events) {
        auto eventName = event->asString();
        if (eventName.isEmpty()) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "Event name must be a valid non-empty string."_s)));
            return;
        }
        eventNames.add(AtomString { eventName });
    }

    for (const auto& subscription : m_eventSubscriptions) {
        auto commonEventNames = eventNames.intersectionWith(subscription.value.events);
        if (!commonEventNames.size()) {
            subscriptionsToKeep.add(subscription.value.id, subscription.value);
            continue;
        }

        // FIXME: Add support to subscribe to specific browsing contexts
        // https://bugs.webkit.org/show_bug.cgi?id=282981
        // In this case, only process them if we were given the "contexts" parameter
        if (!subscription.value.isGlobal()) {
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

    if (matchedEvents.size() != eventNames.size()) {
        callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "Some events are not subscribed to."_s)));
        return;
    }

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

void BidiSessionAgent::unsubscribe(RefPtr<JSON::Array>&& subscriptions, RefPtr<JSON::Array>&& events, RefPtr<JSON::Array>&& contexts, Inspector::CommandCallback<void>&& callback)
{
    RELEASE_LOG(Automation, "BidiSessionAgent::unsubscribe: subscriptions=%s, events=%s, contexts=%s",
        subscriptions ? subscriptions->toJSONString().utf8().data() : "null",
        events ? events->toJSONString().utf8().data() : "null",
        contexts ? contexts->toJSONString().utf8().data() : "null");

    if (!subscriptions) {
        unsubscribeByEventName(WTF::move(events), WTF::move(contexts), WTF::move(callback));
        return;
    }

    Vector<String> subscriptionIDs;
    for (auto& subscription : *subscriptions) {
        auto subscriptionID = subscription->asString();
        if (!subscriptionID) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "Subscription ID must be a valid string."_s)));
            return;
        }

        if (!m_eventSubscriptions.contains(subscriptionID)) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(InvalidParameter, "At least one subscription ID is unknown."_s)));
            return;
        }
        subscriptionIDs.append(subscriptionID);
    }

    for (const auto& id : subscriptionIDs) {
        const auto& subscription = m_eventSubscriptions.get(id);

        for (const auto& event : subscription.events) {
            auto removeResult = m_eventSubscriptionCounts.find(event);
            if (removeResult != m_eventSubscriptionCounts.end()) {
                if (!(--removeResult->value))
                    m_eventSubscriptionCounts.remove(event);
            }
        }
        m_eventSubscriptions.remove(id);
    }

    callback({ });
};

bool BidiSessionAgent::eventIsEnabled(const String& eventName, const HashSet<String>& browsingContexts)
{
    // https://w3c.github.io/webdriver-bidi/#event-is-enabled

    AtomString atomEventName { eventName };

    if (!m_eventSubscriptionCounts.contains(atomEventName))
        return false;

    for (const auto& subscription : m_eventSubscriptions) {
        // FIXME: Add support to subscribe to specific browsing contexts
        // https://bugs.webkit.org/show_bug.cgi?id=282981
        if (!subscription.value.isGlobal())
            continue;

        if (subscription.value.events.contains(atomEventName))
            return true;
    }

    return false;
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

