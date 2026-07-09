/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include "NavigationAPIMethodTracker.h"

#include "Exception.h"
#include "JSDOMConvertAny.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "JSNavigationHistoryEntry.h"
#include "JSValueInWrappedObjectInlines.h"
#include "NavigationHistoryEntry.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NavigationAPIMethodTracker);

Ref<NavigationAPIMethodTracker> NavigationAPIMethodTracker::create(JSC::JSGlobalObject& globalObject, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState)
{
    return adoptRef(*new NavigationAPIMethodTracker(globalObject, WTF::move(committed), WTF::move(finished), WTF::move(info), WTF::move(serializedState)));
}

NavigationAPIMethodTracker::NavigationAPIMethodTracker(JSC::JSGlobalObject& globalObject, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& infoValue, RefPtr<SerializedScriptValue>&& serializedStateValue)
    : m_info(globalObject, infoValue)
    , m_serializedState(serializedStateValue)
    , m_committedPromise(WTF::move(committed))
    , m_finishedPromise(WTF::move(finished))
    , m_identifier(Identifier::generate())
{
    // Because rejection is also reported via the navigateerror event, the finished promise
    // never causes unhandled rejection reporting.
    m_finishedPromise->markAsHandled();
}

NavigationAPIMethodTracker::~NavigationAPIMethodTracker() = default;

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#notify-about-the-committed-to-entry
void NavigationAPIMethodTracker::commitTo(NavigationHistoryEntry& entry, NavigationNavigationType navigationType)
{
    // The navigation may have been aborted (settling both promises) before the commit signal arrives,
    // for example by an intercept handler starting another navigation.
    if (m_state == State::Settled)
        return;

    ASSERT(!m_committedToEntry);
    m_committedToEntry = &entry;
    if (navigationType != NavigationNavigationType::Traverse && m_serializedState)
        entry.setState(WTF::move(m_serializedState));

    protect(m_committedPromise)->resolve<IDLInterface<NavigationHistoryEntry>>(entry);

    if (m_state == State::FinishedBeforeCommit) {
        protect(m_finishedPromise)->resolve<IDLInterface<NavigationHistoryEntry>>(entry);
        m_state = State::Settled;
    } else
        m_state = State::Committed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#resolve-the-finished-promise
void NavigationAPIMethodTracker::resolveFinished()
{
    if (m_state == State::Settled)
        return;

    RefPtr committedToEntry = m_committedToEntry;
    if (!committedToEntry) {
        // The committed promise must resolve first; hold the finish signal until commitTo() runs.
        m_state = State::FinishedBeforeCommit;
        return;
    }

    ASSERT(m_state == State::Committed);
    protect(m_finishedPromise)->resolve<IDLInterface<NavigationHistoryEntry>>(*committedToEntry);
    m_state = State::Settled;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#reject-the-finished-promise
void NavigationAPIMethodTracker::rejectFinished(const Exception& exception, JSC::JSValue exceptionObject)
{
    if (m_state == State::Settled)
        return;

    // Only reject the committed promise if it hasn't been fulfilled yet. If the navigation was committed
    // before being aborted, the committed promise stays fulfilled while only the finished promise rejects.
    if (m_state != State::Committed)
        protect(m_committedPromise)->reject(exception, RejectAsHandled::No, exceptionObject);
    protect(m_finishedPromise)->reject(exception, RejectAsHandled::Yes, exceptionObject);
    m_state = State::Settled;
}

void NavigationAPIMethodTracker::rejectFinished(JSC::JSValue error)
{
    if (m_state == State::Settled)
        return;

    if (m_state != State::Committed)
        protect(m_committedPromise)->reject<IDLAny>(error, RejectAsHandled::No);
    protect(m_finishedPromise)->reject<IDLAny>(error, RejectAsHandled::Yes);
    m_state = State::Settled;
}

void NavigationMethodTrackerRegistry::setUpcomingNonTraverse(Ref<NavigationAPIMethodTracker>&& tracker)
{
    assertIsMainThread();
    Locker locker { m_lock };
    // FIXME: We should be able to assert m_upcomingNonTraverse is empty.
    m_upcomingNonTraverse = WTF::move(tracker);
}

void NavigationMethodTrackerRegistry::addUpcomingTraverse(const String& key, Ref<NavigationAPIMethodTracker>&& tracker)
{
    assertIsMainThread();
    Locker locker { m_lock };
    m_upcomingTraverse.add(key, WTF::move(tracker));
}

NavigationAPIMethodTracker* NavigationMethodTrackerRegistry::upcomingTraverse(const String& key) const
{
    assertIsMainThread();
    Locker locker { m_lock };
    if (key.isNull())
        return nullptr;
    return m_upcomingTraverse.get(key);
}

NavigationAPIMethodTracker* NavigationMethodTrackerRegistry::ongoing() const
{
    assertIsMainThread();
    Locker locker { m_lock };
    return m_ongoing.get();
}

RefPtr<NavigationAPIMethodTracker> NavigationMethodTrackerRegistry::takeUpcomingNonTraverseIfEquals(NavigationAPIMethodTracker& tracker)
{
    assertIsMainThread();
    Locker locker { m_lock };
    if (m_upcomingNonTraverse != &tracker)
        return nullptr;
    return std::exchange(m_upcomingNonTraverse, nullptr);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#promote-an-upcoming-api-method-tracker-to-ongoing
NavigationAPIMethodTracker* NavigationMethodTrackerRegistry::promoteUpcomingNonTraverseToOngoing()
{
    assertIsMainThread();
    Locker locker { m_lock };
    // FIXME: We should be able to assert m_ongoing is unset.
    m_ongoing = WTF::move(m_upcomingNonTraverse);
    return m_ongoing.get();
}

NavigationAPIMethodTracker* NavigationMethodTrackerRegistry::promoteUpcomingTraverseToOngoing(const String& destinationKey)
{
    assertIsMainThread();
    Locker locker { m_lock };
    // FIXME: We should be able to assert m_ongoing is unset.
    ASSERT(destinationKey.isNull() || !destinationKey.isEmpty());
    if (destinationKey.isNull())
        return nullptr;
    m_ongoing = m_upcomingTraverse.take(destinationKey);
    return m_ongoing.get();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker-clean-up
void NavigationMethodTrackerRegistry::unregister(NavigationAPIMethodTracker& tracker)
{
    assertIsMainThread();
    Locker locker { m_lock };
    if (m_ongoing == &tracker) {
        m_ongoing = nullptr;
        return;
    }
    auto& key = tracker.key();
    // FIXME: We should be able to assert key isn't null and m_upcomingTraverse contains it.
    if (!key.isNull())
        m_upcomingTraverse.remove(key);
}

bool NavigationMethodTrackerRegistry::isEmpty() const
{
    assertIsMainThread();
    Locker locker { m_lock };
    return !m_ongoing && !m_upcomingNonTraverse && m_upcomingTraverse.isEmpty();
}

void NavigationMethodTrackerRegistry::visitInGCThread(JSC::AbstractSlotVisitor& visitor) const
{
    Locker locker { m_lock };
    if (m_ongoing)
        m_ongoing->info().visitInGCThread(visitor);
    if (m_upcomingNonTraverse)
        m_upcomingNonTraverse->info().visitInGCThread(visitor);
    for (auto& tracker : m_upcomingTraverse.values())
        tracker->info().visitInGCThread(visitor);
}

} // namespace WebCore
