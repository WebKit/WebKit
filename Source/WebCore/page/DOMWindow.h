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

#pragma once

#include "AbstractDOMWindow.h"
#include "Base64Utilities.h"
#include "ContextDestructionObserver.h"
#include "ExceptionOr.h"
#include "Frame.h"
#include "FrameDestructionObserver.h"
#include "ImageBitmap.h"
#include "ScrollToOptions.h"
#include "ScrollTypes.h"
#include "Supplementable.h"
#include <JavaScriptCore/HandleTypes.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class ExecState;
class JSObject;
class JSValue;
template<typename> class Strong;
}

namespace WebCore {

class BarProp;
class CSSRuleList;
class CSSStyleDeclaration;
class Crypto;
class CustomElementRegistry;
class DOMApplicationCache;
class DOMSelection;
class DOMWindowProperty;
class DOMWrapperWorld;
class Document;
class Element;
class EventListener;
class FloatRect;
class History;
class Location;
class MediaQueryList;
class Navigator;
class Node;
class NodeList;
class Page;
class PageConsoleClient;
class Performance;
class PostMessageTimer;
class RequestAnimationFrameCallback;
class ScheduledAction;
class Screen;
class Storage;
class StyleMedia;
class VisualViewport;
class WebKitNamespace;
class WebKitPoint;

#if ENABLE(DEVICE_ORIENTATION)
class DeviceMotionController;
class DeviceOrientationController;
#endif

struct ImageBitmapOptions;
struct WindowFeatures;

enum SetLocationLocking { LockHistoryBasedOnGestureState, LockHistoryAndBackForwardList };
enum class IncludeTargetOrigin { No, Yes };

// FIXME: Rename DOMWindow to LocalWindow and AbstractDOMWindow to DOMWindow.
class DOMWindow final
    : public AbstractDOMWindow
    , public CanMakeWeakPtr<DOMWindow>
    , public ContextDestructionObserver
    , public Base64Utilities
    , public Supplementable<DOMWindow> {
public:
    static Ref<DOMWindow> create(Document& document) { return adoptRef(*new DOMWindow(document)); }
    WEBCORE_EXPORT virtual ~DOMWindow();

    // In some rare cases, we'll reuse a DOMWindow for a new Document. For example,
    // when a script calls window.open("..."), the browser gives JavaScript a window
    // synchronously but kicks off the load in the window asynchronously. Web sites
    // expect that modifications that they make to the window object synchronously
    // won't be blown away when the network load commits. To make that happen, we
    // "securely transition" the existing DOMWindow to the Document that results from
    // the network load. See also SecurityContext::isSecureTransitionTo.
    void didSecureTransitionTo(Document&);

    class Observer {
    public:
        virtual ~Observer() { }

        virtual void suspendForPageCache() { }
        virtual void resumeFromPageCache() { }
        virtual void willDestroyGlobalObjectInCachedFrame() { }
        virtual void willDestroyGlobalObjectInFrame() { }
        virtual void willDetachGlobalObjectFromFrame() { }
    };

    void registerObserver(Observer&);
    void unregisterObserver(Observer&);

    void resetUnlessSuspendedForDocumentSuspension();
    void suspendForPageCache();
    void resumeFromPageCache();

    WEBCORE_EXPORT Frame* frame() const final;

    RefPtr<MediaQueryList> matchMedia(const String&);

    WEBCORE_EXPORT unsigned pendingUnloadEventListeners() const;

    WEBCORE_EXPORT static bool dispatchAllPendingBeforeUnloadEvents();
    WEBCORE_EXPORT static void dispatchAllPendingUnloadEvents();

    static FloatRect adjustWindowRect(Page&, const FloatRect& pendingChanges);

    bool allowPopUp(); // Call on first window, not target window.
    static bool allowPopUp(Frame& firstFrame);
    static bool canShowModalDialog(const Frame&);
    WEBCORE_EXPORT void setCanShowModalDialogOverride(bool);

    Screen& screen();
    History& history();
    Crypto& crypto() const;
    BarProp& locationbar();
    BarProp& menubar();
    BarProp& personalbar();
    BarProp& scrollbars();
    BarProp& statusbar();
    BarProp& toolbar();
    Navigator& navigator();
    Navigator* optionalNavigator() const { return m_navigator.get(); }
    Navigator& clientInformation() { return navigator(); }

    Location& location();
    void setLocation(DOMWindow& activeWindow, const URL& completedURL, SetLocationLocking = LockHistoryBasedOnGestureState);

    DOMSelection* getSelection();

    Element* frameElement() const;

    WEBCORE_EXPORT void focus(bool allowFocus = false);
    void focus(DOMWindow& incumbentWindow);
    void blur();
    WEBCORE_EXPORT void close();
    void close(Document&);
    void print();
    void stop();

    WEBCORE_EXPORT ExceptionOr<RefPtr<WindowProxy>> open(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& urlString, const AtomicString& frameName, const String& windowFeaturesString);

    void showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, const WTF::Function<void(DOMWindow&)>& prepareDialogFunction);

    void alert(const String& message = emptyString());
    bool confirm(const String& message);
    String prompt(const String& message, const String& defaultValue);

    bool find(const String&, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const;

    bool offscreenBuffering() const;

    int outerHeight() const;
    int outerWidth() const;
    int innerHeight() const;
    int innerWidth() const;
    int screenX() const;
    int screenY() const;
    int screenLeft() const { return screenX(); }
    int screenTop() const { return screenY(); }
    int scrollX() const;
    int scrollY() const;

    bool closed() const;

    unsigned length() const;

    String name() const;
    void setName(const String&);

    String status() const;
    void setStatus(const String&);
    String defaultStatus() const;
    void setDefaultStatus(const String&);

    WindowProxy* self() const;

    WindowProxy* opener() const;
    void disownOpener();
    WindowProxy* parent() const;
    WindowProxy* top() const;

    String origin() const;

    // DOM Level 2 AbstractView Interface

    WEBCORE_EXPORT Document* document() const;

    // CSSOM View Module

    StyleMedia& styleMedia();

    // DOM Level 2 Style Interface

    WEBCORE_EXPORT Ref<CSSStyleDeclaration> getComputedStyle(Element&, const String& pseudoElt) const;

    // WebKit extensions

    WEBCORE_EXPORT RefPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt, bool authorOnly = true) const;
    double devicePixelRatio() const;

    RefPtr<WebKitPoint> webkitConvertPointFromPageToNode(Node*, const WebKitPoint*) const;
    RefPtr<WebKitPoint> webkitConvertPointFromNodeToPage(Node*, const WebKitPoint*) const;

    PageConsoleClient* console() const;

    void printErrorMessage(const String&);

    String crossDomainAccessErrorMessage(const DOMWindow& activeWindow, IncludeTargetOrigin);

    ExceptionOr<void> postMessage(JSC::ExecState&, DOMWindow& incumbentWindow, JSC::JSValue message, const String& targetOrigin, Vector<JSC::Strong<JSC::JSObject>>&&);
    void postMessageTimerFired(PostMessageTimer&);

    void languagesChanged();

    void scrollBy(const ScrollToOptions&) const;
    void scrollBy(double x, double y) const;
    void scrollTo(const ScrollToOptions&, ScrollClamping = ScrollClamping::Clamped) const;
    void scrollTo(double x, double y, ScrollClamping = ScrollClamping::Clamped) const;

    void moveBy(float x, float y) const;
    void moveTo(float x, float y) const;

    void resizeBy(float x, float y) const;
    void resizeTo(float width, float height) const;

    VisualViewport& visualViewport();

    // Timers
    ExceptionOr<int> setTimeout(JSC::ExecState&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearTimeout(int timeoutId);
    ExceptionOr<int> setInterval(JSC::ExecState&, std::unique_ptr<ScheduledAction>, int timeout, Vector<JSC::Strong<JSC::Unknown>>&& arguments);
    void clearInterval(int timeoutId);

    int requestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    int webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&&);
    void cancelAnimationFrame(int id);

    // ImageBitmap
    void createImageBitmap(ImageBitmap::Source&&, ImageBitmapOptions&&, ImageBitmap::Promise&&);
    void createImageBitmap(ImageBitmap::Source&&, int sx, int sy, int sw, int sh, ImageBitmapOptions&&, ImageBitmap::Promise&&);

    // Secure Contexts
    bool isSecureContext() const;

    // Events
    // EventTarget API
    bool addEventListener(const AtomicString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) final;
    bool removeEventListener(const AtomicString& eventType, EventListener&, const ListenerOptions&) final;
    void removeAllEventListeners() final;

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&, EventTarget*);

    void dispatchLoadEvent();

    void captureEvents();
    void releaseEvents();

    void finishedLoading();

    // HTML 5 key/value storage
    ExceptionOr<Storage*> sessionStorage();
    ExceptionOr<Storage*> localStorage();
    Storage* optionalSessionStorage() const { return m_sessionStorage.get(); }
    Storage* optionalLocalStorage() const { return m_localStorage.get(); }

    DOMApplicationCache& applicationCache();
    DOMApplicationCache* optionalApplicationCache() const { return m_applicationCache.get(); }

    CustomElementRegistry* customElementRegistry() { return m_customElementRegistry.get(); }
    CustomElementRegistry& ensureCustomElementRegistry();

    ExceptionOr<Ref<NodeList>> collectMatchingElementsInFlatTree(Node&, const String& selectors);
    ExceptionOr<RefPtr<Element>> matchingElementInFlatTree(Node&, const String& selectors);

#if ENABLE(ORIENTATION_EVENTS)
    // This is the interface orientation in degrees. Some examples are:
    //  0 is straight up; -90 is when the device is rotated 90 clockwise;
    //  90 is when rotated counter clockwise.
    int orientation() const;
#endif

    Performance& performance() const;
    WEBCORE_EXPORT double nowTimestamp() const;

#if PLATFORM(IOS_FAMILY)
    void incrementScrollEventListenersCount();
    void decrementScrollEventListenersCount();
    unsigned scrollEventListenerCount() const { return m_scrollEventListenerCount; }
#endif

#if ENABLE(DEVICE_ORIENTATION)
    void startListeningForDeviceOrientationIfNecessary();
    void stopListeningForDeviceOrientationIfNecessary();
    void startListeningForDeviceMotionIfNecessary();
    void stopListeningForDeviceMotionIfNecessary();

    bool isAllowedToUseDeviceMotionOrientation(String& message) const;
    bool isAllowedToAddDeviceMotionOrientationListener(String& message) const;

    DeviceOrientationController* deviceOrientationController() const;
    DeviceMotionController* deviceMotionController() const;
#endif

    void resetAllGeolocationPermission();

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    bool hasTouchOrGestureEventListeners() const { return m_touchAndGestureEventListenerCount > 0; }
#endif

#if ENABLE(USER_MESSAGE_HANDLERS)
    bool shouldHaveWebKitNamespaceForWorld(DOMWrapperWorld&);
    WebKitNamespace* webkitNamespace();
#endif

    // FIXME: When this DOMWindow is no longer the active DOMWindow (i.e.,
    // when its document is no longer the document that is displayed in its
    // frame), we would like to zero out m_frame to avoid being confused
    // by the document that is currently active in m_frame.
    bool isCurrentlyDisplayedInFrame() const;

    void willDetachDocumentFromFrame();
    void willDestroyCachedFrame();

    void enableSuddenTermination();
    void disableSuddenTermination();

    void willDestroyDocumentInFrame();
    void frameDestroyed();

private:
    explicit DOMWindow(Document&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    bool isLocalDOMWindow() const final { return true; }
    bool isRemoteDOMWindow() const final { return false; }

    Page* page();
    bool allowedToChangeWindowGeometry() const;

    static ExceptionOr<RefPtr<Frame>> createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures&, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, const WTF::Function<void(DOMWindow&)>& prepareDialogFunction = nullptr);
    bool isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString);

#if ENABLE(DEVICE_ORIENTATION)
    void failedToRegisterDeviceMotionEventListener();
#endif

    bool isSameSecurityOriginAsMainFrame() const;

#if ENABLE(GAMEPAD)
    void incrementGamepadEventListenerCount();
    void decrementGamepadEventListenerCount();
#endif

    bool m_shouldPrintWhenFinishedLoading { false };
    bool m_suspendedForDocumentSuspension { false };
    Optional<bool> m_canShowModalDialogOverride;

    HashSet<Observer*> m_observers;

    mutable RefPtr<Crypto> m_crypto;
    mutable RefPtr<History> m_history;
    mutable RefPtr<BarProp> m_locationbar;
    mutable RefPtr<StyleMedia> m_media;
    mutable RefPtr<BarProp> m_menubar;
    mutable RefPtr<Navigator> m_navigator;
    mutable RefPtr<BarProp> m_personalbar;
    mutable RefPtr<Screen> m_screen;
    mutable RefPtr<BarProp> m_scrollbars;
    mutable RefPtr<DOMSelection> m_selection;
    mutable RefPtr<BarProp> m_statusbar;
    mutable RefPtr<BarProp> m_toolbar;
    mutable RefPtr<Location> m_location;
    mutable RefPtr<VisualViewport> m_visualViewport;

    String m_status;
    String m_defaultStatus;

    enum class PageStatus { None, Shown, Hidden };
    PageStatus m_lastPageStatus { PageStatus::None };

#if PLATFORM(IOS_FAMILY)
    unsigned m_scrollEventListenerCount { 0 };
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    unsigned m_touchAndGestureEventListenerCount { 0 };
#endif

#if ENABLE(GAMEPAD)
    unsigned m_gamepadEventListenerCount { 0 };
#endif

    mutable RefPtr<Storage> m_sessionStorage;
    mutable RefPtr<Storage> m_localStorage;
    mutable RefPtr<DOMApplicationCache> m_applicationCache;

    RefPtr<CustomElementRegistry> m_customElementRegistry;

    mutable RefPtr<Performance> m_performance;

#if ENABLE(USER_MESSAGE_HANDLERS)
    mutable RefPtr<WebKitNamespace> m_webkitNamespace;
#endif
};

inline String DOMWindow::status() const
{
    return m_status;
}

inline String DOMWindow::defaultStatus() const
{
    return m_defaultStatus;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DOMWindow)
    static bool isType(const WebCore::AbstractDOMWindow& window) { return window.isLocalDOMWindow(); }
    static bool isType(const WebCore::EventTarget& target) { return target.eventTargetInterface() == WebCore::DOMWindowEventTargetInterfaceType; }
SPECIALIZE_TYPE_TRAITS_END()
