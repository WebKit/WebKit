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

#include "config.h"
#include "Navigation.h"

#include "AbortController.h"
#include "EventNames.h"
#include "Exception.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderTypes.h"
#include "HistoryItem.h"
#include "JSDOMPromise.h"
#include "JSNavigationHistoryEntry.h"
#include "MessagePort.h"
#include "NavigateEvent.h"
#include "NavigationCurrentEntryChangeEvent.h"
#include "NavigationDestination.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Navigation);

Navigation::Navigation(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-cangoback
bool Navigation::canGoBack() const
{
    if (hasEntriesAndEventsDisabled())
        return false;
    ASSERT(m_currentEntryIndex);
    if (!*m_currentEntryIndex)
        return false;
    return true;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-cangoforward
bool Navigation::canGoForward() const
{
    if (hasEntriesAndEventsDisabled())
        return false;
    ASSERT(m_currentEntryIndex);
    if (*m_currentEntryIndex == m_entries.size() - 1)
        return false;
    return true;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#initialize-the-navigation-api-entries-for-a-new-document
void Navigation::initializeEntries(const Ref<HistoryItem>& currentItem, Vector<Ref<HistoryItem>>& items)
{
    for (Ref item : items)
        m_entries.append(NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), item));
    m_currentEntryIndex = items.find(currentItem);
}

const Vector<Ref<NavigationHistoryEntry>>& Navigation::entries() const
{
    static NeverDestroyed<Vector<Ref<NavigationHistoryEntry>>> emptyEntries;
    if (hasEntriesAndEventsDisabled())
        return emptyEntries;
    return m_entries;
}

NavigationHistoryEntry* Navigation::currentEntry() const
{
    if (!hasEntriesAndEventsDisabled() && m_currentEntryIndex)
        return m_entries.at(*m_currentEntryIndex).ptr();
    return nullptr;
}

Navigation::~Navigation() = default;

ScriptExecutionContext* Navigation::scriptExecutionContext() const
{
    RefPtr window = this->window();
    return window ? window->document() : nullptr;
}

RefPtr<ScriptExecutionContext> Navigation::protectedScriptExecutionContext() const
{
    return scriptExecutionContext();
}

enum EventTargetInterfaceType Navigation::eventTargetInterface() const
{
    return EventTargetInterfaceType::Navigation;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-early-error-result
static Navigation::Result createErrorResult(Ref<DeferredPromise> committed, Ref<DeferredPromise> finished, Exception&& exception)
{
    ASSERT(committed->globalObject() == finished->globalObject());
    auto globalObject = committed->globalObject();
    Navigation::Result result = {
        DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(committed->promise())),
        DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(finished->promise()))
    };

    JSC::JSValue exceptionObject;
    committed->reject(exception, RejectAsHandled::No, exceptionObject);
    finished->reject(exception, RejectAsHandled::No, exceptionObject);

    return result;
}

static Navigation::Result createErrorResult(Ref<DeferredPromise> committed, Ref<DeferredPromise> finished, ExceptionCode exceptionCode, const String& errorMessage)
{
    return createErrorResult(committed, finished, Exception { exceptionCode, errorMessage });
}

ExceptionOr<RefPtr<SerializedScriptValue>> Navigation::serializeState(JSC::JSValue state)
{
    if (state.isUndefined())
        return { nullptr };

    Vector<RefPtr<MessagePort>> dummyPorts;
    auto serializeResult = SerializedScriptValue::create(*protectedScriptExecutionContext()->globalObject(), state, { }, dummyPorts, SerializationForStorage::Yes);
    if (serializeResult.hasException())
        return serializeResult.releaseException();

    return { serializeResult.releaseReturnValue().ptr() };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#maybe-set-the-upcoming-non-traverse-api-method-tracker
NavigationAPIMethodTracker Navigation::maybeSetUpcomingNonTraversalTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue info, RefPtr<SerializedScriptValue>&& serializedState)
{
    static uint64_t lastTrackerID;
    auto apiMethodTracker = NavigationAPIMethodTracker(lastTrackerID++, WTFMove(committed), WTFMove(finished), WTFMove(info), WTFMove(serializedState));

    // FIXME: apiMethodTracker.finishedPromise needs to be considered Handled

    ASSERT(!m_upcomingNonTraverseMethodTracker);
    if (!hasEntriesAndEventsDisabled())
        m_upcomingNonTraverseMethodTracker = apiMethodTracker;

    return apiMethodTracker;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-reload
Navigation::Result Navigation::reload(ReloadOptions&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    auto serializedState = serializeState(options.state);
    if (serializedState.hasException())
        return createErrorResult(committed, finished, serializedState.releaseException());

    auto apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTFMove(committed), WTFMove(finished), WTFMove(options.info), serializedState.releaseReturnValue());

    // FIXME: Only a stub to reload for testing.
    window()->frame()->loader().reload();

    // FIXME: keep track of promises to resolve later.
    Ref entry = NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), { });
    Navigation::Result result = { apiMethodTracker.committedPromise.ptr(), apiMethodTracker.finishedPromise.ptr() };
    committed->resolve<IDLInterface<NavigationHistoryEntry>>(entry.get());
    finished->resolve<IDLInterface<NavigationHistoryEntry>>(entry.get());
    return result;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-navigate
Navigation::Result Navigation::navigate(const String& url, NavigateOptions&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    auto currentURL = scriptExecutionContext()->url();
    auto newURL = URL { currentURL, url };

    if (!newURL.isValid())
        return createErrorResult(committed, finished, ExceptionCode::SyntaxError, "Invalid URL"_s);

    if (options.history == HistoryBehavior::Auto) {
        if (newURL.protocolIsJavaScript() || currentURL.isAboutBlank())
            options.history = HistoryBehavior::Replace;
        else
            options.history = HistoryBehavior::Push;
    }

    if (options.history == HistoryBehavior::Push && newURL.protocolIsJavaScript())
        return createErrorResult(committed, finished, ExceptionCode::NotSupportedError, "A \"push\" navigation was explicitly requested, but only a \"replace\" navigation is possible when navigating to a javascript: URL."_s);

    if (options.history == HistoryBehavior::Push && currentURL.isAboutBlank())
        return createErrorResult(committed, finished, ExceptionCode::NotSupportedError, "A \"push\" navigation was explicitly requested, but only a \"replace\" navigation is possible while on an about:blank document."_s);

    auto serializedState = serializeState(options.state);
    if (serializedState.hasException())
        return createErrorResult(committed, finished, serializedState.releaseException());

    if (!window()->frame() || !window()->frame()->document())
        return createErrorResult(committed, finished, ExceptionCode::InvalidStateError, "Invalid state"_s);

    auto apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTFMove(committed), WTFMove(finished), WTFMove(options.info), serializedState.releaseReturnValue());

    // FIXME: This is not a proper Navigation API initiated traversal, just a simple load for now.
    window()->frame()->loader().load(FrameLoadRequest(*window()->frame(), newURL));

    if (m_upcomingNonTraverseMethodTracker == apiMethodTracker) {
        // FIXME: Once the frameloader properly promotes the upcoming tracker with the navigate event `m_upcomingNonTraverseMethodTracker` should be unset or this will throw.
        m_upcomingNonTraverseMethodTracker = std::nullopt;
    }

    // FIXME: keep track of promises to resolve later.
    Ref entry = NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), newURL);
    Navigation::Result result = { apiMethodTracker.committedPromise.ptr(), apiMethodTracker.finishedPromise.ptr() };
    committed->resolve<IDLInterface<NavigationHistoryEntry>>(entry.get());
    finished->resolve<IDLInterface<NavigationHistoryEntry>>(entry.get());
    return result;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#performing-a-navigation-api-traversal
Navigation::Result Navigation::performTraversal(NavigationHistoryEntry& entry, Ref<DeferredPromise> committed, Ref<DeferredPromise> finished)
{
    // FIXME: This is just a stub that loads a URL for now.
    window()->frame()->loader().load(FrameLoadRequest(*window()->frame(), URL(entry.url())));

    // FIXME: keep track of promises to resolve later.
    auto globalObject = committed->globalObject();
    Navigation::Result result = { DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(committed->promise())), DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(finished->promise())) };
    committed->resolve<IDLInterface<NavigationHistoryEntry>>(entry);
    finished->resolve<IDLInterface<NavigationHistoryEntry>>(entry);
    return result;
}

std::optional<Ref<NavigationHistoryEntry>> Navigation::findEntryByKey(const String& key)
{
    auto entryIndex = m_entries.findIf([&key](const Ref<NavigationHistoryEntry> entry) {
        return entry->key() == key;
    });

    if (entryIndex == notFound)
        return std::nullopt;

    return m_entries[entryIndex];
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-traverseto
Navigation::Result Navigation::traverseTo(const String& key, Options&&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    auto current = currentEntry();
    if (current && current->key() == key) {
        auto globalObject = committed->globalObject();
        Navigation::Result result = { DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(committed->promise())), DOMPromise::create(*globalObject, *JSC::jsCast<JSC::JSPromise*>(finished->promise())) };
        committed->resolve<IDLInterface<NavigationHistoryEntry>>(*current);
        finished->resolve<IDLInterface<NavigationHistoryEntry>>(*current);
        return result;
    }

    auto entry = findEntryByKey(key);
    if (!entry)
        return createErrorResult(committed, finished, ExceptionCode::InvalidStateError, "Invalid key"_s);

    return performTraversal(*entry, committed, finished);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-back
Navigation::Result Navigation::back(Options&&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoBack())
        return createErrorResult(committed, finished, ExceptionCode::InvalidStateError, "Cannot go back"_s);

    return performTraversal(m_entries[m_currentEntryIndex.value() - 1], committed, finished);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-forward
Navigation::Result Navigation::forward(Options&&, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoForward())
        return createErrorResult(committed, finished, ExceptionCode::InvalidStateError, "Cannot go forward"_s);

    return performTraversal(m_entries[m_currentEntryIndex.value() + 1], committed, finished);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-updatecurrententry
ExceptionOr<void> Navigation::updateCurrentEntry(UpdateCurrentEntryOptions&& options)
{
    if (!window()->frame() || !window()->frame()->document())
        return Exception { ExceptionCode::InvalidStateError };

    RefPtr current = currentEntry();
    if (!current)
        return Exception { ExceptionCode::InvalidStateError };

    auto serializedState = SerializedScriptValue::create(*protectedScriptExecutionContext()->globalObject(), options.state, SerializationForStorage::Yes, SerializationErrorMode::Throwing);
    if (!serializedState)
        return { };

    current->setState(WTFMove(serializedState));

    auto currentEntryChangeEvent = NavigationCurrentEntryChangeEvent::create(eventNames().currententrychangeEvent, {
        { false, false, false },
        std::nullopt,
        current
    });
    dispatchEvent(currentEntryChangeEvent);

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#has-entries-and-events-disabled
bool Navigation::hasEntriesAndEventsDisabled() const
{
    if (!window()->document() || !window()->document()->isFullyActive())
        return true;
    if (window()->securityOrigin() && window()->securityOrigin()->isOpaque())
        return true;
    return false;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-a-same-document-navigation
void Navigation::updateForNavigation(Ref<HistoryItem>&& item, NavigationNavigationType navigationType)
{
    if (hasEntriesAndEventsDisabled())
        return;

    RefPtr oldCurrentEntry = currentEntry();
    ASSERT(oldCurrentEntry);

    Vector<Ref<NavigationHistoryEntry>> disposedEntries;

    // FIXME: handle NavigationNavigationType::Traverse
    if (navigationType == NavigationNavigationType::Push) {
        m_currentEntryIndex = *m_currentEntryIndex + 1;
        for (size_t i = *m_currentEntryIndex; i < m_entries.size(); i++)
            disposedEntries.append(m_entries[i]);
        m_entries.resize(*m_currentEntryIndex + 1);
    } else if (navigationType == NavigationNavigationType::Replace)
        disposedEntries.append(*oldCurrentEntry);

    if (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace)
        m_entries[*m_currentEntryIndex] = NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), item);

    // FIXME: implement Step 8 Handle API method tracker.

    auto currentEntryChangeEvent = NavigationCurrentEntryChangeEvent::create(eventNames().currententrychangeEvent, {
        { false, false, false }, navigationType, oldCurrentEntry
    });
    dispatchEvent(currentEntryChangeEvent);

    for (auto& disposedEntry : disposedEntries)
        disposedEntry->dispatchEvent(Event::create(eventNames().disposeEvent, { }));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#can-have-its-url-rewritten
static bool documentCanHaveURLRewritten(const RefPtr<Document>& document, const URL& targetURL)
{
    const URL& documentURL = document->url();
    auto& documentOrigin = document->securityOrigin();
    auto targetOrigin = SecurityOrigin::create(targetURL);

    if (!documentOrigin.isSameSiteAs(targetOrigin))
        return false;

    if (targetURL.protocolIsInHTTPFamily())
        return true;

    if (targetURL.protocolIsFile() && !isEqualIgnoringQueryAndFragments(documentURL, targetURL))
        return false;

    return equalIgnoringFragmentIdentifier(documentURL, targetURL);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#inner-navigate-event-firing-algorithm
bool Navigation::innerDispatchNavigateEvent(NavigationNavigationType navigationType, Ref<NavigationDestination>&& destination, const String& downloadRequestFilename)
{
    // FIXME: pass in formDataEntryList

    if (hasEntriesAndEventsDisabled()) {
        ASSERT(!m_ongoingAPIMethodTracker);
        ASSERT(!m_upcomingNonTraverseMethodTracker);
        ASSERT(m_upcomingTraverseMethodTrackers.isEmpty());
        return true;
    }

    // FIXME: promoteUpcomingAPIMethodTracker(destination->key());

    RefPtr document = window()->protectedDocument();

    auto apiMethodTracker = m_ongoingAPIMethodTracker;
    bool isSameDocument = destination->sameDocument();
    bool isTraversal = navigationType == NavigationNavigationType::Traverse;
    bool canIntercept = documentCanHaveURLRewritten(document, destination->url()) && (!isTraversal || isSameDocument);
    bool canBeCanceled = !isTraversal || (document->isTopDocument() && isSameDocument); // FIXME: and either userInvolvement is not "browser UI", or navigation's relevant global object has transient activation.
    bool hashChange = equalIgnoringFragmentIdentifier(document->url(), destination->url()) && !equalRespectingNullity(document->url().fragmentIdentifier(),  destination->url().fragmentIdentifier());
    auto info = apiMethodTracker ? apiMethodTracker->info : JSC::jsUndefined();

    RefPtr abortController = AbortController::create(*scriptExecutionContext());

    auto init = NavigateEvent::Init {
        { false, canBeCanceled, false },
        navigationType,
        destination.ptr(),
        abortController->protectedSignal(),
        nullptr, // FIXME: formData
        downloadRequestFilename,
        info,
        canIntercept,
        false, // FIXME: userInitiated
        hashChange,
        document->page() && document->page()->isInSwipeAnimation(),
    };

    // Free up no longer needed info.
    if (apiMethodTracker)
        apiMethodTracker->info = JSC::jsUndefined();

    Ref event = NavigateEvent::create(eventNames().navigateEvent, init, abortController.get());
    m_ongoingNavigateEvent = event.ptr();
    m_focusChangedDuringOnoingNavigation = false;
    m_suppressNormalScrollRestorationDuringOngoingNavigation = false;

    dispatchEvent(event);

    if (event->defaultPrevented()) {
        // FIXME: If navigationType is "traverse", then consume history-action user activation.
        // FIXME: If event's abort controller's signal is not aborted, then abort the ongoing navigation given navigation.
        m_ongoingNavigateEvent = nullptr;
        return false;
    }

    bool endResultIsSameDocument = event->wasIntercepted() || destination->sameDocument();

    // FIXME: Prepare to run script given navigation's relevant settings object.

    // Step 32:
    if (event->wasIntercepted()) {
        event->setInterceptionState(InterceptionState::Committed);

        RefPtr fromNavigationHistoryEntry = currentEntry();
        ASSERT(fromNavigationHistoryEntry);

        // FIXME: Create finished promise
        m_transition = NavigationTransition::create(navigationType, *fromNavigationHistoryEntry);

        if (navigationType == NavigationNavigationType::Traverse)
            m_suppressNormalScrollRestorationDuringOngoingNavigation = true;

        // FIXME: Step 7: URL and history update
    }

    if (endResultIsSameDocument) {
        // FIXME: Step 33: Wait on Handler promises

        if (document->isFullyActive() && !abortController->signal().aborted()) {
            ASSERT(m_ongoingNavigateEvent == event.ptr());
            m_ongoingNavigateEvent = nullptr;

            event->finish();

            // FIXME: 6. Fire an event named navigatesuccess at navigation.
            // FIXME: 7. If navigation's transition is not null, then resolve navigation's transition's finished promise with undefined.
            m_transition = nullptr;

            // FIXME: if (apiMethodTracker)
            //             resolveFinishedPromise(*apiMethodTracker);
        }

        // FIXME: and the following failure steps given reason rejectionReason:
        m_ongoingNavigateEvent = nullptr;
    }

    // FIXME: if (apiMethodTracker)
    //            cleanupAPIMethodTracker(*apiMethodTracker);

    // FIXME: Step 35 Clean up after running script

    return !event->wasIntercepted();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-traverse-navigate-event
bool Navigation::dispatchTraversalNavigateEvent(Ref<HistoryItem> historyItem)
{
    // FIME: isCurrentDocument may not match spec
    bool isSameDocument = historyItem->isCurrentDocument(*window()->protectedDocument());
    // FIXME: Set destinations state
    // FIXME: Get Entry for historyItem
    Ref destination = NavigationDestination::create(historyItem->url(), currentEntry(), isSameDocument);

    return innerDispatchNavigateEvent(NavigationNavigationType::Traverse, WTFMove(destination), { });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-push/replace/reload-navigate-event
bool Navigation::dispatchPushReplaceReloadNavigateEvent(const URL& url, NavigationNavigationType navigationType, bool isSameDocument)
{
    // FIXME: Set event's classic history API state to classicHistoryAPIState.
    Ref destination = NavigationDestination::create(url, nullptr, isSameDocument);
    return innerDispatchNavigateEvent(navigationType, WTFMove(destination), { });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-download-request-navigate-event
bool Navigation::dispatchDownloadNavigateEvent(const URL&, const String& downloadFilename)
{
    // FIXME
    UNUSED_PARAM(downloadFilename);
    return false;
}

} // namespace WebCore
