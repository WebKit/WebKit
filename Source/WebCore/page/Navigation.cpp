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
#include "Navigation.h"

#include "AbortController.h"
#include "BackForwardController.h"
#include "CallbackResult.h"
#include "CommonVM.h"
#include "DOMFormData.h"
#include "DocumentEventLoop.h"
#include "DocumentLoader.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "ErrorEvent.h"
#include "EventNames.h"
#include "EventTargetInterfaces.h"
#include "Exception.h"
#include "ExceptionOr.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromise.h"
#include "JSNavigationHistoryEntry.h"
#include "Logging.h"
#include "MessagePort.h"
#include "NavigateEvent.h"
#include "NavigationActivation.h"
#include "NavigationCurrentEntryChangeEvent.h"
#include "NavigationDestination.h"
#include "NavigationHistoryEntry.h"
#include "NavigationNavigationType.h"
#include "NavigationScheduler.h"
#include "ScriptExecutionContextInlines.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "ShouldTreatAsContinuingLoad.h"
#include "UserGestureIndicator.h"
#include <optional>
#include <wtf/Assertions.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Navigation);

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
    if (!m_currentEntryIndex || !*m_currentEntryIndex)
        return false;
    return true;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-cangoforward
bool Navigation::canGoForward() const
{
    if (hasEntriesAndEventsDisabled())
        return false;
    ASSERT(m_currentEntryIndex);
    if (!m_currentEntryIndex || *m_currentEntryIndex == m_entries.size() - 1)
        return false;
    return true;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#getting-the-navigation-api-entry-index
static std::optional<size_t> getEntryIndexOfHistoryItem(const Vector<Ref<NavigationHistoryEntry>>& entries, const HistoryItem& item)
{
    // FIXME: We could have a more efficient solution than iterating through a list.
    for (size_t index = 0; index < entries.size(); index++) {
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

    RefPtr currentItem = frame()->loader().history().currentItem();
    if (!currentItem)
        return;

    if (previousWindow) {
        Ref previousNavigation = previousWindow->navigation();

        bool shouldProcessPreviousNavigationEntries = [&]() {
            if (!previousNavigation->m_currentEntryIndex)
                return false;

            if (!previousNavigation->m_entries.size())
                return false;

            if (!protect(protect(frame()->document())->securityOrigin())->isSameOriginAs(protect(protect(previousWindow->document())->securityOrigin())))
                return false;

            return true;
        }();

        if (shouldProcessPreviousNavigationEntries) {
            for (auto& entry : previousNavigation->m_entries)
                m_entries.append(NavigationHistoryEntry::create(*this, entry.get()));

            RELEASE_ASSERT(m_entries.size() > previousNavigation->m_currentEntryIndex);

            if (navigationType == NavigationNavigationType::Traverse) {
                m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, *currentItem);
                if (m_currentEntryIndex) {
                    setActivation(protect(frame()->loader().history().previousItem()).get(), navigationType);
                    return;
                }
                // We are doing a cross document traversal, we can't rely on previous window, so clear
                // m_entries and fall back to the normal algorithm for new windows.
                m_entries = { };
            } else if (navigationType == NavigationNavigationType::Push)
                m_entries.shrink(*previousNavigation->m_currentEntryIndex + 1); // Prune forward entries.
            else {
                auto previousEntry = m_entries[*previousNavigation->m_currentEntryIndex];

                if (navigationType == NavigationNavigationType::Replace)
                    m_entries[*previousNavigation->m_currentEntryIndex] = NavigationHistoryEntry::create(*this, *currentItem);

                m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, *currentItem);
                if (m_currentEntryIndex) {
                    m_activation = NavigationActivation::create(*navigationType, *currentEntry(), WTF::move(previousEntry));
                    return;
                }
            }
        }
    }

    m_entries.append(NavigationHistoryEntry::create(*this, *currentItem));
    m_currentEntryIndex = m_entries.size() - 1;

    setActivation(protect(frame()->loader().history().previousItem()).get(), navigationType);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-activation
void Navigation::setActivation(HistoryItem* previousItem, std::optional<NavigationNavigationType> type)
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

    bool isSameOrigin = frame()->document() && previousItem && SecurityOrigin::create(previousItem->url())->isSameOriginAs(protect(protect(frame()->document())->securityOrigin()));
    auto previousEntryIndex = previousItem ? getEntryIndexOfHistoryItem(m_entries, *previousItem) : std::nullopt;

    RefPtr<NavigationHistoryEntry> previousEntry = nullptr;
    if (previousEntryIndex && isSameOrigin)
        previousEntry = m_entries.at(previousEntryIndex.value()).ptr();
    if (type == NavigationNavigationType::Reload)
        previousEntry = currentEntry();
    else if (type == NavigationNavigationType::Replace && (isSameOrigin || wasAboutBlank))
        previousEntry = NavigationHistoryEntry::create(*this, *previousItem);

    m_activation = NavigationActivation::create(*type, *currentEntry(), WTF::move(previousEntry));
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#fire-the-pageswap-event
RefPtr<NavigationActivation> Navigation::createForPageswapEvent(HistoryItem* newItem, DocumentLoader* documentLoader, bool fromBackForwardCache)
{
    auto type = documentLoader->triggeringAction().navigationAPIType();
    if (!type || !frame())
        return nullptr;

    // Skip cross-origin requests, or if any cross-origin redirects have been made.
    bool isSameOrigin = SecurityOrigin::create(documentLoader->documentURL())->isSameOriginAs(protect(protect(protectedWindow()->document())->securityOrigin()));
    if (!isSameOrigin || (!documentLoader->request().isSameSite() && !fromBackForwardCache))
        return nullptr;

    RefPtr<NavigationHistoryEntry> oldEntry;
    if (frame()->document() && frame()->document()->settings().navigationAPIEnabled())
        oldEntry = currentEntry();
    else if (RefPtr currentItem = frame()->loader().history().currentItem())
        oldEntry = NavigationHistoryEntry::create(*this, *currentItem);

    RefPtr<NavigationHistoryEntry> newEntry;
    if (*type == NavigationNavigationType::Reload) {
        newEntry = oldEntry;
    } else if (*type == NavigationNavigationType::Traverse) {
        ASSERT(newItem);
        // FIXME: For a traverse navigation, we should be identifying the right existing history
        // entry for 'newEntry' instead of allocating a new one.
        if (newItem)
            newEntry = NavigationHistoryEntry::create(*this, *newItem);
    } else {
        ASSERT(newItem);
        if (newItem)
            newEntry = NavigationHistoryEntry::create(*this, *newItem);
    }

    if (newEntry)
        return NavigationActivation::create(*type, newEntry.releaseNonNull(), WTF::move(oldEntry));
    return nullptr;
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
    return createErrorResult(WTF::move(committed), WTF::move(finished), Exception { exceptionCode, errorMessage });
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
    RefPtr apiMethodTracker = NavigationAPIMethodTracker::create(WTF::move(committed), WTF::move(finished), WTF::move(info), WTF::move(serializedState));

    Ref { apiMethodTracker->finishedPromise }->markAsHandled();

    // FIXME: We should be able to assert m_upcomingNonTraverseMethodTracker is empty.
    if (!hasEntriesAndEventsDisabled()) {
        Locker locker { m_apiMethodTrackersLock };
        m_upcomingNonTraverseMethodTracker = apiMethodTracker;
    }

    return apiMethodTracker;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#add-an-upcoming-traverse-api-method-tracker
RefPtr<NavigationAPIMethodTracker> Navigation::addUpcomingTraverseAPIMethodTracker(Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished, const String& key, JSC::JSValue info)
{
    RefPtr apiMethodTracker = NavigationAPIMethodTracker::create(WTF::move(committed), WTF::move(finished), WTF::move(info), nullptr);
    apiMethodTracker->key = key;

    Ref { apiMethodTracker->finishedPromise }->markAsHandled();

    {
        Locker locker { m_apiMethodTrackersLock };
        m_upcomingTraverseMethodTrackers.add(key, *apiMethodTracker);
    }

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
        return createErrorResult(WTF::move(committed), WTF::move(finished), serializedState.releaseException());
    auto state = serializedState.releaseReturnValue();
    if (!state && currentEntry())
        state = currentEntry()->associatedHistoryItem().navigationAPIStateObject();

    RefPtr window = this->window();
    if (!protect(window->document())->isFullyActive() || window->document()->unloadCounter())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    RefPtr apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTF::move(committed), WTF::move(finished), WTF::move(options.info), WTF::move(state));

    RefPtr lexicalFrame = lexicalFrameFromCommonVM();
    auto initiatedByMainFrame = lexicalFrame && lexicalFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;
    RefPtr frame = this->frame();
    RefPtr document = frame->document();
    ResourceRequest resourceRequest { URL { document->url() }, frame->loader().outgoingReferrer(), ResourceRequestCachePolicy::ReloadIgnoringCacheData };
    FrameLoadRequest frameLoadRequest { *document, document->securityOrigin(), WTF::move(resourceRequest), selfTargetFrameName(), initiatedByMainFrame };
    frameLoadRequest.setLockHistory(LockHistory::Yes);
    frameLoadRequest.setLockBackForwardList(LockBackForwardList::Yes);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(document->shouldOpenExternalURLsPolicyToPropagate());

    frame->loader().changeLocation(WTF::move(frameLoadRequest));

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-navigate
Navigation::Result Navigation::navigate(const String& url, NavigateOptions&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    RefPtr window = this->window();
    auto newURL = protect(window->document())->completeURL(url, ScriptExecutionContext::ForceUTF8::Yes);
    const URL& currentURL = protectedScriptExecutionContext()->url();

    if (!newURL.isValid())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::SyntaxError, "Invalid URL"_s);

    // Reject all JavaScript URLs in Navigation API.
    if (newURL.protocolIsJavaScript())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::NotSupportedError, "Navigation API does not support javascript: URLs."_s);

    if (options.history == HistoryBehavior::Push && currentURL.isAboutBlank())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::NotSupportedError, "A \"push\" navigation was explicitly requested, but only a \"replace\" navigation is possible while on an about:blank document."_s);

    auto serializedState = serializeState(options.state);
    if (serializedState.hasException())
        return createErrorResult(WTF::move(committed), WTF::move(finished), serializedState.releaseException());

    if (!protect(window->document())->isFullyActive() || window->document()->unloadCounter())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    RefPtr apiMethodTracker = maybeSetUpcomingNonTraversalTracker(WTF::move(committed), WTF::move(finished), WTF::move(options.info), serializedState.releaseReturnValue());

    auto request = FrameLoadRequest(*frame(), WTF::move(newURL));
    request.setNavigationHistoryBehavior(options.history);
    request.setIsFromNavigationAPI(true);
    frame()->loader().loadFrameRequest(WTF::move(request), nullptr, { });

    // If the load() call never made it to the point that NavigateEvent was emitted, thus promoteUpcomingAPIMethodTracker() called, this will be true.
    {
        Locker locker { m_apiMethodTrackersLock };
        if (m_upcomingNonTraverseMethodTracker == apiMethodTracker) {
            m_upcomingNonTraverseMethodTracker = nullptr;
            return createErrorResult(WTF::move(apiMethodTracker->committedPromise), WTF::move(apiMethodTracker->finishedPromise), ExceptionCode::AbortError, "Navigation aborted"_s);
        }
    }

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#performing-a-navigation-api-traversal
Navigation::Result Navigation::performTraversal(const String& key, Navigation::Options options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    RefPtr window = this->window();
    if (!protect(window->document())->isFullyActive() || window->document()->unloadCounter())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Invalid state"_s);

    if (!hasEntryWithKey(key))
        createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::AbortError, "Navigation aborted"_s);

    RefPtr frame = this->frame();
    if (!frame->isMainFrame() && protect(window->document())->canNavigate(&protect(frame->page())->protectedMainFrame().get()) != CanNavigateState::Able)
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::SecurityError, "Invalid state"_s);

    RefPtr current = currentEntry();
    if (current->key() == key) {
        committed->resolve<IDLInterface<NavigationHistoryEntry>>(*current.get());
        finished->resolve<IDLInterface<NavigationHistoryEntry>>(*current.get());
        return { createDOMPromise(committed), createDOMPromise(finished) };
    }

    {
        Locker locker { m_apiMethodTrackersLock };
        if (auto existingMethodTracker = m_upcomingTraverseMethodTrackers.getOptional(key))
            return apiMethodTrackerDerivedResult(*existingMethodTracker);
    }

    RefPtr apiMethodTracker = addUpcomingTraverseAPIMethodTracker(WTF::move(committed), WTF::move(finished), key, options.info);

    // FIXME: 11. Let sourceSnapshotParams be the result of snapshotting source snapshot params given document.
    frame->protectedNavigationScheduler()->scheduleHistoryNavigationByKey(key, [apiMethodTracker] (ScheduleHistoryNavigationResult result) {
        if (result == ScheduleHistoryNavigationResult::Aborted)
            createErrorResult(WTF::move(apiMethodTracker->committedPromise), WTF::move(apiMethodTracker->finishedPromise), ExceptionCode::AbortError, "Navigation aborted"_s);
    });

    return apiMethodTrackerDerivedResult(*apiMethodTracker);
}

size_t Navigation::entryIndexOfKey(const String& key) const
{
    if (key.isEmpty())
        return notFound;

    return m_entries.findIf([&key](const Ref<NavigationHistoryEntry> entry) {
        return entry->key() == key;
    });
}

bool Navigation::hasEntryWithKey(const String& key) const
{
    return entryIndexOfKey(key) != notFound;
}

NavigationHistoryEntry* Navigation::findEntryByKey(const String& key) const
{
    auto index = entryIndexOfKey(key);

    if (index == notFound)
        return nullptr;

    return m_entries[index].ptr();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-traverseto
Navigation::Result Navigation::traverseTo(const String& key, Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!hasEntryWithKey(key))
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Invalid key"_s);

    return performTraversal(key, options, WTF::move(committed), WTF::move(finished));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-back
Navigation::Result Navigation::back(Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoBack())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Cannot go back"_s);

    Ref previousEntry = m_entries[m_currentEntryIndex.value() - 1];

    return performTraversal(previousEntry->key(), options, WTF::move(committed), WTF::move(finished));
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#dom-navigation-forward
Navigation::Result Navigation::forward(Options&& options, Ref<DeferredPromise>&& committed, Ref<DeferredPromise>&& finished)
{
    if (!canGoForward())
        return createErrorResult(WTF::move(committed), WTF::move(finished), ExceptionCode::InvalidStateError, "Cannot go forward"_s);

    Ref nextEntry = m_entries[m_currentEntryIndex.value() + 1];

    return performTraversal(nextEntry->key(), options, WTF::move(committed), WTF::move(finished));
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

    current->setState(WTF::move(serializedState));

    auto currentEntryChangeEvent = NavigationCurrentEntryChangeEvent::create(eventNames().currententrychangeEvent, {
        { false, false, false },
        std::nullopt,
        current.releaseNonNull()
    });
    dispatchEvent(currentEntryChangeEvent);

    return { };
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#has-entries-and-events-disabled
bool Navigation::hasEntriesAndEventsDisabled() const
{
    RefPtr window = this->window();
    RefPtr document = window->document();
    if (!document || !document->isFullyActive())
        return true;
    if (document->loader() && document->loader()->isInitialAboutBlank())
        return true;
    if (window->securityOrigin() && window->securityOrigin()->isOpaque())
        return true;
    return false;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#resolve-the-finished-promise
void Navigation::resolveFinishedPromise(NavigationAPIMethodTracker* apiMethodTracker)
{
    RefPtr committedToEntry = apiMethodTracker->committedToEntry;
    if (!committedToEntry) {
        apiMethodTracker->finishedBeforeCommit = true;
        return;
    }

    Ref { apiMethodTracker->committedPromise }->resolve<IDLInterface<NavigationHistoryEntry>>(*committedToEntry);
    Ref { apiMethodTracker->finishedPromise }->resolve<IDLInterface<NavigationHistoryEntry>>(*committedToEntry);
    cleanupAPIMethodTracker(apiMethodTracker);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#reject-the-finished-promise
void Navigation::rejectFinishedPromise(NavigationAPIMethodTracker* apiMethodTracker, const Exception& exception, JSC::JSValue exceptionObject)
{
    RELEASE_LOG(Navigation, "rejectFinishedPromise: rejecting promises for tracker=%p with exception='%s'", apiMethodTracker, exception.message().utf8().data());

    // Only reject committed promise if it hasn't been fulfilled yet (committedToEntry is null). If the navigation was "committed" (state updated)
    // before being aborted, the committed promise should remain fulfilled while only the finished promise gets rejected.
    if (!apiMethodTracker->committedToEntry)
        Ref { apiMethodTracker->committedPromise }->reject(exception, RejectAsHandled::No, exceptionObject);
    Ref { apiMethodTracker->finishedPromise }->reject(exception, RejectAsHandled::Yes, exceptionObject);
    cleanupAPIMethodTracker(apiMethodTracker);
}

void Navigation::rejectFinishedPromise(NavigationAPIMethodTracker* apiMethodTracker)
{
    if (!apiMethodTracker)
        return;

    auto* globalObject = protectedScriptExecutionContext()->globalObject();
    if (!globalObject && apiMethodTracker)
        globalObject = apiMethodTracker->committedPromise->globalObject();
    if (!globalObject)
        return;

    JSC::JSLockHolder locker(globalObject->vm());
    auto exception = Exception(ExceptionCode::AbortError, "Navigation aborted"_s);
    auto domException = createDOMException(*globalObject, exception.isolatedCopy());
    rejectFinishedPromise(apiMethodTracker, exception, domException);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#notify-about-the-committed-to-entry
void Navigation::notifyCommittedToEntry(NavigationAPIMethodTracker* apiMethodTracker, NavigationHistoryEntry* entry, NavigationNavigationType navigationType)
{
    ASSERT(entry);
    apiMethodTracker->committedToEntry = entry;
    if (navigationType != NavigationNavigationType::Traverse) {
        if (apiMethodTracker->serializedState)
            RefPtr { apiMethodTracker->committedToEntry }->setState(WTF::move(apiMethodTracker->serializedState));
    }

    if (apiMethodTracker->finishedBeforeCommit)
        resolveFinishedPromise(apiMethodTracker);
    else
        Ref { apiMethodTracker->committedPromise }->resolve<IDLInterface<NavigationHistoryEntry>>(*entry);
}

void Navigation::updateNavigationEntry(Ref<HistoryItem>&& item, ShouldCopyStateObjectFromCurrentEntry shouldCopyStateObjectFromCurrentEntry)
{
    if (!m_currentEntryIndex)
        return;

    m_entries[*m_currentEntryIndex] = NavigationHistoryEntry::create(*this, item.copyRef());

    if (!frame())
        return;

    for (RefPtr child = frame()->tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr localChild = dynamicDowncast<LocalFrame>(child.get());
        if (!localChild)
            continue;

        if (RefPtr childItem = item->childItemWithFrameID(localChild->frameID())) {
            RefPtr window = localChild->window();
            if (!window)
                continue;

            window->protectedNavigation()->updateNavigationEntry(childItem.releaseNonNull(), shouldCopyStateObjectFromCurrentEntry);
        }
    }
}

void Navigation::disposeOfForwardEntriesInParents(BackForwardItemIdentifier itemID)
{
    RefPtr localMainFrame = protectedFrame()->localMainFrame();
    if (!localMainFrame)
        return;

    RefPtr localMainFrameWindow = localMainFrame->window();
    if (!localMainFrameWindow)
        return;

    localMainFrameWindow->protectedNavigation()->recursivelyDisposeOfForwardEntriesInParents(itemID, protectedFrame().get());
}

void Navigation::recursivelyDisposeOfForwardEntriesInParents(BackForwardItemIdentifier itemID, LocalFrame* navigatedFrame)
{
    if (frame() == navigatedFrame)
        return;

    std::optional<size_t> index = std::nullopt;
    for (size_t i = 0; i < m_entries.size(); i++) {
        if (m_entries[i]->associatedHistoryItem().itemID() == itemID) {
            index = i;
            break;
        }
    }

    if (!index)
        return;

    for (size_t i = *index + 1; i < m_entries.size(); i++)
        Ref { m_entries[i] }->dispatchDisposeEvent();

    m_currentEntryIndex = index;
    m_entries.resize(*m_currentEntryIndex + 1);

    for (RefPtr child = frame()->tree().firstChild(); child; child = child->tree().nextSibling()) {
        RefPtr localChild = dynamicDowncast<LocalFrame>(child.get());
        if (!localChild)
            continue;

        RefPtr window = localChild->window();
        if (!window)
            continue;

        window->protectedNavigation()->recursivelyDisposeOfForwardEntriesInParents(itemID, navigatedFrame);
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-a-same-document-navigation
void Navigation::updateForNavigation(Ref<HistoryItem>&& item, NavigationNavigationType navigationType, ShouldCopyStateObjectFromCurrentEntry shouldCopyStateObjectFromCurrentEntry, ShouldNotifyCommitted shouldNotifyCommitted)
{
    if (hasEntriesAndEventsDisabled())
        return;

    RefPtr oldCurrentEntry = currentEntry();
    if (!oldCurrentEntry)
        return;

    Vector<Ref<NavigationHistoryEntry>> disposedEntries;

    switch (navigationType) {
    case NavigationNavigationType::Traverse:
        m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, item);
        if (!m_currentEntryIndex)
            return;
        break;
    case NavigationNavigationType::Push:
        disposeOfForwardEntriesInParents(oldCurrentEntry->associatedHistoryItem().itemID());
        m_currentEntryIndex = *m_currentEntryIndex + 1;
        for (size_t i = *m_currentEntryIndex; i < m_entries.size(); i++)
            disposedEntries.append(m_entries[i]);
        m_entries.resize(*m_currentEntryIndex + 1);
        break;
    case NavigationNavigationType::Replace:
        disposedEntries.append(*oldCurrentEntry);
        break;
    default:
        break;
    }

    if (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace) {
        updateNavigationEntry(WTF::move(item), shouldCopyStateObjectFromCurrentEntry);
        if (shouldCopyStateObjectFromCurrentEntry == ShouldCopyStateObjectFromCurrentEntry::Yes)
            Ref { m_entries[*m_currentEntryIndex] }->setState(oldCurrentEntry->state());
    }

    RefPtr<NavigationAPIMethodTracker> ongoingAPIMethodTracker;
    {
        Locker locker { m_apiMethodTrackersLock };
        ongoingAPIMethodTracker = m_ongoingAPIMethodTracker;
    }
    if (ongoingAPIMethodTracker && shouldNotifyCommitted == ShouldNotifyCommitted::Yes)
        notifyCommittedToEntry(ongoingAPIMethodTracker.get(), protectedCurrentEntry().get(), navigationType);

    auto currentEntryChangeEvent = NavigationCurrentEntryChangeEvent::create(eventNames().currententrychangeEvent, {
        { false, false, false },
        navigationType,
        oldCurrentEntry.releaseNonNull()
    });
    dispatchEvent(currentEntryChangeEvent);

    for (auto& disposedEntry : disposedEntries)
        disposedEntry->dispatchDisposeEvent();
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#update-the-navigation-api-entries-for-reactivation
void Navigation::updateForReactivation(Vector<Ref<HistoryItem>>&& newHistoryItems, HistoryItem& reactivatedItem, HistoryItem* previousItem)
{
    if (hasEntriesAndEventsDisabled())
        return;

    Vector<Ref<NavigationHistoryEntry>> newEntries;
    Vector<Ref<NavigationHistoryEntry>> oldEntries = std::exchange(m_entries, { });

    for (Ref item : newHistoryItems) {
        RefPtr<NavigationHistoryEntry> newEntry;

        for (size_t entryIndex = 0; entryIndex < oldEntries.size(); entryIndex++) {
            auto& entry = oldEntries.at(entryIndex);
            if (entry->associatedHistoryItem().itemSequenceNumber() == item->itemSequenceNumber()) {
                newEntry = entry.ptr();
                oldEntries.removeAt(entryIndex);
                break;
            }
        }

        if (!newEntry)
            newEntry = NavigationHistoryEntry::create(*this, WTF::move(item));

        newEntries.append(newEntry.releaseNonNull());
    }

    m_entries = WTF::move(newEntries);
    m_currentEntryIndex = getEntryIndexOfHistoryItem(m_entries, reactivatedItem);

    for (auto& disposedEntry : oldEntries)
        disposedEntry->dispatchDisposeEvent();

    // Clear any existing activation before setting a new one for BFCache restore.
    m_activation = nullptr;

    // Update activation for BFCache restore with traverse navigation type.
    setActivation(previousItem, NavigationNavigationType::Traverse);
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#can-have-its-url-rewritten
static bool documentCanHaveURLRewritten(const Document& document, const URL& targetURL)
{
    const URL& documentURL = document.url();
    Ref documentOrigin = document.securityOrigin();
    auto targetOrigin = SecurityOrigin::create(targetURL);
    bool isSameSite = documentOrigin->isSameSiteAs(targetOrigin);
    bool isSameOrigin = documentOrigin->isSameOriginAs(targetOrigin);

    // For cross-window navigation with document.domain, we need to check same-origin rather than same-site
    // to account for document.domain modifications that make cross-origin windows same-origin-domain
    if (!isSameSite && !isSameOrigin)
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

    Locker locker { m_apiMethodTrackersLock };
    if (!destinationKey.isEmpty())
        m_ongoingAPIMethodTracker = m_upcomingTraverseMethodTrackers.take(destinationKey);
    else if (destinationKey.isNull()) {
        m_ongoingAPIMethodTracker = WTF::move(m_upcomingNonTraverseMethodTracker);
        m_upcomingNonTraverseMethodTracker = nullptr;
    } else if (destinationKey.isEmpty() && !m_upcomingTraverseMethodTrackers.isEmpty()) {
        // For traverse navigation where destination key is empty, try to use any available traverse method tracker.
        // (e.g., cross-document navigation where NavigationHistoryEntry is not found).
        auto firstTracker = m_upcomingTraverseMethodTrackers.begin();
        if (firstTracker != m_upcomingTraverseMethodTrackers.end()) {
            String trackerKey = firstTracker->key;
            m_ongoingAPIMethodTracker = m_upcomingTraverseMethodTrackers.take(trackerKey);
        }
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#navigation-api-method-tracker-clean-up
void Navigation::cleanupAPIMethodTracker(NavigationAPIMethodTracker* apiMethodTracker)
{
    Locker locker { m_apiMethodTrackersLock };
    if (m_ongoingAPIMethodTracker == apiMethodTracker)
        m_ongoingAPIMethodTracker = nullptr;
    else {
        auto& key = apiMethodTracker->key;
        // FIXME: We should be able to assert key isn't null and m_upcomingTraverseMethodTrackers contains it.
        if (!key.isNull())
            m_upcomingTraverseMethodTrackers.remove(key);
    }
}

NavigationAPIMethodTracker* Navigation::upcomingTraverseMethodTracker(const String& key) const
{
    Locker locker { m_apiMethodTrackersLock };
    return m_upcomingTraverseMethodTrackers.get(key);
}

auto Navigation::registerAbortHandler() -> Ref<AbortHandler>
{
    Ref abortHandler = AbortHandler::create();
    m_abortHandlers.add(abortHandler.get());
    return abortHandler;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#abort-the-ongoing-navigation
void Navigation::abortOngoingNavigation(NavigateEvent& event)
{
    m_abortHandlers.forEach([](auto& abortHandler) {
        abortHandler.markAsAborted();
    });

    RefPtr scriptExecutionContext = this->scriptExecutionContext();
    auto* globalObject = scriptExecutionContext->globalObject();
    if (!globalObject) {
        RefPtr<NavigationAPIMethodTracker> ongoingAPIMethodTracker;
        {
            Locker locker { m_apiMethodTrackersLock };
            ongoingAPIMethodTracker = m_ongoingAPIMethodTracker;
        }
        if (ongoingAPIMethodTracker)
            globalObject = ongoingAPIMethodTracker->committedPromise->globalObject();
    }
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
            errorInformation = WTF::move(*result);
        // Default to document url if extractErrorInformationFromErrorInstance was not able to determine sourceURL.
        if (errorInformation.sourceURL.isEmpty())
            errorInformation.sourceURL = scriptExecutionContext->url().string();
    }

    if (RefPtr signal = event.signal())
        signal->signalAbort(domException);

    m_ongoingNavigateEvent = nullptr;

    dispatchEvent(ErrorEvent::create(eventNames().navigateerrorEvent, exception.message(), errorInformation.sourceURL, errorInformation.line, errorInformation.column, { globalObject->vm(), domException }));

    RefPtr<NavigationAPIMethodTracker> ongoingAPIMethodTracker;
    {
        Locker locker { m_apiMethodTrackersLock };
        ongoingAPIMethodTracker = m_ongoingAPIMethodTracker;
    }
    if (ongoingAPIMethodTracker)
        rejectFinishedPromise(ongoingAPIMethodTracker.get(), exception, domException);

    if (RefPtr transition = m_transition) {
        transition->rejectPromise(exception, domException);
        m_transition = nullptr;
    }
}

struct AwaitingPromiseData : public RefCounted<AwaitingPromiseData> {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(AwaitingPromiseData);
    Function<void()> fulfilledCallback;
    Function<void(JSC::JSValue)> rejectionCallback;
    size_t remainingPromises = 0;
    bool rejected = false;

    AwaitingPromiseData() = delete;
    AwaitingPromiseData(Function<void()>&& fulfilledCallback, Function<void(JSC::JSValue)>&& rejectionCallback, size_t remainingPromises)
        : fulfilledCallback(WTF::move(fulfilledCallback))
        , rejectionCallback(WTF::move(rejectionCallback))
        , remainingPromises(remainingPromises)
    {
    }
};

// https://webidl.spec.whatwg.org/#wait-for-all
static void waitForAllPromises(Document& document, const Vector<Ref<DOMPromise>>& promises, Function<void()>&& fulfilledCallback, Function<void(JSC::JSValue)>&& rejectionCallback)
{
    if (promises.isEmpty()) {
        document.checkedEventLoop()->queueMicrotask(WTF::move(fulfilledCallback));
        return;
    }

    Ref awaitingData = adoptRef(*new AwaitingPromiseData(WTF::move(fulfilledCallback), WTF::move(rejectionCallback), promises.size()));

    for (const auto& promise : promises) {
        // At any point between promises the frame could have been detached.
        // FIXME: There is possibly a better way to handle this rather than just never complete.
        if (promise->isSuspended())
            return;

        promise->whenSettledWithResult([awaitingData](auto* globalObject, bool isFulfilled, auto result) mutable {
            RefPtr context = globalObject ? globalObject->scriptExecutionContext() : nullptr;
            if (!context || context->activeDOMObjectsAreSuspended() || context->activeDOMObjectsAreStopped())
                return;

            if (!isFulfilled) {
                if (awaitingData->rejected)
                    return;
                awaitingData->rejected = true;
                awaitingData->rejectionCallback(result);
                return;
            }
            if (--awaitingData->remainingPromises > 0)
                return;
            awaitingData->fulfilledCallback();
        });
    }
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#inner-navigate-event-firing-algorithm
Navigation::DispatchResult Navigation::innerDispatchNavigateEvent(NavigationNavigationType navigationType, Ref<NavigationDestination>&& destination, const String& downloadRequestFilename, FormState* formState, SerializedScriptValue* classicHistoryAPIState, Element* sourceElement)
{
    if (hasEntriesAndEventsDisabled()) {
#if ASSERT_ENABLED
        Locker locker { m_apiMethodTrackersLock };
#endif
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

    RefPtr<NavigationAPIMethodTracker> ongoingAPIMethodTracker;
    {
        Locker locker { m_apiMethodTrackersLock };
        ongoingAPIMethodTracker = m_ongoingAPIMethodTracker;
    }

    // Enforce rate limiting to prevent excessive navigation requests.
    // Only check for script-initiated navigations (those with an API method tracker).
    if (ongoingAPIMethodTracker && !m_rateLimiter.navigationAllowed()) {
        // Log a warning once per window when the limit is reached.
        if (!m_rateLimiter.wasReported()) {
            m_rateLimiter.markReported();
            if (RefPtr document = protect(window())->document())
                document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Excessive navigation attempts blocked."_s);
        }

        // Reject the promises and clean up.
        auto exception = Exception { ExceptionCode::QuotaExceededError, "Navigation rate limit exceeded"_s };
        Ref { ongoingAPIMethodTracker->committedPromise }->reject(exception);
        Ref { ongoingAPIMethodTracker->finishedPromise }->reject(exception);
        cleanupAPIMethodTracker(ongoingAPIMethodTracker.get());

        return DispatchResult::Aborted;
    }

    RefPtr document = protect(window())->document();

    RefPtr<NavigationAPIMethodTracker> apiMethodTracker;
    {
        Locker locker { m_apiMethodTrackersLock };
        apiMethodTracker = m_ongoingAPIMethodTracker;
    }
    // FIXME: this should not be needed, we should pass it into FrameLoader.
    if (apiMethodTracker && apiMethodTracker->serializedState)
        destination->setStateObject(apiMethodTracker->serializedState.get());
    bool isSameDocument = destination->sameDocument();
    bool isTraversal = navigationType == NavigationNavigationType::Traverse;
    bool canIntercept = documentCanHaveURLRewritten(*document, destination->url()) && (!isTraversal || isSameDocument);
    bool canBeCanceled = !isTraversal || (document->isTopDocument() && isSameDocument); // FIXME: and user involvement is not browser-ui or navigation's relevant global object has transient activation.
    bool hashChange = !classicHistoryAPIState && equalIgnoringFragmentIdentifier(document->url(), destination->url()) && !equalRespectingNullity(document->url().fragmentIdentifier(),  destination->url().fragmentIdentifier());
    auto info = apiMethodTracker ? apiMethodTracker->info.getValue() : JSC::jsUndefined();

    RefPtr scriptExecutionContext = this->scriptExecutionContext();
    RefPtr<DOMFormData> formData = nullptr;
    RefPtr updatedSourceElement = sourceElement;
    if (RefPtr state = formState) {
        RefPtr submitter = state->submitter();
        Ref form = state->form();

        if (form->isMethodPost() && (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace)) {
            if (auto domFormData = DOMFormData::create(*scriptExecutionContext, form.ptr(), submitter.get()); !domFormData.hasException())
                formData = domFormData.releaseReturnValue();
        }

        updatedSourceElement = submitter.get();
        if (!updatedSourceElement)
            updatedSourceElement = form.ptr();
    }

    RefPtr abortController = AbortController::create(*scriptExecutionContext);

    auto init = NavigateEvent::Init {
        { false, canBeCanceled, false },
        navigationType,
        destination,
        canIntercept,
        UserGestureIndicator::processingUserGesture(document.get()),
        hashChange,
        Ref { abortController->signal() },
        formData,
        downloadRequestFilename,
        info,
        document->page() && document->page()->isInSwipeAnimation(),
        updatedSourceElement.get(),
    };

    // Free up no longer needed info.
    if (apiMethodTracker)
        apiMethodTracker->info.clear();

    Ref event = NavigateEvent::create(eventNames().navigateEvent, WTF::move(init), abortController.get());
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
        if (!event->signal().aborted())
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
        if (!fromNavigationHistoryEntry) {
            abortOngoingNavigation(event);
            return DispatchResult::Aborted;
        }

        {
            auto& domGlobalObject = *jsCast<JSDOMGlobalObject*>(scriptExecutionContext->globalObject());
            JSC::JSLockHolder locker(domGlobalObject.vm());
            m_transition = NavigationTransition::create(navigationType, *fromNavigationHistoryEntry, DeferredPromise::create(domGlobalObject, DeferredPromise::Mode::RetainPromiseOnResolve).releaseNonNull());
        }

        if (navigationType == NavigationNavigationType::Traverse) {
            m_suppressNormalScrollRestorationDuringOngoingNavigation = true;
            // For intercepted traverse navigations, update the Navigation API state and fire currententrychange.
            // This must happen AFTER the navigate event but BEFORE intercept handlers run.
            // For committed promise timing:
            // - If there are NO handlers (just intercept() called), fulfill committed now (before currententrychange)
            // - If there ARE handlers (intercept({ handler() {...} })), fulfill committed after handlers are invoked
            if (destination->sameDocument()) {
                RefPtr entry = findEntryByKey(destination->key());
                if (entry) {
                    document->updateURLForPushOrReplaceState(destination->url());

                    // Only notify committed now if there are no handlers to wait for
                    auto shouldNotifyCommited = event->handlers().isEmpty() ? ShouldNotifyCommitted::Yes : ShouldNotifyCommitted::No;
                    updateForNavigation(entry->associatedHistoryItem(), navigationType, ShouldCopyStateObjectFromCurrentEntry::No, shouldNotifyCommited);
                }
            }
        } else if (navigationType == NavigationNavigationType::Reload) {
            // Not in specification but matches chromium implementation and tests.
            updateForNavigation(currentEntry()->associatedHistoryItem(), navigationType);
        } else if (navigationType == NavigationNavigationType::Push || navigationType == NavigationNavigationType::Replace) {
            auto historyHandling = navigationType == NavigationNavigationType::Replace ? NavigationHistoryBehavior::Replace : NavigationHistoryBehavior::Push;
            frame()->loader().updateURLAndHistory(destination->url(), classicHistoryAPIState, historyHandling);
        }
    }

    if (endResultIsSameDocument) {
        Vector<Ref<DOMPromise>> promiseList;

        for (auto& handler : event->handlers()) {
            auto callbackResult = handler->invoke();
            if (callbackResult.type() != CallbackResultType::UnableToExecute) {
                Ref promise = callbackResult.releaseReturnValue();
                // Because rejection is reported as `navigateerror` event, we can mark this as handled.
                if (!promise->isSuspended())
                    promise->markAsHandled();
                promiseList.append(WTF::move(promise));
            }
        }

        // For intercepted traverse navigations, notify committed after handlers have been invoked but before
        // they complete. This ensures the correct event ordering.
        if (navigationType == NavigationNavigationType::Traverse && event->wasIntercepted() && apiMethodTracker && !apiMethodTracker->committedToEntry)
            notifyCommittedToEntry(apiMethodTracker.get(), protectedCurrentEntry().get(), navigationType);

        // FIXME: this emulates the behavior of a Promise wrapped around waitForAll, but we may want the real
        // thing if the ordering-and-transition tests show timing related issues related to this.
        scriptExecutionContext->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { this }, promiseList, abortController, document, apiMethodTracker]() {
            waitForAllPromises(*document, promiseList, [abortController, document, apiMethodTracker, weakThis]() mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || abortController->signal().aborted() || !document->isFullyActive() || !protectedThis->m_ongoingNavigateEvent)
                    return;

                auto focusChanged = std::exchange(protectedThis->m_focusChangedDuringOngoingNavigation, FocusDidChange::No);
                protectedThis->protectedOngoingNavigateEvent()->finish(*document, InterceptionHandlersDidFulfill::Yes, focusChanged);
                protectedThis->m_ongoingNavigateEvent = nullptr;

                protectedThis->dispatchEvent(Event::create(eventNames().navigatesuccessEvent, { }));

                if (apiMethodTracker)
                    protectedThis->resolveFinishedPromise(apiMethodTracker.get());

                if (RefPtr transition = std::exchange(protectedThis->m_transition, nullptr))
                    transition->resolvePromise();

                protectedThis->m_ongoingNavigateEvent = nullptr;

            }, [abortController, document, apiMethodTracker, weakThis](JSC::JSValue result) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || abortController->signal().aborted() || !document->isFullyActive() || !protectedThis->m_ongoingNavigateEvent)
                    return;

                auto focusChanged = std::exchange(protectedThis->m_focusChangedDuringOngoingNavigation, FocusDidChange::No);
                protectedThis->protectedOngoingNavigateEvent()->finish(*document, InterceptionHandlersDidFulfill::No, focusChanged);

                if (abortController)
                    abortController->signal().signalAbort(result);

                protectedThis->m_ongoingNavigateEvent = nullptr;

                ErrorInformation errorInformation;
                String errorMessage;
                if (auto* errorInstance = jsDynamicCast<JSC::ErrorInstance*>(result)) {
                    if (auto result = extractErrorInformationFromErrorInstance(protectedThis->protectedScriptExecutionContext()->globalObject(), *errorInstance)) {
                        errorInformation = WTF::move(*result);
                        errorMessage = makeString("Uncaught "_s, errorInformation.errorTypeString, ": "_s, errorInformation.message);
                    }
                }

                protectedThis->dispatchEvent(ErrorEvent::create(eventNames().navigateerrorEvent, errorMessage, errorInformation.sourceURL, errorInformation.line, errorInformation.column, { protectedThis->protectedScriptExecutionContext()->globalObject()->vm(), result }));

                if (apiMethodTracker)
                    Ref { apiMethodTracker->finishedPromise }->reject<IDLAny>(result, RejectAsHandled::Yes);

                if (RefPtr transition = std::exchange(protectedThis->m_transition, nullptr))
                    transition->rejectPromise(result);
            });
        });

        // If a new event has been dispatched in our event handler then we were aborted above.
        if (m_ongoingNavigateEvent != event.ptr())
            return DispatchResult::Aborted;
    } else if (apiMethodTracker) {
        // For cross-document navigations, don't cleanup the tracker immediately.
        // It should remain ongoing until the navigation completes, fails, or gets interrupted.
        RELEASE_LOG(Navigation, "innerDispatchNavigateEvent: cross-document navigation, keeping tracker=%p alive", apiMethodTracker.get());
    } else {
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
    RefPtr currentItem = frame() ? frame()->loader().history().currentItem() : nullptr;
    bool isSameDocument = currentItem && currentItem->documentSequenceNumber() == historyItem.documentSequenceNumber();

    RefPtr<NavigationHistoryEntry> destinationEntry;
    auto index = m_entries.findIf([&historyItem](const auto& entry) {
        return entry->associatedHistoryItem().itemSequenceNumber() == historyItem.itemSequenceNumber();
    });
    if (index != notFound)
        destinationEntry = m_entries[index].ptr();

    // FIXME: Set destinations state
    Ref destination = NavigationDestination::create(historyItem.url(), WTF::move(destinationEntry), isSameDocument);

    return innerDispatchNavigateEvent(NavigationNavigationType::Traverse, WTF::move(destination), { });
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-push/replace/reload-navigate-event
bool Navigation::dispatchPushReplaceReloadNavigateEvent(const URL& url, NavigationNavigationType navigationType, bool isSameDocument, FormState* formState, SerializedScriptValue* classicHistoryAPIState, Element* sourceElement)
{
    Ref destination = NavigationDestination::create(url, nullptr, isSameDocument);
    if (classicHistoryAPIState)
        destination->setStateObject(classicHistoryAPIState);

    if (navigationType == NavigationNavigationType::Reload) {
        formState = nullptr;
        sourceElement = nullptr;
    }

    return innerDispatchNavigateEvent(navigationType, WTF::move(destination), { }, formState, classicHistoryAPIState, sourceElement) == DispatchResult::Completed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#fire-a-download-request-navigate-event
bool Navigation::dispatchDownloadNavigateEvent(const URL& url, const String& downloadFilename, Element* sourceElement)
{
    Ref destination = NavigationDestination::create(url, nullptr, false);
    return innerDispatchNavigateEvent(NavigationNavigationType::Push, WTF::move(destination), downloadFilename, nullptr, nullptr, sourceElement) == DispatchResult::Completed;
}

// https://html.spec.whatwg.org/multipage/nav-history-apis.html#inform-the-navigation-api-about-aborting-navigation
void Navigation::abortOngoingNavigationIfNeeded()
{
    if (RefPtr ongoingNavigateEvent = m_ongoingNavigateEvent)
        abortOngoingNavigation(*ongoingNavigateEvent);
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#getting-session-history-entries-for-the-navigation-api
Vector<Ref<HistoryItem>> Navigation::filterHistoryItemsForNavigationAPI(Vector<Ref<HistoryItem>>&& allItems, HistoryItem& currentItem)
{
    auto startingIndex = allItems.findIf([currentItemID = currentItem.itemID()](const Ref<HistoryItem> entry) {
        return entry->itemID() == currentItemID;
    });

    if (startingIndex == notFound)
        return { currentItem };

    Vector<Ref<HistoryItem>> filteredItems;
    Ref startingOrigin = SecurityOrigin::create(currentItem.url());

    for (int i = static_cast<int>(startingIndex) - 1; i >= 0; --i) {
        Ref item = allItems[i];
        if (!SecurityOrigin::create(item->url())->isSameOriginAs(startingOrigin))
            break;
        filteredItems.append(WTF::move(item));
    }

    filteredItems.reverse();
    filteredItems.append(currentItem);

    for (size_t i = startingIndex + 1; i < allItems.size(); ++i) {
        Ref item = allItems[i];
        if (!SecurityOrigin::create(item->url())->isSameOriginAs(startingOrigin))
            break;
        filteredItems.append(WTF::move(item));
    }

    return filteredItems;
}

bool Navigation::RateLimiter::navigationAllowed()
{
    auto currentTime = MonotonicTime::now();

    // Check if we've exceeded the time window and need to reset.
    if (currentTime - m_windowStartTime > m_windowDuration) {
        m_windowStartTime = currentTime;
        m_navigationCount = 0;
        m_limitMessageSent = false;
    }

    // Allow navigation if we're still under the limit.
    if (m_navigationCount < m_maxNavigationsPerWindow) {
        ++m_navigationCount;
        return true;
    }

    return false;
}

void Navigation::visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor)
{
    Locker locker { m_apiMethodTrackersLock };
    if (m_ongoingAPIMethodTracker)
        m_ongoingAPIMethodTracker->info.visit(visitor);
    if (m_upcomingNonTraverseMethodTracker)
        m_upcomingNonTraverseMethodTracker->info.visit(visitor);
    for (auto& tracker : m_upcomingTraverseMethodTrackers.values())
        tracker->info.visit(visitor);
}

} // namespace WebCore
