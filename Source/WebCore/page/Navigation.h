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
#include "JSDOMPromise.h"
#include "LocalDOMWindowProperty.h"
#include "NavigationHistoryEntry.h"
#include "NavigationTransition.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class HistoryItem;
class SerializedScriptValue;

enum class FrameLoadType : uint8_t;

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker
struct NavigationAPIMethodTracker {
    NavigationAPIMethodTracker(uint64_t id, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue&& info, RefPtr<SerializedScriptValue>&& serializedState)
        : info(info)
        , serializedState(serializedState)
        , committedPromise(DOMPromise::create(*committed->globalObject(), *JSC::jsCast<JSC::JSPromise*>(committed->promise())))
        , finishedPromise(DOMPromise::create(*finished->globalObject(), *JSC::jsCast<JSC::JSPromise*>(finished->promise())))
        , id(id)
    {
    };

    bool operator==(const NavigationAPIMethodTracker& other) const
    {
        // key is optional so we manually identify each tracker.
        return id == other.id;
    };

    String key;
    JSC::JSValue info;
    RefPtr<SerializedScriptValue> serializedState;
    RefPtr<NavigationHistoryEntry> committedToEntry;
    Ref<DOMPromise> committedPromise;
    Ref<DOMPromise> finishedPromise;

private:
    uint64_t id;
};

class Navigation final : public RefCounted<Navigation>, public EventTarget, public LocalDOMWindowProperty {
    WTF_MAKE_ISO_ALLOCATED(Navigation);
public:
    ~Navigation();

    static Ref<Navigation> create(LocalDOMWindow& window) { return adoptRef(*new Navigation(window)); }

    using RefCounted<Navigation>::ref;
    using RefCounted<Navigation>::deref;

    enum class HistoryBehavior : uint8_t {
        Auto,
        Push,
        Replace,
    };

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
    RefPtr<NavigationTransition> transition() { return m_transition; };

    bool canGoBack() const;
    bool canGoForward() const;

    void initializeEntries(const Ref<HistoryItem>& currentItem, Vector<Ref<HistoryItem>> &items);

    Result navigate(const String& url, NavigateOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result reload(ReloadOptions&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    Result traverseTo(const String& key, Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result back(Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);
    Result forward(Options&&, Ref<DeferredPromise>&&, Ref<DeferredPromise>&&);

    ExceptionOr<void> updateCurrentEntry(UpdateCurrentEntryOptions&&);

    void updateForNavigation(Ref<HistoryItem>&&, FrameLoadType);

private:
    explicit Navigation(LocalDOMWindow&);

    enum EventTargetInterfaceType eventTargetInterface() const final;
    RefPtr<ScriptExecutionContext> protectedScriptExecutionContext() const;
    ScriptExecutionContext* scriptExecutionContext() const final;
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool hasEntriesAndEventsDisabled() const;
    Result performTraversal(NavigationHistoryEntry&, Ref<DeferredPromise> committed, Ref<DeferredPromise> finished);
    std::optional<Ref<NavigationHistoryEntry>> findEntryByKey(const String& key);
    ExceptionOr<RefPtr<SerializedScriptValue>> serializeState(JSC::JSValue state);
    NavigationAPIMethodTracker maybeSetUpcomingNonTraversalTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue info, RefPtr<SerializedScriptValue>&&);


    std::optional<size_t> m_currentEntryIndex;
    RefPtr<NavigationTransition> m_transition;
    Vector<Ref<NavigationHistoryEntry>> m_entries;

    std::optional<NavigationAPIMethodTracker> m_upcomingNonTraverseMethodTracker;
    HashMap<String, NavigationAPIMethodTracker> m_upcomingTraverseMethodTrackers;
};

} // namespace WebCore
