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
#include "BackForwardController.h"
#include "CallbackResult.h"
#include "CommonVM.h"
#include "DOMFormData.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "Exception.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromise.h"
#include "JSNavigationHistoryEntry.h"
#include "MessagePort.h"
#include "NavigateEvent.h"
#include "NavigationActivation.h"
#include "NavigationCurrentEntryChangeEvent.h"
#include "NavigationDestination.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"
#include "NavigationScheduler.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "ShouldTreatAsContinuingLoad.h"
#include "UserGestureIndicator.h"
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Navigation);

class Navigation::NavigationHistoryEntryWrapper {
public:
    NavigationHistoryEntryWrapper() = default;

    NavigationHistoryEntryWrapper(Ref<NavigationHistoryEntry>&& entry)
        : m_entry(WTFMove(entry))
        , m_pendingActivity(m_entry->makePendingActivity(*m_entry))
    { }

    NavigationHistoryEntry* operator->() const { return m_entry.get(); }
    NavigationHistoryEntry* get() const { return m_entry.get(); }
    NavigationHistoryEntry& operator*() const { ASSERT(m_entry); return *m_entry; }

private:
    RefPtr<NavigationHistoryEntry> m_entry;
    RefPtr<ActiveDOMObject::PendingActivity<NavigationHistoryEntry>> m_pendingActivity;
};

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

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#getting-the-navigation-api-entry-index
std::optional<size_t> Navigation::getEntryIndexOfHistoryItem(const Vector<NavigationHistoryEntryWrapper>& entries, const HistoryItem& item, size_t start)
{
    // FIXME: We could have a more efficient solution than iterating through a list.
    for (size_t index = start; index < entries.size(); index++) {
        if (entries[index]->associatedHistoryItem().itemSequenceNumber() == item.itemSequenceNumber())
            return index;
    }

    return std::nullopt;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#initialize-the-navigation-api-entries-for-a-new-document
void Navigation::initializeForNewWindow(std::optional<NavigationNavigationType> navigationType, LocalDOMWindow* previousWindow)
{
    ASSERT(m_entries.isEmpty());
    ASSERT(!m_currentEntryIndex);

    if (hasEntriesAndEventsDisabled())
        return;

    RefPtr page = frame()->page();
    if (!page)
        return;

    RefPtr currentItem = frame()->history().currentItem();
    if (!currentItem)
        return;
    // For main frames we can still rely on the page b/f list. However for subframes we need below logic to not lose the bookkeeping done in the previous window.
    if (previousWindow && !frame()->isMainFrame()) {
        Ref previousNavigation = previousWindow->protectedNavigation();
        bool shouldProcessPreviousNavigationEntries = [&]() {
            if (!previousNavigation->m_entries.size())
                return false;
            if (!frame()->document()->protectedSecurityOrigin()->isSameOriginAs(previousWindow->document()->protectedSecurityOrigin()))
                return false;
            return true;
        }();
        if (shouldProcessPreviousNavigationEntries) {
            for (auto& entry : previousNavigation->m_entries)
                m_entries.append(NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), *entry));

            RELEASE_ASSERT(m_entries.size() > previousNavigation->m_currentEntryIndex);

            // FIXME: This should handle Push, however somewhere before here the currentEntry of previousNavigation was updated
            // to the new navigation so we get duplicate entries.
            if (navigationType != NavigationNavigationType::Push) {
                if (navigationType == NavigationNavigationType::Traverse) {
                    m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, *currentItem);
                    if (m_currentEntryIndex) {
                        updateForActivation(frame()->history().previousItem(), navigationType);
                        return;
                    }
                    // We are doing a cross document subframe traversal, we can't rely on previous window, so clear
                    // m_entries and fall back to the normal algorithm for new windows.
                    m_entries = { };
                } else {
                    auto previousEntry = m_entries[*previousNavigation->m_currentEntryIndex];

                    if (navigationType == NavigationNavigationType::Replace)
                        m_entries[*previousNavigation->m_currentEntryIndex] = NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), *currentItem);

                    m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, *currentItem);

                    if (navigationType) // Unset in the case of forms/POST requests.
                        m_activation = NavigationActivation::create(*navigationType, *currentEntry(), previousEntry.get());

                    return;
                }
            }
        }
    }

    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#getting-session-history-entries-for-the-navigation-api
    Vector<Ref<HistoryItem>> items;
    auto rawEntries = page->checkedBackForward()->allItems();
    auto startingIndex = rawEntries.find(*currentItem);
    if (startingIndex != notFound) {
        Ref startingOrigin = SecurityOrigin::create(rawEntries[startingIndex]->url());

        for (size_t i = 0; i < startingIndex; i++) {
            Ref item = rawEntries[i];

            if (!SecurityOrigin::create(item->url())->isSameOriginAs(startingOrigin))
                break;
            items.append(WTFMove(item));
        }

        items.append(*currentItem);

        for (size_t i = startingIndex + 1; i < rawEntries.size(); i++) {
            Ref item = rawEntries[i];
            if (!SecurityOrigin::create(item->url())->isSameOriginAs(startingOrigin))
                break;
            items.append(WTFMove(item));
        }
    } else
        items.append(*currentItem);

    size_t start = m_entries.size();

    for (Ref item : items)
        m_entries.append(NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), WTFMove(item)));

    m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, *currentItem, start);
    updateForActivation(frame()->history().previousItem(), navigationType);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-activation
void Navigation::updateForActivation(HistoryItem* previousItem, std::optional<NavigationNavigationType> type)
{
    ASSERT(!m_activation);
    if (hasEntriesAndEventsDisabled() || !type)
        return;

    ASSERT(m_currentEntryIndex);
    if (currentEntry()->associatedHistoryItem().url().isAboutBlank())
        return;

    bool wasAboutBlank = previousItem && previousItem->url().isAboutBlank(); // FIXME: *Initial* about:blank
    if (wasAboutBlank) // FIXME: For navigations on the initial about blank this should already be the type.
        type = NavigationNavigationType::Replace;

    bool isSameOrigin = frame()->document() && previousItem && SecurityOrigin::create(previousItem->url())->isSameOriginAs(frame()->document()->securityOrigin());
    auto previousEntryIndex = previousItem ? getEntryIndexOfHistoryItem(m_entries, *previousItem) : std::nullopt;

    RefPtr<NavigationHistoryEntry> previousEntry;
    if (previousEntryIndex && isSameOrigin)
        previousEntry = m_entries.at(previousEntryIndex.value()).get();
    if (type == NavigationNavigationType::Reload)
        previousEntry = currentEntry();
    else if (type == NavigationNavigationType::Replace && (isSameOrigin || wasAboutBlank))
        previousEntry = NavigationHistoryEntry::create(scriptExecutionContext(), *previousItem);

    m_activation = NavigationActivation::create(*type, *currentEntry(), WTFMove(previousEntry));
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#fire-the-pageswap-event
RefPtr<NavigationActivation> Navigation::createForPageswapEvent(HistoryItem* newItem, DocumentLoader* documentLoader, bool fromBackForwardCache)
{
    auto type = documentLoader->triggeringAction().navigationAPIType();
    if (!type || !frame())
        return nullptr;

    // Skip cross-origin requests, or if any cross-origin redirects have been made.
    bool isSameOrigin = SecurityOrigin::create(documentLoader->documentURL())->isSameOriginAs(window()->protectedDocument()->securityOrigin());
    if (!isSameOrigin || (!documentLoader->request().isSameSite() && !fromBackForwardCache))
        return nullptr;

    RefPtr<NavigationHistoryEntry> oldEntry;
    if (frame()->document() && frame()->document()->settings().navigationAPIEnabled())
        oldEntry = currentEntry();
    else if (RefPtr currentItem = frame()->checkedHistory()->currentItem())
        oldEntry = NavigationHistoryEntry::create(scriptExecutionContext(), *currentItem);

    RefPtr<NavigationHistoryEntry> newEntry;
    if (*type == NavigationNavigationType::Reload) {
        newEntry = oldEntry;
    } else if (*type == NavigationNavigationType::Traverse) {
        ASSERT(newItem);
        // FIXME: For a traverse navigation, we should be identifying the right existing history
        // entry for 'newEntry' instead of allocating a new one.
        if (newItem)
            newEntry = NavigationHistoryEntry::create(scriptExecutionContext(), *newItem);
    } else {
        ASSERT(newItem);
        if (newItem)
            newEntry = NavigationHistoryEntry::create(scriptExecutionContext(), *newItem);
    }

    if (newEntry)
        return NavigationActivation::create(*type, newEntry.releaseNonNull(), WTFMove(oldEntry));
    return nullptr;
}

Vector<Ref<NavigationHistoryEntry>> Navigation::entries() const
{
    if (hasEntriesAndEventsDisabled())
        return { };
    return WTF::map(m_entries, [](auto& entryWrapper) {
        return Ref { *entryWrapper };
    });
}

NavigationHistoryEntry* Navigation::currentEntry() const
{
    if (!hasEntriesAndEventsDisabled() && m_currentEntryIndex)
        return m_entries.at(*m_currentEntryIndex).get();
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

static RefPtr<DOMPromise> createDOMPromise(const DeferredPromise& deferredPromise)
{
    Locker<JSC::JSLock> locker(commonVM().apiLock());

    auto promiseValue = deferredPromise.promise();
    auto& jsPromise = *JSC::jsCast<JSC::JSPromise*>(promiseValue);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(jsPromise.globalObject());

    return DOMPromise::create(globalObject, jsPromise);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-early-error-result
static Navigation::Result createErrorResult(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, Exception&& exception)
{
    Navigation::Result result = {
        createDOMPromise(committed),
        createDOMPromise(finished)
    };

    JSC::JSValue exceptionObject;
    committed->reject(exception, RejectAsHandled::No, exceptionObject);
    finished->reject(exception, RejectAsHandled::No, exceptionObject);

    return result;
}

static Navigation::Result createErrorResult(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, ExceptionCode exceptionCode, const String& errorMessage)
{
    return createErrorResult(WTFMove(committed), WTFMove(finished), Exception { exceptionCode, errorMessage });
}

ExceptionOr<RefPtr<SerializedScriptValue>> Navigation::serializeState(JSC::JSValue state)
{
    if (state.isUndefined())
        return { nullptr };

    if (!frame())
        return Exception(ExceptionCode::DataCloneError, "Cannot serialize state: Detached frame"_s);

    Vector<Ref<MessagePort>> dummyPorts;
    auto serializeResult = SerializedScriptValue::create(*protectedScriptExecutionContext()->globalObject(), state, { }, dummyPorts, SerializationForStorage::Yes);
    if (serializeResult.hasException())
        return serializeResult.releaseException();

    return { serializeResult.releaseReturnValue().ptr() };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#maybe-set-the-upcoming-non-traverse-api-method-tracker
RefPtr<NavigationAPIMethodTracker> Navigation::maybeSetUpcomingNonTraversalTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, JSC::JSValue info, RefPtr<SerializedScriptValue>&& serializedState)
{
    RefPtr apiMethodTracker = NavigationAPIMethodTracker::create(WTFMove(committed), WTFMove(finished), WTFMove(info), WTFMove(serializedState));

    apiMethodTracker->finishedPromise->markAsHandled();

    // FIXME: We should be able to assert m_upcomingNonTraverseMethodTracker is empty.
    if (!hasEntriesAndEventsDisabled())
        m_upcomingNonTraverseMethodTracker = apiMethodTracker;

    return apiMethodTracker;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#add-an-upcoming-traverse-api-method-tracker
RefPtr<NavigationAPIMethodTracker> Navigation::addUpcomingTrarveseAPIMethodTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, const String& key, JSC::JSValue info)
{
    RefPtr apiMethodTracker = NavigationAPIMethodTracker::create(WTFMove(committed), WTFMove(finished), WTFMove(info), nullptr);
    apiMethodTracker->key = key;

    apiMethodTracker->finishedPromise->markAsHandled();

    m_upcomingTraverseMethodTrackers.add(key, *apiMethodTracker);

    return apiMethodTracker;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker-derived-result
Navigation::Result Navigation::apiMethodTrackerDerivedResult(const NavigationAPIMethodTracker& apiMethodTracker)
{
    return {
        createDOMPromise(apiMethodTracker.committedPromise),
        createDOMPromise(apiMethodTracker.finishedPromise),
    };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-reload
Navigation::Result Navigation::reload(ReloadOptions&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    auto serializedState = serializeState(options.state);
    if (serializedState.hasException())
        return createErrorResult(WTFMove(committed), WTFMove(finished), serializedState.releaseException());
    auto state = serializedState.releaseReturnValue();
    if (!state && currentEntry())
        state = currentEntry()->associatedHistoryItem().navigationAPIStateObject();

    if (!window()->protectedDocument()->isFullyActive() || window()->document()->unloadCounter())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    RefPtr apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTFMove(committed), WTFMove(finished), WTFMove(options.info), WTFMove(state));

    RefPtr lexicalFrame = lexicalFrameFromCommonVM();
    auto initiatedByMainFrame = lexicalFrame && lexicalFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;
    ResourceRequest resourceRequest { frame()->protectedDocument()->url(), frame()->protectedLoader()->outgoingReferrer(), ResourceRequestCachePolicy::ReloadIgnoringCacheData };
    FrameLoadRequest frameLoadRequest { *frame()->document(), frame()->protectedDocument()->securityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame };
    frameLoadRequest.setLockHistory(LockHistory::Yes);
    frameLoadRequest.setLockBackForwardList(LockBackForwardList::Yes);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(frame()->protectedDocument()->shouldOpenExternalURLsPolicyToPropagate());

    frame()->protectedLoader()->changeLocation(WTFMove(frameLoadRequest));

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-navigate
Navigation::Result Navigation::navigate(const String& url, NavigateOptions&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    auto newURL = window()->document()->completeURL(url, ScriptExecutionContext::ForceUTF8::Yes);
    const URL& currentURL = scriptExecutionContext()->url();

    if (!newURL.isValid())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::SyntaxError, "Invalid URL"_s);

    if (options.history == HistoryBehavior::Push && newURL.protocolIsJavaScript())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::NotSupportedError, "A \"push\" navigation was explicitly requested, but only a \"replace\" navigation is possible when navigating to a javascript: URL."_s);

    if (options.history == HistoryBehavior::Push && currentURL.isAboutBlank())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::NotSupportedError, "A \"push\" navigation was explicitly requested, but only a \"replace\" navigation is possible while on an about:blank document."_s);

    auto serializedState = serializeState(options.state);
    if (serializedState.hasException())
        return createErrorResult(WTFMove(committed), WTFMove(finished), serializedState.releaseException());

    if (!window()->protectedDocument()->isFullyActive() || window()->document()->unloadCounter())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    RefPtr apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTFMove(committed), WTFMove(finished), WTFMove(options.info), serializedState.releaseReturnValue());

    auto request = FrameLoadRequest(*frame(), newURL);
    request.setNavigationHistoryBehavior(options.history);
    request.setIsFromNavigationAPI(true);
    frame()->loader().loadFrameRequest(WTFMove(request), nullptr, { });

    // If the load() call never made it to the point that NavigateEvent was emitted, thus promoteUpcomingAPIMethodTracker() called, this will be true.
    if (m_upcomingNonTraverseMethodTracker == apiMethodTracker) {
        m_upcomingNonTraverseMethodTracker = nullptr;
        return createErrorResult(WTFMove(apiMethodTracker->committedPromise), WTFMove(apiMethodTracker->finishedPromise), ExceptionCode::AbortError, "Navigation aborted"_s);
    }

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#performing-a-navigation-api-traversal
Navigation::Result Navigation::performTraversal(const String& key, Navigation::Options options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!window()->protectedDocument()->isFullyActive() || window()->document()->unloadCounter())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    if (!findEntryByKey(key))
        createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::AbortError, "Navigation aborted"_s);

    if (!frame()->isMainFrame() && !window()->protectedDocument()->canNavigate(&frame()->page()->mainFrame()))
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::SecurityError, "Invalid state"_s);

    RefPtr current = currentEntry();
    if (current->key() == key) {
        committed->resolve<IDLInterface<NavigationHistoryEntry>>(*current.get());
        finished->resolve<IDLInterface<NavigationHistoryEntry>>(*current.get());
        return { createDOMPromise(committed), createDOMPromise(finished) };
    }

    if (auto existingMethodTracker = m_upcomingTraverseMethodTrackers.getOptional(key))
        return apiMethodTrackerDerivedResult(*existingMethodTracker);

    RefPtr apiMethodTracker = addUpcomingTrarveseAPIMethodTracker(WTFMove(committed), WTFMove(finished), key, options.info);

    // FIXME: 11. Let sourceSnapshotParams be the result of snapshotting source snapshot params given document.
    frame()->protectedNavigationScheduler()->scheduleHistoryNavigationByKey(key, [apiMethodTracker] (ScheduleHistoryNavigationResult result) {
        if (result == ScheduleHistoryNavigationResult::Aborted)
            createErrorResult(WTFMove(apiMethodTracker->committedPromise), WTFMove(apiMethodTracker->finishedPromise), ExceptionCode::AbortError, "Navigation aborted"_s);
    });

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

RefPtr<NavigationHistoryEntry> Navigation::findEntryByKey(const String& key)
{
    auto entryIndex = m_entries.findIf([&key](auto& entry) {
        return entry->key() == key;
    });

    if (entryIndex == notFound)
        return nullptr;

    return m_entries[entryIndex].get();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-traverseto
Navigation::Result Navigation::traverseTo(const String& key, Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!findEntryByKey(key))
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Invalid key"_s);

    return performTraversal(key, options, WTFMove(committed), WTFMove(finished));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-back
Navigation::Result Navigation::back(Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoBack())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Cannot go back"_s);

    auto previousEntry = m_entries[m_currentEntryIndex.value() - 1];

    return performTraversal(previousEntry->key(), options, WTFMove(committed), WTFMove(finished));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-forward
Navigation::Result Navigation::forward(Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoForward())
        return createErrorResult(WTFMove(committed), WTFMove(finished), ExceptionCode::InvalidStateError, "Cannot go forward"_s);

    auto nextEntry = m_entries[m_currentEntryIndex.value() + 1];

    return performTraversal(nextEntry->key(), options, WTFMove(committed), WTFMove(finished));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-updatecurrententry
ExceptionOr<void> Navigation::updateCurrentEntry(UpdateCurrentEntryOptions&& options)
{
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
    if (window()->document()->loader() && window()->document()->loader()->isInitialAboutBlank())
        return true;
    if (window()->securityOrigin() && window()->securityOrigin()->isOpaque())
        return true;
    return false;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#resolve-the-finished-promise
void Navigation::resolveFinishedPromise(NavigationAPIMethodTracker* apiMethodTracker)
{
    if (!apiMethodTracker->committedToEntry) {
        apiMethodTracker->finishedBeforeCommit = true;
        return;
    }

    apiMethodTracker->committedPromise->resolve<IDLInterface<NavigationHistoryEntry>>(*apiMethodTracker->committedToEntry);
    apiMethodTracker->finishedPromise->resolve<IDLInterface<NavigationHistoryEntry>>(*apiMethodTracker->committedToEntry);
    cleanupAPIMethodTracker(apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#reject-the-finished-promise
void Navigation::rejectFinishedPromise(NavigationAPIMethodTracker* apiMethodTracker, const Exception& exception, JSC::JSValue exceptionObject)
{
    // finished is already marked as handled at this point so don't overwrite that.
    apiMethodTracker->finishedPromise->reject(exception, RejectAsHandled::Yes, exceptionObject);
    apiMethodTracker->committedPromise->reject(exception, RejectAsHandled::No, exceptionObject);
    cleanupAPIMethodTracker(apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#notify-about-the-committed-to-entry
void Navigation::notifyCommittedToEntry(NavigationAPIMethodTracker* apiMethodTracker, NavigationHistoryEntry* entry, NavigationNavigationType navigationType)
{
    ASSERT(entry);
    apiMethodTracker->committedToEntry = entry;
    if (navigationType != NavigationNavigationType::Traverse) {
        if (apiMethodTracker->serializedState)
            apiMethodTracker->committedToEntry->setState(WTFMove(apiMethodTracker->serializedState));
    }

    if (apiMethodTracker->finishedBeforeCommit)
        resolveFinishedPromise(apiMethodTracker);
    else
        apiMethodTracker->committedPromise->resolve<IDLInterface<NavigationHistoryEntry>>(*entry);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-a-same-document-navigation
void Navigation::updateForNavigation(Ref<HistoryItem>&& item, NavigationNavigationType navigationType)
{
    if (hasEntriesAndEventsDisabled())
        return;

    RefPtr oldCurrentEntry = currentEntry();
    if (!oldCurrentEntry)
        return;

    Vector<NavigationHistoryEntryWrapper> disposedEntries;

    if (navigationType == NavigationNavigationType::Traverse) {
        m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, item);
        if (!m_currentEntryIndex)
            return;
    } else if (navigationType == NavigationNavigationType::Push) {
        m_currentEntryIndex = *m_currentEntryIndex + 1;
        for (size_t i = *m_currentEntryIndex; i < m_entries.size(); i++)
            disposedEntries.append(m_entries[i]);
        m_entries.resize(*m_currentEntryIndex + 1);
    } else if (navigationType == NavigationNavigationType::Replace)
        disposedEntries.append(*oldCurrentEntry);

    if (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace)
        m_entries[*m_currentEntryIndex] = NavigationHistoryEntry::create(protectedScriptExecutionContext().get(), WTFMove(item));

    if (m_ongoingAPIMethodTracker)
        notifyCommittedToEntry(m_ongoingAPIMethodTracker.get(), currentEntry(), navigationType);

    auto currentEntryChangeEvent = NavigationCurrentEntryChangeEvent::create(eventNames().currententrychangeEvent, {
        { false, false, false }, navigationType, oldCurrentEntry
    });
    dispatchEvent(currentEntryChangeEvent);

    for (auto& disposedEntry : disposedEntries)
        disposedEntry->dispatchEvent(Event::create(eventNames().disposeEvent, { }));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-reactivation
void Navigation::updateForReactivation(Vector<Ref<HistoryItem>>& newHistoryItems, HistoryItem& reactivatedItem)
{
    if (hasEntriesAndEventsDisabled())
        return;

    Vector<NavigationHistoryEntryWrapper> newEntries;
    Vector<NavigationHistoryEntryWrapper> oldEntries = std::exchange(m_entries, { });

    for (Ref item : newHistoryItems) {
        RefPtr<NavigationHistoryEntry> newEntry;

        for (size_t entryIndex = 0; entryIndex < oldEntries.size(); entryIndex++) {
            auto& entry = oldEntries.at(entryIndex);
            if (entry->associatedHistoryItem() == item) {
                newEntry = entry.get();
                oldEntries.remove(entryIndex);
                break;
            }
        }

        if (!newEntry)
            newEntry = NavigationHistoryEntry::create(scriptExecutionContext(), WTFMove(item));

        newEntries.append(*newEntry);
    }

    m_entries = WTFMove(newEntries);
    m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, reactivatedItem);

    for (auto& disposedEntry : oldEntries)
        disposedEntry->dispatchEvent(Event::create(eventNames().disposeEvent, { }));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#can-have-its-url-rewritten
static bool documentCanHaveURLRewritten(const Document& document, const URL& targetURL)
{
    const URL& documentURL = document.url();
    auto& documentOrigin = document.securityOrigin();
    auto targetOrigin = SecurityOrigin::create(targetURL);

    if (!documentOrigin.isSameSiteAs(targetOrigin))
        return false;

    if (targetURL.protocolIsInHTTPFamily())
        return true;

    if (targetURL.protocolIsFile() && !isEqualIgnoringQueryAndFragments(documentURL, targetURL))
        return false;

    return equalIgnoringFragmentIdentifier(documentURL, targetURL);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#promote-an-upcoming-api-method-tracker-to-ongoing
void Navigation::promoteUpcomingAPIMethodTracker(const String& destinationKey)
{
    // FIXME: We should be able to assert m_ongoingAPIMethodTracker is unset.

    if (!destinationKey.isNull())
        m_ongoingAPIMethodTracker = m_upcomingTraverseMethodTrackers.take(destinationKey);
    else {
        m_ongoingAPIMethodTracker = WTFMove(m_upcomingNonTraverseMethodTracker);
        m_upcomingNonTraverseMethodTracker = nullptr;
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker-clean-up
void Navigation::cleanupAPIMethodTracker(NavigationAPIMethodTracker* apiMethodTracker)
{
    if (m_ongoingAPIMethodTracker == apiMethodTracker)
        m_ongoingAPIMethodTracker = nullptr;
    else {
        auto& key = apiMethodTracker->key;
        // FIXME: We should be able to assert key isn't null and m_upcomingTraverseMethodTrackers contains it.
        if (!key.isNull())
            m_upcomingTraverseMethodTrackers.remove(key);
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#abort-the-ongoing-navigation
void Navigation::abortOngoingNavigation(NavigateEvent& event)
{
    auto* globalObject = scriptExecutionContext()->globalObject();
    if (!globalObject && m_ongoingAPIMethodTracker)
        globalObject = m_ongoingAPIMethodTracker->committedPromise->globalObject();
    if (!globalObject)
        return;

    m_focusChangedDuringOngoingNavigation = FocusDidChange::No;
    m_suppressNormalScrollRestorationDuringOngoingNavigation = false;

    if (event.isBeingDispatched())
        event.preventDefault();

    JSC::JSLockHolder locker(globalObject->vm());
    auto exception = Exception(ExceptionCode::AbortError, "Navigation aborted"_s);
    auto domException = createDOMException(*globalObject, exception.isolatedCopy());

    auto error = JSC::createError(globalObject, "Navigation aborted"_s);

    ErrorInformation errorInformation;
    if (auto* errorInstance = jsDynamicCast<JSC::ErrorInstance*>(error)) {
        if (auto result = extractErrorInformationFromErrorInstance(globalObject, *errorInstance))
            errorInformation = WTFMove(*result);
        // Default to document url if extractErrorInformationFromErrorInstance was not able to determine sourceURL.
        if (errorInformation.sourceURL.isEmpty())
            errorInformation.sourceURL = scriptExecutionContext()->url().string();
    }

    if (RefPtr signal = event.signal())
        signal->signalAbort(domException);

    m_ongoingNavigateEvent = nullptr;

    dispatchEvent(ErrorEvent::create(eventNames().navigateerrorEvent, exception.message(), errorInformation.sourceURL, errorInformation.line, errorInformation.column, { globalObject->vm(), domException }));

    if (m_ongoingAPIMethodTracker)
        rejectFinishedPromise(m_ongoingAPIMethodTracker.get(), exception, domException);

    if (m_transition) {
        m_transition->rejectPromise(exception, domException);
        m_transition = nullptr;
    }
}

struct AwaitingPromiseData : public RefCounted<AwaitingPromiseData> {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    Function<void()> fulfilledCallback;
    Function<void(JSC::JSValue)> rejectionCallback;
    size_t remainingPromises = 0;
    bool rejected = false;

    AwaitingPromiseData() = delete;
    AwaitingPromiseData(Function<void()>&& fulfilledCallback, Function<void(JSC::JSValue)>&& rejectionCallback, size_t remainingPromises)
        : fulfilledCallback(WTFMove(fulfilledCallback))
        , rejectionCallback(WTFMove(rejectionCallback))
        , remainingPromises(remainingPromises)
    {
    }
};

// https://webidl.spec.whatwg.org/#wait-for-all
static void waitForAllPromises(const Vector<RefPtr<DOMPromise>>& promises, Function<void()>&& fulfilledCallback, Function<void(JSC::JSValue)>&& rejectionCallback)
{
    Ref awaitingData = adoptRef(*new AwaitingPromiseData(WTFMove(fulfilledCallback), WTFMove(rejectionCallback), promises.size()));

    for (const auto& promise : promises) {
        // At any point between promises the frame could have been detached.
        // FIXME: There is possibly a better way to handle this rather than just never complete.
        if (promise->isSuspended())
            return;

        promise->whenSettled([awaitingData, promise] () mutable {
            if (promise->isSuspended())
                return;

            switch (promise->status()) {
            case DOMPromise::Status::Fulfilled:
                if (--awaitingData->remainingPromises > 0)
                    break;
                awaitingData->fulfilledCallback();
                break;
            case DOMPromise::Status::Rejected:
                if (awaitingData->rejected)
                    break;
                awaitingData->rejected = true;
                awaitingData->rejectionCallback(promise->result());
                break;
            case DOMPromise::Status::Pending:
                ASSERT_NOT_REACHED();
                break;
            }
        });
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#inner-navigate-event-firing-algorithm
Navigation::DispatchResult Navigation::innerDispatchNavigateEvent(NavigationNavigationType navigationType, Ref<NavigationDestination>&& destination, const String& downloadRequestFilename, FormState* formState, SerializedScriptValue* classicHistoryAPIState)
{
    if (hasEntriesAndEventsDisabled()) {
        ASSERT(!m_ongoingAPIMethodTracker);
        ASSERT(!m_upcomingNonTraverseMethodTracker);
        ASSERT(m_upcomingTraverseMethodTrackers.isEmpty());
        return DispatchResult::Completed;
    }

    bool wasBeingDispatched = m_ongoingNavigateEvent ? m_ongoingNavigateEvent->isBeingDispatched() : false;

    abortOngoingNavigationIfNeeded();

    // Prevent recursion on synchronous history navigation steps issued
    // from the navigate event handler.
    if (wasBeingDispatched && classicHistoryAPIState)
        return DispatchResult::Completed;

    promoteUpcomingAPIMethodTracker(destination->key());

    RefPtr document = window()->protectedDocument();

    RefPtr apiMethodTracker = m_ongoingAPIMethodTracker;
    // FIXME: this should not be needed, we should pass it into FrameLoader.
    if (apiMethodTracker && apiMethodTracker->serializedState)
        destination->setStateObject(apiMethodTracker->serializedState.get());
    bool isSameDocument = destination->sameDocument();
    bool isTraversal = navigationType == NavigationNavigationType::Traverse;
    bool canIntercept = documentCanHaveURLRewritten(*document, destination->url()) && (!isTraversal || isSameDocument);
    bool canBeCanceled = !isTraversal || (document->isTopDocument() && isSameDocument); // FIXME: and user involvement is not browser-ui or navigation's relevant global object has transient activation.
    bool hashChange = !classicHistoryAPIState && equalIgnoringFragmentIdentifier(document->url(), destination->url()) && !equalRespectingNullity(document->url().fragmentIdentifier(),  destination->url().fragmentIdentifier());
    auto info = apiMethodTracker ? apiMethodTracker->info : JSC::jsUndefined();

    RefPtr<DOMFormData> formData = nullptr;
    if (formState && (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace)) {
        // FIXME: Set submitter element.
        if (auto domFormData = DOMFormData::create(*scriptExecutionContext(), &formState->form(), nullptr); !domFormData.hasException())
            formData = domFormData.releaseReturnValue();
    }

    RefPtr abortController = AbortController::create(*scriptExecutionContext());

    auto init = NavigateEvent::Init {
        { false, canBeCanceled, false },
        navigationType,
        destination.ptr(),
        abortController->protectedSignal(),
        formData,
        downloadRequestFilename,
        info,
        canIntercept,
        UserGestureIndicator::processingUserGesture(document.get()),
        hashChange,
        document->page() && document->page()->isInSwipeAnimation(),
    };

    // Free up no longer needed info.
    if (apiMethodTracker)
        apiMethodTracker->info = JSC::jsUndefined();

    Ref event = NavigateEvent::create(eventNames().navigateEvent, init, abortController.get());
    m_ongoingNavigateEvent = event.ptr();
    m_focusChangedDuringOngoingNavigation = FocusDidChange::No;
    m_suppressNormalScrollRestorationDuringOngoingNavigation = false;

    dispatchEvent(event);

    // If the frame was detached in our event.
    if (!frame()) {
        abortOngoingNavigation(event);
        return DispatchResult::Aborted;
    }

    if (event->defaultPrevented()) {
        // FIXME: If navigationType is "traverse", then consume history-action user activation.
        if (!event->signal()->aborted())
            abortOngoingNavigation(event);
        return DispatchResult::Aborted;
    }

    bool endResultIsSameDocument = event->wasIntercepted() || destination->sameDocument();

    // FIXME: Prepare to run script given navigation's relevant settings object.

    // Step 32:
    if (event->wasIntercepted()) {
        event->setInterceptionState(InterceptionState::Committed);

        RefPtr fromNavigationHistoryEntry = currentEntry();
        ASSERT(fromNavigationHistoryEntry);

        {
            auto& domGlobalObject = *jsCast<JSDOMGlobalObject*>(scriptExecutionContext()->globalObject());
            JSC::JSLockHolder locker(domGlobalObject.vm());
            m_transition = NavigationTransition::create(navigationType, *fromNavigationHistoryEntry, DeferredPromise::create(domGlobalObject, DeferredPromise::Mode::RetainPromiseOnResolve).releaseNonNull());
        }

        if (navigationType == NavigationNavigationType::Traverse)
            m_suppressNormalScrollRestorationDuringOngoingNavigation = true;

        if (navigationType == NavigationNavigationType::Reload) {
            // Not in specification but matches chromium implementation and tests.
            updateForNavigation(currentEntry()->associatedHistoryItem(), navigationType);
        } else if (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace) {
            auto historyHandling = navigationType == NavigationNavigationType::Replace ? NavigationHistoryBehavior::Replace : NavigationHistoryBehavior::Push;
            frame()->loader().updateURLAndHistory(destination->url(), classicHistoryAPIState, historyHandling);
        }
    }

    if (endResultIsSameDocument) {
        Vector<RefPtr<DOMPromise>> promiseList;

        for (auto& handler : event->handlers()) {
            auto callbackResult = handler->handleEvent();
            if (callbackResult.type() == CallbackResultType::Success)
                promiseList.append(callbackResult.releaseReturnValue());
            else if (callbackResult.type() == CallbackResultType::ExceptionThrown) {
                // FIXME: We need to keep around the failure reason but the generated handleEvent() catches and consumes it.
                auto promiseAndWrapper = createPromiseAndWrapper(*document);
                promiseAndWrapper.second->reject(ExceptionCode::TypeError);
                promiseList.append(WTFMove(promiseAndWrapper.first));
            }
        }

        if (promiseList.isEmpty()) {
            auto promiseAndWrapper = createPromiseAndWrapper(*document);
            promiseAndWrapper.second->resolveWithCallback([](JSDOMGlobalObject&) {
                return JSC::jsUndefined();
            });
            promiseList.append(WTFMove(promiseAndWrapper.first));
        }

        // FIXME: this emulates the behavior of a Promise wrapped around waitForAll, but we may want the real
        // thing if the ordering-and-transition tests show timing related issues related to this.
        protectedScriptExecutionContext()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, promiseList, abortController, document, apiMethodTracker]() {
            waitForAllPromises(promiseList, [abortController, document, apiMethodTracker, weakThis = WeakPtr { *this }]() mutable {
                if (!weakThis || abortController->signal().aborted() || !document->isFullyActive() || !weakThis->m_ongoingNavigateEvent)
                    return;

                RefPtr strongThis = weakThis.get();

                auto focusChanged = std::exchange(strongThis->m_focusChangedDuringOngoingNavigation, FocusDidChange::No);
                strongThis->m_ongoingNavigateEvent->finish(*document, InterceptionHandlersDidFulfill::Yes, focusChanged);
                strongThis->m_ongoingNavigateEvent = nullptr;

                strongThis->dispatchEvent(Event::create(eventNames().navigatesuccessEvent, { }));

                if (apiMethodTracker)
                    strongThis->resolveFinishedPromise(apiMethodTracker.get());

                if (RefPtr transition = std::exchange(strongThis->m_transition, nullptr))
                    transition->resolvePromise();

                strongThis->m_ongoingNavigateEvent = nullptr;

            }, [abortController, document, apiMethodTracker, weakThis = WeakPtr { *this }](JSC::JSValue result) mutable {
                if (!weakThis || abortController->signal().aborted() || !document->isFullyActive() || !weakThis->m_ongoingNavigateEvent)
                    return;

                RefPtr strongThis = weakThis.get();

                auto focusChanged = std::exchange(strongThis->m_focusChangedDuringOngoingNavigation, FocusDidChange::No);
                strongThis->m_ongoingNavigateEvent->finish(*document, InterceptionHandlersDidFulfill::No, focusChanged);
                strongThis->m_ongoingNavigateEvent = nullptr;

                ErrorInformation errorInformation;
                String errorMessage;
                if (auto* errorInstance = jsDynamicCast<JSC::ErrorInstance*>(result)) {
                    if (auto result = extractErrorInformationFromErrorInstance(strongThis->protectedScriptExecutionContext()->globalObject(), *errorInstance)) {
                        errorInformation = WTFMove(*result);
                        errorMessage = makeString("Uncaught "_s, errorInformation.errorTypeString, ": "_s, errorInformation.message);
                    }
                }
                auto exception = Exception(ExceptionCode::UnknownError, errorMessage);
                auto domException = createDOMException(*strongThis->protectedScriptExecutionContext()->globalObject(), exception.isolatedCopy());

                strongThis->dispatchEvent(ErrorEvent::create(eventNames().navigateerrorEvent, errorMessage, errorInformation.sourceURL, errorInformation.line, errorInformation.column, { strongThis->protectedScriptExecutionContext()->globalObject()->vm(), result }));

                if (apiMethodTracker)
                    apiMethodTracker->finishedPromise->reject<IDLAny>(result, RejectAsHandled::Yes);

                if (RefPtr transition = std::exchange(strongThis->m_transition, nullptr))
                    transition->rejectPromise(exception, domException);
            });
        });

        // If a new event has been dispatched in our event handler then we were aborted above.
        if (m_ongoingNavigateEvent != event.ptr())
            return DispatchResult::Aborted;
    } else if (apiMethodTracker)
        cleanupAPIMethodTracker(apiMethodTracker.get());
    else {
        // FIXME: This situation isn't clear, we've made it through the event doing nothing so
        // to avoid incorrectly being aborted we clear this.
        // To reproduce see `inspector/runtime/execution-context-in-scriptless-page.html`.
        m_ongoingNavigateEvent = nullptr;
    }

    // FIXME: Step 35 Clean up after running script

    return event->wasIntercepted() ? DispatchResult::Intercepted : DispatchResult::Completed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-traverse-navigate-event
Navigation::DispatchResult Navigation::dispatchTraversalNavigateEvent(HistoryItem& historyItem)
{
    auto* currentItem = frame() ? frame()->history().currentItem() : nullptr;
    bool isSameDocument = currentItem && currentItem->documentSequenceNumber() == historyItem.documentSequenceNumber();

    RefPtr<NavigationHistoryEntry> destinationEntry;
    auto index = m_entries.findIf([&historyItem](const auto& entry) {
        return entry->associatedHistoryItem().itemSequenceNumber() == historyItem.itemSequenceNumber();
    });
    if (index != notFound)
        destinationEntry = m_entries[index].get();

    // FIXME: Set destinations state
    Ref destination = NavigationDestination::create(historyItem.url(), WTFMove(destinationEntry), isSameDocument);

    return innerDispatchNavigateEvent(NavigationNavigationType::Traverse, WTFMove(destination), { });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-push/replace/reload-navigate-event
bool Navigation::dispatchPushReplaceReloadNavigateEvent(const URL& url, NavigationNavigationType navigationType, bool isSameDocument, FormState* formState, SerializedScriptValue* classicHistoryAPIState)
{
    Ref destination = NavigationDestination::create(url, nullptr, isSameDocument);
    if (classicHistoryAPIState)
        destination->setStateObject(classicHistoryAPIState);
    return innerDispatchNavigateEvent(navigationType, WTFMove(destination), { }, formState, classicHistoryAPIState) == DispatchResult::Completed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-download-request-navigate-event
bool Navigation::dispatchDownloadNavigateEvent(const URL& url, const String& downloadFilename)
{
    Ref destination = NavigationDestination::create(url, nullptr, false);
    return innerDispatchNavigateEvent(NavigationNavigationType::Push, WTFMove(destination), downloadFilename) == DispatchResult::Completed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#inform-the-navigation-api-about-aborting-navigation
void Navigation::abortOngoingNavigationIfNeeded()
{
    if (!m_ongoingNavigateEvent)
        return;
    abortOngoingNavigation(*m_ongoingNavigateEvent);
}

} // namespace WebCore
