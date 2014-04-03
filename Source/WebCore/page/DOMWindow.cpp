/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2013 Apple Inc. All rights reserved.
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
#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Crypto.h"
#include "DOMApplicationCache.h"
#include "DOMSelection.h"
#include "DOMSettableTokenList.h"
#include "DOMStringList.h"
#include "DOMTimer.h"
#include "DOMTokenList.h"
#include "DOMURL.h"
#include "DOMWindowCSS.h"
#include "DOMWindowExtension.h"
#include "DOMWindowNotifications.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventException.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "History.h"
#include "InspectorInstrumentation.h"
#include "JSMainThreadExecState.h"
#include "Location.h"
#include "MainFrame.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MessageEvent.h"
#include "Navigator.h"
#include "Page.h"
#include "PageConsole.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "Performance.h"
#include "PlatformScreen.h"
#include "RuntimeEnabledFeatures.h"
#include "ScheduledAction.h"
#include "Screen.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "Storage.h"
#include "StorageArea.h"
#include "StorageNamespace.h"
#include "StyleMedia.h"
#include "StyleResolver.h"
#include "SuddenTermination.h"
#include "URL.h"
#include "WebKitPoint.h"
#include "WindowFeatures.h"
#include "WindowFocusAllowedIndicator.h"
#include <algorithm>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

#if ENABLE(PROXIMITY_EVENTS)
#include "DeviceProximityController.h"
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "RequestAnimationFrameCallback.h"
#endif

#if PLATFORM(IOS)
#if ENABLE(GEOLOCATION)
#include "NavigatorGeolocation.h"
#endif
#include "WKContentObservation.h"
#endif

using namespace Inspector;

namespace WebCore {

class PostMessageTimer : public TimerBase {
public:
    PostMessageTimer(DOMWindow* window, PassRefPtr<SerializedScriptValue> message, const String& sourceOrigin, PassRefPtr<DOMWindow> source, std::unique_ptr<MessagePortChannelArray> channels, SecurityOrigin* targetOrigin, PassRefPtr<ScriptCallStack> stackTrace)
        : m_window(window)
        , m_message(message)
        , m_origin(sourceOrigin)
        , m_source(source)
        , m_channels(std::move(channels))
        , m_targetOrigin(targetOrigin)
        , m_stackTrace(stackTrace)
    {
    }

    PassRefPtr<MessageEvent> event(ScriptExecutionContext* context)
    {
        std::unique_ptr<MessagePortArray> messagePorts = MessagePort::entanglePorts(*context, std::move(m_channels));
        return MessageEvent::create(std::move(messagePorts), m_message, m_origin, String(), m_source);
    }
    SecurityOrigin* targetOrigin() const { return m_targetOrigin.get(); }
    ScriptCallStack* stackTrace() const { return m_stackTrace.get(); }

private:
    virtual void fired()
    {
        m_window->postMessageTimerFired(adoptPtr(this));
        // This object is deleted now.
    }

    RefPtr<DOMWindow> m_window;
    RefPtr<SerializedScriptValue> m_message;
    String m_origin;
    RefPtr<DOMWindow> m_source;
    std::unique_ptr<MessagePortChannelArray> m_channels;
    RefPtr<SecurityOrigin> m_targetOrigin;
    RefPtr<ScriptCallStack> m_stackTrace;
};

typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithUnloadEventListeners, ());
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(DOMWindowSet, windowsWithBeforeUnloadEventListeners, ());
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
    for (auto it = set.begin(), end = set.end(); it != end; ++it)
        windows.uncheckedAppend(*it->key);

    for (unsigned i = 0; i < windows.size(); ++i) {
        DOMWindow& window = windows[i].get();
        if (!set.contains(&window))
            continue;

        Frame* frame = window.frame();
        if (!frame)
            continue;

        if (!frame->loader().shouldClose())
            return false;

        window.enableSuddenTermination();
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

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto it = set.begin(), end = set.end(); it != end; ++it)
        windows.uncheckedAppend(*it->key);

    for (unsigned i = 0; i < windows.size(); ++i) {
        DOMWindow& window = windows[i].get();
        if (!set.contains(&window))
            continue;

        window.dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), window.document());
        window.dispatchEvent(Event::create(eventNames().unloadEvent, false, false), window.document());

        window.enableSuddenTermination();
    }

    alreadyDispatched = true;
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect DOMWindow::adjustWindowRect(Page* page, const FloatRect& pendingChanges)
{
    ASSERT(page);

    FloatRect screen = screenAvailableRect(page->mainFrame().view());
    FloatRect window = page->chrome().windowRect();

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

    FloatSize minimumSize = page->chrome().client().minimumWindowSize();
    window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool DOMWindow::allowPopUp(Frame* firstFrame)
{
    ASSERT(firstFrame);

    if (ScriptController::processingUserGesture())
        return true;

    return firstFrame->settings().javaScriptCanOpenWindowsAutomatically();
}

bool DOMWindow::allowPopUp()
{
    return m_frame && allowPopUp(m_frame);
}

bool DOMWindow::canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;
    Page* page = frame->page();
    if (!page)
        return false;
    return page->chrome().canRunModal();
}

bool DOMWindow::canShowModalDialogNow(const Frame* frame)
{
    if (!frame)
        return false;
    Page* page = frame->page();
    if (!page)
        return false;
    return page->chrome().canRunModalNow();
}

DOMWindow::DOMWindow(Document* document)
    : ContextDestructionObserver(document)
    , FrameDestructionObserver(document->frame())
    , m_shouldPrintWhenFinishedLoading(false)
    , m_suspendedForPageCache(false)
    , m_lastPageStatus(PageStatusNone)
#if PLATFORM(IOS)
    , m_scrollEventListenerCount(0)
#endif
#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    , m_touchEventListenerCount(0)
#endif
{
    ASSERT(frame());
    ASSERT(DOMWindow::document());
}

void DOMWindow::didSecureTransitionTo(Document* document)
{
    observeContext(document);
}

DOMWindow::~DOMWindow()
{
#ifndef NDEBUG
    if (!m_suspendedForPageCache) {
        ASSERT(!m_screen);
        ASSERT(!m_history);
        ASSERT(!m_crypto);
        ASSERT(!m_locationbar);
        ASSERT(!m_menubar);
        ASSERT(!m_personalbar);
        ASSERT(!m_scrollbars);
        ASSERT(!m_statusbar);
        ASSERT(!m_toolbar);
        ASSERT(!m_navigator);
#if ENABLE(WEB_TIMING)
        ASSERT(!m_performance);
#endif
        ASSERT(!m_location);
        ASSERT(!m_media);
        ASSERT(!m_sessionStorage);
        ASSERT(!m_localStorage);
        ASSERT(!m_applicationCache);
    }
#endif

    if (m_suspendedForPageCache)
        willDestroyCachedFrame();
    else
        willDestroyDocumentInFrame();

    // As the ASSERTs above indicate, this reset should only be necessary if this DOMWindow is suspended for the page cache.
    // But we don't want to risk any of these objects hanging around after we've been destroyed.
    resetDOMWindowProperties();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

DOMWindow* DOMWindow::toDOMWindow()
{
    return this;
}

PassRefPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : 0;
}

Page* DOMWindow::page()
{
    return frame() ? frame()->page() : 0;
}

void DOMWindow::frameDestroyed()
{
    willDestroyDocumentInFrame();
    FrameDestructionObserver::frameDestroyed();
    resetDOMWindowProperties();
}

void DOMWindow::willDetachPage()
{
    InspectorInstrumentation::frameWindowDiscarded(m_frame, this);
}

void DOMWindow::willDestroyCachedFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInCachedFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDestroyGlobalObjectInCachedFrame();
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDestroyGlobalObjectInFrame();
}

void DOMWindow::willDetachDocumentFromFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->willDetachGlobalObjectFromFrame();
}

void DOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void DOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void DOMWindow::resetUnlessSuspendedForPageCache()
{
    if (m_suspendedForPageCache)
        return;
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void DOMWindow::suspendForPageCache()
{
    disconnectDOMWindowProperties();
    m_suspendedForPageCache = true;
}

void DOMWindow::resumeFromPageCache()
{
    reconnectDOMWindowProperties();
    m_suspendedForPageCache = false;
}

void DOMWindow::disconnectDOMWindowProperties()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to disconnectFrameForPageCache.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->disconnectFrameForPageCache();
}

void DOMWindow::reconnectDOMWindowProperties()
{
    ASSERT(m_suspendedForPageCache);
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to reconnectFromPageCache.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (size_t i = 0; i < properties.size(); ++i)
        properties[i]->reconnectFrameFromPageCache(m_frame);
}

void DOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_screen = 0;
    m_history = 0;
    m_crypto = 0;
    m_locationbar = 0;
    m_menubar = 0;
    m_personalbar = 0;
    m_scrollbars = 0;
    m_statusbar = 0;
    m_toolbar = 0;
    m_navigator = 0;
#if ENABLE(WEB_TIMING)
    m_performance = 0;
#endif
    m_location = 0;
    m_media = 0;
    m_sessionStorage = 0;
    m_localStorage = 0;
    m_applicationCache = 0;
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    return m_frame && m_frame->document()->domWindow() == this;
}

#if ENABLE(ORIENTATION_EVENTS)
int DOMWindow::orientation() const
{
    if (!m_frame)
        return 0;

    return m_frame->orientation();
}
#endif

Screen* DOMWindow::screen() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_screen)
        m_screen = Screen::create(m_frame);
    return m_screen.get();
}

History* DOMWindow::history() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_history)
        m_history = History::create(m_frame);
    return m_history.get();
}

Crypto* DOMWindow::crypto() const
{
    // FIXME: Why is crypto not available when the window is not currently displayed in a frame?
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_crypto)
        m_crypto = Crypto::create(*document());
    return m_crypto.get();
}

BarProp* DOMWindow::locationbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_locationbar)
        m_locationbar = BarProp::create(m_frame, BarProp::Locationbar);
    return m_locationbar.get();
}

BarProp* DOMWindow::menubar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_menubar)
        m_menubar = BarProp::create(m_frame, BarProp::Menubar);
    return m_menubar.get();
}

BarProp* DOMWindow::personalbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_personalbar)
        m_personalbar = BarProp::create(m_frame, BarProp::Personalbar);
    return m_personalbar.get();
}

BarProp* DOMWindow::scrollbars() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_scrollbars)
        m_scrollbars = BarProp::create(m_frame, BarProp::Scrollbars);
    return m_scrollbars.get();
}

BarProp* DOMWindow::statusbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_statusbar)
        m_statusbar = BarProp::create(m_frame, BarProp::Statusbar);
    return m_statusbar.get();
}

BarProp* DOMWindow::toolbar() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_toolbar)
        m_toolbar = BarProp::create(m_frame, BarProp::Toolbar);
    return m_toolbar.get();
}

PageConsole* DOMWindow::pageConsole() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    return m_frame->page() ? &m_frame->page()->console() : 0;
}

DOMApplicationCache* DOMWindow::applicationCache() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_applicationCache)
        m_applicationCache = DOMApplicationCache::create(m_frame);
    return m_applicationCache.get();
}

Navigator* DOMWindow::navigator() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_navigator)
        m_navigator = Navigator::create(m_frame);
    return m_navigator.get();
}

#if ENABLE(WEB_TIMING)
Performance* DOMWindow::performance() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_performance)
        m_performance = Performance::create(m_frame);
    return m_performance.get();
}
#endif

Location* DOMWindow::location() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_location)
        m_location = Location::create(m_frame);
    return m_location.get();
}

Storage* DOMWindow::sessionStorage(ExceptionCode& ec) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    if (!document->securityOrigin()->canAccessSessionStorage(document->topOrigin())) {
        ec = SECURITY_ERR;
        return 0;
    }

    if (m_sessionStorage) {
        if (!m_sessionStorage->area().canAccessStorage(m_frame)) {
            ec = SECURITY_ERR;
            return 0;
        }
        return m_sessionStorage.get();
    }

    Page* page = document->page();
    if (!page)
        return 0;

    RefPtr<StorageArea> storageArea = page->sessionStorage()->storageArea(document->securityOrigin());
    if (!storageArea->canAccessStorage(m_frame)) {
        ec = SECURITY_ERR;
        return 0;
    }

    m_sessionStorage = Storage::create(m_frame, storageArea.release());
    return m_sessionStorage.get();
}

Storage* DOMWindow::localStorage(ExceptionCode& ec) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    Document* document = this->document();
    if (!document)
        return 0;

    if (!document->securityOrigin()->canAccessLocalStorage(0)) {
        ec = SECURITY_ERR;
        return 0;
    }

    if (m_localStorage) {
        if (!m_localStorage->area().canAccessStorage(m_frame)) {
            ec = SECURITY_ERR;
            return 0;
        }
        return m_localStorage.get();
    }

    Page* page = document->page();
    if (!page)
        return 0;

    if (!page->settings().localStorageEnabled())
        return 0;

    RefPtr<StorageArea> storageArea;
    if (!document->securityOrigin()->canAccessLocalStorage(document->topOrigin()))
        storageArea = page->group().transientLocalStorage(document->topOrigin())->storageArea(document->securityOrigin());
    else
        storageArea = page->group().localStorage()->storageArea(document->securityOrigin());

    if (!storageArea->canAccessStorage(m_frame)) {
        ec = SECURITY_ERR;
        return 0;
    }

    m_localStorage = Storage::create(m_frame, storageArea.release());
    return m_localStorage.get();
}

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, MessagePort* port, const String& targetOrigin, DOMWindow& source, ExceptionCode& ec)
{
    MessagePortArray ports;
    if (port)
        ports.append(port);
    postMessage(message, &ports, targetOrigin, source, ec);
}

void DOMWindow::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, const String& targetOrigin, DOMWindow& source, ExceptionCode& ec)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* sourceDocument = source.document();

    // Compute the target origin.  We need to do this synchronously in order
    // to generate the SYNTAX_ERR exception correctly.
    RefPtr<SecurityOrigin> target;
    if (targetOrigin == "/") {
        if (!sourceDocument)
            return;
        target = sourceDocument->securityOrigin();
    } else if (targetOrigin != "*") {
        target = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at a unique origin
        // because there's no way to represent a unique origin in a string.
        if (target->isUnique()) {
            ec = SYNTAX_ERR;
            return;
        }
    }

    std::unique_ptr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, ec);
    if (ec)
        return;

    // Capture the source of the message.  We need to do this synchronously
    // in order to capture the source of the message correctly.
    if (!sourceDocument)
        return;
    String sourceOrigin = sourceDocument->securityOrigin()->toString();

    // Capture stack trace only when inspector front-end is loaded as it may be time consuming.
    RefPtr<ScriptCallStack> stackTrace;
    if (InspectorInstrumentation::consoleAgentEnabled(sourceDocument))
        stackTrace = createScriptCallStack(JSMainThreadExecState::currentState(), ScriptCallStack::maxCallStackSizeToCapture);

    // Schedule the message.
    PostMessageTimer* timer = new PostMessageTimer(this, message, sourceOrigin, &source, std::move(channels), target.get(), stackTrace.release());
    timer->startOneShot(0);
}

void DOMWindow::postMessageTimerFired(PassOwnPtr<PostMessageTimer> t)
{
    OwnPtr<PostMessageTimer> timer(t);

    if (!document() || !isCurrentlyDisplayedInFrame())
        return;

    dispatchMessageEventWithOriginCheck(timer->targetOrigin(), timer->event(document()), timer->stackTrace());
}

void DOMWindow::dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, PassRefPtr<Event> event, PassRefPtr<ScriptCallStack> stackTrace)
{
    if (intendedTargetOrigin) {
        // Check target origin now since the target document may have changed since the timer was scheduled.
        if (!intendedTargetOrigin->isSameSchemeHostPort(document()->securityOrigin())) {
            String message = "Unable to post message to " + intendedTargetOrigin->toString() +
                             ". Recipient has origin " + document()->securityOrigin()->toString() + ".\n";
            pageConsole()->addMessage(MessageSource::Security, MessageLevel::Error, message, stackTrace);
            return;
        }
    }

    dispatchEvent(event);
}

DOMSelection* DOMWindow::getSelection()
{
    if (!isCurrentlyDisplayedInFrame() || !m_frame)
        return 0;

    return m_frame->document()->getSelection();
}

Element* DOMWindow::frameElement() const
{
    if (!m_frame)
        return 0;

    return m_frame->ownerElement();
}

void DOMWindow::focus(ScriptExecutionContext* context)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    bool allowFocus = WindowFocusAllowedIndicator::windowFocusAllowed() || !m_frame->settings().windowFocusRestricted();
    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (opener() && opener() != this && activeDocument->domWindow() == opener())
            allowFocus = true;
    }

    // If we're a top level window, bring the window to the front.
    if (m_frame->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!m_frame)
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    Frame* focusedFrame = page->focusController().focusedFrame();
    if (focusedFrame && focusedFrame != m_frame)
        focusedFrame->document()->setFocusedElement(0);

    m_frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame->settings().windowFocusRestricted())
        return;

    if (!m_frame->isMainFrame())
        return;

    page->chrome().unfocus();
}

void DOMWindow::close(ScriptExecutionContext* context)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (!m_frame->isMainFrame())
        return;

    if (context) {
        ASSERT(isMainThread());
        Document* activeDocument = toDocument(context);
        if (!activeDocument)
            return;

        if (!activeDocument->canNavigate(m_frame))
            return;
    }

    bool allowScriptsToCloseWindows = m_frame->settings().allowScriptsToCloseWindows();

    if (!(page->openedByDOM() || page->backForward().count() <= 1 || allowScriptsToCloseWindows)) {
        pageConsole()->addMessage(MessageSource::JS, MessageLevel::Warning, ASCIILiteral("Can't close the window since it was not opened by JavaScript"));
        return;
    }

    if (!m_frame->loader().shouldClose())
        return;

    page->chrome().closeWindowSoon();
}

void DOMWindow::print()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    // Pages are not allowed to bring up a modal print dialog during BeforeUnload dispatch.
    if (page->isAnyFrameHandlingBeforeUnloadEvent()) {
        printErrorMessage("Use of window.print is not allowed during beforeunload event dispatch.");
        return;
    }

    if (m_frame->loader().activeDocumentLoader()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(m_frame);
}

void DOMWindow::stop()
{
    if (!m_frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    m_frame->loader().stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    // Pages are not allowed to cause modal alerts during BeforeUnload dispatch.
    if (page() && page()->isAnyFrameHandlingBeforeUnloadEvent()) {
        printErrorMessage("Use of window.alert is not allowed during beforeunload event dispatch.");
        return;
    }

    m_frame->document()->updateStyleIfNeeded();

    Page* page = m_frame->page();
    if (!page)
        return;

    page->chrome().runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;
    
    // Pages are not allowed to cause modal alerts during BeforeUnload dispatch.
    if (page() && page()->isAnyFrameHandlingBeforeUnloadEvent()) {
        printErrorMessage("Use of window.confirm is not allowed during beforeunload event dispatch.");
        return false;
    }

    m_frame->document()->updateStyleIfNeeded();

    Page* page = m_frame->page();
    if (!page)
        return false;

    return page->chrome().runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    // Pages are not allowed to cause modal alerts during BeforeUnload dispatch.
    if (page() && page()->isAnyFrameHandlingBeforeUnloadEvent()) {
        printErrorMessage("Use of window.prompt is not allowed during beforeunload event dispatch.");
        return String();
    }

    m_frame->document()->updateStyleIfNeeded();

    Page* page = m_frame->page();
    if (!page)
        return String();

    String returnValue;
    if (page->chrome().runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

String DOMWindow::btoa(const String& stringToEncode, ExceptionCode& ec)
{
    if (stringToEncode.isNull())
        return String();

    if (!stringToEncode.containsOnlyLatin1()) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    return base64Encode(stringToEncode.latin1());
}

String DOMWindow::atob(const String& encodedString, ExceptionCode& ec)
{
    if (encodedString.isNull())
        return String();

    if (!encodedString.containsOnlyLatin1()) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    Vector<char> out;
    if (!base64Decode(encodedString, out, Base64FailOnInvalidCharacterOrExcessPadding)) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    return String(out.data(), out.size());
}

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog.    
    FindOptions options = (backwards ? Backwards : 0) | (caseSensitive ? 0 : CaseInsensitive) | (wrap ? WrapAround : 0);
    return m_frame->editor().findString(string, options);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().height());
}

int DOMWindow::outerWidth() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().width());
}

int DOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int DOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int DOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollX;
#if PLATFORM(IOS)
    scrollX = view->actualScrollX();
#else
    scrollX = view->scrollX();
#endif
    if (!scrollX)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

#if PLATFORM(IOS)
    return view->mapFromLayoutToCSSUnits(view->actualScrollX());
#else
    return view->mapFromLayoutToCSSUnits(view->scrollX());
#endif
}

int DOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollY;
#if PLATFORM(IOS)
    scrollY = view->actualScrollY();
#else
    scrollY = view->scrollY();
#endif
    if (!scrollY)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

#if PLATFORM(IOS)
    return view->mapFromLayoutToCSSUnits(view->actualScrollY());
#else
    return view->mapFromLayoutToCSSUnits(view->scrollY());
#endif
}

bool DOMWindow::closed() const
{
    return !m_frame;
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return m_frame->tree().scopedChildCount();
}

String DOMWindow::name() const
{
    if (!m_frame)
        return String();

    return m_frame->tree().name();
}

void DOMWindow::setName(const String& string)
{
    if (!m_frame)
        return;

    m_frame->tree().setName(string);
}

void DOMWindow::setStatus(const String& string) 
{
    m_status = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_status);
} 
    
void DOMWindow::setDefaultStatus(const String& string) 
{
    m_defaultStatus = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_defaultStatus);
}

DOMWindow* DOMWindow::self() const
{
    if (!m_frame)
        return 0;

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    if (!m_frame)
        return 0;

    Frame* opener = m_frame->loader().opener();
    if (!opener)
        return 0;

    return opener->document()->domWindow();
}

DOMWindow* DOMWindow::parent() const
{
    if (!m_frame)
        return 0;

    Frame* parent = m_frame->tree().parent();
    if (parent)
        return parent->document()->domWindow();

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return m_frame->tree().top().document()->domWindow();
}

Document* DOMWindow::document() const
{
    ScriptExecutionContext* context = ContextDestructionObserver::scriptExecutionContext();
    return toDocument(context);
}

PassRefPtr<StyleMedia> DOMWindow::styleMedia() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return m_media.get();
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* elt, const String& pseudoElt) const
{
    if (!elt)
        return 0;

    return CSSComputedStyleDeclaration::create(elt, false, pseudoElt);
}

PassRefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement, bool authorOnly) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoType pseudoType = CSSSelector::parsePseudoElementType(pseudoElement.substringSharingImpl(colonStart));
    if (pseudoType == CSSSelector::PseudoUnknown && !pseudoElement.isEmpty())
        return 0;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= StyleResolver::UAAndUserCSSRules;
    if (m_frame->settings().crossOriginCheckInGetMatchedCSSRulesDisabled())
        rulesToInclude |= StyleResolver::CrossOriginCSSRules;

    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);

    auto matchedRules = m_frame->document()->ensureStyleResolver().pseudoStyleRulesForElement(element, pseudoId, rulesToInclude);
    if (matchedRules.isEmpty())
        return 0;

    RefPtr<StaticCSSRuleList> ruleList = StaticCSSRuleList::create();
    for (unsigned i = 0; i < matchedRules.size(); ++i)
        ruleList->rules().append(matchedRules[i]->createCSSOMWrapper());

    return ruleList.release();
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;

    if (!document())
        return 0;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint pagePoint(p->x(), p->y());
    pagePoint = node->convertToPage(pagePoint);
    return WebKitPoint::create(pagePoint.x(), pagePoint.y());
}

PassRefPtr<WebKitPoint> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const
{
    if (!node || !p)
        return 0;

    if (!document())
        return 0;

    document()->updateLayoutIgnorePendingStylesheets();

    FloatPoint nodePoint(p->x(), p->y());
    nodePoint = node->convertFromPage(nodePoint);
    return WebKitPoint::create(nodePoint.x(), nodePoint.y());
}

double DOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    Page* page = m_frame->page();
    if (!page)
        return 0.0;

    return page->deviceScaleFactor();
}

void DOMWindow::scrollBy(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    IntSize scaledOffset(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
#if PLATFORM(IOS)
    view->setActualScrollPosition(view->actualScrollPosition() + scaledOffset);
#else
    view->scrollBy(scaledOffset);
#endif
}

void DOMWindow::scrollTo(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;


#if PLATFORM(IOS)
    int zoomedX = static_cast<int>(x * m_frame->pageZoomFactor() * m_frame->frameScaleFactor());
    int zoomedY = static_cast<int>(y * m_frame->pageZoomFactor() * m_frame->frameScaleFactor());
    view->setActualScrollPosition(IntPoint(zoomedX, zoomedY));
#else
    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
    view->setScrollPosition(layoutPos);
#endif
}

bool DOMWindow::allowedToChangeWindowGeometry() const
{
    if (!m_frame)
        return false;
    if (!m_frame->page())
        return false;
    if (!m_frame->isMainFrame())
        return false;
    // Prevent web content from tricking the user into initiating a drag.
    if (m_frame->eventHandler().mousePressed())
        return false;
    return true;
}

void DOMWindow::moveBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect sr = screenAvailableRect(page->mainFrame().view());
    fr.setLocation(sr.location());
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

int DOMWindow::setTimeout(PassOwnPtr<ScheduledAction> action, int timeout, ExceptionCode& ec)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }
    return DOMTimer::install(context, action, timeout, true);
}

void DOMWindow::clearTimeout(int timeoutId)
{
#if PLATFORM(IOS)
    if (m_frame) {
        Document* document = m_frame->document();
        if (timeoutId > 0 && document) {
            DOMTimer* timer = document->findTimeout(timeoutId);
            if (timer && WebThreadContainsObservedContentModifier(timer)) {
                WebThreadRemoveObservedContentModifier(timer);

                if (!WebThreadCountOfObservedContentModifiers()) {
                    if (Page* page = m_frame->page())
                        page->chrome().client().observedContentChange(m_frame);
                }
            }
        }
    }
#endif
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(context, timeoutId);
}

int DOMWindow::setInterval(PassOwnPtr<ScheduledAction> action, int timeout, ExceptionCode& ec)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context) {
        ec = INVALID_ACCESS_ERR;
        return -1;
    }
    return DOMTimer::install(context, action, timeout, false);
}

void DOMWindow::clearInterval(int timeoutId)
{
    ScriptExecutionContext* context = scriptExecutionContext();
    if (!context)
        return;
    DOMTimer::removeById(context, timeoutId);
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
int DOMWindow::requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int DOMWindow::webkitRequestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void DOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}
#endif

#if ENABLE(CSS3_CONDITIONAL_RULES)
DOMWindowCSS* DOMWindow::css()
{
    if (!m_css)
        m_css = DOMWindowCSS::create();
    return m_css.get();
}
#endif

static void didAddStorageEventListener(DOMWindow* window)
{
    // Creating these WebCore::Storage objects informs the system that we'd like to receive
    // notifications about storage events that might be triggered in other processes. Rather
    // than subscribe to these notifications explicitly, we subscribe to them implicitly to
    // simplify the work done by the system. 
    window->localStorage(IGNORE_EXCEPTION);
    window->sessionStorage(IGNORE_EXCEPTION);
}

bool DOMWindow::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    if (!EventTarget::addEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
            document->didAddWheelEventHandler();
        else if (eventNames().isTouchEventType(eventType))
            document->didAddTouchEventHandler(document);
        else if (eventType == eventNames().storageEvent)
            didAddStorageEventListener(this);
    }

    if (eventType == eventNames().unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().devicemotionEvent && document())
        document()->deviceMotionController()->addDeviceEventListener(this);
    else if (eventType == eventNames().deviceorientationEvent && document())
        document()->deviceOrientationController()->addDeviceEventListener(this);
#else
    else if (eventType == eventNames().devicemotionEvent && RuntimeEnabledFeatures::sharedFeatures().deviceMotionEnabled()) {
        if (DeviceMotionController* controller = DeviceMotionController::from(page()))
            controller->addDeviceEventListener(this);
    } else if (eventType == eventNames().deviceorientationEvent && RuntimeEnabledFeatures::sharedFeatures().deviceOrientationEnabled()) {
        if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
            controller->addDeviceEventListener(this);
    }
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().scrollEvent)
        incrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType))
        ++m_touchEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType))
        ++m_touchEventListenerCount;
#endif

#if ENABLE(PROXIMITY_EVENTS)
    else if (eventType == eventNames().webkitdeviceproximityEvent) {
        if (DeviceProximityController* controller = DeviceProximityController::from(page()))
            controller->addDeviceEventListener(this);
    }
#endif

    return true;
}

#if PLATFORM(IOS)
void DOMWindow::incrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (++m_scrollEventListenerCount == 1 && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page())
            frame->page()->chrome().client().setNeedsScrollNotifications(frame, true);
    }
}

void DOMWindow::decrementScrollEventListenersCount()
{
    Document* document = this->document();
    if (!--m_scrollEventListenerCount && document == &document->topDocument()) {
        Frame* frame = this->frame();
        if (frame && frame->page() && !document->inPageCache())
            frame->page()->chrome().client().setNeedsScrollNotifications(frame, false);
    }
}
#endif

void DOMWindow::resetAllGeolocationPermission()
{
    // FIXME: Remove PLATFORM(IOS)-guard once we upstream the iOS changes to Geolocation.cpp.
#if ENABLE(GEOLOCATION) && PLATFORM(IOS)
    if (m_navigator)
        NavigatorGeolocation::from(m_navigator.get())->resetAllGeolocationPermission();
#endif
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document()) {
        if (eventType == eventNames().wheelEvent || eventType == eventNames().mousewheelEvent)
            document->didRemoveWheelEventHandler();
        else if (eventNames().isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(document);
    }

    if (eventType == eventNames().unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);
#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
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
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    else if (eventType == eventNames().scrollEvent)
        decrementScrollEventListenersCount();
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif

#if ENABLE(PROXIMITY_EVENTS)
    else if (eventType == eventNames().webkitdeviceproximityEvent) {
        if (DeviceProximityController* controller = DeviceProximityController::from(page()))
            controller->removeDeviceEventListener(this);
    }
#endif

    return true;
}

void DOMWindow::dispatchLoadEvent()
{
    RefPtr<Event> loadEvent(Event::create(eventNames().loadEvent, false, false));
    if (m_frame && m_frame->loader().documentLoader() && !m_frame->loader().documentLoader()->timing()->loadEventStart()) {
        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = m_frame->loader().documentLoader();
        DocumentLoadTiming* timing = documentLoader->timing();
        timing->markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing->markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = m_frame ? m_frame->ownerElement() : 0;
    if (ownerElement)
        ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, false, false));

    InspectorInstrumentation::loadEventFired(frame());
}

bool DOMWindow::dispatchEvent(PassRefPtr<Event> prpEvent, PassRefPtr<EventTarget> prpTarget)
{
    Ref<EventTarget> protect(*this);
    RefPtr<Event> event = prpEvent;

    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // <http://www.whatwg.org/specs/web-apps/current-work/multipage/history.html#event-pageshow>.
    if (event->eventInterface() == PageTransitionEventInterfaceType) {
        if (event->type() == eventNames().pageshowEvent) {
            if (m_lastPageStatus == PageStatusShown)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatusShown;
        } else if (event->type() == eventNames().pagehideEvent) {
            if (m_lastPageStatus == PageStatusHidden)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatusHidden;
        }
    }

    event->setTarget(prpTarget ? prpTarget : this);
    event->setCurrentTarget(this);
    event->setEventPhase(Event::AT_TARGET);

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEventOnWindow(frame(), *event, this);

    bool result = fireEventListeners(event.get());

    InspectorInstrumentation::didDispatchEventOnWindow(cookie);

    return result;
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(DEVICE_ORIENTATION)
#if PLATFORM(IOS)
    if (Document* document = this->document()) {
        document->deviceMotionController()->removeAllDeviceEventListeners(this);
        document->deviceOrientationController()->removeAllDeviceEventListeners(this);
    }
#else
    if (DeviceMotionController* controller = DeviceMotionController::from(page()))
        controller->removeAllDeviceEventListeners(this);
    if (DeviceOrientationController* controller = DeviceOrientationController::from(page()))
        controller->removeAllDeviceEventListeners(this);
#endif // PLATFORM(IOS)
#endif // ENABLE(DEVICE_ORIENTATION)

#if PLATFORM(IOS)
    if (m_scrollEventListenerCount) {
        m_scrollEventListenerCount = 1;
        decrementScrollEventListenersCount();
    }
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    m_touchEventListenerCount = 0;
#endif

#if ENABLE(TOUCH_EVENTS)
    if (Document* document = this->document())
        document->didRemoveEventTargetNode(document);
#endif

#if ENABLE(PROXIMITY_EVENTS)
    if (DeviceProximityController* controller = DeviceProximityController::from(page()))
        controller->removeAllDeviceEventListeners(this);
#endif

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
        print();
    }
}

void DOMWindow::setLocation(const String& urlString, DOMWindow& activeWindow, DOMWindow& firstWindow, SetLocationLocking locking)
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return;

    if (!activeDocument->canNavigate(m_frame))
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
    LockHistory lockHistory = (locking != LockHistoryBasedOnGestureState || !ScriptController::processingUserGesture()) ? LockHistory::Yes : LockHistory::No;
    LockBackForwardList lockBackForwardList = (locking != LockHistoryBasedOnGestureState) ? LockBackForwardList::Yes : LockBackForwardList::No;
    m_frame->navigationScheduler().scheduleLocationChange(activeDocument->securityOrigin(),
        // FIXME: What if activeDocument()->frame() is 0?
        completedURL, activeDocument->frame()->loader().outgoingReferrer(),
        lockHistory, lockBackForwardList);
}

void DOMWindow::printErrorMessage(const String& message)
{
    if (message.isEmpty())
        return;

    if (PageConsole* console = pageConsole())
        console->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String DOMWindow::crossDomainAccessErrorMessage(const DOMWindow& activeWindow)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    ASSERT(!activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin* activeOrigin = activeWindow.document()->securityOrigin();
    SecurityOrigin* targetOrigin = document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a frame with origin \"" + targetOrigin->toString() + "\". ";

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    URL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || activeWindow.document()->isSandboxed(SandboxOrigin)) {
        message = "Blocked a frame at \"" + SecurityOrigin::create(activeURL)->toString() + "\" from accessing a frame at \"" + SecurityOrigin::create(targetURL)->toString() + "\". ";
        if (document()->isSandboxed(SandboxOrigin) && activeWindow.document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " Both frames are sandboxed and lack the \"allow-same-origin\" flag.";
        if (document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.";
        return "Sandbox access violation: " + message + " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.";
    }

    // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
    if (targetOrigin->protocol() != activeOrigin->protocol())
        return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

    // 'document.domain' errors.
    if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", the frame being accessed set it to \"" + targetOrigin->domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
    if (activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
    if (targetOrigin->domainWasSetInDOM())
        return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin->domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";

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
        if (activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow));
    return true;
}

PassRefPtr<Frame> DOMWindow::createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures, DOMWindow& activeWindow, Frame* firstFrame, Frame* openerFrame, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    Frame* activeFrame = activeWindow.frame();

    URL completedURL = urlString.isEmpty() ? URL(ParsedURLString, emptyString()) : firstFrame->document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        activeWindow.printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return 0;
    }

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = SecurityPolicy::generateReferrerHeader(firstFrame->document()->referrerPolicy(), completedURL, firstFrame->loader().outgoingReferrer());

    ResourceRequest request(completedURL, referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, firstFrame->loader().outgoingOrigin());
    FrameLoadRequest frameRequest(activeWindow.document()->securityOrigin(), request, frameName);

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    RefPtr<Frame> newFrame = WebCore::createWindow(activeFrame, openerFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader().setOpener(openerFrame);
    newFrame->page()->setOpenedByDOM();

    if (newFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
        return newFrame.release();

    if (prepareDialogFunction)
        prepareDialogFunction(*newFrame->document()->domWindow());

    if (created)
        newFrame->loader().changeLocation(activeWindow.document()->securityOrigin(), completedURL, referrer, LockHistory::No, LockBackForwardList::No);
    else if (!urlString.isEmpty()) {
        LockHistory lockHistory = ScriptController::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->navigationScheduler().scheduleLocationChange(activeWindow.document()->securityOrigin(), completedURL.string(), referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return 0;

    return newFrame.release();
}

PassRefPtr<DOMWindow> DOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    DOMWindow& activeWindow, DOMWindow& firstWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return 0;
    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return 0;

    if (!firstWindow.allowPopUp()) {
        // Because FrameTree::find() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !m_frame->tree().find(frameName))
            return 0;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = 0;
    if (frameName == "_top")
        targetFrame = &m_frame->tree().top();
    else if (frameName == "_parent") {
        if (Frame* parent = m_frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = m_frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame))
            return 0;

        URL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
            return targetFrame->document()->domWindow();

        if (urlString.isEmpty())
            return targetFrame->document()->domWindow();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        LockHistory lockHistory = ScriptController::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        targetFrame->navigationScheduler().scheduleLocationChange(activeDocument->securityOrigin(), completedURL, firstFrame->loader().outgoingReferrer(),
            lockHistory, LockBackForwardList::No);
        return targetFrame->document()->domWindow();
    }

    WindowFeatures windowFeatures(windowFeaturesString);
    RefPtr<Frame> result = createWindow(urlString, frameName, windowFeatures, activeWindow, firstFrame, m_frame);
    return result ? result->document()->domWindow() : 0;
}

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return;
    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    // Pages are not allowed to cause modal alerts during BeforeUnload dispatch.
    if (page() && page()->isAnyFrameHandlingBeforeUnloadEvent()) {
        printErrorMessage("Use of window.showModalDialog is not allowed during beforeunload event dispatch.");
        return;
    }

    if (!canShowModalDialogNow(m_frame) || !firstWindow.allowPopUp())
        return;

    WindowFeatures windowFeatures(dialogFeaturesString, screenAvailableRect(m_frame->view()));
    RefPtr<Frame> dialogFrame = createWindow(urlString, emptyAtom, windowFeatures, activeWindow, firstFrame, m_frame, std::move(prepareDialogFunction));
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

} // namespace WebCore
