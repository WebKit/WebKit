/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LocalDOMWindow.h"

#include "BackForwardController.h"
#include "BarProp.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelectorParser.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ComposedTreeIterator.h"
#include "ContentExtensionActions.h"
#include "ContentExtensionRule.h"
#include "ContentRuleListResults.h"
#include "CookieStore.h"
#include "CrossOriginOpenerPolicy.h"
#include "Crypto.h"
#include "CustomElementRegistry.h"
#include "DOMSelection.h"
#include "DOMStringList.h"
#include "DOMTimer.h"
#include "DOMTokenList.h"
#include "DeviceMotionController.h"
#include "DeviceMotionData.h"
#include "DeviceMotionEvent.h"
#include "DeviceOrientationAndMotionAccessController.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "EventPath.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTTPParsers.h"
#include "History.h"
#include "IdleRequestOptions.h"
#include "InspectorInstrumentation.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindowBase.h"
#include "JSExecState.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Location.h"
#include "Logging.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MessageEvent.h"
#include "MessageWithMessagePorts.h"
#include "Navigation.h"
#include "NavigationScheduler.h"
#include "Navigator.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageTransitionEvent.h"
#include "Performance.h"
#include "PerformanceNavigationTiming.h"
#include "PermissionsPolicy.h"
#include "Quirks.h"
#include "RemoteFrame.h"
#include "RequestAnimationFrameCallback.h"
#include "ResourceLoadInfo.h"
#include "ResourceLoadObserver.h"
#include "ScheduledAction.h"
#include "Screen.h"
#include "ScrollAnimator.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "SecurityPolicy.h"
#include "SelectorQuery.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "StaticNodeList.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StorageNamespaceProvider.h"
#include "StyleMedia.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "SuddenTermination.h"
#include "UserContentProvider.h"
#include "UserGestureIndicator.h"
#include "VisualViewport.h"
#include "WebCoreOpaqueRoot.h"
#include "WebKitPoint.h"
#include "WindowFeatures.h"
#include "WindowFocusAllowedIndicator.h"
#include "WindowProxy.h"
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "UserContentController.h"
#include "UserMessageHandlerDescriptor.h"
#include "WebKitNamespace.h"
#endif

#if ENABLE(GAMEPAD)
#include "GamepadManager.h"
#endif

#if ENABLE(GEOLOCATION)
#include "NavigatorGeolocation.h"
#endif

#if ENABLE(POINTER_LOCK)
#include "PointerLockController.h"
#endif

namespace WebCore {
using namespace Inspector;

static constexpr Seconds defaultTransientActivationDuration { 5_s };

static WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData>& windowsInterestedInStorageEvents()
{
    static MainThreadNeverDestroyed<WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData>> set;
    return set;
}

void LocalDOMWindow::forEachWindowInterestedInStorageEvents(const Function<void(LocalDOMWindow&)>& apply)
{
    for (auto& window : copyToVectorOf<Ref<LocalDOMWindow>>(windowsInterestedInStorageEvents()))
        apply(window);
}

static std::optional<Seconds>& transientActivationDurationOverrideForTesting()
{
    static NeverDestroyed<std::optional<Seconds>> overrideForTesting;
    return overrideForTesting;
}

static Seconds transientActivationDuration()
{
    if (auto override = transientActivationDurationOverrideForTesting())
        return *override;
    return defaultTransientActivationDuration;
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(LocalDOMWindow);

typedef HashCountedSet<LocalDOMWindow*> LocalDOMWindowSet;

static LocalDOMWindowSet& windowsWithUnloadEventListeners()
{
    static NeverDestroyed<LocalDOMWindowSet> windowsWithUnloadEventListeners;
    return windowsWithUnloadEventListeners;
}

static LocalDOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    static NeverDestroyed<LocalDOMWindowSet> windowsWithBeforeUnloadEventListeners;
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(LocalDOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeUnloadEventListener(LocalDOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllUnloadEventListeners(LocalDOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static void addBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeBeforeUnloadEventListener(LocalDOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(LocalDOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(LocalDOMWindow* window)
{
    ASSERT_ARG(window, window);
    RefPtr frame = window->frame();
    if (!frame)
        return false;
    if (!frame->page())
        return false;
    return frame->isMainFrame();
}

bool LocalDOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    auto& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    auto windows = WTF::map(set, [](auto& entry) -> Ref<LocalDOMWindow> {
        return *entry.key;
    });

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        RefPtr frame = window->frame();
        if (!frame)
            continue;

        if (!frame->protectedLoader()->shouldClose())
            return false;

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
    return true;
}

unsigned LocalDOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<LocalDOMWindow*>(this));
}

void LocalDOMWindow::dispatchAllPendingUnloadEvents()
{
    auto& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    auto windows = WTF::map(set, [] (auto& keyValue) {
        return Ref<LocalDOMWindow>(*(keyValue.key));
    });

    auto& eventNames = WebCore::eventNames();
    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        RefPtr document = window->document();
        if (document)
            document->dispatchPagehideEvent(PageshowEventPersistence::NotPersisted);
        window->dispatchEvent(Event::create(eventNames.unloadEvent, Event::CanBubble::No, Event::IsCancelable::No), document.get());

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect LocalDOMWindow::adjustWindowRect(Page& page, const FloatRect& pendingChanges)
{
    FloatRect screen = screenAvailableRect(page.mainFrame().virtualView());
    FloatRect window = page.chrome().windowRect();

    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(std::isfinite(screen.x()));
    ASSERT(std::isfinite(screen.y()));
    ASSERT(std::isfinite(screen.width()));
    ASSERT(std::isfinite(screen.height()));
    ASSERT(std::isfinite(window.x()));
    ASSERT(std::isfinite(window.y()));
    ASSERT(std::isfinite(window.width()));
    ASSERT(std::isfinite(window.height()));

    // Update window values if new requested values are not NaN.
    if (!std::isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!std::isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!std::isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!std::isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());

    FloatSize minimumSize = page.chrome().client().minimumWindowSize();
    window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool LocalDOMWindow::allowPopUp(LocalFrame& firstFrame)
{
    if (RefPtr documentLoader = firstFrame.loader().documentLoader()) {
        // If pop-up policy was set during navigation, use it. If not, use the global settings.
        PopUpPolicy popUpPolicy = documentLoader->popUpPolicy();
        if (popUpPolicy == PopUpPolicy::Allow)
            return true;

        if (popUpPolicy == PopUpPolicy::Block)
            return false;
    }

    return UserGestureIndicator::processingUserGesture()
        || firstFrame.settings().javaScriptCanOpenWindowsAutomatically();
}

bool LocalDOMWindow::allowPopUp()
{
    RefPtr frame = this->frame();
    return frame && allowPopUp(*frame);
}

bool LocalDOMWindow::canShowModalDialog(const LocalFrame& frame)
{
    // Override support for layout testing purposes.
    if (RefPtr document = frame.document()) {
        if (RefPtr window = document->domWindow()) {
            if (window->m_canShowModalDialogOverride)
                return window->m_canShowModalDialogOverride.value();
        }
    }

    RefPtr page = frame.page();
    return page && page->chrome().canRunModal();
}

static void languagesChangedCallback(void* context)
{
    Ref window = *static_cast<LocalDOMWindow*>(context);
    window->languagesChanged();
}

void LocalDOMWindow::setCanShowModalDialogOverride(bool allow)
{
    m_canShowModalDialogOverride = allow;
}

LocalDOMWindow::LocalDOMWindow(Document& document)
    : DOMWindow(GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() }, DOMWindowType::Local)
    , ContextDestructionObserver(&document)
{
    ASSERT(frame());
    addLanguageChangeObserver(this, &languagesChangedCallback);
}

void LocalDOMWindow::didSecureTransitionTo(Document& document)
{
    RefPtr oldDocument = downcast<Document>(scriptExecutionContext());
    observeContext(&document);

    if (auto* eventTargetData = this->eventTargetData()) {
        eventTargetData->eventListenerMap.enumerateEventListenerTypes([&](auto& eventType, unsigned count) {
            if (oldDocument)
                oldDocument->didRemoveEventListenersOfType(eventType, count);
            document.didAddEventListenersOfType(eventType, count);
        });
    }

    // The Window is being transferred from one document to another so we need to reset data
    // members that store the window's document (rather than the window itself).
    m_crypto = nullptr;
    m_navigator = nullptr;
    m_performance = nullptr;
    m_customElementRegistry = nullptr;
}

void LocalDOMWindow::prewarmLocalStorageIfNecessary()
{
    RefPtr page = this->page();

    // No need to prewarm for ephemeral sessions since the data is in memory only.
    if (!page || page->usesEphemeralSession())
        return;

    // This eagerly constructs the StorageArea, which will load items from disk.
    auto localStorageResult = this->localStorage();
    if (localStorageResult.hasException())
        return;

    RefPtr localStorage = localStorageResult.returnValue();
    if (!localStorage)
        return;

    localStorage->protectedArea()->prewarm();
}

LocalDOMWindow::~LocalDOMWindow()
{
    if (m_suspendedForDocumentSuspension)
        willDestroyCachedFrame();
    else
        willDestroyDocumentInFrame();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);

#if ENABLE(GAMEPAD)
    if (m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(*this);
#endif

    removeLanguageChangeObserver(this);

    windowsInterestedInStorageEvents().remove(*this);
}

RefPtr<MediaQueryList> LocalDOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* LocalDOMWindow::page() const
{
    return frame() ? frame()->page() : nullptr;
}

RefPtr<Page> LocalDOMWindow::protectedPage() const
{
    return page();
}

void LocalDOMWindow::frameDestroyed()
{
    Ref protectedThis { *this };

    willDestroyDocumentInFrame();
    JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(this);
}

void LocalDOMWindow::willDestroyCachedFrame()
{
    // It is necessary to copy m_observers to a separate vector because the LocalDOMWindowObserver may
    // unregister themselves from the LocalDOMWindow as a result of the call to willDestroyGlobalObjectInCachedFrame.
    m_observers.forEach([](auto& observer) {
        observer.willDestroyGlobalObjectInCachedFrame();
    });
}

void LocalDOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_observers to a separate vector because the LocalDOMWindowObserver may
    // unregister themselves from the LocalDOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    m_observers.forEach([](auto& observer) {
        observer.willDestroyGlobalObjectInFrame();
    });
}

void LocalDOMWindow::willDetachDocumentFromFrame()
{
    if (!frame())
        return;

    RELEASE_ASSERT(!m_isSuspendingObservers);

    // It is necessary to copy m_observers to a separate vector because the LocalDOMWindowObserver may
    // unregister themselves from the LocalDOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    m_observers.forEach([](auto& observer) {
        observer.willDetachGlobalObjectFromFrame();
    });

    if (RefPtr performance = m_performance)
        performance->clearResourceTimings();

    windowsInterestedInStorageEvents().remove(*this);

    JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(this);
    InspectorInstrumentation::frameWindowDiscarded(*protectedFrame(), this);
}

#if ENABLE(GAMEPAD)

void LocalDOMWindow::incrementGamepadEventListenerCount()
{
    if (++m_gamepadEventListenerCount == 1)
        GamepadManager::singleton().registerDOMWindow(*this);
}

void LocalDOMWindow::decrementGamepadEventListenerCount()
{
    ASSERT(m_gamepadEventListenerCount);

    if (!--m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(*this);
}

#endif

void LocalDOMWindow::registerObserver(LocalDOMWindowObserver& observer)
{
    m_observers.add(observer);
}

void LocalDOMWindow::unregisterObserver(LocalDOMWindowObserver& observer)
{
    m_observers.remove(observer);
}

void LocalDOMWindow::resetUnlessSuspendedForDocumentSuspension()
{
    if (m_suspendedForDocumentSuspension)
        return;
    willDestroyDocumentInFrame();
}

void LocalDOMWindow::suspendForBackForwardCache()
{
    SetForScope isSuspendingObservers(m_isSuspendingObservers, true);
    RELEASE_ASSERT(frame());

    m_observers.forEach([](auto& observer) {
        observer.suspendForBackForwardCache();
    });
    RELEASE_ASSERT(frame());

    m_suspendedForDocumentSuspension = true;
}

void LocalDOMWindow::resumeFromBackForwardCache()
{
    m_observers.forEach([](auto& observer) {
        observer.resumeFromBackForwardCache();
    });

    m_suspendedForDocumentSuspension = false;
}

bool LocalDOMWindow::isCurrentlyDisplayedInFrame() const
{
    RefPtr frame = this->frame();
    return frame && frame->document()->domWindow() == this;
}

CustomElementRegistry& LocalDOMWindow::ensureCustomElementRegistry()
{
    if (!m_customElementRegistry)
        m_customElementRegistry = CustomElementRegistry::create(*this, scriptExecutionContext());
    ASSERT(m_customElementRegistry->scriptExecutionContext() == document());
    return *m_customElementRegistry;
}

static ExceptionOr<SelectorQuery&> selectorQueryInFrame(LocalFrame* frame, const String& selectors)
{
    if (!frame)
        return Exception { ExceptionCode::NotSupportedError };

    RefPtr document = frame->document();
    if (!document)
        return Exception { ExceptionCode::NotSupportedError };

    return document->selectorQueryForString(selectors);
}

ExceptionOr<Ref<NodeList>> LocalDOMWindow::collectMatchingElementsInFlatTree(Node& scope, const String& selectors)
{
    auto queryOrException = selectorQueryInFrame(frame(), selectors);
    if (queryOrException.hasException())
        return queryOrException.releaseException();

    RefPtr scopeContainer = dynamicDowncast<ContainerNode>(scope);
    if (!scopeContainer)
        return Ref<NodeList> { StaticElementList::create() };

    SelectorQuery& query = queryOrException.releaseReturnValue();

    Vector<Ref<Element>> result;
    for (auto& node : composedTreeDescendants(*scopeContainer)) {
        if (RefPtr element = dynamicDowncast<Element>(node); element && query.matches(*element) && !element->isInUserAgentShadowTree())
            result.append(element.releaseNonNull());
    }

    return Ref<NodeList> { StaticElementList::create(WTFMove(result)) };
}

ExceptionOr<RefPtr<Element>> LocalDOMWindow::matchingElementInFlatTree(Node& scope, const String& selectors)
{
    auto queryOrException = selectorQueryInFrame(frame(), selectors);
    if (queryOrException.hasException())
        return queryOrException.releaseException();

    RefPtr scopeContainer = dynamicDowncast<ContainerNode>(scope);
    if (!scopeContainer)
        return RefPtr<Element> { nullptr };

    SelectorQuery& query = queryOrException.releaseReturnValue();

    for (auto& node : composedTreeDescendants(*scopeContainer)) {
        if (RefPtr element = dynamicDowncast<Element>(node); element && query.matches(*element) && !element->isInUserAgentShadowTree())
            return element;
    }

    return RefPtr<Element> { nullptr };
}

#if ENABLE(ORIENTATION_EVENTS)
IntDegrees LocalDOMWindow::orientation() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    return frame->orientation();
}
#endif

Screen& LocalDOMWindow::screen()
{
    if (!m_screen)
        m_screen = Screen::create(*this);
    return *m_screen;
}

History& LocalDOMWindow::history()
{
    if (!m_history)
        m_history = History::create(*this);
    return *m_history;
}

Navigation& LocalDOMWindow::navigation()
{
    if (!m_navigation)
        m_navigation = Navigation::create(*this);
    return *m_navigation;
}

Ref<Navigation> LocalDOMWindow::protectedNavigation()
{
    return navigation();
}

Crypto& LocalDOMWindow::crypto() const
{
    if (!m_crypto)
        m_crypto = Crypto::create(protectedDocument().get());
    ASSERT(m_crypto->scriptExecutionContext() == document());
    return *m_crypto;
}

BarProp& LocalDOMWindow::locationbar()
{
    if (!m_locationbar)
        m_locationbar = BarProp::create(*this, BarProp::Locationbar);
    return *m_locationbar;
}

BarProp& LocalDOMWindow::menubar()
{
    if (!m_menubar)
        m_menubar = BarProp::create(*this, BarProp::Menubar);
    return *m_menubar;
}

BarProp& LocalDOMWindow::personalbar()
{
    if (!m_personalbar)
        m_personalbar = BarProp::create(*this, BarProp::Personalbar);
    return *m_personalbar;
}

BarProp& LocalDOMWindow::scrollbars()
{
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(*this, BarProp::Scrollbars);
    return *m_scrollbars;
}

BarProp& LocalDOMWindow::statusbar()
{
    if (!m_statusbar)
        m_statusbar = BarProp::create(*this, BarProp::Statusbar);
    return *m_statusbar;
}

BarProp& LocalDOMWindow::toolbar()
{
    if (!m_toolbar)
        m_toolbar = BarProp::create(*this, BarProp::Toolbar);
    return *m_toolbar;
}

Navigator& LocalDOMWindow::navigator()
{
    if (!m_navigator)
        m_navigator = Navigator::create(protectedScriptExecutionContext().get(), *this);
    ASSERT(m_navigator->scriptExecutionContext() == document());

    return *m_navigator;
}

Ref<Navigator> LocalDOMWindow::protectedNavigator()
{
    return navigator();
}

Performance& LocalDOMWindow::performance() const
{
    if (!m_performance) {
        RefPtr documentLoader = document() ? document()->loader() : nullptr;
        auto timeOrigin = documentLoader ? documentLoader->timing().timeOrigin() : MonotonicTime::now();
        m_performance = Performance::create(protectedDocument().get(), timeOrigin);
    }
    ASSERT(m_performance->scriptExecutionContext() == document());
    return *m_performance;
}

Ref<Performance> LocalDOMWindow::protectedPerformance() const
{
    return performance();
}

ReducedResolutionSeconds LocalDOMWindow::nowTimestamp() const
{
    return performance().nowInReducedResolutionSeconds();
}

void LocalDOMWindow::freezeNowTimestamp()
{
    m_frozenNowTimestamp = nowTimestamp();
}

void LocalDOMWindow::unfreezeNowTimestamp()
{
    m_frozenNowTimestamp = std::nullopt;
}

ReducedResolutionSeconds LocalDOMWindow::frozenNowTimestamp() const
{
    return m_frozenNowTimestamp.value_or(nowTimestamp());
}

VisualViewport& LocalDOMWindow::visualViewport()
{
    if (!m_visualViewport)
        m_visualViewport = VisualViewport::create(*this);
    return *m_visualViewport;
}

#if ENABLE(USER_MESSAGE_HANDLERS)

bool LocalDOMWindow::shouldHaveWebKitNamespaceForWorld(DOMWrapperWorld& world)
{
    RefPtr frame = this->frame();
    if (!frame)
        return false;

    RefPtr page = frame->page();
    if (!page)
        return false;

    bool hasUserMessageHandler = false;
    page->protectedUserContentProvider()->forEachUserMessageHandler([&](const UserMessageHandlerDescriptor& descriptor) {
        if (&descriptor.world() == &world) {
            hasUserMessageHandler = true;
            return;
        }
    });

    return hasUserMessageHandler;
}

WebKitNamespace* LocalDOMWindow::webkitNamespace()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    RefPtr page = frame()->page();
    if (!page)
        return nullptr;
    if (!m_webkitNamespace)
        m_webkitNamespace = WebKitNamespace::create(*this, page->protectedUserContentProvider());
    return m_webkitNamespace.get();
}

#endif

ExceptionOr<Storage*> LocalDOMWindow::sessionStorage()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    if (document->canAccessResource(ScriptExecutionContext::ResourceType::SessionStorage) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { ExceptionCode::SecurityError };

    if (m_sessionStorage)
        return m_sessionStorage.get();

    RefPtr page = document->page();
    if (!page)
        return nullptr;

    Ref storageArea = page->storageNamespaceProvider().sessionStorageArea(*document);
    m_sessionStorage = Storage::create(*this, WTFMove(storageArea));
    if (hasEventListeners(eventNames().storageEvent))
        windowsInterestedInStorageEvents().add(*this);
    return m_sessionStorage.get();
}

ExceptionOr<Storage*> LocalDOMWindow::localStorage()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    if (document->canAccessResource(ScriptExecutionContext::ResourceType::LocalStorage) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { ExceptionCode::SecurityError };

    RefPtr page = document->page();
    // FIXME: We should consider supporting access/modification to local storage
    // after calling window.close(). See <https://bugs.webkit.org/show_bug.cgi?id=135330>.
    if (!page || !page->isClosing()) {
        if (m_localStorage)
            return m_localStorage.get();
    }

    if (!page)
        return nullptr;

    if (page->isClosing())
        return nullptr;

    if (!page->settings().localStorageEnabled())
        return nullptr;

    Ref storageArea = page->storageNamespaceProvider().localStorageArea(*document);
    m_localStorage = Storage::create(*this, WTFMove(storageArea));
    if (hasEventListeners(eventNames().storageEvent))
        windowsInterestedInStorageEvents().add(*this);
    return m_localStorage.get();
}

void LocalDOMWindow::processPostMessage(JSC::JSGlobalObject& lexicalGlobalObject, const String& sourceOrigin, const MessageWithMessagePorts& message, RefPtr<WindowProxy>&& incumbentWindowProxy, RefPtr<SecurityOrigin>&& targetOrigin)
{
    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtr document = this->document();
    RefPtr<ScriptCallStack> stackTrace;
    if (InspectorInstrumentation::consoleAgentEnabled(document.get()))
        stackTrace = createScriptCallStack(JSExecState::currentState());

    auto postMessageIdentifier = InspectorInstrumentation::willPostMessage(*frame());

    auto userGestureToForward = UserGestureIndicator::currentUserGesture();

    document->checkedEventLoop()->queueTask(TaskSource::PostedMessageQueue, [this, protectedThis = Ref { *this }, message = message, incumbentWindowProxy = WTFMove(incumbentWindowProxy), sourceOrigin, userGestureToForward = WTFMove(userGestureToForward), postMessageIdentifier, stackTrace = WTFMove(stackTrace), targetOrigin = WTFMove(targetOrigin)]() mutable {
        if (!isCurrentlyDisplayedInFrame())
            return;

        RefPtr document = this->document();
        Ref frame = *this->frame();
        if (targetOrigin) {
            // Check target origin now since the target document may have changed since the timer was scheduled.
            if (!targetOrigin->isSameSchemeHostPort(document->protectedSecurityOrigin())) {
                if (CheckedPtr pageConsole = console()) {
                    auto message = makeString("Unable to post message to "_s, targetOrigin->toString(), ". Recipient has origin "_s, document->securityOrigin().toString(), ".\n"_s);
                    if (stackTrace)
                        pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message, *stackTrace);
                    else
                        pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message);
                }

                InspectorInstrumentation::didFailPostMessage(frame, postMessageIdentifier);
                return;
            }
        }

        auto* globalObject = document->globalObject();
        if (!globalObject)
            return;

        auto& vm = globalObject->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        UserGestureIndicator userGestureIndicator(userGestureToForward);
        InspectorInstrumentation::willDispatchPostMessage(frame, postMessageIdentifier);

        auto ports = MessagePort::entanglePorts(*document, WTFMove(message.transferredPorts));
        auto event = MessageEvent::create(*globalObject, message.message.releaseNonNull(), sourceOrigin, { }, incumbentWindowProxy ? std::make_optional(MessageEventSource(WTFMove(incumbentWindowProxy))) : std::nullopt, WTFMove(ports));
        if (UNLIKELY(scope.exception())) {
            // Currently, we assume that the only way we can get here is if we have a termination.
            RELEASE_ASSERT(vm.hasPendingTerminationException());
            return;
        }

        dispatchEvent(event.event);

        InspectorInstrumentation::didDispatchPostMessage(frame, postMessageIdentifier);
    });

    InspectorInstrumentation::didPostMessage(*protectedFrame(), postMessageIdentifier, lexicalGlobalObject);
}

ExceptionOr<void> LocalDOMWindow::postMessage(JSC::JSGlobalObject& lexicalGlobalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue messageValue, WindowPostMessageOptions&& options)
{
    if (!isCurrentlyDisplayedInFrame())
        return { };

    RefPtr sourceDocument = incumbentWindow.document();
    if (!sourceDocument)
        return { };

    auto targetSecurityOrigin = createTargetOriginForPostMessage(options.targetOrigin, *sourceDocument);
    if (targetSecurityOrigin.hasException())
        return targetSecurityOrigin.releaseException();

    Vector<Ref<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(lexicalGlobalObject, messageValue, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WindowPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    auto disentangledPorts = MessagePort::disentanglePorts(WTFMove(ports));
    if (disentangledPorts.hasException())
        return disentangledPorts.releaseException();

    // Capture the source of the message. We need to do this synchronously
    // in order to capture the source of the message correctly.
    auto sourceOrigin = sourceDocument->securityOrigin().toString();

    // Schedule the message.
    RefPtr incumbentWindowProxy = incumbentWindow.frame() ? &incumbentWindow.frame()->windowProxy() : nullptr;
    MessageWithMessagePorts message { messageData.releaseReturnValue(), disentangledPorts.releaseReturnValue() };
    processPostMessage(lexicalGlobalObject, sourceOrigin, message, WTFMove(incumbentWindowProxy), targetSecurityOrigin.releaseReturnValue());
    return { };
}

void LocalDOMWindow::postMessageFromRemoteFrame(JSC::JSGlobalObject& lexicalGlobalObject, RefPtr<WindowProxy>&& source, const String& sourceOrigin, std::optional<WebCore::SecurityOriginData>&& targetOriginData, const WebCore::MessageWithMessagePorts& message)
{
    if (!frame())
        return;

    RefPtr<SecurityOrigin> targetOrigin;
    if (targetOriginData)
        targetOrigin = targetOriginData->securityOrigin();

    processPostMessage(lexicalGlobalObject, sourceOrigin, message, WTFMove(source), WTFMove(targetOrigin));
}

DOMSelection* LocalDOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_selection)
        m_selection = DOMSelection::create(*this);
    return m_selection.get();
}

HTMLFrameOwnerElement* LocalDOMWindow::frameElement() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    return frame->ownerElement();
}

RefPtr<HTMLFrameOwnerElement> LocalDOMWindow::protectedFrameElement() const
{
    return frameElement();
}

void LocalDOMWindow::focus(LocalDOMWindow& incumbentWindow)
{
    RefPtr frame = this->frame();
    RefPtr openerFrame = frame ? frame->opener() : nullptr;
    focus([&] {
        if (!openerFrame || openerFrame == frame || incumbentWindow.frame() != openerFrame)
            return false;

        RefPtr page = openerFrame->page();
        return page && page->isVisibleAndActive();
    }());
}

void LocalDOMWindow::focus(bool allowFocus)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    allowFocus = allowFocus || WindowFocusAllowedIndicator::windowFocusAllowed() || !frame->settings().windowFocusRestricted();

    // If we're a top level window, bring the window to the front.
    if (frame->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!frame->hasHadUserInteraction() && !isSameSecurityOriginAsMainFrame())
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    RefPtr focusedFrame = page->checkedFocusController()->focusedLocalFrame();
    if (focusedFrame && focusedFrame != frame)
        focusedFrame->protectedDocument()->setFocusedElement(nullptr);

    frame->checkedEventHandler()->focusDocumentView();
}

void LocalDOMWindow::blur()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    if (frame->settings().windowFocusRestricted())
        return;

    if (!frame->isMainFrame())
        return;

    page->chrome().unfocus();
}

void LocalDOMWindow::closePage()
{
    protectedDocument()->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [this, protectedThis = Ref { *this }] {
        // Calling closeWindow() may destroy the page.
        if (RefPtr page = this->page())
            page->chrome().closeWindow();
    });
}

void LocalDOMWindow::print()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.print is not allowed while unloading a page."_s);
        return;
    }

    if (page->isControlledByAutomation())
        return;

    if (RefPtr loader = frame->loader().activeDocumentLoader(); loader && loader->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(*frame);
}

void LocalDOMWindow::stop()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    SetForScope isStopping { m_isStopping, true };
    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    frame->protectedLoader()->stopForUserCancel(true);
}

void LocalDOMWindow::alert(const String& message)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxFlag::Modals)) {
        printErrorMessage("Use of window.alert is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return;
    }

    RefPtr page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.alert is not allowed while unloading a page."_s);
        return;
    }

    document->updateStyleIfNeeded();
#if ENABLE(POINTER_LOCK)
    page->pointerLockController().requestPointerUnlock();
#endif

    page->chrome().runJavaScriptAlert(*frame, message);
}

bool LocalDOMWindow::confirmForBindings(const String& message)
{
    RefPtr frame = this->frame();
    if (!frame)
        return false;

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxFlag::Modals)) {
        printErrorMessage("Use of window.confirm is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return false;
    }

    RefPtr page = frame->page();
    if (!page)
        return false;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.confirm is not allowed while unloading a page."_s);
        return false;
    }

    document->updateStyleIfNeeded();
#if ENABLE(POINTER_LOCK)
    page->pointerLockController().requestPointerUnlock();
#endif

    return page->chrome().runJavaScriptConfirm(*frame, message);
}

String LocalDOMWindow::prompt(const String& message, const String& defaultValue)
{
    RefPtr frame = this->frame();
    if (!frame)
        return String();

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxFlag::Modals)) {
        printErrorMessage("Use of window.prompt is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return String();
    }

    RefPtr page = frame->page();
    if (!page)
        return String();

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.prompt is not allowed while unloading a page."_s);
        return String();
    }

    document->updateStyleIfNeeded();
#if ENABLE(POINTER_LOCK)
    page->pointerLockController().requestPointerUnlock();
#endif

    String returnValue;
    if (page->chrome().runJavaScriptPrompt(*frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

bool LocalDOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    // SearchBuffer allocates memory much larger than the searched string, so it's necessary to limit its length.
    // Most searches are for a phrase or a paragraph, so an upper limit of 64kB is more than enough in practice.
    constexpr auto maximumStringLength = std::numeric_limits<uint16_t>::max();
    if (!isCurrentlyDisplayedInFrame() || string.length() > maximumStringLength)
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog.    
    FindOptions options { FindOption::DoNotTraverseFlatTree };
    if (backwards)
        options.add(FindOption::Backwards);
    if (!caseSensitive)
        options.add(FindOption::CaseInsensitive);
    if (wrap)
        options.add(FindOption::WrapAround);
    return frame()->editor().findString(string, options);
}

bool LocalDOMWindow::offscreenBuffering() const
{
    return true;
}

int LocalDOMWindow::outerHeight() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr page = frame->page();
    if (!page)
        return 0;

    if (page->shouldApplyScreenFingerprintingProtections(*protectedDocument()))
        return innerHeight();

#if PLATFORM(IOS_FAMILY)
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return 0;

    RefPtr view = frame->isMainFrame() ? frame->view() : localFrame->view();
    if (!view)
        return 0;

    return view->frameRect().height();
#else
    return static_cast<int>(page->chrome().windowRect().height());
#endif
}

int LocalDOMWindow::outerWidth() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr page = frame->page();
    if (!page)
        return 0;

    if (page->shouldApplyScreenFingerprintingProtections(*protectedDocument()))
        return innerWidth();

#if PLATFORM(IOS_FAMILY)
    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return 0;

    RefPtr view = frame->isMainFrame() ? frame->view() : localFrame->view();
    if (!view)
        return 0;

    return view->frameRect().width();
#else
    return static_cast<int>(page->chrome().windowRect().width());
#endif
}

int LocalDOMWindow::innerHeight() const
{
    if (!frame())
        return 0;
    
    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (RefPtr ownerElement = frameElement())
        ownerElement->protectedDocument()->updateLayoutIfDimensionsOutOfDate(*ownerElement, { DimensionsCheck::Height });

    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    
    RefPtr view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int LocalDOMWindow::innerWidth() const
{
    if (!frame())
        return 0;

    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (RefPtr ownerElement = frameElement())
        ownerElement->protectedDocument()->updateLayoutIfDimensionsOutOfDate(*ownerElement, { DimensionsCheck::Width });

    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int LocalDOMWindow::screenX() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr page = frame->page();
    if (!page || page->shouldApplyScreenFingerprintingProtections(*protectedDocument()))
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int LocalDOMWindow::screenY() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr page = frame->page();
    if (!page || page->shouldApplyScreenFingerprintingProtections(*protectedDocument()))
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int LocalDOMWindow::scrollX() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr view = frame->view();
    if (!view)
        return 0;

    int scrollX = view->contentsScrollPosition().x();
    if (!scrollX)
        return 0;

    frame->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    // Layout may have affected the current frame:
    RefPtr frameAfterLayout = this->frame();
    if (!frameAfterLayout)
        return 0;

    RefPtr viewAfterLayout = frameAfterLayout->view();
    if (!viewAfterLayout)
        return 0;

    return viewAfterLayout->mapFromLayoutToCSSUnits(viewAfterLayout->contentsScrollPosition().x());
}

int LocalDOMWindow::scrollY() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr view = frame->view();
    if (!view)
        return 0;

    int scrollY = view->contentsScrollPosition().y();
    if (!scrollY)
        return 0;

    frame->protectedDocument()->updateLayoutIgnorePendingStylesheets();

    // Layout may have affected the current frame:
    RefPtr frameAfterLayout = this->frame();
    if (!frameAfterLayout)
        return 0;

    RefPtr viewAfterLayout = frameAfterLayout->view();
    if (!viewAfterLayout)
        return 0;

    return viewAfterLayout->mapFromLayoutToCSSUnits(viewAfterLayout->contentsScrollPosition().y());
}

unsigned LocalDOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return protectedFrame()->tree().scopedChildCount();
}

AtomString LocalDOMWindow::name() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullAtom();

    return frame->tree().specifiedName();
}

void LocalDOMWindow::setName(const AtomString& string)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    frame->tree().setSpecifiedName(string);
    frame->protectedLoader()->client().frameNameChanged(string.string());
}

void LocalDOMWindow::setStatus(const String& string)
{
    m_status = string;
}

void LocalDOMWindow::disownOpener()
{
    if (RefPtr frame = this->frame())
        frame->disownOpener();
}

String LocalDOMWindow::origin() const
{
    RefPtr document = this->document();
    return document ? document->securityOrigin().toString() : emptyString();
}

SecurityOrigin* LocalDOMWindow::securityOrigin() const
{
    RefPtr document = this->document();
    return document ? &document->securityOrigin() : nullptr;
}

Document* LocalDOMWindow::document() const
{
    return downcast<Document>(ContextDestructionObserver::scriptExecutionContext());
}

RefPtr<Document> LocalDOMWindow::protectedDocument() const
{
    return document();
}

void LocalDOMWindow::overrideTransientActivationDurationForTesting(std::optional<Seconds>&& override)
{
    transientActivationDurationOverrideForTesting() = WTFMove(override);
}

// When the current high resolution time is greater than or equal to the last activation timestamp in W, and
// less than the last activation timestamp in W plus the transient activation duration, then W is said to
// have transient activation. (https://html.spec.whatwg.org/multipage/interaction.html#transient-activation)
bool LocalDOMWindow::hasTransientActivation() const
{
    auto now = MonotonicTime::now();
    return now >= m_lastActivationTimestamp && now < (m_lastActivationTimestamp + transientActivationDuration());
}

// When the current high resolution time given W is greater than or equal to the last activation timestamp in W,
// W is said to have sticky activation. (https://html.spec.whatwg.org/multipage/interaction.html#sticky-activation)
bool LocalDOMWindow::hasStickyActivation() const
{
    auto now = MonotonicTime::now();
    return now >= m_lastActivationTimestamp;
}

// When the last history-action activation timestamp of W is not equal to the last activation timestamp of W,
// then W is said to have history-action activation.
// (https://html.spec.whatwg.org/multipage/interaction.html#history-action-activation)
bool LocalDOMWindow::hasHistoryActionActivation() const
{
    return m_lastHistoryActionActivationTimestamp != m_lastActivationTimestamp;
}

// https://html.spec.whatwg.org/multipage/interaction.html#consume-user-activation
bool LocalDOMWindow::consumeTransientActivation()
{
    if (!hasTransientActivation())
        return false;

    for (RefPtr<Frame> frame = this->frame() ? &this->frame()->tree().top() : nullptr; frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        if (RefPtr window = localFrame->window())
            window->consumeLastActivationIfNecessary();
    }

    return true;
}

void LocalDOMWindow::consumeLastActivationIfNecessary()
{
    if (!m_lastActivationTimestamp.isInfinity())
        m_lastActivationTimestamp = -MonotonicTime::infinity();
}

// https://html.spec.whatwg.org/multipage/interaction.html#consume-history-action-user-activation
bool LocalDOMWindow::consumeHistoryActionUserActivation()
{
    if (!hasHistoryActionActivation())
        return false;

    for (RefPtr<Frame> frame = this->frame() ? &this->frame()->tree().top() : nullptr; frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        if (RefPtr window = localFrame->window())
            window->m_lastHistoryActionActivationTimestamp = window->m_lastActivationTimestamp;
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/interaction.html#activation-notification
void LocalDOMWindow::notifyActivated(MonotonicTime activationTime)
{
    setLastActivationTimestamp(activationTime);
    if (!frame())
        return;

    for (RefPtr ancestor = frame() ? frame()->tree().parent() : nullptr; ancestor; ancestor = ancestor->tree().parent()) {
        RefPtr localAncestor = dynamicDowncast<LocalFrame>(ancestor.get());
        if (!localAncestor)
            continue;
        if (RefPtr window = localAncestor->window())
            window->setLastActivationTimestamp(activationTime);
    }

    RefPtr securityOrigin = this->securityOrigin();
    if (!securityOrigin)
        return;

    RefPtr<Frame> descendant = frame();
    while ((descendant = descendant->tree().traverseNext(frame()))) {
        RefPtr localDescendant = dynamicDowncast<LocalFrame>(descendant.get());
        if (!localDescendant)
            continue;
        RefPtr descendantWindow = localDescendant->window();
        if (!descendantWindow)
            continue;

        RefPtr descendantSecurityOrigin = descendantWindow->securityOrigin();
        if (!descendantSecurityOrigin || !descendantSecurityOrigin->isSameOriginAs(*securityOrigin))
            continue;

        descendantWindow->setLastActivationTimestamp(activationTime);
    }
}

StyleMedia& LocalDOMWindow::styleMedia()
{
    if (!m_media)
        m_media = StyleMedia::create(*this);
    return *m_media;
}

Ref<CSSStyleDeclaration> LocalDOMWindow::getComputedStyle(Element& element, const String& pseudoElt) const
{
    if (!pseudoElt.startsWith(':'))
        return CSSComputedStyleDeclaration::create(element, std::nullopt);

    // FIXME: This does not work for pseudo-elements that take arguments (webkit.org/b/264103).
    auto [pseudoElementIsParsable, pseudoElementIdentifier] = CSSSelectorParser::parsePseudoElement(pseudoElt, CSSSelectorParserContext { element.protectedDocument() });
    if (!pseudoElementIsParsable)
        return CSSComputedStyleDeclaration::createEmpty(element);
    // FIXME: CSSSelectorParser::parsePseudoElement should never return PseudoId::None.
    return CSSComputedStyleDeclaration::create(element, pseudoElementIdentifier);
}

RefPtr<CSSRuleList> LocalDOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement, bool authorOnly) const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    // FIXME: This parser context won't get the right settings without a document.
    auto parserContext = document() ? CSSSelectorParserContext { *protectedDocument() } : CSSSelectorParserContext { CSSParserContext { HTMLStandardMode } };
    auto [pseudoElementIsParsable, pseudoElementIdentifier] = CSSSelectorParser::parsePseudoElement(pseudoElement, parserContext);
    if (!(pseudoElementIsParsable || (pseudoElementIdentifier && !pseudoElementIdentifier->nameArgument.isNull())) && !pseudoElement.isEmpty())
        return nullptr;

    RefPtr frame = this->frame();
    frame->protectedDocument()->styleScope().flushPendingUpdate();

    unsigned rulesToInclude = Style::Resolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= Style::Resolver::UAAndUserCSSRules;

    auto matchedRules = frame->document()->styleScope().resolver().pseudoStyleRulesForElement(element, pseudoElementIdentifier, rulesToInclude);
    if (matchedRules.isEmpty())
        return nullptr;

    bool allowCrossOrigin = frame->settings().crossOriginCheckInGetMatchedCSSRulesDisabled();

    auto ruleList = StaticCSSRuleList::create();
    for (auto& rule : matchedRules) {
        if (!allowCrossOrigin && !rule->hasDocumentSecurityOrigin())
            continue;
        ruleList->rules().append(rule->createCSSOMWrapper());
    }

    if (ruleList->rules().isEmpty())
        return nullptr;

    return ruleList;
}

RefPtr<WebKitPoint> LocalDOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    document->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

RefPtr<WebKitPoint> LocalDOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    document->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return WebKitPoint::create(nodePoint.x(), nodePoint.y());
}

double LocalDOMWindow::devicePixelRatio() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0.0;

    RefPtr page = frame->page();
    if (!page)
        return 0.0;

    return page->deviceScaleFactor();
}

void LocalDOMWindow::scrollBy(double x, double y) const
{
    scrollBy(ScrollToOptions(x, y));
}

void LocalDOMWindow::scrollBy(const ScrollToOptions& options) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    protectedDocument()->updateLayoutIgnorePendingStylesheets();

    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr view = frame->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    FloatSize originalDelta(scrollToOptions.left.value(), scrollToOptions.top.value());
    scrollToOptions.left.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
    scrollToOptions.top.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
    scrollTo(scrollToOptions, ScrollClamping::Clamped, ScrollSnapPointSelectionMethod::Directional, originalDelta);
}

void LocalDOMWindow::scrollTo(double x, double y, ScrollClamping clamping) const
{
    scrollTo(ScrollToOptions(x, y), clamping);
}

void LocalDOMWindow::scrollTo(const ScrollToOptions& options, ScrollClamping clamping, ScrollSnapPointSelectionMethod snapPointSelectionMethod, std::optional<FloatSize> originalScrollDelta) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr view = frame()->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        view->contentsScrollPosition().x(), view->contentsScrollPosition().y()
    );

    // This is an optimization for the common case of scrolling to (0, 0) when the scroller is already at the origin.
    // If an animated scroll is in progress, this optimization is skipped to ensure that the animated scroll is really stopped.
    if (view->scrollAnimationStatus() == ScrollAnimationStatus::NotAnimating && !scrollToOptions.left.value() && !scrollToOptions.top.value() && view->contentsScrollPosition().isZero()) {
        LOG_WITH_STREAM(Scrolling, stream << "LocalDOMWindow::scrollTo bailing because going to 0,0");
        return;
    }

    view->cancelScheduledScrolls();
    protectedDocument()->updateLayoutIgnorePendingStylesheets(LayoutOptions::UpdateCompositingLayers);

    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(scrollToOptions.left.value()), view->mapFromCSSToLayoutUnits(scrollToOptions.top.value()));

    // FIXME: Should we use document()->scrollingElement()?
    // See https://bugs.webkit.org/show_bug.cgi?id=205059
    auto animated = useSmoothScrolling(scrollToOptions.behavior.value_or(ScrollBehavior::Auto), document()->protectedDocumentElement().get()) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    auto scrollPositionChangeOptions = ScrollPositionChangeOptions::createProgrammaticWithOptions(clamping, animated, snapPointSelectionMethod, originalScrollDelta);
    view->setContentsScrollPosition(layoutPos, scrollPositionChangeOptions);
}

bool LocalDOMWindow::allowedToChangeWindowGeometry() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return false;
    if (!frame->page())
        return false;
    if (!frame->isMainFrame())
        return false;
    // Prevent web content from tricking the user into initiating a drag.
    if (frame->eventHandler().mousePressed())
        return false;
    return true;
}

void LocalDOMWindow::moveBy(int x, int y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    RefPtr page = frame()->page();
    auto fr = page->chrome().windowRect();
    auto update = fr;
    update.move(x, y);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void LocalDOMWindow::moveTo(int x, int y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    RefPtr page = frame()->page();
    auto update = page->chrome().windowRect();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->mainFrame());
    if (!localMainFrame)
        return;

    update.setLocation(LayoutPoint(x, y));
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void LocalDOMWindow::resizeBy(int x, int y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    RefPtr page = frame()->page();
    auto fr = page->chrome().windowRect();
    auto dest = fr.size() + LayoutSize(x, y);
    LayoutRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void LocalDOMWindow::resizeTo(int width, int height) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    RefPtr page = frame()->page();
    auto fr = page->chrome().windowRect();
    auto dest = LayoutSize(width, height);
    LayoutRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

ExceptionOr<int> LocalDOMWindow::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->checkedContentSecurityPolicy()->allowEval(context->globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*context, WTFMove(action), Seconds::fromMilliseconds(timeout), DOMTimer::Type::SingleShot);
}

void LocalDOMWindow::clearTimeout(int timeoutId)
{
    if (RefPtr context = scriptExecutionContext())
        DOMTimer::removeById(*context, timeoutId);
}

ExceptionOr<int> LocalDOMWindow::setInterval(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->checkedContentSecurityPolicy()->allowEval(context->globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*context, WTFMove(action), Seconds::fromMilliseconds(timeout), DOMTimer::Type::Repeating);
}

void LocalDOMWindow::clearInterval(int timeoutId)
{
    if (RefPtr context = scriptExecutionContext())
        DOMTimer::removeById(*context, timeoutId);
}

int LocalDOMWindow::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    RefPtr document = this->document();
    return document ? document->requestAnimationFrame(WTFMove(callback)) : 0;
}

int LocalDOMWindow::webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    static bool firstTime = true;
    if (firstTime && document()) {
        protectedDocument()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "webkitRequestAnimationFrame() is deprecated and will be removed. Please use requestAnimationFrame() instead."_s);
        firstTime = false;
    }
    return requestAnimationFrame(WTFMove(callback));
}

void LocalDOMWindow::cancelAnimationFrame(int id)
{
    if (RefPtr document = this->document())
        document->cancelAnimationFrame(id);
}

int LocalDOMWindow::requestIdleCallback(Ref<IdleRequestCallback>&& callback, const IdleRequestOptions& options)
{
    RefPtr document = this->document();
    return document ? document->requestIdleCallback(WTFMove(callback), Seconds::fromMilliseconds(options.timeout)) : 0;
}

void LocalDOMWindow::cancelIdleCallback(int id)
{
    if (RefPtr document = this->document())
        document->cancelIdleCallback(id);
}

void LocalDOMWindow::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), WTFMove(promise));
}

void LocalDOMWindow::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), sx, sy, sw, sh, WTFMove(promise));
}

bool LocalDOMWindow::isSecureContext() const
{
    RefPtr document = this->document();
    return document && document->isSecureContext();
}

bool LocalDOMWindow::crossOriginIsolated() const
{
    ASSERT(ScriptExecutionContext::crossOriginMode() == CrossOriginMode::Shared || !document() || document()->topDocument().crossOriginOpenerPolicy().value == CrossOriginOpenerPolicyValue::SameOriginPlusCOEP);
    return ScriptExecutionContext::crossOriginMode() == CrossOriginMode::Isolated;
}

static void didAddStorageEventListener(LocalDOMWindow& window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system. 
    window.localStorage();
    window.sessionStorage();
}

bool LocalDOMWindow::isSameSecurityOriginAsMainFrame() const
{
    RefPtr frame = this->frame();
    if (!frame || !frame->page() || !document())
        return false;

    if (frame->isMainFrame())
        return true;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return false;

    RefPtr mainFrameDocument = localFrame->document();

    if (mainFrameDocument && document()->protectedSecurityOrigin()->isSameOriginDomain(mainFrameDocument->protectedSecurityOrigin()))
        return true;

    return false;
}

bool LocalDOMWindow::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), options))
        return false;

    RefPtr document = this->document();
    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    if (document) {
        document->didAddEventListenersOfType(eventType);
        if (typeInfo.isInCategory(EventCategory::Wheel)) {
            document->didAddWheelEventHandler(*document);
            document->invalidateEventListenerRegions();
        } else if (isTouchRelatedEventType(typeInfo, *document))
            document->didAddTouchEventHandler(*document);
        else if (eventType == eventNames.storageEvent)
            didAddStorageEventListener(*this);
    }

    switch (typeInfo.type()) {
    case EventType::unload:
        addUnloadEventListener(this);
        break;
    case EventType::beforeunload:
        if (allowsBeforeUnloadListeners(this))
            addBeforeUnloadEventListener(this);
        break;
#if PLATFORM(IOS_FAMILY)
    case EventType::scroll:
        incrementScrollEventListenersCount();
        break;
#endif
#if ENABLE(DEVICE_ORIENTATION)
    case EventType::deviceorientation:
        startListeningForDeviceOrientationIfNecessary();
        break;
    case EventType::devicemotion:
        startListeningForDeviceMotionIfNecessary();
        break;
#endif
    default:
#if ENABLE(IOS_TOUCH_EVENTS)
        if (isTouchRelatedEventType(typeInfo, *document))
            ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
        if (typeInfo.isInCategory(EventCategory::Gesture))
            ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(GAMEPAD)
        if (typeInfo.isInCategory(EventCategory::Gamepad))
            incrementGamepadEventListenerCount();
#endif
        break;
    }

    return true;
}

#if ENABLE(DEVICE_ORIENTATION)

DeviceOrientationController* LocalDOMWindow::deviceOrientationController() const
{
#if PLATFORM(IOS_FAMILY)
    return document() ? &document()->deviceOrientationController() : nullptr;
#else
    return DeviceOrientationController::from(protectedPage().get());
#endif
}

DeviceMotionController* LocalDOMWindow::deviceMotionController() const
{
#if PLATFORM(IOS_FAMILY)
    return document() ? &document()->deviceMotionController() : nullptr;
#else
    return DeviceMotionController::from(protectedPage().get());
#endif
}

bool LocalDOMWindow::isAllowedToUseDeviceMotionOrOrientation(String& message) const
{
    if (!frame() || !document() || !frame()->settings().deviceOrientationEventEnabled()) {
        message = "API is disabled"_s;
        return false;
    }

    if (!isSecureContext()) {
        message = "Browsing context is not secure"_s;
        return false;
    }

    return true;
}

bool LocalDOMWindow::isAllowedToUseDeviceMotion(String& message) const
{
    if (!isAllowedToUseDeviceMotionOrOrientation(message))
        return false;

    Ref document = *this->document();
    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Gyroscope, document, PermissionsPolicy::ShouldReportViolation::No)
        || !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Accelerometer, document, PermissionsPolicy::ShouldReportViolation::No)) {
        message = "Third-party iframes are not allowed access to device motion unless explicitly allowed via Feature-Policy (gyroscope & accelerometer)"_s;
        return false;
    }

    return true;
}

bool LocalDOMWindow::isAllowedToUseDeviceOrientation(String& message) const
{
    if (!isAllowedToUseDeviceMotionOrOrientation(message))
        return false;

    Ref document = *this->document();
    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Gyroscope, document, PermissionsPolicy::ShouldReportViolation::No)
        || !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Accelerometer, document, PermissionsPolicy::ShouldReportViolation::No)
        || !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Magnetometer, document, PermissionsPolicy::ShouldReportViolation::No)) {
        message = "Third-party iframes are not allowed access to device orientation unless explicitly allowed via Feature-Policy (gyroscope & accelerometer & magnetometer)"_s;
        return false;
    }

    return true;
}

bool LocalDOMWindow::hasPermissionToReceiveDeviceMotionOrOrientationEvents(String& message) const
{
    if (frame()->settings().deviceOrientationPermissionAPIEnabled()) {
        if (!page()) {
            message = "No browsing context"_s;
            return false;
        }
        Ref document = *this->document();
        auto accessState = document->deviceOrientationAndMotionAccessController().accessState(document);
        switch (accessState) {
        case DeviceOrientationOrMotionPermissionState::Denied:
            message = "Permission to use the API was denied"_s;
            return false;
        case DeviceOrientationOrMotionPermissionState::Prompt:
            message = "Permission to use the API was not yet requested"_s;
            return false;
        case DeviceOrientationOrMotionPermissionState::Granted:
            break;
        }
    }

    return true;
}

void LocalDOMWindow::startListeningForDeviceOrientationIfNecessary()
{
    if (!hasEventListeners(eventNames().deviceorientationEvent))
        return;

    CheckedPtr deviceController = deviceOrientationController();
    if (!deviceController || deviceController->hasDeviceEventListener(*this))
        return;

    String innerMessage;
    if (!isAllowedToUseDeviceOrientation(innerMessage) || !hasPermissionToReceiveDeviceMotionOrOrientationEvents(innerMessage)) {
        if (RefPtr document = this->document())
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("No device orientation events will be fired, reason: "_s, innerMessage, '.'));
        return;
    }

    deviceController->addDeviceEventListener(*this);
}

void LocalDOMWindow::stopListeningForDeviceOrientationIfNecessary()
{
    if (hasEventListeners(eventNames().deviceorientationEvent))
        return;

    if (CheckedPtr deviceController = deviceOrientationController())
        deviceController->removeDeviceEventListener(*this);
}

void LocalDOMWindow::startListeningForDeviceMotionIfNecessary()
{
    if (!hasEventListeners(eventNames().devicemotionEvent))
        return;

    CheckedPtr deviceController = deviceMotionController();
    if (!deviceController || deviceController->hasDeviceEventListener(*this))
        return;

    String innerMessage;
    if (!isAllowedToUseDeviceMotion(innerMessage) || !hasPermissionToReceiveDeviceMotionOrOrientationEvents(innerMessage)) {
        failedToRegisterDeviceMotionEventListener();
        if (RefPtr document = this->document())
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("No device motion events will be fired, reason: "_s, innerMessage, '.'));
        return;
    }

    deviceController->addDeviceEventListener(*this);
}

void LocalDOMWindow::stopListeningForDeviceMotionIfNecessary()
{
    if (hasEventListeners(eventNames().devicemotionEvent))
        return;

    if (CheckedPtr deviceController = deviceMotionController())
        deviceController->removeDeviceEventListener(*this);
}

void LocalDOMWindow::failedToRegisterDeviceMotionEventListener()
{
#if PLATFORM(IOS_FAMILY)
    if (!isSameSecurityOriginAsMainFrame() || !isSecureContext())
        return;

    // FIXME: This is a quirk for chase.com on iPad (<rdar://problem/48423023>).
    if (RegistrableDomain::uncheckedCreateFromRegistrableDomainString("chase.com"_s).matches(document()->url())) {
        // Fire a fake DeviceMotionEvent with acceleration data to unblock the site's login flow.
        protectedDocument()->postTask([](auto& context) {
            if (RefPtr window = downcast<Document>(context).domWindow()) {
                auto acceleration = DeviceMotionData::Acceleration::create();
                window->dispatchEvent(DeviceMotionEvent::create(eventNames().devicemotionEvent, DeviceMotionData::create(acceleration.copyRef(), acceleration.copyRef(), DeviceMotionData::RotationRate::create(), std::nullopt).ptr()));
            }
        });
    }
#endif // PLATFORM(IOS_FAMILY)
}

#endif // ENABLE(DEVICE_ORIENTATION)

#if PLATFORM(IOS_FAMILY)

void LocalDOMWindow::incrementScrollEventListenersCount()
{
    RefPtr document = this->document();
    if (++m_scrollEventListenerCount == 1 && document == &document->topDocument()) {
        if (RefPtr frame = this->frame(); frame && frame->page())
            frame->protectedPage()->chrome().client().setNeedsScrollNotifications(*frame, true);
    }
}

void LocalDOMWindow::decrementScrollEventListenersCount()
{
    RefPtr document = this->document();
    if (!--m_scrollEventListenerCount && document == &document->topDocument()) {
        RefPtr frame = this->frame();
        if (frame && frame->page() && document->backForwardCacheState() == Document::NotInBackForwardCache)
            frame->protectedPage()->chrome().client().setNeedsScrollNotifications(*frame, false);
    }
}

#endif

void LocalDOMWindow::resetAllGeolocationPermission()
{
    // FIXME: Can we remove the PLATFORM(IOS_FAMILY)-guard?
#if ENABLE(GEOLOCATION) && PLATFORM(IOS_FAMILY)
    if (RefPtr navigator = m_navigator)
        NavigatorGeolocation::from(*navigator)->resetAllGeolocationPermission();
#endif
}

bool LocalDOMWindow::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
    if (!EventTarget::removeEventListener(eventType, listener, options.capture))
        return false;

    RefPtr document = this->document();
    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    if (document) {
        document->didRemoveEventListenersOfType(eventType);
        if (typeInfo.isInCategory(EventCategory::Wheel)) {
            document->didRemoveWheelEventHandler(*document);
            document->invalidateEventListenerRegions();
        } else if (isTouchRelatedEventType(typeInfo, *document))
            document->didRemoveTouchEventHandler(*document);
    }

    switch (typeInfo.type()) {
    case EventType::unload:
        removeUnloadEventListener(this);
        break;
    case EventType::beforeunload:
        if (allowsBeforeUnloadListeners(this))
            removeBeforeUnloadEventListener(this);
        break;
#if PLATFORM(IOS_FAMILY)
    case EventType::scroll:
        decrementScrollEventListenersCount();
        break;
#endif
#if ENABLE(DEVICE_ORIENTATION)
    case EventType::deviceorientation:
        stopListeningForDeviceOrientationIfNecessary();
        break;
    case EventType::devicemotion:
        stopListeningForDeviceMotionIfNecessary();
        break;
#endif
    default:
#if ENABLE(IOS_TOUCH_EVENTS)
        if (document && isTouchRelatedEventType(typeInfo, *document)) {
            ASSERT(m_touchAndGestureEventListenerCount > 0);
            --m_touchAndGestureEventListenerCount;
        }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
        if (typeInfo.isInCategory(EventCategory::Gesture)) {
            ASSERT(m_touchAndGestureEventListenerCount > 0);
            --m_touchAndGestureEventListenerCount;
        }
#endif
#if ENABLE(GAMEPAD)
        if (typeInfo.isInCategory(EventCategory::Gamepad))
            decrementGamepadEventListenerCount();
#endif
        break;
    }

    return true;
}

void LocalDOMWindow::languagesChanged()
{
    // https://html.spec.whatwg.org/multipage/system-state.html#dom-navigator-languages
    if (RefPtr document = this->document())
        document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, Event::create(eventNames().languagechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void LocalDOMWindow::dispatchLoadEvent()
{
    // If we did not protect it, the document loader and its timing subobject might get destroyed
    // as a side effect of what event handling code does.
    Ref protectedThis { *this };
    RefPtr document = this->document();
    RefPtr protectedLoader = frame() ? frame()->loader().documentLoader() : nullptr;
    bool shouldMarkLoadEventTimes = protectedLoader && !protectedLoader->timing().loadEventStart();

    if (shouldMarkLoadEventTimes) {
        auto now = MonotonicTime::now();
        protectedLoader->timing().setLoadEventStart(now);
        if (RefPtr navigationTiming = performance().navigationTiming())
            navigationTiming->documentLoadTiming().setLoadEventStart(now);
        WTFEmitSignpost(document.get(), NavigationAndPaintTiming, "loadEventBegin");
    }

    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No), document.get());

    if (shouldMarkLoadEventTimes) {
        auto now = MonotonicTime::now();
        protectedLoader->timing().setLoadEventEnd(now);
        if (RefPtr navigationTiming = performance().navigationTiming())
            navigationTiming->documentLoadTiming().setLoadEventEnd(now);
        WTFEmitSignpost(document.get(), NavigationAndPaintTiming, "loadEventEnd");
        WTFEndSignpost(document.get(), NavigationAndPaintTiming);
    }

    // Send a separate load event to the element that owns this frame.
    if (RefPtr ownerFrame = frame()) {
        if (is<RemoteFrame>(ownerFrame->tree().parent()))
            ownerFrame->protectedLoader()->client().dispatchLoadEventToOwnerElementInAnotherProcess();
        else if (RefPtr owner = ownerFrame->ownerElement())
            owner->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }

    InspectorInstrumentation::loadEventFired(protectedFrame().get());
}

void LocalDOMWindow::dispatchEvent(Event& event, EventTarget* target)
{
    // FIXME: It's confusing to have both the inherited EventTarget::dispatchEvent function
    // and this function, which does something nearly identical but subtly different if
    // called with a target of null. Most callers pass the document as the target, though.
    // Fixing this could allow us to remove the special case in DocumentEventQueue::dispatchEvent.

    Ref protectedThis { *this };

    if (target)
        event.setTarget(target);
    else
        event.setTarget(Ref { *this });

    EventPath eventPath { *this };
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    event.resetBeforeDispatch();
    event.setEventPath(eventPath);

    RefPtr<LocalFrame> protectedFrame;
    bool hasListenersForEvent = false;
    // FIXME: It doesn't seem right to have the inspector instrumentation here since not all
    // events dispatched to the window object are guaranteed to flow through this function.
    // But the instrumentation prevents us from calling EventDispatcher::dispatchEvent here.
    if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
        protectedFrame = frame();
        hasListenersForEvent = hasEventListeners(event.type());
        if (hasListenersForEvent)
            InspectorInstrumentation::willDispatchEventOnWindow(protectedFrame.get(), event, *this);
    }

    // FIXME: We should use EventDispatcher everywhere.
    fireEventListeners(event, EventInvokePhase::Capturing);
    fireEventListeners(event, EventInvokePhase::Bubbling);

    if (hasListenersForEvent)
        InspectorInstrumentation::didDispatchEventOnWindow(protectedFrame.get(), event);

    event.resetAfterDispatch();
}

void LocalDOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(DEVICE_ORIENTATION)
        stopListeningForDeviceOrientationIfNecessary();
        stopListeningForDeviceMotionIfNecessary();
#endif

#if PLATFORM(IOS_FAMILY)
    if (m_scrollEventListenerCount) {
        m_scrollEventListenerCount = 1;
        decrementScrollEventListenersCount();
    }
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    m_touchAndGestureEventListenerCount = 0;
#endif

#if ENABLE(TOUCH_EVENTS)
    if (RefPtr<Document> document = this->document())
        document->didRemoveEventTargetNode(*document);
#endif

    if (RefPtr performance = m_performance) {
        performance->removeAllEventListeners();
        performance->removeAllObservers();
    }

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void LocalDOMWindow::captureEvents()
{
    // Not implemented.
}

void LocalDOMWindow::releaseEvents()
{
    // Not implemented.
}

void LocalDOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        if (RefPtr loader = frame()->loader().activeDocumentLoader(); !loader || loader->mainDocumentError().isNull())
            print();
    }
}

void LocalDOMWindow::setLocation(LocalDOMWindow& activeWindow, const URL& completedURL, NavigationHistoryBehavior historyHandling, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    RefPtr frame = this->frame();
    if (!activeDocument->canNavigate(frame.get(), completedURL))
        return;

    if (isInsecureScriptAccess(activeWindow, completedURL.string()))
        return;

    // Check the CSP of the embedder to determine if we allow execution of javascript: URLs via child frame navigation.
    if (completedURL.protocolIsJavaScript() && frameElement() && !frameElement()->protectedDocument()->checkedContentSecurityPolicy()->allowJavaScriptURLs(aboutBlankURL().string(), { }, completedURL.string(), protectedFrameElement().get()))
        return;

    RefPtr localParent = dynamicDowncast<LocalFrame>(frame->tree().parent());
    // If the loader for activeWindow's frame (browsing context) has no outgoing referrer, set its outgoing referrer
    // to the URL of its parent frame's Document.
    if (RefPtr activeFrame = activeWindow.frame(); activeFrame && activeFrame->loader().outgoingReferrer().isEmpty() && localParent)
        activeFrame->loader().setOutgoingReferrer(protectedDocument()->completeURL(localParent->document()->url().strippedForUseAsReferrer().string));

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != SetLocationLocking::LockHistoryBasedOnGestureState || !UserGestureIndicator::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != SetLocationLocking::LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    frame->checkedNavigationScheduler()->scheduleLocationChange(*activeDocument, activeDocument->protectedSecurityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList,
        historyHandling);
}

void LocalDOMWindow::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    if (CheckedPtr pageConsole = console())
        pageConsole->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String LocalDOMWindow::crossDomainAccessErrorMessage(const LocalDOMWindow& activeWindow, IncludeTargetOrigin includeTargetOrigin)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    Ref activeOrigin = activeWindow.document()->securityOrigin();
    Ref targetOrigin = document()->securityOrigin();
    ASSERT(!activeOrigin->isSameOriginDomain(targetOrigin));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    String message;
    if (includeTargetOrigin == IncludeTargetOrigin::Yes)
        message = makeString("Blocked a frame with origin \""_s, activeOrigin->toString(), "\" from accessing a frame with origin \""_s, targetOrigin->toString(), "\". "_s);
    else
        message = makeString("Blocked a frame with origin \""_s, activeOrigin->toString(), "\" from accessing a cross-origin frame. "_s);

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    URL targetURL = document()->url();
    if (document()->isSandboxed(SandboxFlag::Origin) || activeWindow.document()->isSandboxed(SandboxFlag::Origin)) {
        if (includeTargetOrigin == IncludeTargetOrigin::Yes)
            message = makeString("Blocked a frame at \""_s, SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a frame at \""_s, SecurityOrigin::create(targetURL).get().toString(), "\". "_s);
        else
            message = makeString("Blocked a frame at \""_s, SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a cross-origin frame. "_s);

        if (document()->isSandboxed(SandboxFlag::Origin) && activeWindow.document()->isSandboxed(SandboxFlag::Origin))
            return makeString("Sandbox access violation: "_s, message, " Both frames are sandboxed and lack the \"allow-same-origin\" flag."_s);
        if (document()->isSandboxed(SandboxFlag::Origin))
            return makeString("Sandbox access violation: "_s, message, " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag."_s);
        return makeString("Sandbox access violation: "_s, message, " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag."_s);
    }

    if (includeTargetOrigin == IncludeTargetOrigin::Yes) {
        // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
        if (targetOrigin->protocol() != activeOrigin->protocol())
            return makeString(message, " The frame requesting access has a protocol of \""_s, activeURL.protocol(), "\", the frame being accessed has a protocol of \""_s, targetURL.protocol(), "\". Protocols must match.\n"_s);

        // 'document.domain' errors.
        if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
            return makeString(message, "The frame requesting access set \"document.domain\" to \""_s, activeOrigin->domain(), "\", the frame being accessed set it to \""_s, targetOrigin->domain(), "\". Both must set \"document.domain\" to the same value to allow access."_s);
        if (activeOrigin->domainWasSetInDOM())
            return makeString(message, "The frame requesting access set \"document.domain\" to \""_s, activeOrigin->domain(), "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access."_s);
        if (targetOrigin->domainWasSetInDOM())
            return makeString(message, "The frame being accessed set \"document.domain\" to \""_s, targetOrigin->domain(), "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access."_s);
    }

    // Default.
    return makeString(message, "Protocols, domains, and ports must match."_s);
}

bool LocalDOMWindow::isInsecureScriptAccess(LocalDOMWindow& activeWindow, const String& urlString)
{
    if (!WTF::protocolIsJavaScript(urlString))
        return false;

    // If this LocalDOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect LocalDOMWindow from
    // Frame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "activeWindow == this" check?
        if (&activeWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (activeWindow.document()->protectedSecurityOrigin()->isSameOriginDomain(document()->protectedSecurityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow, IncludeTargetOrigin::Yes));
    return true;
}

ExceptionOr<RefPtr<Frame>> LocalDOMWindow::createWindow(const String& urlString, const AtomString& frameName, const WindowFeatures& initialWindowFeatures, LocalDOMWindow& activeWindow, LocalFrame& firstFrame, LocalFrame& openerFrame, const Function<void(LocalDOMWindow&)>& prepareDialogFunction)
{
    RefPtr activeFrame = activeWindow.frame();
    if (!activeFrame)
        return RefPtr<Frame> { nullptr };

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return RefPtr<Frame> { nullptr };

    URL completedURL = urlString.isEmpty() ? URL({ }, emptyString()) : firstFrame.protectedDocument()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid())
        return Exception { ExceptionCode::SyntaxError };

    WindowFeatures windowFeatures = initialWindowFeatures;

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = windowFeatures.wantsNoReferrer() ? String() : SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.loader().outgoingReferrerURL(), OriginAccessPatternsForWebProcess::singleton());
    auto initiatedByMainFrame = activeFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;

    ResourceRequest resourceRequest { completedURL, referrer };
    RefPtr openerDocumentLoader = openerFrame.document() ? openerFrame.document()->loader() : nullptr;
    if (openerDocumentLoader)
        resourceRequest.setIsAppInitiated(openerDocumentLoader->lastNavigationWasAppInitiated());
    FrameLoadRequest frameLoadRequest { *activeDocument, activeDocument->protectedSecurityOrigin(), WTFMove(resourceRequest), frameName, initiatedByMainFrame };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(activeDocument->shouldOpenExternalURLsPolicyToPropagate());

    // https://html.spec.whatwg.org/#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name (Step 8.2)
    if (openerFrame.document()->shouldForceNoOpenerBasedOnCOOP()) {
        frameLoadRequest.setFrameName(blankTargetFrameName());
        windowFeatures.noopener = true;
    }

    if (openerFrame.document()->settingsValues().blobRegistryTopOriginPartitioningEnabled && frameLoadRequest.resourceRequest().url().protocolIsBlob() && !openerFrame.document()->protectedSecurityOrigin()->isSameOriginAs(openerFrame.document()->protectedTopOrigin())) {
        frameLoadRequest.setFrameName(blankTargetFrameName());
        windowFeatures.noopener = true;
    }
    bool noopener = windowFeatures.wantsNoOpener();

    auto [newFrame, created] = WebCore::createWindow(openerFrame, WTFMove(frameLoadRequest), WTFMove(windowFeatures));
    if (!newFrame)
        return RefPtr<Frame> { nullptr };

    if (!noopener) {
        ASSERT(!newFrame->opener() || newFrame->opener() == &openerFrame);
        newFrame->page()->setOpenedByDOMWithOpener(true);
    }

    if (created == CreatedNewPage::Yes)
        newFrame->protectedPage()->setOpenedByDOM();

    RefPtr localNewFrame = dynamicDowncast<LocalFrame>(newFrame);
    if (localNewFrame && localNewFrame->document()->protectedWindow()->isInsecureScriptAccess(activeWindow, completedURL.string()))
        return noopener ? RefPtr<Frame> { nullptr } : newFrame;

    if (prepareDialogFunction && localNewFrame)
        prepareDialogFunction(*localNewFrame->document()->protectedWindow());

    if (created == CreatedNewPage::Yes) {
        ResourceRequest resourceRequest { completedURL, referrer, ResourceRequestCachePolicy::UseProtocolCachePolicy };
        FrameLoader::addSameSiteInfoToRequestIfNeeded(resourceRequest, openerFrame.protectedDocument().get());
        FrameLoadRequest frameLoadRequest { activeWindow.protectedDocument().releaseNonNull(), activeWindow.document()->protectedSecurityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame };
        frameLoadRequest.setShouldOpenExternalURLsPolicy(activeDocument->shouldOpenExternalURLsPolicyToPropagate());
        newFrame->changeLocation(WTFMove(frameLoadRequest));
    } else if (!urlString.isEmpty()) {
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->checkedNavigationScheduler()->scheduleLocationChange(*activeDocument, activeDocument->protectedSecurityOrigin(), completedURL, referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return RefPtr<Frame> { nullptr };

    return noopener ? RefPtr<Frame> { nullptr } : newFrame;
}

ExceptionOr<RefPtr<WindowProxy>> LocalDOMWindow::open(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const String& urlStringToOpen, const AtomString& frameName, const String& windowFeaturesString)
{
    if (!isCurrentlyDisplayedInFrame())
        return RefPtr<WindowProxy> { nullptr };

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return RefPtr<WindowProxy> { nullptr };

    RefPtr firstFrame = firstWindow.frame();
    if (!firstFrame)
        return RefPtr<WindowProxy> { nullptr };

    auto urlString = urlStringToOpen;
    if (activeDocument->quirks().shouldOpenAsAboutBlank(urlStringToOpen))
        urlString = "about:blank"_s;

#if ENABLE(CONTENT_EXTENSIONS)
    RefPtr page = firstFrame->page();
    RefPtr firstFrameDocument = firstFrame->document();

    RefPtr localFrame = dynamicDowncast<LocalFrame>(firstFrame->mainFrame());

    // FIXME: <rdar://118280717> Make WKContentRuleLists apply in this case.
    RefPtr mainFrameDocument = localFrame ? localFrame->document() : nullptr;
    RefPtr mainFrameDocumentLoader = mainFrameDocument ? mainFrameDocument->loader() : nullptr;
    if (firstFrameDocument && page && mainFrameDocumentLoader) {
        auto results = page->protectedUserContentProvider()->processContentRuleListsForLoad(*page, firstFrameDocument->completeURL(urlString), ContentExtensions::ResourceType::Popup, *mainFrameDocumentLoader);
        if (results.summary.blockedLoad)
            return RefPtr<WindowProxy> { nullptr };
    }
#endif

    RefPtr frame = this->frame();
    if (!frame)
        return RefPtr<WindowProxy> { nullptr };

    if (!firstWindow.allowPopUp()) {
        // Because FrameTree::findFrameForNavigation() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !frame->protectedLoader()->findFrameForNavigation(frameName, activeDocument.get()))
            return RefPtr<WindowProxy> { nullptr };
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    RefPtr<Frame> targetFrame;
    if (isTopTargetFrameName(frameName))
        targetFrame = &frame->tree().top();
    else if (isParentTargetFrameName(frameName)) {
        if (RefPtr parent = frame->tree().parent())
            targetFrame = parent.get();
        else
            targetFrame = frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame.get()))
            return RefPtr<WindowProxy> { nullptr };

        URL completedURL = firstFrame->protectedDocument()->completeURL(urlString);

        RefPtr localTargetFrame = dynamicDowncast<LocalFrame>(targetFrame.get());
        if (localTargetFrame && localTargetFrame->document()->protectedWindow()->isInsecureScriptAccess(activeWindow, completedURL.string()))
            return &targetFrame->windowProxy();

        if (urlString.isEmpty())
            return &targetFrame->windowProxy();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        targetFrame->checkedNavigationScheduler()->scheduleLocationChange(*activeDocument, activeDocument->protectedSecurityOrigin(), completedURL, firstFrame->loader().outgoingReferrer(),
            lockHistory, LockBackForwardList::No);
        return &targetFrame->windowProxy();
    }

    auto newFrameOrException = createWindow(urlString, frameName, parseWindowFeatures(windowFeaturesString), activeWindow, *firstFrame, *frame);
    if (newFrameOrException.hasException())
        return newFrameOrException.releaseException();

    auto newFrame = newFrameOrException.releaseReturnValue();
    return newFrame ? &newFrame->windowProxy() : RefPtr<WindowProxy> { nullptr };
}

void LocalDOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const Function<void(LocalDOMWindow&)>& prepareDialogFunction)
{
    if (RefPtr document = this->document())
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "showModalDialog() is deprecated and will be removed. Please use the <dialog> element instead."_s);

    if (!isCurrentlyDisplayedInFrame())
        return;
    if (!activeWindow.frame())
        return;
    RefPtr firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    RefPtr frame = this->frame();
    auto* page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.showModalDialog is not allowed while unloading a page."_s);
        return;
    }

    if (!canShowModalDialog(*frame) || !firstWindow.allowPopUp())
        return;

    auto dialogFrameOrException = createWindow(urlString, emptyAtom(), parseDialogFeatures(dialogFeaturesString, screenAvailableRect(frame->protectedView().get())), activeWindow, *firstFrame, *frame, prepareDialogFunction);
    if (dialogFrameOrException.hasException())
        return;
    if (RefPtr dialogFrame = dialogFrameOrException.releaseReturnValue())
        dialogFrame->page()->chrome().runModal();
}

void LocalDOMWindow::enableSuddenTermination()
{
    if (RefPtr page = this->page())
        page->chrome().enableSuddenTermination();
}

void LocalDOMWindow::disableSuddenTermination()
{
    if (RefPtr page = this->page())
        page->chrome().disableSuddenTermination();
}

LocalFrame* LocalDOMWindow::frame() const
{
    auto* document = this->document();
    return document ? document->frame() : nullptr;
}

RefPtr<LocalFrame> LocalDOMWindow::protectedFrame() const
{
    return frame();
}

void LocalDOMWindow::eventListenersDidChange()
{
    if (m_localStorage || m_sessionStorage) {
        if (hasEventListeners(eventNames().storageEvent))
            windowsInterestedInStorageEvents().add(*this);
        else
            windowsInterestedInStorageEvents().remove(*this);
    }
}

CookieStore& LocalDOMWindow::cookieStore()
{
    if (!m_cookieStore)
        m_cookieStore = CookieStore::create(protectedDocument().get());
    return *m_cookieStore;
}

} // namespace WebCore
