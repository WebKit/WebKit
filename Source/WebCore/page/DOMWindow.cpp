/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#include "Crypto.h"
#include "CustomElementRegistry.h"
#include "DOMApplicationCache.h"
#include "DOMSelection.h"
#include "DOMStringList.h"
#include "DOMTimer.h"
#include "DOMTokenList.h"
#include "DOMURL.h"
#include "DOMWindowExtension.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
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
#include "InspectorInstrumentation.h"
#include "JSDOMWindowBase.h"
#include "JSExecState.h"
#include "Location.h"
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
#include "RequestAnimationFrameCallback.h"
#include "ResourceLoadInfo.h"
#include "ResourceLoadObserver.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "ScheduledAction.h"
#include "Screen.h"
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
#include "URL.h"
#include "UserGestureIndicator.h"
#include "VisualViewport.h"
#include "WebKitPoint.h"
#include "WindowFeatures.h"
#include "WindowFocusAllowedIndicator.h"
#include "WindowProxy.h"
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <algorithm>
#include <memory>
#include <wtf/Language.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/Variant.h>
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

#if PLATFORM(IOS_FAMILY)
#include "WKContentObservation.h"
#include "WKContentObservationInternal.h"
#endif


namespace WebCore {
using namespace Inspector;

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow& window, MessageWithMessagePorts&& message, const String& sourceOrigin, RefPtr<WindowProxy>&& source, RefPtr<SecurityOrigin>&& targetOrigin, RefPtr<ScriptCallStack>&& stackTrace)
        : m_window(window)
        , m_message(WTFMove(message))
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_targetOrigin(WTFMove(targetOrigin))
        , m_stackTrace(stackTrace)
        , m_userGestureToForward(UserGestureIndicator::currentUserGesture())
    {
    }

    Ref<MessageEvent> event(ScriptExecutionContext& context)
    {
        return MessageEvent::create(MessagePort::entanglePorts(context, WTFMove(m_message.transferredPorts)), m_message.message.releaseNonNull(), m_origin, { }, m_source ? std::make_optional(MessageEventSource(WTFMove(m_source))) : std::nullopt);
    }

    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }

private:
    void fired() override
    {
        // This object gets deleted when std::unique_ptr falls out of scope..
        std::unique_ptr<PostMessageTimer> timer(this);
        
        UserGestureIndicator userGestureIndicator(m_userGestureToForward);
        m_window->postMessageTimerFired(*timer);
    }

    Ref<DOMWindow> m_window;
    MessageWithMessagePorts m_message;
    String m_origin;
    RefPtr<WindowProxy> m_source;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtr<ScriptCallStack> m_stackTrace;
    RefPtr<UserGestureToken> m_userGestureToForward;
};

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

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto& window : set)
        windows.uncheckedAppend(*window.key);

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        Frame* frame = window->frame();
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

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        window->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), window->document());
        window->dispatchEvent(Event::create(eventNames().unloadEvent, Event::CanBubble::No, Event::IsCancelable::No), window->document());

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
    : AbstractDOMWindow(GlobalWindowIdentifier { Process::identifier(), generateObjectIdentifier<WindowIdentifierType>() })
    , ContextDestructionObserver(&document)
{
    ASSERT(frame());
    addLanguageChangeObserver(this, &languagesChangedCallback);
}

void DOMWindow::didSecureTransitionTo(Document& document)
{
    observeContext(&document);
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
}

RefPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : nullptr;
}

Page* DOMWindow::page()
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
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInCachedFrame.
    for (auto& property : copyToVector(m_properties))
        property->willDestroyGlobalObjectInCachedFrame();
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    for (auto& property : copyToVector(m_properties))
        property->willDestroyGlobalObjectInFrame();
}

void DOMWindow::willDetachDocumentFromFrame()
{
    if (!frame())
        return;

    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    for (auto& property : copyToVector(m_properties))
        property->willDetachGlobalObjectFromFrame();

    if (m_performance)
        m_performance->clearResourceTimings();
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

void DOMWindow::registerProperty(DOMWindowProperty& property)
{
    m_properties.add(&property);
}

void DOMWindow::unregisterProperty(DOMWindowProperty& property)
{
    m_properties.remove(&property);
}

void DOMWindow::resetUnlessSuspendedForDocumentSuspension()
{
    if (m_suspendedForDocumentSuspension)
        return;
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void DOMWindow::suspendForPageCache()
{
    for (auto& property : copyToVector(m_properties))
        property->suspendForPageCache();

    m_suspendedForDocumentSuspension = true;
}

void DOMWindow::resumeFromPageCache()
{
    for (auto& property : copyToVector(m_properties))
        property->resumeFromPageCache();

    m_suspendedForDocumentSuspension = false;
}

void DOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_applicationCache = nullptr;
    m_crypto = nullptr;
    m_history = nullptr;
    m_localStorage = nullptr;
    m_location = nullptr;
    m_locationbar = nullptr;
    m_media = nullptr;
    m_menubar = nullptr;
    m_navigator = nullptr;
    m_personalbar = nullptr;
    m_screen = nullptr;
    m_scrollbars = nullptr;
    m_selection = nullptr;
    m_sessionStorage = nullptr;
    m_statusbar = nullptr;
    m_toolbar = nullptr;
    m_performance = nullptr;
    m_visualViewport = nullptr;
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
    return *m_customElementRegistry;
}

static ExceptionOr<SelectorQuery&> selectorQueryInFrame(Frame* frame, const String& selectors)
{
    if (!frame)
        return Exception { NotSupportedError };

    Document* document = frame->document();
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

#if ENABLE(ORIENTATION_EVENTS)

int DOMWindow::orientation() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    return frame->orientation();
}

#endif

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

    return *m_navigator;
}

Performance& DOMWindow::performance() const
{
    if (!m_performance) {
        MonotonicTime timeOrigin = document() && document()->loader() ? document()->loader()->timing().referenceMonotonicTime() : MonotonicTime::now();
        m_performance = Performance::create(document(), timeOrigin);
    }
    return *m_performance;
}

double DOMWindow::nowTimestamp() const
{
    return performance().now() / 1000.;
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
    auto* frame = this->frame();
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

    auto* document = this->document();
    if (!document)
        return nullptr;

    if (!document->securityOrigin().canAccessSessionStorage(document->topOrigin()))
        return Exception { SecurityError };

    if (m_sessionStorage)
        return m_sessionStorage.get();

    auto* page = document->page();
    if (!page)
        return nullptr;

    auto storageArea = page->sessionStorage()->storageArea(document->securityOrigin().data());
    m_sessionStorage = Storage::create(*this, WTFMove(storageArea));
    return m_sessionStorage.get();
}

ExceptionOr<Storage*> DOMWindow::localStorage()
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    auto* document = this->document();
    if (!document)
        return nullptr;

    if (!document->securityOrigin().canAccessLocalStorage(nullptr))
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
    return m_localStorage.get();
}

ExceptionOr<void> DOMWindow::postMessage(JSC::ExecState& state, DOMWindow& incumbentWindow, JSC::JSValue messageValue, const String& targetOrigin, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    if (!isCurrentlyDisplayedInFrame())
        return { };

    Document* sourceDocument = incumbentWindow.document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SyntaxError exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return { };
        target = &sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique())
            return Exception { SyntaxError };
    }

    Vector<RefPtr<MessagePort>> ports;
    auto messageData = SerializedScriptValue::create(state, messageValue, WTFMove(transfer), ports, SerializationContext::WindowPostMessage);
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
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(JSExecState::currentState());

    MessageWithMessagePorts message { messageData.releaseReturnValue(), disentangledPorts.releaseReturnValue() };

    // Schedule the message.
    RefPtr<WindowProxy> incumbentWindowProxy = incumbentWindow.frame() ? &incumbentWindow.frame()->windowProxy() : nullptr;
    auto* timer = new PostMessageTimer(*this, WTFMove(message), sourceOrigin, WTFMove(incumbentWindowProxy), WTFMove(target), WTFMove(stackTrace));
    timer->startOneShot(0_s);

    InspectorInstrumentation::didPostMessage(*frame(), *timer, state);

    return { };
}

void DOMWindow::postMessageTimerFired(PostMessageTimer& timer)
{
    if (!document() || !isCurrentlyDisplayedInFrame())
        return;

    auto* frame = this->frame();
    if (auto* intendedTargetOrigin = timer.targetOrigin()) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            if (auto* pageConsole = console()) {
                String message = makeString("Unable to post message to ", intendedTargetOrigin->toString(), ". Recipient has origin ", document()->securityOrigin().toString(), ".\n");
                if (timer.stackTrace())
                    pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message, *timer.stackTrace());
                else
                    pageConsole->addMessage(MessageSource::Security, MessageLevel::Error, message);
            }

            InspectorInstrumentation::didFailPostMessage(*frame, timer);
            return;
        }
    }

    InspectorInstrumentation::willDispatchPostMessage(*frame, timer);

    dispatchEvent(timer.event(*document()));

    InspectorInstrumentation::didDispatchPostMessage(*frame, timer);
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
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    return frame->ownerElement();
}

void DOMWindow::focus(DOMWindow& incumbentWindow)
{
    auto* opener = this->opener();
    focus(opener && opener != self() && incumbentWindow.self() == opener);
}

void DOMWindow::focus(bool allowFocus)
{
    if (!frame())
        return;

    Page* page = frame()->page();
    if (!page)
        return;

    allowFocus = allowFocus || WindowFocusAllowedIndicator::windowFocusAllowed() || !frame()->settings().windowFocusRestricted();

    // If we're a top level window, bring the window to the front.
    if (frame()->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!frame())
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    Frame* focusedFrame = page->focusController().focusedFrame();
    if (focusedFrame && focusedFrame != frame())
        focusedFrame->document()->setFocusedElement(nullptr);

    // setFocusedElement may clear frame(), so recheck before using it.
    if (auto* frame = this->frame())
        frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
    auto* frame = this->frame();
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
    auto* frame = this->frame();
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

    page->setIsClosing();
    page->chrome().closeWindowSoon();
}

void DOMWindow::print()
{
    auto* frame = this->frame();
    if (!frame)
        return;

    auto* page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.print is not allowed while unloading a page.");
        return;
    }

    if (frame->loader().activeDocumentLoader()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(*frame);
}

void DOMWindow::stop()
{
    auto* frame = this->frame();
    if (!frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    frame->loader().stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    auto* frame = this->frame();
    if (!frame)
        return;

    if (document()->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.alert is not allowed in a sandboxed frame when the allow-modals flag is not set.");
        return;
    }

    auto* page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.alert is not allowed while unloading a page.");
        return;
    }

    frame->document()->updateStyleIfNeeded();
#if ENABLE(POINTER_LOCK)
    page->pointerLockController().requestPointerUnlock();
#endif

    page->chrome().runJavaScriptAlert(*frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    auto* frame = this->frame();
    if (!frame)
        return false;
    
    if (document()->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.confirm is not allowed in a sandboxed frame when the allow-modals flag is not set.");
        return false;
    }

    auto* page = frame->page();
    if (!page)
        return false;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.confirm is not allowed while unloading a page.");
        return false;
    }

    frame->document()->updateStyleIfNeeded();
#if ENABLE(POINTER_LOCK)
    page->pointerLockController().requestPointerUnlock();
#endif

    return page->chrome().runJavaScriptConfirm(*frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    auto* frame = this->frame();
    if (!frame)
        return String();

    if (document()->isSandboxed(SandboxModals)) {
        printErrorMessage("Use of window.prompt is not allowed in a sandboxed frame when the allow-modals flag is not set.");
        return String();
    }

    auto* page = frame->page();
    if (!page)
        return String();

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.prompt is not allowed while unloading a page.");
        return String();
    }

    frame->document()->updateStyleIfNeeded();
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
    if (!isCurrentlyDisplayedInFrame())
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

    auto* view = frame()->isMainFrame() ? frame()->view() : frame()->mainFrame().view();
    if (!view)
        return 0;

    return view->frameRect().height();
#else
    auto* frame = this->frame();
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

    auto* view = frame()->isMainFrame() ? frame()->view() : frame()->mainFrame().view();
    if (!view)
        return 0;

    return view->frameRect().width();
#else
    auto* frame = this->frame();
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
    auto* frame = this->frame();
    if (!frame)
        return 0;

    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (auto* frameElement = this->frameElement())
        frameElement->document().updateLayoutIfDimensionsOutOfDate(*frameElement, HeightDimensionsCheck);

    FrameView* view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int DOMWindow::innerWidth() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    // Force enough layout in the parent document to ensure that the FrameView has been resized.
    if (auto* frameElement = this->frameElement())
        frameElement->document().updateLayoutIfDimensionsOutOfDate(*frameElement, WidthDimensionsCheck);

    FrameView* view = frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int DOMWindow::screenX() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    FrameView* view = frame->view();
    if (!view)
        return 0;

    int scrollX = view->contentsScrollPosition().x();
    if (!scrollX)
        return 0;

    frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
}

int DOMWindow::scrollY() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;

    FrameView* view = frame->view();
    if (!view)
        return 0;

    int scrollY = view->contentsScrollPosition().y();
    if (!scrollY)
        return 0;

    frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
}

bool DOMWindow::closed() const
{
    return !frame();
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return frame()->tree().scopedChildCount();
}

String DOMWindow::name() const
{
    auto* frame = this->frame();
    if (!frame)
        return String();

    return frame->tree().name();
}

void DOMWindow::setName(const String& string)
{
    auto* frame = this->frame();
    if (!frame)
        return;

    frame->tree().setName(string);
}

void DOMWindow::setStatus(const String& string) 
{
    m_status = string;

    auto* frame = this->frame();
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

    auto* frame = this->frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    ASSERT(frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(*frame, m_defaultStatus);
}

WindowProxy* DOMWindow::self() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    return &frame->windowProxy();
}

WindowProxy* DOMWindow::opener() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    auto* openerFrame = frame->loader().opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

void DOMWindow::disownOpener()
{
    if (auto* frame = this->frame())
        frame->loader().setOpener(nullptr);
}

WindowProxy* DOMWindow::parent() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    auto* parentFrame = frame->tree().parent();
    if (parentFrame)
        return &parentFrame->windowProxy();

    return &frame->windowProxy();
}

WindowProxy* DOMWindow::top() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    if (!frame->page())
        return nullptr;

    return &frame->tree().top().windowProxy();
}

String DOMWindow::origin() const
{
    auto document = this->document();
    return document ? document->securityOrigin().toString() : emptyString();
}

Document* DOMWindow::document() const
{
    return downcast<Document>(ContextDestructionObserver::scriptExecutionContext());
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
    CSSSelector::PseudoElementType pseudoType = CSSSelector::parsePseudoElementType(pseudoElement.substringSharingImpl(colonStart));
    if (pseudoType == CSSSelector::PseudoElementUnknown && !pseudoElement.isEmpty())
        return nullptr;

    auto* frame = this->frame();
    frame->document()->styleScope().flushPendingUpdate();

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= StyleResolver::UAAndUserCSSRules;

    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);

    auto matchedRules = frame->document()->styleScope().resolver().pseudoStyleRulesForElement(element, pseudoId, rulesToInclude);
    if (matchedRules.isEmpty())
        return nullptr;

    bool allowCrossOrigin = frame->settings().crossOriginCheckInGetMatchedCSSRulesDisabled();

    RefPtr<StaticCSSRuleList> ruleList = StaticCSSRuleList::create();
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
    scrollBy({ x, y });
}

void DOMWindow::scrollBy(const ScrollToOptions& options) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = frame()->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options, 0, 0);
    scrollToOptions.left.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
    scrollToOptions.top.value() += view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
    scrollTo(scrollToOptions);
}

void DOMWindow::scrollTo(double x, double y, ScrollClamping clamping) const
{
    scrollTo({ x, y }, clamping);
}

void DOMWindow::scrollTo(const ScrollToOptions& options, ScrollClamping) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr<FrameView> view = frame()->view();
    if (!view)
        return;

    ScrollToOptions scrollToOptions = normalizeNonFiniteCoordinatesOrFallBackTo(options,
        view->contentsScrollPosition().x(), view->contentsScrollPosition().y()
    );

    if (!scrollToOptions.left.value() && !scrollToOptions.top.value() && view->contentsScrollPosition() == IntPoint(0, 0))
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(scrollToOptions.left.value()), view->mapFromCSSToLayoutUnits(scrollToOptions.top.value()));
    view->setContentsScrollPosition(layoutPos);
}

bool DOMWindow::allowedToChangeWindowGeometry() const
{
    auto* frame = this->frame();
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

ExceptionOr<int> DOMWindow::setTimeout(JSC::ExecState& state, std::unique_ptr<ScheduledAction> action, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->contentSecurityPolicy()->allowEval(&state))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*context, WTFMove(action), Seconds::fromMilliseconds(timeout), true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
#if PLATFORM(IOS_FAMILY)
    if (auto* frame = this->frame()) {
        Document* document = frame->document();
        if (timeoutId > 0 && document) {
            DOMTimer* timer = document->findTimeout(timeoutId);
            if (timer && WebThreadContainsObservedContentModifier(timer)) {
                WebThreadRemoveObservedContentModifier(timer);

                if (!WebThreadCountOfObservedContentModifiers()) {
                    if (Page* page = frame->page())
                        page->chrome().client().observedContentChange(*frame);
                }
            }
        }
    }
#endif
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(*context, timeoutId);
}

ExceptionOr<int> DOMWindow::setInterval(JSC::ExecState& state, std::unique_ptr<ScheduledAction> action, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return Exception { InvalidAccessError };

    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!context->contentSecurityPolicy()->allowEval(&state))
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
    callback->m_useLegacyTimeBase = false;
    auto* document = this->document();
    if (!document)
        return 0;
    return document->requestAnimationFrame(WTFMove(callback));
}

int DOMWindow::webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    callback->m_useLegacyTimeBase = true;
    auto* document = this->document();
    if (!document)
        return 0;
    return document->requestAnimationFrame(WTFMove(callback));
}

void DOMWindow::cancelAnimationFrame(int id)
{
    auto* document = this->document();
    if (!document)
        return;
    document->cancelAnimationFrame(id);
}

void DOMWindow::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    auto* document = this->document();
    if (!document) {
        promise.reject(InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), WTFMove(promise));
}

void DOMWindow::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    auto* document = this->document();
    if (!document) {
        promise.reject(InvalidStateError);
        return;
    }
    ImageBitmap::createPromise(*document, WTFMove(source), WTFMove(options), sx, sy, sw, sh, WTFMove(promise));
}

bool DOMWindow::isSecureContext() const
{
    auto* document = this->document();
    if (!document)
        return false;
    return document->isSecureContext();
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

    if (mainFrameDocument && document()->securityOrigin().canAccess(mainFrameDocument->securityOrigin()))
        return true;

    return false;
}

bool DOMWindow::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), options))
        return false;

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (eventNames().isWheelEventType(eventType))
            document->didAddWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didAddTouchEventHandler(*document);
        else if (eventType == eventNames().storageEvent)
            didAddStorageEventListener(*this);
    }

    if (eventType == eventNames().unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS_FAMILY)
    else if ((eventType == eventNames().devicemotionEvent || eventType == eventNames().deviceorientationEvent) && document()) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (eventType == eventNames().deviceorientationEvent)
                document()->deviceOrientationController()->addDeviceEventListener(this);
            else
                document()->deviceMotionController()->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Blocked attempt add device motion or orientation listener from child frame that wasn't the same security origin as the main page."_s);
    }
#else
    else if (eventType == eventNames().devicemotionEvent) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (DeviceMotionController* controller = DeviceMotionController::from(page()))
                controller->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Blocked attempt add device motion listener from child frame that wasn't the same security origin as the main page."_s);
    } else if (eventType == eventNames().deviceorientationEvent) {
        if (isSameSecurityOriginAsMainFrame()) {
            if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
                controller->addDeviceEventListener(this);
        } else if (document())
            document()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Blocked attempt add device orientation listener from child frame that wasn't the same security origin as the main page."_s);
    }
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS_FAMILY)
    else if (eventType == eventNames().scrollEvent)
        incrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType))
        ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType))
        ++m_touchAndGestureEventListenerCount;
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        incrementGamepadEventListenerCount();
#endif

    return true;
}

#if PLATFORM(IOS_FAMILY)

void DOMWindow::incrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (++m_scrollEventListenerCount == 1 && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page())
            frame->page()->chrome().client().setNeedsScrollNotifications(*frame, true);
    }
}

void DOMWindow::decrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (!--m_scrollEventListenerCount && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page() && document->pageCacheState() == Document::NotInPageCache)
            frame->page()->chrome().client().setNeedsScrollNotifications(*frame, false);
    }
}

#endif

void DOMWindow::resetAllGeolocationPermission()
{
    // FIXME: Can we remove the PLATFORM(IOS_FAMILY)-guard?
#if ENABLE(GEOLOCATION) && PLATFORM(IOS_FAMILY)
    if (m_navigator)
        NavigatorGeolocation::from(m_navigator.get())->resetAllGeolocationPermission();
#endif
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener& listener, const ListenerOptions& options)
{
    if (!EventTarget::removeEventListener(eventType, listener, options.capture))
        return false;

    if (Document* document = this->document()) {
        if (eventNames().isWheelEventType(eventType))
            document->didRemoveWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(*document);
    }

    if (eventType == eventNames().unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS_FAMILY)
    else if (eventType == eventNames().devicemotionEvent && document())
        document()->deviceMotionController()->removeDeviceEventListener(this);
    else if (eventType == eventNames().deviceorientationEvent && document())
        document()->deviceOrientationController()->removeDeviceEventListener(this);
#else
    else if (eventType == eventNames().devicemotionEvent) {
        if (DeviceMotionController* controller = DeviceMotionController::from(page()))
            controller->removeDeviceEventListener(this);
    } else if (eventType == eventNames().deviceorientationEvent) {
        if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
            controller->removeDeviceEventListener(this);
    }
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS_FAMILY)
    else if (eventType == eventNames().scrollEvent)
        decrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType)) {
        ASSERT(m_touchAndGestureEventListenerCount > 0);
        --m_touchAndGestureEventListenerCount;
    }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType)) {
        ASSERT(m_touchAndGestureEventListenerCount > 0);
        --m_touchAndGestureEventListenerCount;
    }
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        decrementGamepadEventListenerCount();
#endif

    return true;
}

void DOMWindow::languagesChanged()
{
    if (auto* document = this->document())
        document->enqueueWindowEvent(Event::create(eventNames().languagechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void DOMWindow::dispatchLoadEvent()
{
    // If we did not protect it, the document loader and its timing subobject might get destroyed
    // as a side effect of what event handling code does.
    auto protectedThis = makeRef(*this);
    auto protectedLoader = makeRefPtr(frame() ? frame()->loader().documentLoader() : nullptr);
    bool shouldMarkLoadEventTimes = protectedLoader && !protectedLoader->timing().loadEventStart();

    if (shouldMarkLoadEventTimes)
        protectedLoader->timing().markLoadEventStart();

    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No), document());

    if (shouldMarkLoadEventTimes)
        protectedLoader->timing().markLoadEventEnd();

    // Send a separate load event to the element that owns this frame.
    if (frame()) {
        if (auto* owner = frame()->ownerElement())
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

    auto protectedThis = makeRef(*this);

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
    event.setTarget(target ? target : this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);
    event.resetBeforeDispatch();
    auto cookie = InspectorInstrumentation::willDispatchEventOnWindow(frame(), event, *this);
    // FIXME: We should use EventDispatcher everywhere.
    fireEventListeners(event, EventInvokePhase::Capturing);
    fireEventListeners(event, EventInvokePhase::Bubbling);
    InspectorInstrumentation::didDispatchEventOnWindow(cookie);
    event.resetAfterDispatch();
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS_FAMILY)
    if (Document* document = this->document()) {
        document->deviceMotionController()->removeAllDeviceEventListeners(this);
        document->deviceOrientationController()->removeAllDeviceEventListeners(this);
    }
#else
    if (DeviceMotionController* controller = DeviceMotionController::from(page()))
        controller->removeAllDeviceEventListeners(this);
    if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
        controller->removeAllDeviceEventListeners(this);
#endif // PLATFORM(IOS_FAMILY)
#endif // ENABLE(DEVICE_ORIENTATION)

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
    if (Document* document = this->document())
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
        if (frame()->loader().activeDocumentLoader()->mainDocumentError().isNull())
            print();
    }
}

void DOMWindow::setLocation(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& urlString, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    auto* frame = this->frame();
    if (!activeDocument->canNavigate(frame))
        return;

    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    URL completedURL = firstFrame->document()->completeURL(urlString);
    if (completedURL.isNull())
        return;

    if (isInsecureScriptAccess(activeWindow, completedURL))
        return;

    // We want a new history item if we are processing a user gesture.
    LockHistory lockHistory = (locking != LockHistoryBasedOnGestureState || !UserGestureIndicator::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    frame->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList);
}

void DOMWindow::printErrorMessage(const String& message)
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

    ASSERT(!activeWindow.document()->securityOrigin().canAccess(document()->securityOrigin()));

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
    if (!protocolIsJavaScript(urlString))
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
        if (activeWindow.document()->securityOrigin().canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow, IncludeTargetOrigin::Yes));
    return true;
}

ExceptionOr<RefPtr<Frame>> DOMWindow::createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, const WTF::Function<void(DOMWindow&)>& prepareDialogFunction)
{
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return RefPtr<Frame> { nullptr };

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return RefPtr<Frame> { nullptr };

    URL completedURL = urlString.isEmpty() ? URL({ }, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid())
        return Exception { SyntaxError };

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.loader().outgoingReferrer());
    auto initiatedByMainFrame = activeFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;

    ResourceRequest resourceRequest { completedURL, referrer };
    FrameLoader::addHTTPOriginIfNeeded(resourceRequest, firstFrame.loader().outgoingOrigin());
    FrameLoadRequest frameLoadRequest { *activeDocument, activeDocument->securityOrigin(), resourceRequest, frameName, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, activeDocument->shouldOpenExternalURLsPolicyToPropagate(), initiatedByMainFrame };

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    RefPtr<Frame> newFrame = WebCore::createWindow(*activeFrame, openerFrame, WTFMove(frameLoadRequest), windowFeatures, created);
    if (!newFrame)
        return RefPtr<Frame> { nullptr };

    if (!windowFeatures.noopener) {
        newFrame->loader().setOpener(&openerFrame);
        newFrame->page()->setOpenedViaWindowOpenWithOpener();
    }
    if (created)
        newFrame->page()->setOpenedByDOM();

    if (newFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
        return windowFeatures.noopener ? RefPtr<Frame> { nullptr } : newFrame;

    if (prepareDialogFunction)
        prepareDialogFunction(*newFrame->document()->domWindow());

    if (created) {
        ResourceRequest resourceRequest { completedURL, referrer, ResourceRequestCachePolicy::UseProtocolCachePolicy };
        FrameLoader::addSameSiteInfoToRequestIfNeeded(resourceRequest, openerFrame.document());
        FrameLoadRequest frameLoadRequest { *activeWindow.document(), activeWindow.document()->securityOrigin(), resourceRequest, "_self"_s, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, activeDocument->shouldOpenExternalURLsPolicyToPropagate(), initiatedByMainFrame };
        newFrame->loader().changeLocation(WTFMove(frameLoadRequest));
    } else if (!urlString.isEmpty()) {
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->navigationScheduler().scheduleLocationChange(*activeDocument, activeDocument->securityOrigin(), completedURL, referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return RefPtr<Frame> { nullptr };

    return windowFeatures.noopener ? RefPtr<Frame> { nullptr } : newFrame;
}

ExceptionOr<RefPtr<WindowProxy>> DOMWindow::open(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& urlString, const AtomicString& frameName, const String& windowFeaturesString)
{
    if (!isCurrentlyDisplayedInFrame())
        return RefPtr<WindowProxy> { nullptr };

    auto* activeDocument = activeWindow.document();
    if (!activeDocument)
        return RefPtr<WindowProxy> { nullptr };

    auto* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return RefPtr<WindowProxy> { nullptr };

#if ENABLE(CONTENT_EXTENSIONS)
    if (firstFrame->document()
        && firstFrame->page()
        && firstFrame->mainFrame().document()
        && firstFrame->mainFrame().document()->loader()) {
        ResourceLoadInfo resourceLoadInfo { firstFrame->document()->completeURL(urlString), firstFrame->mainFrame().document()->url(), ResourceType::Popup };
        for (auto& action : firstFrame->page()->userContentProvider().actionsForResourceLoad(resourceLoadInfo, *firstFrame->mainFrame().document()->loader()).first) {
            if (action.type() == ContentExtensions::ActionType::BlockLoad)
                return RefPtr<WindowProxy> { nullptr };
        }
    }
#endif

    auto* frame = this->frame();
    if (!firstWindow.allowPopUp()) {
        // Because FrameTree::findFrameForNavigation() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !frame->loader().findFrameForNavigation(frameName, activeDocument))
            return RefPtr<WindowProxy> { nullptr };
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = nullptr;
    if (equalIgnoringASCIICase(frameName, "_top"))
        targetFrame = &frame->tree().top();
    else if (equalIgnoringASCIICase(frameName, "_parent")) {
        if (Frame* parent = frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame))
            return RefPtr<WindowProxy> { nullptr };

        URL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
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

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, const WTF::Function<void (DOMWindow&)>& prepareDialogFunction)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    if (!activeWindow.frame())
        return;
    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    auto* frame = this->frame();
    auto* page = frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.showModalDialog is not allowed while unloading a page.");
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

} // namespace WebCore
