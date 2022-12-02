/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"

#include "BackForwardController.h"
#include "BarProp.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ComposedTreeIterator.h"
#include "ContentExtensionActions.h"
#include "ContentExtensionRule.h"
#include "ContentRuleListResults.h"
#include "CrossOriginOpenerPolicy.h"
#include "Crypto.h"
#include "CustomElementRegistry.h"
#include "DOMApplicationCache.h"
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
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FeaturePolicy.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTTPParsers.h"
#include "History.h"
#include "IdleRequestOptions.h"
#include "InspectorInstrumentation.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindowBase.h"
#include "JSExecState.h"
#include "Location.h"
#include "Logging.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MessageEvent.h"
#include "MessageWithMessagePorts.h"
#include "NavigationScheduler.h"
#include "Navigator.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageTransitionEvent.h"
#include "Performance.h"
#include "PerformanceNavigationTiming.h"
#include "RequestAnimationFrameCallback.h"
#include "ResourceLoadInfo.h"
#include "ResourceLoadObserver.h"
#include "RuntimeApplicationChecks.h"
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
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/URL.h>
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

static WeakHashSet<DOMWindow, WeakPtrImplWithEventTargetData>& windowsInterestedInStorageEvents()
{
    static MainThreadNeverDestroyed<WeakHashSet<DOMWindow, WeakPtrImplWithEventTargetData>> set;
    return set;
}

void DOMWindow::forEachWindowInterestedInStorageEvents(const Function<void(DOMWindow&)>& apply)
{
    for (auto& window : copyToVectorOf<Ref<DOMWindow>>(windowsInterestedInStorageEvents()))
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

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMWindow);

typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithUnloadEventListeners;
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithBeforeUnloadEventListeners;
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static void addBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    Frame* frame = window->frame();
    if (!frame)
        return false;
    if (!frame->page())
        return false;
    return frame->isMainFrame();
}

bool DOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    auto windows = WTF::map(set, [](auto& entry) -> Ref<DOMWindow> {
        return *entry.key;
    });

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        RefPtr frame = window->frame();
        if (!frame)
            continue;

        if (!frame->loader().shouldClose())
            return false;

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
    return true;
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<DOMWindow*>(this));
}

void DOMWindow::dispatchAllPendingUnloadEvents()
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    auto windows = WTF::map(set, [] (auto& keyValue) {
        return Ref<DOMWindow>(*(keyValue.key));
    });

    auto& eventNames = WebCore::eventNames();
    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        window->dispatchEvent(PageTransitionEvent::create(eventNames.pagehideEvent, false), window->document());
        window->dispatchEvent(Event::create(eventNames.unloadEvent, Event::CanBubble::No, Event::IsCancelable::No), window->document());

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
FloatRect DOMWindow::adjustWindowRect(Page& page, const FloatRect& pendingChanges)
{
    FloatRect screen = screenAvailableRect(page.mainFrame().view());
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

bool DOMWindow::allowPopUp(Frame& firstFrame)
{
    if (DocumentLoader* documentLoader = firstFrame.loader().documentLoader()) {
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

bool DOMWindow::allowPopUp()
{
    auto* frame = this->frame();
    return frame && allowPopUp(*frame);
}

bool DOMWindow::canShowModalDialog(const Frame& frame)
{
    // Override support for layout testing purposes.
    if (auto* document = frame.document()) {
        if (auto* window = document->domWindow()) {
            if (window->m_canShowModalDialogOverride)
                return window->m_canShowModalDialogOverride.value();
        }
    }

    auto* page = frame.page();
    return page && page->chrome().canRunModal();
}

static void languagesChangedCallback(void* context)
{
    static_cast<DOMWindow*>(context)->languagesChanged();
}

void DOMWindow::setCanShowModalDialogOverride(bool allow)
{
    m_canShowModalDialogOverride = allow;
}

DOMWindow::DOMWindow(Document& document)
    : AbstractDOMWindow(GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() })
    , ContextDestructionObserver(&document)
{
    ASSERT(frame());
    addLanguageChangeObserver(this, &languagesChangedCallback);
}

void DOMWindow::didSecureTransitionTo(Document& document)
{
    observeContext(&document);

    // The Window is being transferred from one document to another so we need to reset data
    // members that store the window's document (rather than the window itself).
    m_crypto = nullptr;
    m_navigator = nullptr;
    m_performance = nullptr;
    m_customElementRegistry = nullptr;
}

void DOMWindow::prewarmLocalStorageIfNecessary()
{
    auto* page = this->page();

    // No need to prewarm for ephemeral sessions since the data is in memory only.
    if (!page || page->usesEphemeralSession())
        return;

    // This eagerly constructs the StorageArea, which will load items from disk.
    auto localStorageResult = this->localStorage();
    if (localStorageResult.hasException())
        return;

    auto* localStorage = localStorageResult.returnValue();
    if (!localStorage)
        return;

    localStorage->area().prewarm();
}

DOMWindow::~DOMWindow()
{
    if (m_suspendedForDocumentSuspension)
        willDestroyCachedFrame();
    else
        willDestroyDocumentInFrame();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);

#if ENABLE(GAMEPAD)
    if (m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
#endif

    removeLanguageChangeObserver(this);

    windowsInterestedInStorageEvents().remove(*this);
}

RefPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* DOMWindow::page() const
{
    return frame() ? frame()->page() : nullptr;
}

void DOMWindow::frameDestroyed()
{
    Ref<DOMWindow> protectedThis(*this);

    willDestroyDocumentInFrame();
    JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(this);
}

void DOMWindow::willDestroyCachedFrame()
{
    // It is necessary to copy m_observers to a separate vector because the Observer may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInCachedFrame.
    for (auto* observer : copyToVector(m_observers)) {
        if (m_observers.contains(observer))
            observer->willDestroyGlobalObjectInCachedFrame();
    }
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_observers to a separate vector because the Observer may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    for (auto* observer : copyToVector(m_observers)) {
        if (m_observers.contains(observer))
            observer->willDestroyGlobalObjectInFrame();
    }
}

void DOMWindow::willDetachDocumentFromFrame()
{
    if (!frame())
        return;

    RELEASE_ASSERT(!m_isSuspendingObservers);

    // It is necessary to copy m_observers to a separate vector because the Observer may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    for (auto& observer : copyToVector(m_observers)) {
        if (m_observers.contains(observer))
            observer->willDetachGlobalObjectFromFrame();
    }

    if (m_performance)
        m_performance->clearResourceTimings();

    windowsInterestedInStorageEvents().remove(*this);

    JSDOMWindowBase::fireFrameClearedWatchpointsForWindow(this);
    InspectorInstrumentation::frameWindowDiscarded(*frame(), this);
}

#if ENABLE(GAMEPAD)

void DOMWindow::incrementGamepadEventListenerCount()
{
    if (++m_gamepadEventListenerCount == 1)
        GamepadManager::singleton().registerDOMWindow(this);
}

void DOMWindow::decrementGamepadEventListenerCount()
{
    ASSERT(m_gamepadEventListenerCount);

    if (!--m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
}

#endif

void DOMWindow::registerObserver(Observer& observer)
{
    m_observers.add(&observer);
}

void DOMWindow::unregisterObserver(Observer& observer)
{
    m_observers.remove(&observer);
}

void DOMWindow::resetUnlessSuspendedForDocumentSuspension()
{
    if (m_suspendedForDocumentSuspension)
        return;
    willDestroyDocumentInFrame();
}

void DOMWindow::suspendForBackForwardCache()
{
    SetForScope isSuspendingObservers(m_isSuspendingObservers, true);
    RELEASE_ASSERT(frame());

    for (auto* observer : copyToVector(m_observers)) {
        if (m_observers.contains(observer))
            observer->suspendForBackForwardCache();
    }
    RELEASE_ASSERT(frame());

    m_suspendedForDocumentSuspension = true;
}

void DOMWindow::resumeFromBackForwardCache()
{
    for (auto* observer : copyToVector(m_observers)) {
        if (m_observers.contains(observer))
            observer->resumeFromBackForwardCache();
    }

    m_suspendedForDocumentSuspension = false;
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    auto* frame = this->frame();
    return frame && frame->document()->domWindow() == this;
}

CustomElementRegistry& DOMWindow::ensureCustomElementRegistry()
{
    if (!m_customElementRegistry)
        m_customElementRegistry = CustomElementRegistry::create(*this, scriptExecutionContext());
    ASSERT(m_customElementRegistry->scriptExecutionContext() == document());
    return *m_customElementRegistry;
}

static ExceptionOr<SelectorQuery&> selectorQueryInFrame(Frame* frame, const String& selectors)
{
    if (!frame)
        return Exception { NotSupportedError };

    RefPtr document = frame->document();
    if (!document)
        return Exception { NotSupportedError };

    return document->selectorQueryForString(selectors);
}

ExceptionOr<Ref<NodeList>> DOMWindow::collectMatchingElementsInFlatTree(Node& scope, const String& selectors)
{
    auto queryOrException = selectorQueryInFrame(frame(), selectors);
    if (queryOrException.hasException())
        return queryOrException.releaseException();

    if (!is<ContainerNode>(scope))
        return Ref<NodeList> { StaticElementList::create() };

    SelectorQuery& query = queryOrException.releaseReturnValue();

    Vector<Ref<Element>> result;
    for (auto& node : composedTreeDescendants(downcast<ContainerNode>(scope))) {
        if (is<Element>(node) && query.matches(downcast<Element>(node)) && !node.isInUserAgentShadowTree())
            result.append(downcast<Element>(node));
    }

    return Ref<NodeList> { StaticElementList::create(WTFMove(result)) };
}

ExceptionOr<RefPtr<Element>> DOMWindow::matchingElementInFlatTree(Node& scope, const String& selectors)
{
    auto queryOrException = selectorQueryInFrame(frame(), selectors);
    if (queryOrException.hasException())
        return queryOrException.releaseException();

    if (!is<ContainerNode>(scope))
        return RefPtr<Element> { nullptr };

    SelectorQuery& query = queryOrException.releaseReturnValue();

    for (auto& node : composedTreeDescendants(downcast<ContainerNode>(scope))) {
        if (is<Element>(node) && query.matches(downcast<Element>(node)) && !node.isInUserAgentShadowTree())
            return &downcast<Element>(node);
    }

    return RefPtr<Element> { nullptr };
}

int DOMWindow::orientation() const
{
#if !ENABLE(ORIENTATION_EVENTS)
    return 0;
#else
    auto* frame = this->frame();
    if (!frame)
        return 0;
    return frame->orientation();
#endif
}

Screen& DOMWindow::screen()
{
    if (!m_screen)
        m_screen = Screen::create(*this);
    return *m_screen;
}

History& DOMWindow::history()
{
    if (!m_history)
        m_history = History::create(*this);
    return *m_history;
}

Crypto& DOMWindow::crypto() const
{
    if (!m_crypto)
        m_crypto = Crypto::create(document());
    ASSERT(m_crypto->scriptExecutionContext() == document());
    return *m_crypto;
}

BarProp& DOMWindow::locationbar()
{
    if (!m_locationbar)
        m_locationbar = BarProp::create(*this, BarProp::Locationbar);
    return *m_locationbar;
}

BarProp& DOMWindow::menubar()
{
    if (!m_menubar)
        m_menubar = BarProp::create(*this, BarProp::Menubar);
    return *m_menubar;
}

BarProp& DOMWindow::personalbar()
{
    if (!m_personalbar)
        m_personalbar = BarProp::create(*this, BarProp::Personalbar);
    return *m_personalbar;
}

BarProp& DOMWindow::scrollbars()
{
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(*this, BarProp::Scrollbars);
    return *m_scrollbars;
}

BarProp& DOMWindow::statusbar()
{
    if (!m_statusbar)
        m_statusbar = BarProp::create(*this, BarProp::Statusbar);
    return *m_statusbar;
}

BarProp& DOMWindow::toolbar()
{
    if (!m_toolbar)
        m_toolbar = BarProp::create(*this, BarProp::Toolbar);
    return *m_toolbar;
}

PageConsoleClient* DOMWindow::console() const
{
    // FIXME: This should not return nullptr when frameless.
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    auto* frame = this->frame();
    return frame->page() ? &frame->page()->console() : nullptr;
}

DOMApplicationCache& DOMWindow::applicationCache()
{
    if (!m_applicationCache)
        m_applicationCache = DOMApplicationCache::create(*this);
    return *m_applicationCache;
}

Navigator& DOMWindow::navigator()
{
    if (!m_navigator)
        m_navigator = Navigator::create(scriptExecutionContext(), *this);
    ASSERT(m_navigator->scriptExecutionContext() == document());

    return *m_navigator;
}

Performance& DOMWindow::performance() const
{
    if (!m_performance) {
        RefPtr documentLoader = document() ? document()->loader() : nullptr;
        auto timeOrigin = documentLoader ? documentLoader->timing().timeOrigin() : MonotonicTime::now();
        m_performance = Performance::create(document(), timeOrigin);
    }
    ASSERT(m_performance->scriptExecutionContext() == document());
    return *m_performance;
}

ReducedResolutionSeconds DOMWindow::nowTimestamp() const
{
    return performance().nowInReducedResolutionSeconds();
}

void DOMWindow::freezeNowTimestamp()
{
    m_frozenNowTimestamp = nowTimestamp();
}

void DOMWindow::unfreezeNowTimestamp()
{
    m_frozenNowTimestamp = std::nullopt;
}

ReducedResolutionSeconds DOMWindow::frozenNowTimestamp() const
{
    return m_frozenNowTimestamp.value_or(nowTimestamp());
}

Location& DOMWindow::location()
{
    if (!m_location)
        m_location = Location::create(*this);
    return *m_location;
}

VisualViewport& DOMWindow::visualViewport()
{
    if (!m_visualViewport)
        m_visualViewport = VisualViewport::create(*this);
    return *m_visualViewport;
}

#if ENABLE(USER_MESSAGE_HANDLERS)

bool DOMWindow::shouldHaveWebKitNamespaceForWorld(DOMWrapperWorld& world)
{
    RefPtr frame = this->frame();
    if (!frame)
        return false;

    auto* page = frame->page();
    if (!page)
        return false;

    bool hasUserMessageHandler = false;
    page->userContentProvider().forEachUserMessageHandler([&](const UserMessageHandlerDescriptor& descriptor) {
        if (&descriptor.world() == &world) {
            hasUserMessageHandler = true;
            return;
        }
    });

    return hasUserMessageHandler;
}

WebKitNamespace* DOMWindow::webkitNamespace()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    auto* page = frame()->page();
    if (!page)
        return nullptr;
    if (!m_webkitNamespace)
        m_webkitNamespace = WebKitNamespace::create(*this, page->userContentProvider());
    return m_webkitNamespace.get();
}

#endif

ExceptionOr<Storage*> DOMWindow::sessionStorage()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    if (document->canAccessResource(ScriptExecutionContext::ResourceType::SessionStorage) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { SecurityError };

    if (m_sessionStorage)
        return m_sessionStorage.get();

    auto* page = document->page();
    if (!page)
        return nullptr;

    auto storageArea = page->storageNamespaceProvider().sessionStorageArea(*document);
    m_sessionStorage = Storage::create(*this, WTFMove(storageArea));
    if (hasEventListeners(eventNames().storageEvent))
        windowsInterestedInStorageEvents().add(*this);
    return m_sessionStorage.get();
}

ExceptionOr<Storage*> DOMWindow::localStorage()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    RefPtr document = this->document();
    if (!document)
        return nullptr;

    if (document->canAccessResource(ScriptExecutionContext::ResourceType::LocalStorage) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { SecurityError };

    auto* page = document->page();
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

    auto storageArea = page->storageNamespaceProvider().localStorageArea(*document);
    m_localStorage = Storage::create(*this, WTFMove(storageArea));
    if (hasEventListeners(eventNames().storageEvent))
        windowsInterestedInStorageEvents().add(*this);
    return m_localStorage.get();
}

ExceptionOr<void> DOMWindow::postMessage(JSC::JSGlobalObject& lexicalGlobalObject, DOMWindow& incumbentWindow, JSC::JSValue messageValue, WindowPostMessageOptions&& options)
{
    if (!isCurrentlyDisplayedInFrame())
        return { };

    RefPtr sourceDocument = incumbentWindow.document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SyntaxError exception correctly.
    RefPtr<SecurityOrigin> target;
    if (options.targetOrigin == "/"_s) {
        if (!sourceDocument)
            return { };
        target = &sourceDocument->securityOrigin();
    } else if (options.targetOrigin != "*"_s) {
        target = SecurityOrigin::createFromString(options.targetOrigin);
        // It doesn't make sense target a postMessage at an opaque origin
        // because there's no way to represent an opaque origin in a string.
        if (target->isOpaque())
            return Exception { SyntaxError };
    }

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(lexicalGlobalObject, messageValue, WTFMove(options.transfer), ports, SerializationForStorage::No, SerializationContext::WindowPostMessage);
    if (messageData.hasException())
        return messageData.releaseException();

    auto disentangledPorts = MessagePort::disentanglePorts(WTFMove(ports));
    if (disentangledPorts.hasException())
        return disentangledPorts.releaseException();

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return { };
    auto sourceOrigin = sourceDocument->securityOrigin().toString();

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtr<ScriptCallStack> stackTrace;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument.get()))
        stackTrace = createScriptCallStack(JSExecState::currentState());

    auto postMessageIdentifier = InspectorInstrumentation::willPostMessage(*frame());

    MessageWithMessagePorts message { messageData.releaseReturnValue(), disentangledPorts.releaseReturnValue() };

    // Schedule the message.
    RefPtr<WindowProxy> incumbentWindowProxy = incumbentWindow.frame() ? &incumbentWindow.frame()->windowProxy() : nullptr;
    auto userGestureToForward = UserGestureIndicator::currentUserGesture();

    document()->eventLoop().queueTask(TaskSource::PostedMessageQueue, [this, protectedThis = Ref { *this }, message = WTFMove(message), incumbentWindowProxy = WTFMove(incumbentWindowProxy), sourceOrigin = WTFMove(sourceOrigin), userGestureToForward = WTFMove(userGestureToForward), postMessageIdentifier, stackTrace = WTFMove(stackTrace), targetOrigin = WTFMove(target)]() mutable {
        if (!isCurrentlyDisplayedInFrame())
            return;

        Ref frame = *this->frame();
        if (targetOrigin) {
            // Check target origin now since the target document may have changed since the timer was scheduled.
            if (!targetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
                if (auto* pageConsole = console()) {
                    auto message = makeString("Unable to post message to ", targetOrigin->toString(), ". Recipient has origin ", document()->securityOrigin().toString(), ".\n");
                    if (stackTrace)
                        pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message, *stackTrace);
                    else
                        pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message);
                }

                InspectorInstrumentation::didFailPostMessage(frame, postMessageIdentifier);
                return;
            }
        }

        auto* globalObject = document()->globalObject();
        if (!globalObject)
            return;

        UserGestureIndicator userGestureIndicator(userGestureToForward);
        InspectorInstrumentation::willDispatchPostMessage(frame, postMessageIdentifier);

        auto ports = MessagePort::entanglePorts(*document(), WTFMove(message.transferredPorts));
        auto event = MessageEvent::create(*globalObject, message.message.releaseNonNull(), sourceOrigin, { }, incumbentWindowProxy ? std::make_optional(MessageEventSource(WTFMove(incumbentWindowProxy))) : std::nullopt, WTFMove(ports));
        dispatchEvent(event.event);

        InspectorInstrumentation::didDispatchPostMessage(frame, postMessageIdentifier);
    });

    InspectorInstrumentation::didPostMessage(*frame(), postMessageIdentifier, lexicalGlobalObject);

    return { };
}

DOMSelection* DOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_selection)
        m_selection = DOMSelection::create(*this);
    return m_selection.get();
}

Element* DOMWindow::frameElement() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    return frame->ownerElement();
}

void DOMWindow::focus(DOMWindow& incumbentWindow)
{
    RefPtr frame = this->frame();
    RefPtr openerFrame = frame ? frame->loader().opener() : nullptr;
    focus([&] {
        if (!openerFrame || openerFrame == frame || incumbentWindow.frame() != openerFrame)
            return false;

        auto* page = openerFrame->page();
        return page && page->isVisibleAndActive();
    }());
}

void DOMWindow::focus(bool allowFocus)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    allowFocus = allowFocus || WindowFocusAllowedIndicator::windowFocusAllowed() || !frame->settings().windowFocusRestricted();

    // If we're a top level window, bring the window to the front.
    if (frame->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!frame->hasHadUserInteraction() && !isSameSecurityOriginAsMainFrame())
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    RefPtr focusedFrame = CheckedRef(page->focusController())->focusedFrame();
    if (focusedFrame && focusedFrame != frame)
        focusedFrame->document()->setFocusedElement(nullptr);

    frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    if (frame->settings().windowFocusRestricted())
        return;

    if (!frame->isMainFrame())
        return;

    page->chrome().unfocus();
}

void DOMWindow::close(Document& document)
{
    if (!document.canNavigate(frame()))
        return;
    close();
}

void DOMWindow::close()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    if (!frame->isMainFrame())
        return;

    if (!(page->openedByDOM() || page->backForward().count() <= 1)) {
        console()->addMessage(MessageSource::JS, MessageLevel::Warning, "Can't close the window since it was not opened by JavaScript"_s);
        return;
    }

    if (!frame->loader().shouldClose())
        return;

    ResourceLoadObserver::shared().updateCentralStatisticsStore([] { });

    page->setIsClosing();

    document()->eventLoop().queueTask(TaskSource::DOMManipulation, [this, protectedThis = Ref { *this }] {
        if (auto* page = this->page())
            page->chrome().closeWindow();
    });
}

void DOMWindow::print()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    auto* page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.print is not allowed while unloading a page."_s);
        return;
    }

    if (page->isControlledByAutomation())
        return;

    if (auto loader = frame->loader().activeDocumentLoader(); loader && loader->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(*frame);
}

void DOMWindow::stop()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    frame->loader().stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.alert is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return;
    }

    auto* page = frame->page();
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

bool DOMWindow::confirmForBindings(const String& message)
{
    RefPtr frame = this->frame();
    if (!frame)
        return false;

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.confirm is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return false;
    }

    auto* page = frame->page();
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

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    RefPtr frame = this->frame();
    if (!frame)
        return String();

    RefPtr document = this->document();
    if (document->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.prompt is not allowed in a sandboxed frame when the allow-modals flag is not set."_s);
        return String();
    }

    auto* page = frame->page();
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

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    // SearchBuffer allocates memory much larger than the searched string, so it's necessary to limit its length.
    // Most searches are for a phrase or a paragraph, so an upper limit of 64kB is more than enough in practice.
    constexpr auto maximumStringLength = std::numeric_limits<uint16_t>::max();
    if (!isCurrentlyDisplayedInFrame() || string.length() > maximumStringLength)
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog.    
    FindOptions options { DoNotTraverseFlatTree };
    if (backwards)
        options.add(Backwards);
    if (!caseSensitive)
        options.add(CaseInsensitive);
    if (wrap)
        options.add(WrapAround);
    return frame()->editor().findString(string, options);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
#if PLATFORM(IOS_FAMILY)
    if (!frame())
        return 0;

    RefPtr view = frame()->isMainFrame() ? frame()->view() : frame()->mainFrame().view();
    if (!view)
        return 0;

    return view->frameRect().height();
#else
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().height());
#endif
}

int DOMWindow::outerWidth() const
{
#if PLATFORM(IOS_FAMILY)
    if (!frame())
        return 0;

    RefPtr view = frame()->isMainFrame() ? frame()->view() : frame()->mainFrame().view();
    if (!view)
        return 0;

    return view->frameRect().width();
#else
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().width());
#endif
}

int DOMWindow::innerHeight() const
{
    if (!frame())
        return 0;
    
    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (RefPtr ownerElement = frameElement())
        ownerElement->document().updateLayoutIfDimensionsOutOfDate(*ownerElement, HeightDimensionsCheck);

    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    
    RefPtr view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int DOMWindow::innerWidth() const
{
    if (!frame())
        return 0;

    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (RefPtr ownerElement = frameElement())
        ownerElement->document().updateLayoutIfDimensionsOutOfDate(*ownerElement, WidthDimensionsCheck);

    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    RefPtr view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int DOMWindow::screenX() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
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

    frame->document()->updateLayoutIgnorePendingStylesheets();

    // Layout may have affected the current frame:
    RefPtr frameAfterLayout = this->frame();
    if (!frameAfterLayout)
        return 0;

    RefPtr viewAfterLayout = frameAfterLayout->view();
    if (!viewAfterLayout)
        return 0;

    return viewAfterLayout->mapFromLayoutToCSSUnits(viewAfterLayout->contentsScrollPosition().x());
}

int DOMWindow::scrollY() const
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

    frame->document()->updateLayoutIgnorePendingStylesheets();

    // Layout may have affected the current frame:
    RefPtr frameAfterLayout = this->frame();
    if (!frameAfterLayout)
        return 0;

    RefPtr viewAfterLayout = frameAfterLayout->view();
    if (!viewAfterLayout)
        return 0;

    return viewAfterLayout->mapFromLayoutToCSSUnits(viewAfterLayout->contentsScrollPosition().y());
}

bool DOMWindow::closed() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return true;

    auto* page = frame->page();
    return !page || page->isClosing();
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return frame()->tree().scopedChildCount();
}

AtomString DOMWindow::name() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullAtom();

    return frame->tree().name();
}

void DOMWindow::setName(const AtomString& string)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    frame->tree().setName(string);
}

void DOMWindow::setStatus(const String& string) 
{
    m_status = string;

    RefPtr frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    ASSERT(frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(*frame, m_status);
}

void DOMWindow::setDefaultStatus(const String& string) 
{
    m_defaultStatus = string;

    RefPtr frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    ASSERT(frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(*frame, m_defaultStatus);
}

WindowProxy* DOMWindow::opener() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    RefPtr openerFrame = frame->loader().opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

void DOMWindow::disownOpener()
{
    if (RefPtr frame = this->frame())
        frame->loader().setOpener(nullptr);
}

WindowProxy* DOMWindow::parent() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    RefPtr parentFrame = frame->tree().parent();
    if (parentFrame)
        return &parentFrame->windowProxy();

    return &frame->windowProxy();
}

WindowProxy* DOMWindow::top() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    if (!frame->page())
        return nullptr;

    return &frame->tree().top().windowProxy();
}

String DOMWindow::origin() const
{
    auto* document = this->document();
    return document ? document->securityOrigin().toString() : emptyString();
}

SecurityOrigin* DOMWindow::securityOrigin() const
{
    auto* document = this->document();
    return document ? &document->securityOrigin() : nullptr;
}

Document* DOMWindow::document() const
{
    return downcast<Document>(ContextDestructionObserver::scriptExecutionContext());
}

void DOMWindow::overrideTransientActivationDurationForTesting(std::optional<Seconds>&& override)
{
    transientActivationDurationOverrideForTesting() = WTFMove(override);
}

// When the current high resolution time is greater than or equal to the last activation timestamp in W, and
// less than the last activation timestamp in W plus the transient activation duration, then W is said to
// have transient activation. (https://html.spec.whatwg.org/multipage/interaction.html#transient-activation)
bool DOMWindow::hasTransientActivation() const
{
    auto now = MonotonicTime::now();
    return now >= m_lastActivationTimestamp && now < (m_lastActivationTimestamp + transientActivationDuration());
}

// When the current high resolution time given W is greater than or equal to the last activation timestamp in W,
// W is said to have sticky activation. (https://html.spec.whatwg.org/multipage/interaction.html#sticky-activation)
bool DOMWindow::hasStickyActivation() const
{
    auto now = MonotonicTime::now();
    return now >= m_lastActivationTimestamp;
}

// https://html.spec.whatwg.org/multipage/interaction.html#consume-user-activation
bool DOMWindow::consumeTransientActivation()
{
    if (!hasTransientActivation())
        return false;

    for (RefPtr<AbstractFrame> frame = this->frame() ? &this->frame()->tree().top() : nullptr; frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        auto* window = localFrame->window();
        if (!window || window->lastActivationTimestamp() != MonotonicTime::infinity())
            window->setLastActivationTimestamp(-MonotonicTime::infinity());
    }

    return true;
}

// https://html.spec.whatwg.org/multipage/interaction.html#activation-notification
void DOMWindow::notifyActivated(MonotonicTime activationTime)
{
    setLastActivationTimestamp(activationTime);
    if (!frame())
        return;

    for (RefPtr ancestor = frame() ? frame()->tree().parent() : nullptr; ancestor; ancestor = ancestor->tree().parent()) {
        RefPtr localAncestor = dynamicDowncast<LocalFrame>(ancestor.get());
        if (!localAncestor)
            continue;
        if (auto* window = localAncestor->window())
            window->setLastActivationTimestamp(activationTime);
    }

    RefPtr securityOrigin = this->securityOrigin();
    if (!securityOrigin)
        return;

    RefPtr<AbstractFrame> descendant = frame();
    while ((descendant = descendant->tree().traverseNext(frame()))) {
        auto* localDescendant = dynamicDowncast<LocalFrame>(descendant.get());
        if (!localDescendant)
            continue;
        auto* descendantWindow = localDescendant->window();
        if (!descendantWindow)
            continue;

        RefPtr descendantSecurityOrigin = descendantWindow->securityOrigin();
        if (!descendantSecurityOrigin || !descendantSecurityOrigin->isSameOriginAs(*securityOrigin))
            continue;

        descendantWindow->setLastActivationTimestamp(activationTime);
    }
}

StyleMedia& DOMWindow::styleMedia()
{
    if (!m_media)
        m_media = StyleMedia::create(*this);
    return *m_media;
}

Ref<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element& element, const String& pseudoElt) const
{
    return CSSComputedStyleDeclaration::create(element, false, pseudoElt);
}

RefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement, bool authorOnly) const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    auto pseudoType = CSSSelector::parsePseudoElementType(StringView { pseudoElement }.substring(colonStart));
    if (pseudoType == CSSSelector::PseudoElementUnknown && !pseudoElement.isEmpty())
        return nullptr;

    RefPtr frame = this->frame();
    frame->document()->styleScope().flushPendingUpdate();

    unsigned rulesToInclude = Style::Resolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= Style::Resolver::UAAndUserCSSRules;

    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);

    auto matchedRules = frame->document()->styleScope().resolver().pseudoStyleRulesForElement(element, pseudoId, rulesToInclude);
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

RefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

RefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return nullptr;

    if (!document())
        return nullptr;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return WebKitPoint::create(nodePoint.x(), nodePoint.y());
}

double DOMWindow::devicePixelRatio() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0.0;

    Page* page = frame->page();
    if (!page)
        return 0.0;

    return page->deviceScaleFactor();
}

void DOMWindow::scrollBy(double x, double y) const
{
    scrollBy(ScrollToOptions(x, y));
}

void DOMWindow::scrollBy(const ScrollToOptions& options) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr view = frame->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    scrollToOptions.left.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
    scrollToOptions.top.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
    scrollTo(scrollToOptions, ScrollClamping::Clamped, ScrollSnapPointSelectionMethod::Directional);
}

void DOMWindow::scrollTo(double x, double y, ScrollClamping clamping) const
{
    scrollTo(ScrollToOptions(x, y), clamping);
}

void DOMWindow::scrollTo(const ScrollToOptions& options, ScrollClamping clamping, ScrollSnapPointSelectionMethod snapPointSelectionMethod) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr<FrameView> view = frame()->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        view->contentsScrollPosition().x(), view->contentsScrollPosition().y()
    );

    // This is an optimization for the common case of scrolling to (0, 0) when the scroller is already at the origin.
    // If an animated scroll is in progress, this optimization is skipped to ensure that the animated scroll is really stopped.
    if (view->scrollAnimationStatus() == ScrollAnimationStatus::NotAnimating && !scrollToOptions.left.value() && !scrollToOptions.top.value() && view->contentsScrollPosition().isZero()) {
        LOG_WITH_STREAM(Scrolling, stream << "DOMWindow::scrollTo bailing because going to 0,0");
        return;
    }

    view->cancelScheduledScrolls();
    document()->updateLayoutIgnorePendingStylesheets();

    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(scrollToOptions.left.value()), view->mapFromCSSToLayoutUnits(scrollToOptions.top.value()));

    // FIXME: Should we use document()->scrollingElement()?
    // See https://bugs.webkit.org/show_bug.cgi?id=205059
    auto animated = useSmoothScrolling(scrollToOptions.behavior.value_or(ScrollBehavior::Auto), document()->documentElement()) ? ScrollIsAnimated::Yes : ScrollIsAnimated::No;
    auto scrollPositionChangeOptions = ScrollPositionChangeOptions::createProgrammaticWithOptions(clamping, animated, snapPointSelectionMethod);
    view->setContentsScrollPosition(layoutPos, scrollPositionChangeOptions);
}

bool DOMWindow::allowedToChangeWindowGeometry() const
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

void DOMWindow::moveBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    auto* page = frame()->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect update = fr;
    update.move(x, y);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    auto* page = frame()->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect sr = screenAvailableRect(page->mainFrame().view());
    fr.setLocation(sr.location());
    FloatRect update = fr;
    update.move(x, y);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    auto* page = frame()->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    auto* page = frame()->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(*page, update));
}

ExceptionOr<int> DOMWindow::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->contentSecurityPolicy()->allowEval(context->globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*context, WTFMove(action), Seconds::fromMilliseconds(timeout), true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(*context, timeoutId);
}

ExceptionOr<int> DOMWindow::setInterval(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    RefPtr context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->contentSecurityPolicy()->allowEval(context->globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*context, WTFMove(action), Seconds::fromMilliseconds(timeout), false);
}

void DOMWindow::clearInterval(int timeoutId)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(*context, timeoutId);
}

int DOMWindow::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    RefPtr document = this->document();
    if (!document)
        return 0;
    return document->requestAnimationFrame(WTFMove(callback));
}

int DOMWindow::webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    static bool firstTime = true;
    if (firstTime && document()) {
        document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "webkitRequestAnimationFrame() is deprecated and will be removed. Please use requestAnimationFrame() instead."_s);
        firstTime = false;
    }
    return requestAnimationFrame(WTFMove(callback));
}

void DOMWindow::cancelAnimationFrame(int id)
{
    RefPtr document = this->document();
    if (!document)
        return;
    document->cancelAnimationFrame(id);
}

int DOMWindow::requestIdleCallback(Ref<IdleRequestCallback>&& callback, const IdleRequestOptions& options)
{
    RefPtr document = this->document();
    if (!document)
        return 0;
    return document->requestIdleCallback(WTFMove(callback), Seconds::fromMilliseconds(options.timeout));
}

void DOMWindow::cancelIdleCallback(int id)
{
    RefPtr document = this->document();
    if (!document)
        return;
    return document->cancelIdleCallback(id);
}

void DOMWindow::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), WTFMove(promise));
}

void DOMWindow::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), sx, sy, sw, sh, WTFMove(promise));
}

bool DOMWindow::isSecureContext() const
{
    RefPtr document = this->document();
    if (!document)
        return false;
    return document->isSecureContext();
}

bool DOMWindow::crossOriginIsolated() const
{
    ASSERT(ScriptExecutionContext::crossOriginMode() == CrossOriginMode::Shared || !document() || document()->topDocument().crossOriginOpenerPolicy().value == CrossOriginOpenerPolicyValue::SameOriginPlusCOEP);
    return ScriptExecutionContext::crossOriginMode() == CrossOriginMode::Isolated;
}

static void didAddStorageEventListener(DOMWindow& window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system. 
    window.localStorage();
    window.sessionStorage();
}

bool DOMWindow::isSameSecurityOriginAsMainFrame() const
{
    auto* frame = this->frame();
    if (!frame || !frame->page() || !document())
        return false;

    if (frame->isMainFrame())
        return true;

    Document* mainFrameDocument = frame->mainFrame().document();

    if (mainFrameDocument && document()->securityOrigin().isSameOriginDomain(mainFrameDocument->securityOrigin()))
        return true;

    return false;
}

bool DOMWindow::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), options))
        return false;

    RefPtr document = this->document();
    auto& eventNames = WebCore::eventNames();
    if (document) {
        document->addListenerTypeIfNeeded(eventType);
        if (eventNames.isWheelEventType(eventType))
            document->didAddWheelEventHandler(*document);
        else if (eventNames.isTouchRelatedEventType(eventType, *document))
            document->didAddTouchEventHandler(*document);
        else if (eventType == eventNames.storageEvent)
            didAddStorageEventListener(*this);
    }

    if (eventType == eventNames.unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames.beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);
#if PLATFORM(IOS_FAMILY)
    else if (eventType == eventNames.scrollEvent)
        incrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (document && eventNames.isTouchRelatedEventType(eventType, *document))
        ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames.isGestureEventType(eventType))
        ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames.isGamepadEventType(eventType))
        incrementGamepadEventListenerCount();
#endif
#if ENABLE(DEVICE_ORIENTATION)
    else if (eventType == eventNames.deviceorientationEvent)
        startListeningForDeviceOrientationIfNecessary();
    else if (eventType == eventNames.devicemotionEvent)
        startListeningForDeviceMotionIfNecessary();
#endif

    return true;
}

#if ENABLE(DEVICE_ORIENTATION)

DeviceOrientationController* DOMWindow::deviceOrientationController() const
{
#if PLATFORM(IOS_FAMILY)
    return document() ? &document()->deviceOrientationController() : nullptr;
#else
    return DeviceOrientationController::from(page());
#endif
}

DeviceMotionController* DOMWindow::deviceMotionController() const
{
#if PLATFORM(IOS_FAMILY)
    return document() ? &document()->deviceMotionController() : nullptr;
#else
    return DeviceMotionController::from(page());
#endif
}

bool DOMWindow::isAllowedToUseDeviceMotionOrOrientation(String& message) const
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

bool DOMWindow::isAllowedToUseDeviceMotion(String& message) const
{
    if (!isAllowedToUseDeviceMotionOrOrientation(message))
        return false;

    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Gyroscope, *document(), LogFeaturePolicyFailure::No)
        || !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Accelerometer, *document(), LogFeaturePolicyFailure::No)) {
        message = "Third-party iframes are not allowed access to device motion unless explicitly allowed via Feature-Policy (gyroscope & accelerometer)"_s;
        return false;
    }

    return true;
}

bool DOMWindow::isAllowedToUseDeviceOrientation(String& message) const
{
    if (!isAllowedToUseDeviceMotionOrOrientation(message))
        return false;

    if (!isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Gyroscope, *document(), LogFeaturePolicyFailure::No)
        || !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Accelerometer, *document(), LogFeaturePolicyFailure::No)
        || !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Magnetometer, *document(), LogFeaturePolicyFailure::No)) {
        message = "Third-party iframes are not allowed access to device orientation unless explicitly allowed via Feature-Policy (gyroscope & accelerometer & magnetometer)"_s;
        return false;
    }

    return true;
}

bool DOMWindow::hasPermissionToReceiveDeviceMotionOrOrientationEvents(String& message) const
{
    if (frame()->settings().deviceOrientationPermissionAPIEnabled()) {
        if (!page()) {
            message = "No browsing context"_s;
            return false;
        }
        auto accessState = document()->deviceOrientationAndMotionAccessController().accessState(*document());
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

void DOMWindow::startListeningForDeviceOrientationIfNecessary()
{
    if (!hasEventListeners(eventNames().deviceorientationEvent))
        return;

    auto* deviceController = deviceOrientationController();
    if (!deviceController || deviceController->hasDeviceEventListener(*this))
        return;

    String innerMessage;
    if (!isAllowedToUseDeviceOrientation(innerMessage) || !hasPermissionToReceiveDeviceMotionOrOrientationEvents(innerMessage)) {
        if (RefPtr document = this->document())
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("No device orientation events will be fired, reason: ", innerMessage, "."));
        return;
    }

    deviceController->addDeviceEventListener(*this);
}

void DOMWindow::stopListeningForDeviceOrientationIfNecessary()
{
    if (hasEventListeners(eventNames().deviceorientationEvent))
        return;

    if (auto* deviceController = deviceOrientationController())
        deviceController->removeDeviceEventListener(*this);
}

void DOMWindow::startListeningForDeviceMotionIfNecessary()
{
    if (!hasEventListeners(eventNames().devicemotionEvent))
        return;

    auto* deviceController = deviceMotionController();
    if (!deviceController || deviceController->hasDeviceEventListener(*this))
        return;

    String innerMessage;
    if (!isAllowedToUseDeviceMotion(innerMessage) || !hasPermissionToReceiveDeviceMotionOrOrientationEvents(innerMessage)) {
        failedToRegisterDeviceMotionEventListener();
        if (RefPtr document = this->document())
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("No device motion events will be fired, reason: ", innerMessage, "."));
        return;
    }

    deviceController->addDeviceEventListener(*this);
}

void DOMWindow::stopListeningForDeviceMotionIfNecessary()
{
    if (hasEventListeners(eventNames().devicemotionEvent))
        return;

    if (auto* deviceController = deviceMotionController())
        deviceController->removeDeviceEventListener(*this);
}

void DOMWindow::failedToRegisterDeviceMotionEventListener()
{
#if PLATFORM(IOS_FAMILY)
    if (!isSameSecurityOriginAsMainFrame() || !isSecureContext())
        return;

    // FIXME: This is a quirk for chase.com on iPad (<rdar://problem/48423023>).
    if (RegistrableDomain::uncheckedCreateFromRegistrableDomainString("chase.com"_s).matches(document()->url())) {
        // Fire a fake DeviceMotionEvent with acceleration data to unblock the site's login flow.
        document()->postTask([](auto& context) {
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

void DOMWindow::incrementScrollEventListenersCount()
{
    RefPtr document = this->document();
    if (++m_scrollEventListenerCount == 1 && document == &document->topDocument()) {
        if (RefPtr frame = this->frame(); frame && frame->page())
            frame->page()->chrome().client().setNeedsScrollNotifications(*frame, true);
    }
}

void DOMWindow::decrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (!--m_scrollEventListenerCount && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page() && document->backForwardCacheState() == Document::NotInBackForwardCache)
            frame->page()->chrome().client().setNeedsScrollNotifications(*frame, false);
    }
}

#endif

void DOMWindow::resetAllGeolocationPermission()
{
    // FIXME: Can we remove the PLATFORM(IOS_FAMILY)-guard?
#if ENABLE(GEOLOCATION) && PLATFORM(IOS_FAMILY)
    if (m_navigator)
        NavigatorGeolocation::from(*m_navigator)->resetAllGeolocationPermission();
#endif
}

bool DOMWindow::removeEventListener(const AtomString& eventType, EventListener& listener, const EventListenerOptions& options)
{
    if (!EventTarget::removeEventListener(eventType, listener, options.capture))
        return false;

    RefPtr document = this->document();
    auto& eventNames = WebCore::eventNames();
    if (document) {
        if (eventNames.isWheelEventType(eventType))
            document->didRemoveWheelEventHandler(*document);
        else if (eventNames.isTouchRelatedEventType(eventType, *document))
            document->didRemoveTouchEventHandler(*document);
    }

    if (eventType == eventNames.unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames.beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);
#if PLATFORM(IOS_FAMILY)
    else if (eventType == eventNames.scrollEvent)
        decrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (document && eventNames.isTouchRelatedEventType(eventType, *document)) {
        ASSERT(m_touchAndGestureEventListenerCount > 0);
        --m_touchAndGestureEventListenerCount;
    }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames.isGestureEventType(eventType)) {
        ASSERT(m_touchAndGestureEventListenerCount > 0);
        --m_touchAndGestureEventListenerCount;
    }
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames.isGamepadEventType(eventType))
        decrementGamepadEventListenerCount();
#endif
#if ENABLE(DEVICE_ORIENTATION)
    else if (eventType == eventNames.deviceorientationEvent)
        stopListeningForDeviceOrientationIfNecessary();
    else if (eventType == eventNames.devicemotionEvent)
        stopListeningForDeviceMotionIfNecessary();
#endif

    return true;
}

void DOMWindow::languagesChanged()
{
    // https://html.spec.whatwg.org/multipage/system-state.html#dom-navigator-languages
    if (RefPtr document = this->document())
        document->queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, Event::create(eventNames().languagechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void DOMWindow::dispatchLoadEvent()
{
    // If we did not protect it, the document loader and its timing subobject might get destroyed
    // as a side effect of what event handling code does.
    Ref protectedThis { *this };
    RefPtr protectedLoader = frame() ? frame()->loader().documentLoader() : nullptr;
    bool shouldMarkLoadEventTimes = protectedLoader && !protectedLoader->timing().loadEventStart();

    if (shouldMarkLoadEventTimes) {
        auto now = MonotonicTime::now();
        protectedLoader->timing().setLoadEventStart(now);
        if (auto* navigationTiming = performance().navigationTiming())
            navigationTiming->documentLoadTiming().setLoadEventStart(now);
    }

    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No), document());

    if (shouldMarkLoadEventTimes) {
        auto now = MonotonicTime::now();
        protectedLoader->timing().setLoadEventEnd(now);
        if (auto* navigationTiming = performance().navigationTiming())
            navigationTiming->documentLoadTiming().setLoadEventEnd(now);
    }

    // Send a separate load event to the element that owns this frame.
    if (RefPtr ownerFrame = frame()) {
        if (RefPtr owner = ownerFrame->ownerElement())
            owner->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }

    InspectorInstrumentation::loadEventFired(frame());
}

void DOMWindow::dispatchEvent(Event& event, EventTarget* target)
{
    // FIXME: It's confusing to have both the inherited EventTarget::dispatchEvent function
    // and this function, which does something nearly identical but subtly different if
    // called with a target of null. Most callers pass the document as the target, though.
    // Fixing this could allow us to remove the special case in DocumentEventQueue::dispatchEvent.

    Ref protectedThis { *this };

    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // <http://www.whatwg.org/specs/web-apps/current-work/multipage/history.html#event-pageshow>.
    // FIXME: This code should go at call sites where pageshowEvent and pagehideEvents are
    // generated, not here inside the event dispatching process.
    if (event.eventInterface() == PageTransitionEventInterfaceType) {
        if (event.type() == eventNames().pageshowEvent) {
            if (m_lastPageStatus == PageStatus::Shown)
                return; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatus::Shown;
        } else if (event.type() == eventNames().pagehideEvent) {
            if (m_lastPageStatus == PageStatus::Hidden)
                return; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatus::Hidden;
        }
    }

    // FIXME: It doesn't seem right to have the inspector instrumentation here since not all
    // events dispatched to the window object are guaranteed to flow through this function.
    // But the instrumentation prevents us from calling EventDispatcher::dispatchEvent here.
    if (target)
        event.setTarget(target);
    else
        event.setTarget(Ref { *this });

    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    event.resetBeforeDispatch();

    RefPtr<Frame> protectedFrame;
    bool hasListenersForEvent = false;
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

void DOMWindow::removeAllEventListeners()
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
    if (auto* document = this->document())
        document->didRemoveEventTargetNode(*document);
#endif

    if (m_performance) {
        m_performance->removeAllEventListeners();
        m_performance->removeAllObservers();
    }

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void DOMWindow::captureEvents()
{
    // Not implemented.
}

void DOMWindow::releaseEvents()
{
    // Not implemented.
}

void DOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        if (auto loader = frame()->loader().activeDocumentLoader(); !loader || loader->mainDocumentError().isNull())
            print();
    }
}

void DOMWindow::setLocation(DOMWindow& activeWindow, const URL& completedURL, SetLocationLocking locking)
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
    if (completedURL.protocolIsJavaScript() && frameElement() && !frameElement()->document().contentSecurityPolicy()->allowJavaScriptURLs(aboutBlankURL().string(), { }, completedURL.string(), frameElement()))
        return;

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != LockHistoryBasedOnGestureState || !UserGestureIndicator::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    frame->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList);
}

void DOMWindow::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    if (PageConsoleClient* pageConsole = console())
        pageConsole->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String DOMWindow::crossDomainAccessErrorMessage(const DOMWindow& activeWindow, IncludeTargetOrigin includeTargetOrigin)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    ASSERT(!activeWindow.document()->securityOrigin().isSameOriginDomain(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin& activeOrigin = activeWindow.document()->securityOrigin();
    SecurityOrigin& targetOrigin = document()->securityOrigin();
    String message;
    if (includeTargetOrigin == IncludeTargetOrigin::Yes)
        message = makeString("Blocked a frame with origin \"", activeOrigin.toString(), "\" from accessing a frame with origin \"", targetOrigin.toString(), "\". ");
    else
        message = makeString("Blocked a frame with origin \"", activeOrigin.toString(), "\" from accessing a cross-origin frame. ");

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    URL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || activeWindow.document()->isSandboxed(SandboxOrigin)) {
        if (includeTargetOrigin == IncludeTargetOrigin::Yes)
            message = makeString("Blocked a frame at \"", SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a frame at \"", SecurityOrigin::create(targetURL).get().toString(), "\". ");
        else
            message = makeString("Blocked a frame at \"", SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a cross-origin frame. ");

        if (document()->isSandboxed(SandboxOrigin) && activeWindow.document()->isSandboxed(SandboxOrigin))
            return makeString("Sandbox access violation: ", message, " Both frames are sandboxed and lack the \"allow-same-origin\" flag.");
        if (document()->isSandboxed(SandboxOrigin))
            return makeString("Sandbox access violation: ", message, " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.");
        return makeString("Sandbox access violation: ", message, " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.");
    }

    if (includeTargetOrigin == IncludeTargetOrigin::Yes) {
        // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
        if (targetOrigin.protocol() != activeOrigin.protocol())
            return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

        // 'document.domain' errors.
        if (targetOrigin.domainWasSetInDOM() && activeOrigin.domainWasSetInDOM())
            return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin.domain() + "\", the frame being accessed set it to \"" + targetOrigin.domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
        if (activeOrigin.domainWasSetInDOM())
            return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin.domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
        if (targetOrigin.domainWasSetInDOM())
            return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin.domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";
    }

    // Default.
    return message + "Protocols, domains, and ports must match.";
}

bool DOMWindow::isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString)
{
    if (!WTF::protocolIsJavaScript(urlString))
        return false;

    // If this DOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect DOMWindow from
    // Frame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "activeWindow == this" check?
        if (&activeWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (activeWindow.document()->securityOrigin().isSameOriginDomain(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow, IncludeTargetOrigin::Yes));
    return true;
}

ExceptionOr<RefPtr<Frame>> DOMWindow::createWindow(const String& urlString, const AtomString& frameName, const WindowFeatures& initialWindowFeatures, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, const Function<void(DOMWindow&)>& prepareDialogFunction)
{
    RefPtr activeFrame = activeWindow.frame();
    if (!activeFrame)
        return RefPtr<Frame> { nullptr };

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return RefPtr<Frame> { nullptr };

    URL completedURL = urlString.isEmpty() ? URL({ }, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid())
        return Exception { SyntaxError };

    WindowFeatures windowFeatures = initialWindowFeatures;

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = windowFeatures.noreferrer ? String() : SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.loader().outgoingReferrer());
    auto initiatedByMainFrame = activeFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;

    ResourceRequest resourceRequest { completedURL, referrer };
    auto* openerDocumentLoader = openerFrame.document() ? openerFrame.document()->loader() : nullptr;
    if (openerDocumentLoader)
        resourceRequest.setIsAppInitiated(openerDocumentLoader->lastNavigationWasAppInitiated());
    FrameLoadRequest frameLoadRequest { *activeDocument, activeDocument->securityOrigin(), WTFMove(resourceRequest), frameName, initiatedByMainFrame };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(activeDocument->shouldOpenExternalURLsPolicyToPropagate());

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    auto newFrame = WebCore::createWindow(*activeFrame, openerFrame, WTFMove(frameLoadRequest), windowFeatures, created);
    if (!newFrame)
        return RefPtr<Frame> { nullptr };

    bool noopener = windowFeatures.noopener || windowFeatures.noreferrer;
    if (!noopener)
        newFrame->loader().setOpener(&openerFrame);

    if (created)
        newFrame->page()->setOpenedByDOM();

    if (newFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL.string()))
        return noopener ? RefPtr<Frame> { nullptr } : newFrame;

    if (prepareDialogFunction)
        prepareDialogFunction(*newFrame->document()->domWindow());

    if (created) {
        ResourceRequest resourceRequest { completedURL, referrer, ResourceRequestCachePolicy::UseProtocolCachePolicy };
        FrameLoader::addSameSiteInfoToRequestIfNeeded(resourceRequest, openerFrame.document());
        FrameLoadRequest frameLoadRequest { *activeWindow.document(), activeWindow.document()->securityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), initiatedByMainFrame };
        frameLoadRequest.setShouldOpenExternalURLsPolicy(activeDocument->shouldOpenExternalURLsPolicyToPropagate());
        newFrame->loader().changeLocation(WTFMove(frameLoadRequest));
    } else if (!urlString.isEmpty()) {
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(), completedURL, referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return RefPtr<Frame> { nullptr };

    return noopener ? RefPtr<Frame> { nullptr } : newFrame;
}

ExceptionOr<RefPtr<WindowProxy>> DOMWindow::open(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& urlStringToOpen, const AtomString& frameName, const String& windowFeaturesString)
{
#if ENABLE(TRACKING_PREVENTION)
    if (RefPtr document = this->document()) {
        if (document->settings().needsSiteSpecificQuirks() && urlStringToOpen == Quirks::BBCRadioPlayerURLString()) {
            auto radioPlayerDomain = RegistrableDomain(URL { Quirks::staticRadioPlayerURLString() });
            auto BBCDomain = RegistrableDomain(URL { Quirks::BBCRadioPlayerURLString() });
            if (!ResourceLoadObserver::shared().hasCrossPageStorageAccess(radioPlayerDomain, BBCDomain))
                return RefPtr<WindowProxy> { nullptr };
        }
    }
#endif

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
    auto* page = firstFrame->page();
    RefPtr firstFrameDocument = firstFrame->document();
    RefPtr mainFrameDocument = firstFrame->mainFrame().document();
    RefPtr mainFrameDocumentLoader = mainFrameDocument ? mainFrameDocument->loader() : nullptr;
    if (firstFrameDocument && page && mainFrameDocumentLoader) {
        auto results = page->userContentProvider().processContentRuleListsForLoad(*page, firstFrameDocument->completeURL(urlString), ContentExtensions::ResourceType::Popup, *mainFrameDocumentLoader);
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
        if (frameName.isEmpty() || !frame->loader().findFrameForNavigation(frameName, activeDocument.get()))
            return RefPtr<WindowProxy> { nullptr };
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    RefPtr<Frame> targetFrame;
    if (isTopTargetFrameName(frameName))
        targetFrame = dynamicDowncast<LocalFrame>(&frame->tree().top());
    else if (isParentTargetFrameName(frameName)) {
        if (RefPtr parent = frame->tree().parent())
            targetFrame = dynamicDowncast<LocalFrame>(parent.get());
        else
            targetFrame = frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame.get()))
            return RefPtr<WindowProxy> { nullptr };

        URL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL.string()))
            return &targetFrame->windowProxy();

        if (urlString.isEmpty())
            return &targetFrame->windowProxy();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        targetFrame->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(), completedURL, firstFrame->loader().outgoingReferrer(),
            lockHistory, LockBackForwardList::No);
        return &targetFrame->windowProxy();
    }

    auto newFrameOrException = createWindow(urlString, frameName, parseWindowFeatures(windowFeaturesString), activeWindow, *firstFrame, *frame);
    if (newFrameOrException.hasException())
        return newFrameOrException.releaseException();

    auto newFrame = newFrameOrException.releaseReturnValue();
    return newFrame ? &newFrame->windowProxy() : RefPtr<WindowProxy> { nullptr };
}

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, const Function<void(DOMWindow&)>& prepareDialogFunction)
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

    auto dialogFrameOrException = createWindow(urlString, emptyAtom(), parseDialogFeatures(dialogFeaturesString, screenAvailableRect(frame->view())), activeWindow, *firstFrame, *frame, prepareDialogFunction);
    if (dialogFrameOrException.hasException())
        return;
    RefPtr<Frame> dialogFrame = dialogFrameOrException.releaseReturnValue();
    if (!dialogFrame)
        return;
    dialogFrame->page()->chrome().runModal();
}

void DOMWindow::enableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().enableSuddenTermination();
}

void DOMWindow::disableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().disableSuddenTermination();
}

Frame* DOMWindow::frame() const
{
    auto* document = this->document();
    return document ? document->frame() : nullptr;
}

void DOMWindow::eventListenersDidChange()
{
    if (m_localStorage || m_sessionStorage) {
        if (hasEventListeners(eventNames().storageEvent))
            windowsInterestedInStorageEvents().add(*this);
        else
            windowsInterestedInStorageEvents().remove(*this);
    }
}

WebCoreOpaqueRoot root(DOMWindow* window)
{
    return WebCoreOpaqueRoot { window };
}

} // namespace WebCore
