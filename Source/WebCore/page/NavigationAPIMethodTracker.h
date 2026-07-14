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

#pragma once

#include "JSValueInWrappedObject.h"
#include "NavigationNavigationType.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class AbstractSlotVisitor;
class JSGlobalObject;
}

namespace WebCore {

class DeferredPromise;
class Exception;
class NavigationHistoryEntry;
class SerializedScriptValue;

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker
class NavigationAPIMethodTracker : public RefCounted<NavigationAPIMethodTracker> {
    WTF_MAKE_TZONE_ALLOCATED(NavigationAPIMethodTracker);
public:
    static Ref<NavigationAPIMethodTracker> create(JSC::JSGlobalObject&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState);
    ~NavigationAPIMethodTracker();

    bool operator==(const NavigationAPIMethodTracker& other) const
    {
        // key is optional so we manually identify each tracker.
        return m_identifier == other.m_identifier;
    }

    const String& key() const { return m_key; }
    void setKey(const String& key) { m_key = key; }
    JSValueInWrappedObject& info() { return m_info; }
    SerializedScriptValue* serializedState() const { return m_serializedState.get(); }
    DeferredPromise& committedPromise() { return m_committedPromise; }
    const DeferredPromise& committedPromise() const { return m_committedPromise; }
    DeferredPromise& finishedPromise() { return m_finishedPromise; }
    const DeferredPromise& finishedPromise() const { return m_finishedPromise; }

    bool hasCommitted() const { return !!m_committedToEntry; }
    bool isSettled() const { return m_state == State::Settled; }

    // https://html.spec.whatwg.org/multipage/nav-history-apis.html#notify-about-the-committed-to-entry
    void commitTo(NavigationHistoryEntry&, NavigationNavigationType);
    // https://html.spec.whatwg.org/multipage/nav-history-apis.html#resolve-the-finished-promise
    void resolveFinished();
    // https://html.spec.whatwg.org/multipage/nav-history-apis.html#reject-the-finished-promise
    void rejectFinished(const Exception&, JSC::JSValue exceptionObject);
    void rejectFinished(JSC::JSValue error);

private:
    NavigationAPIMethodTracker(JSC::JSGlobalObject&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState);

    enum class IdentifierType { };
    using Identifier = ObjectIdentifier<IdentifierType>;

    // The commit and finish signals can arrive in either order; each promise settles exactly once.
    enum class State : uint8_t {
        Pending,
        FinishedBeforeCommit,
        Committed,
        Settled,
    };

    State m_state { State::Pending };
    String m_key;
    JSValueInWrappedObject m_info;
    RefPtr<SerializedScriptValue> m_serializedState;
    RefPtr<NavigationHistoryEntry> m_committedToEntry;
    Ref<DeferredPromise> m_committedPromise;
    Ref<DeferredPromise> m_finishedPromise;
    Identifier m_identifier;
};

// Owns the three method tracker slots the Navigation object tracks per the spec:
// the ongoing tracker, the upcoming non-traverse tracker, and the upcoming traverse map.
// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker
class NavigationMethodTrackerRegistry {
public:
    void setUpcomingNonTraverse(Ref<NavigationAPIMethodTracker>&&) WTF_EXCLUDES_LOCK(m_lock);
    void addUpcomingTraverse(const String& key, Ref<NavigationAPIMethodTracker>&&) WTF_EXCLUDES_LOCK(m_lock);
    NavigationAPIMethodTracker* upcomingTraverse(const String& key) const WTF_EXCLUDES_LOCK(m_lock);
    NavigationAPIMethodTracker* ongoing() const WTF_EXCLUDES_LOCK(m_lock);

    RefPtr<NavigationAPIMethodTracker> takeUpcomingNonTraverseIfEquals(NavigationAPIMethodTracker&) WTF_EXCLUDES_LOCK(m_lock);

    // https://html.spec.whatwg.org/multipage/nav-history-apis.html#promote-an-upcoming-api-method-tracker-to-ongoing
    NavigationAPIMethodTracker* promoteUpcomingNonTraverseToOngoing() WTF_EXCLUDES_LOCK(m_lock);
    NavigationAPIMethodTracker* promoteUpcomingTraverseToOngoing(const String& destinationKey) WTF_EXCLUDES_LOCK(m_lock);

    // https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker-clean-up
    void unregister(NavigationAPIMethodTracker&) WTF_EXCLUDES_LOCK(m_lock);

    bool isEmpty() const WTF_EXCLUDES_LOCK(m_lock);
    void visitInGCThread(JSC::AbstractSlotVisitor&) const WTF_EXCLUDES_LOCK(m_lock);

private:
    mutable Lock m_lock;
    RefPtr<NavigationAPIMethodTracker> m_ongoing WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<NavigationAPIMethodTracker> m_upcomingNonTraverse WTF_GUARDED_BY_LOCK(m_lock);
    HashMap<String, Ref<NavigationAPIMethodTracker>> m_upcomingTraverse WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore
