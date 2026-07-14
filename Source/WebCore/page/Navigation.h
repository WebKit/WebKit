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

#include "BackForwardItemIdentifier.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "JSDOMPromiseDeferredForward.h"
#include "LocalDOMWindowProperty.h"
#include "NavigateEvent.h"
#include "NavigationAPIMethodTracker.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"
#include "NavigationRateLimiter.h"
#include "NavigationTransition.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Seconds.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class DocumentLoader;
class FormState;
class HistoryItem;
class SerializedScriptValue;
class NavigationActivation;
class NavigationDestination;

enum class ShouldCopyStateObjectFromCurrentEntry : bool { No, Yes };

// Controls whether updateForNavigation should fulfill the committed promise immediately.
// For intercepted traversals, timing depends on handler presence (see spec).
// https://html.spec.whatwg.org/multipage/nav-history-apis.html#notify-about-the-committed-to-entry
enum class ShouldNotifyCommitted : bool { No, Yes };

class Navigation final : public RefCounted<Navigation>, public EventTarget, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED(Navigation);
public:
    ~Navigation();

    static Ref<Navigation> create(LocalDOMWindow& window) { return adoptRef(*new Navigation(window)); }

    using RefCounted::ref;
    using RefCounted::deref;

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

    const Vector<Ref<NavigationHistoryEntry>>& entries() const LIFETIME_BOUND;
    NavigationHistoryEntry* currentEntry() const;
    NavigationTransition* transition() { return m_transition.get(); };
    NavigationActivation* activation() { return m_activation.get(); };

    bool canGoBack() const;
    bool canGoForward() const;

    void initializeForNewWindow(std::optional<NavigationNavigationType>, LocalDOMWindow* previousWindow);

    Result navigate(JSC::JSGlobalObject&, const String& url, NavigateOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result reload(JSC::JSGlobalObject&, ReloadOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result traverseTo(JSC::JSGlobalObject&, const String& key, Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result back(JSC::JSGlobalObject&, Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result forward(JSC::JSGlobalObject&, Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    ExceptionOr<void> updateCurrentEntry(UpdateCurrentEntryOptions&&);

    enum class DispatchResult : uint8_t { Completed, Aborted, Intercepted };
    DispatchResult dispatchTraversalNavigateEvent(HistoryItem&);
    bool dispatchPushReplaceReloadNavigateEvent(const URL&, NavigationNavigationType, bool isSameDocument, FormState*, SerializedScriptValue* classicHistoryAPIState = nullptr, Element* sourceElement = nullptr);
    bool dispatchDownloadNavigateEvent(const URL&, const String& downloadFilename, Element* sourceElement = nullptr);

    void updateForNavigation(Ref<HistoryItem>&&, NavigationNavigationType, ShouldCopyStateObjectFromCurrentEntry = ShouldCopyStateObjectFromCurrentEntry::No, ShouldNotifyCommitted = ShouldNotifyCommitted::Yes);
    void updateForReactivation(Vector<Ref<HistoryItem>>&& newHistoryItems, HistoryItem& reactivatedItem, HistoryItem* previousItem);

    RefPtr<NavigationActivation> createForPageswapEvent(HistoryItem* newItem, DocumentLoader*, bool fromBackForwardCache);

    void abortOngoingNavigationIfNeeded();

    NavigationHistoryEntry* findEntryByKey(const String&) const;
    bool suppressNormalScrollRestoration() const { return m_suppressNormalScrollRestorationDuringOngoingNavigation; }

    void setFocusChanged(FocusDidChange changed) { m_focusChangedDuringOngoingNavigation = changed; }

    // EventTarget.
    ScriptExecutionContext* scriptExecutionContext() const final;

    void rejectFinishedPromise(NavigationAPIMethodTracker*);
    NavigationAPIMethodTracker* upcomingTraverseMethodTracker(const String& key) const;

    void visitAdditionalChildrenInGCThread(JSC::AbstractSlotVisitor&);

    class AbortHandler : public RefCountedAndCanMakeWeakPtr<AbortHandler> {
    public:
        bool wasAborted() const { return m_wasAborted; }

    private:
        friend class Navigation;

        static Ref<AbortHandler> create() { return adoptRef(*new AbortHandler); }
        void markAsAborted() { m_wasAborted = true; }

        bool m_wasAborted { false };
    };
    Ref<AbortHandler> registerAbortHandler();

    // Testing support
    NavigationRateLimiter& rateLimiterForTesting() LIFETIME_BOUND { return m_rateLimiter; }

    NavigateEvent* ongoingNavigateEvent() { return m_ongoingNavigateEvent.get(); } // This may get called on a GC thread.
    bool hasInterceptedOngoingNavigateEvent() const { return m_ongoingNavigateEvent && m_ongoingNavigateEvent->wasIntercepted(); }

    void updateNavigationEntry(Ref<HistoryItem>&&, ShouldCopyStateObjectFromCurrentEntry);

    static Vector<Ref<HistoryItem>> filterHistoryItemsForNavigationAPI(Vector<Ref<HistoryItem>>&&, HistoryItem&);

private:
    explicit Navigation(LocalDOMWindow&);

    // EventTarget.
    enum EventTargetInterfaceType eventTargetInterface() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool hasEntriesAndEventsDisabled() const;
    Result performTraversal(JSC::JSGlobalObject&, const String& key, Navigation::Options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished);
    ExceptionOr<RefPtr<SerializedScriptValue>> serializeState(JSC::JSValue state);
    DispatchResult innerDispatchNavigateEvent(NavigationNavigationType, Ref<NavigationDestination>&&, const String& downloadRequestFilename, FormState* = nullptr, SerializedScriptValue* classicHistoryAPIState = nullptr, Element* sourceElement = nullptr);
    void setupInterceptionState(NavigateEvent&, NavigationNavigationType, NavigationDestination&, Document&, SerializedScriptValue* classicHistoryAPIState);
    std::optional<DispatchResult> handleSameDocumentNavigation(NavigateEvent&, NavigationNavigationType, NavigationAPIMethodTracker*, AbortController&, Document&);

    void setActivation(HistoryItem* previousItem, std::optional<NavigationNavigationType>);

    RefPtr<NavigationAPIMethodTracker> maybeSetUpcomingNonTraversalTracker(JSC::JSGlobalObject&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue info, RefPtr<SerializedScriptValue>&&);
    RefPtr<NavigationAPIMethodTracker> addUpcomingTraverseAPIMethodTracker(JSC::JSGlobalObject&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, const String& key, JSC::JSValue info);
    void resolveFinishedPromise(NavigationAPIMethodTracker*);
    void rejectFinishedPromise(NavigationAPIMethodTracker*, const Exception&, JSC::JSValue exceptionObject);
    void abortOngoingNavigation(NavigateEvent&);
    void notifyCommittedToEntry(NavigationAPIMethodTracker*, NavigationHistoryEntry*, NavigationNavigationType);
    Result apiMethodTrackerDerivedResult(const NavigationAPIMethodTracker&);

    size_t entryIndexOfKey(const String&) const;
    bool hasEntryWithKey(const String&) const;

    void disposeOfForwardEntriesInParents(BackForwardItemIdentifier);
    void recursivelyDisposeOfForwardEntriesInParents(BackForwardItemIdentifier, LocalFrame* navigatedFrame);

    std::optional<size_t> m_currentEntryIndex;
    RefPtr<NavigationTransition> m_transition;
    RefPtr<NavigationActivation> m_activation;
    Vector<Ref<NavigationHistoryEntry>> m_entries;

    RefPtr<NavigateEvent> m_ongoingNavigateEvent;
    FocusDidChange m_focusChangedDuringOngoingNavigation { FocusDidChange::No };
    bool m_suppressNormalScrollRestorationDuringOngoingNavigation { false };
    NavigationMethodTrackerRegistry m_methodTrackers;
    WeakHashSet<AbortHandler> m_abortHandlers;
    NavigationRateLimiter m_rateLimiter;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENTTARGET(Navigation)
