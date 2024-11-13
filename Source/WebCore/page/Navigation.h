/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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

#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindowProperty.h"
#include "NavigateEvent.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"
#include "NavigationTransition.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class FormState;
class HistoryItem;
class SerializedScriptValue;
class NavigateEvent;
class NavigationActivation;
class NavigationDestination;

enum class FrameLoadType : uint8_t;

enum class NavigationAPIMethodTrackerType { };
using NavigationAPIMethodTrackerIdentifier = ObjectIdentifier<NavigationAPIMethodTrackerType>;

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker
struct NavigationAPIMethodTracker : public RefCounted<NavigationAPIMethodTracker> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED(NavigationAPIMethodTracker);

    static Ref<NavigationAPIMethodTracker> create(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState)
    {
        return adoptRef(*new NavigationAPIMethodTracker(WTFMove(committed), WTFMove(finished), WTFMove(info), WTFMove(serializedState)));
    }

    bool operator==(const NavigationAPIMethodTracker& other) const
    {
        // key is optional so we manually identify each tracker.
        return identifier == other.identifier;
    }

    bool finishedBeforeCommit { false };
    String key;
    JSC::JSValue info;
    RefPtr<SerializedScriptValue> serializedState;
    RefPtr<NavigationHistoryEntry> committedToEntry;
    Ref<DeferredPromise> committedPromise;
    Ref<DeferredPromise> finishedPromise;

private:
    explicit NavigationAPIMethodTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState)
        : info(info)
        , serializedState(serializedState)
        , committedPromise(WTFMove(committed))
        , finishedPromise(WTFMove(finished))
        , identifier(NavigationAPIMethodTrackerIdentifier::generate())
    {
    }

    NavigationAPIMethodTrackerIdentifier identifier;
};

class Navigation final : public RefCounted<Navigation>, public EventTarget, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(Navigation);
public:
    ~Navigation();

    static Ref<Navigation> create(LocalDOMWindow& window) { return adoptRef(*new Navigation(window)); }

    using RefCounted<Navigation>::ref;
    using RefCounted<Navigation>::deref;

    using HistoryBehavior = NavigationHistoryBehavior;

    struct UpdateCurrentEntryOptions {
        JSC::JSValue state;
    };

    struct Options {
        JSC::JSValue info;
    };

    struct NavigateOptions : Options {
        JSC::JSValue state;
        HistoryBehavior history;
    };

    struct ReloadOptions : Options {
        JSC::JSValue state;
    };

    struct Result {
        RefPtr<DOMPromise> committed;
        RefPtr<DOMPromise> finished;
    };

    const Vector<Ref<NavigationHistoryEntry>>& entries() const;
    NavigationHistoryEntry* currentEntry() const;
    NavigationTransition* transition() { return m_transition.get(); };
    NavigationActivation* activation() { return m_activation.get(); };

    bool canGoBack() const;
    bool canGoForward() const;

    void initializeForNewWindow(std::optional<NavigationNavigationType>, LocalDOMWindow* previousWindow);

    Result navigate(const String& url, NavigateOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result reload(ReloadOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result traverseTo(const String& key, Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result back(Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result forward(Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    ExceptionOr<void> updateCurrentEntry(UpdateCurrentEntryOptions&&);

    enum class DispatchResult : uint8_t { Completed, Aborted, Intercepted };
    DispatchResult dispatchTraversalNavigateEvent(HistoryItem&);
    bool dispatchPushReplaceReloadNavigateEvent(const URL&, NavigationNavigationType, bool isSameDocument, FormState*, SerializedScriptValue* classicHistoryAPIState = nullptr);
    bool dispatchDownloadNavigateEvent(const URL&, const String& downloadFilename);

    void updateForNavigation(Ref<HistoryItem>&&, NavigationNavigationType);
    void updateForReactivation(Vector<Ref<HistoryItem>>& newHistoryItems, HistoryItem& reactivatedItem);
    void updateForActivation(HistoryItem* previousItem, std::optional<NavigationNavigationType>);

    RefPtr<NavigationActivation> createForPageswapEvent(HistoryItem* newItem, DocumentLoader*, bool fromBackForwardCache);

    void abortOngoingNavigationIfNeeded();

    std::optional<Ref<NavigationHistoryEntry>> findEntryByKey(const String& key);
    bool suppressNormalScrollRestoration() const { return m_suppressNormalScrollRestorationDuringOngoingNavigation; }

    void setFocusChanged(FocusDidChange changed) { m_focusChangedDuringOngoingNavigation = changed; }

private:
    explicit Navigation(LocalDOMWindow&);

    enum EventTargetInterfaceType eventTargetInterface() const final;
    RefPtr<ScriptExecutionContext> protectedScriptExecutionContext() const;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool hasEntriesAndEventsDisabled() const;
    Result performTraversal(const String& key, Navigation::Options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished);
    ExceptionOr<RefPtr<SerializedScriptValue>> serializeState(JSC::JSValue state);
    DispatchResult innerDispatchNavigateEvent(NavigationNavigationType, Ref<NavigationDestination>&&, const String& downloadRequestFilename, FormState* = nullptr, SerializedScriptValue* classicHistoryAPIState = nullptr);

    RefPtr<NavigationAPIMethodTracker> maybeSetUpcomingNonTraversalTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue info, RefPtr<SerializedScriptValue>&&);
    RefPtr<NavigationAPIMethodTracker> addUpcomingTrarveseAPIMethodTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, const String& key, JSC::JSValue info);
    void cleanupAPIMethodTracker(NavigationAPIMethodTracker*);
    void resolveFinishedPromise(NavigationAPIMethodTracker*);
    void rejectFinishedPromise(NavigationAPIMethodTracker*, const Exception&, JSC::JSValue exceptionObject);
    void abortOngoingNavigation(NavigateEvent&);
    void promoteUpcomingAPIMethodTracker(const String& destinationKey);
    void notifyCommittedToEntry(NavigationAPIMethodTracker*, NavigationHistoryEntry*, NavigationNavigationType);
    Result apiMethodTrackerDerivedResult(const NavigationAPIMethodTracker&);

    std::optional<size_t> m_currentEntryIndex;
    RefPtr<NavigationTransition> m_transition;
    RefPtr<NavigationActivation> m_activation;
    Vector<Ref<NavigationHistoryEntry>> m_entries;

    RefPtr<NavigateEvent> m_ongoingNavigateEvent;
    FocusDidChange m_focusChangedDuringOngoingNavigation { FocusDidChange::No };
    bool m_suppressNormalScrollRestorationDuringOngoingNavigation { false };
    RefPtr<NavigationAPIMethodTracker> m_ongoingAPIMethodTracker;
    RefPtr<NavigationAPIMethodTracker> m_upcomingNonTraverseMethodTracker;
    HashMap<String, Ref<NavigationAPIMethodTracker>> m_upcomingTraverseMethodTrackers;
};

} // namespace WebCore
