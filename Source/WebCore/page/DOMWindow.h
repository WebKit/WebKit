/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef DOMWindow_h
#define DOMWindow_h

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "FrameDestructionObserver.h"
#include "URL.h"
#include "Supplementable.h"
#include <functional>
#include <memory>
#include <wtf/Optional.h>
#include <wtf/WeakPtr.h>

namespace Inspector {
class ScriptCallStack;
}

namespace WebCore {

    class BarProp;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class Crypto;
    class DOMApplicationCache;
    class DOMSelection;
    class DOMURL;
    class DOMWindowCSS;
    class DOMWindowProperty;
    class DOMWrapperWorld;
    class Database;
    class DatabaseCallback;
    class Document;
    class Element;
    class EventListener;
    class FloatRect;
    class Frame;
    class History;
    class IDBFactory;
    class Location;
    class MediaQueryList;
    class MessageEvent;
    class Navigator;
    class Node;
    class Page;
    class PageConsoleClient;
    class Performance;
    class PostMessageTimer;
    class ScheduledAction;
    class Screen;
    class SecurityOrigin;
    class SerializedScriptValue;
    class Storage;
    class StyleMedia;
    class WebKitNamespace;
    class WebKitPoint;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    class RequestAnimationFrameCallback;
#endif

    struct WindowFeatures;

    typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;

    typedef int ExceptionCode;

    enum SetLocationLocking { LockHistoryBasedOnGestureState, LockHistoryAndBackForwardList };

    // FIXME: DOMWindow shouldn't subclass FrameDestructionObserver and instead should get to Frame via its Document.
    class DOMWindow final
        : public RefCounted<DOMWindow>
        , public EventTargetWithInlineData
        , public ContextDestructionObserver
        , public FrameDestructionObserver
        , public Supplementable<DOMWindow> {
    public:
        static Ref<DOMWindow> create(Document* document) { return adoptRef(*new DOMWindow(document)); }
        WEBCORE_EXPORT virtual ~DOMWindow();

        // In some rare cases, we'll re-used a DOMWindow for a new Document. For example,
        // when a script calls window.open("..."), the browser gives JavaScript a window
        // synchronously but kicks off the load in the window asynchronously. Web sites
        // expect that modifications that they make to the window object synchronously
        // won't be blown away when the network load commits. To make that happen, we
        // "securely transition" the existing DOMWindow to the Document that results from
        // the network load. See also SecurityContext::isSecureTransitionTo.
        void didSecureTransitionTo(Document*);

        virtual EventTargetInterface eventTargetInterface() const override { return DOMWindowEventTargetInterfaceType; }
        virtual ScriptExecutionContext* scriptExecutionContext() const override { return ContextDestructionObserver::scriptExecutionContext(); }

        virtual DOMWindow* toDOMWindow() override;

        void registerProperty(DOMWindowProperty*);
        void unregisterProperty(DOMWindowProperty*);

        void resetUnlessSuspendedForDocumentSuspension();
        void suspendForDocumentSuspension();
        void resumeFromDocumentSuspension();

        PassRefPtr<MediaQueryList> matchMedia(const String&);

        WEBCORE_EXPORT unsigned pendingUnloadEventListeners() const;

        WEBCORE_EXPORT static bool dispatchAllPendingBeforeUnloadEvents();
        WEBCORE_EXPORT static void dispatchAllPendingUnloadEvents();

        static FloatRect adjustWindowRect(Page*, const FloatRect& pendingChanges);

        bool allowPopUp(); // Call on first window, not target window.
        static bool allowPopUp(Frame* firstFrame);
        static bool canShowModalDialog(const Frame*);
        WEBCORE_EXPORT void setCanShowModalDialogOverride(bool);

        // DOM Level 0

        Screen* screen() const;
        History* history() const;
        Crypto* crypto() const;
        BarProp* locationbar() const;
        BarProp* menubar() const;
        BarProp* personalbar() const;
        BarProp* scrollbars() const;
        BarProp* statusbar() const;
        BarProp* toolbar() const;
        Navigator* navigator() const;
        Navigator* clientInformation() const { return navigator(); }

        Location* location() const;
        void setLocation(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& location,
            SetLocationLocking = LockHistoryBasedOnGestureState);

        DOMSelection* getSelection();

        Element* frameElement() const;

        void focus(ScriptExecutionContext* = nullptr);
        void blur();
        WEBCORE_EXPORT void close(ScriptExecutionContext* = nullptr);
        void print();
        void stop();

        WEBCORE_EXPORT PassRefPtr<DOMWindow> open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
            DOMWindow& activeWindow, DOMWindow& firstWindow);

        void showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, std::function<void (DOMWindow&)> prepareDialogFunction);

        void alert(const String& message);
        bool confirm(const String& message);
        String prompt(const String& message, const String& defaultValue);
        String btoa(const String& stringToEncode, ExceptionCode&);
        String atob(const String& encodedString, ExceptionCode&);

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
        int pageXOffset() const { return scrollX(); }
        int pageYOffset() const { return scrollY(); }

        bool closed() const;

        unsigned length() const;

        String name() const;
        void setName(const String&);

        String status() const;
        void setStatus(const String&);
        String defaultStatus() const;
        void setDefaultStatus(const String&);

        // Self-referential attributes

        DOMWindow* self() const;
        DOMWindow* window() const { return self(); }
        DOMWindow* frames() const { return self(); }

        DOMWindow* opener() const;
        DOMWindow* parent() const;
        DOMWindow* top() const;

        // DOM Level 2 AbstractView Interface

        WEBCORE_EXPORT Document* document() const;

        // CSSOM View Module

        PassRefPtr<StyleMedia> styleMedia() const;

        // DOM Level 2 Style Interface

        PassRefPtr<CSSStyleDeclaration> getComputedStyle(Element*, const String& pseudoElt) const;

        // WebKit extensions

        PassRefPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt, bool authorOnly = true) const;
        double devicePixelRatio() const;

        PassRefPtr<WebKitPoint> webkitConvertPointFromPageToNode(Node*, const WebKitPoint*) const;
        PassRefPtr<WebKitPoint> webkitConvertPointFromNodeToPage(Node*, const WebKitPoint*) const;

        PageConsoleClient* console() const;

        void printErrorMessage(const String&);
        String crossDomainAccessErrorMessage(const DOMWindow& activeWindow);

        void postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, const String& targetOrigin, DOMWindow& source, ExceptionCode&);
        // Needed for Objective-C bindings (see bug 28774).
        void postMessage(PassRefPtr<SerializedScriptValue> message, MessagePort*, const String& targetOrigin, DOMWindow& source, ExceptionCode&);
        void postMessageTimerFired(PostMessageTimer&);
        void dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, Event&, PassRefPtr<Inspector::ScriptCallStack>);

        void scrollBy(int x, int y) const;
        void scrollTo(int x, int y) const;
        void scroll(int x, int y) const { scrollTo(x, y); }

        void moveBy(float x, float y) const;
        void moveTo(float x, float y) const;

        void resizeBy(float x, float y) const;
        void resizeTo(float width, float height) const;

        // Timers
        int setTimeout(std::unique_ptr<ScheduledAction>, int timeout, ExceptionCode&);
        void clearTimeout(int timeoutId);
        int setInterval(std::unique_ptr<ScheduledAction>, int timeout, ExceptionCode&);
        void clearInterval(int timeoutId);

        // WebKit animation extensions
#if ENABLE(REQUEST_ANIMATION_FRAME)
        int requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback>);
        int webkitRequestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback>);
        void cancelAnimationFrame(int id);
#endif

        DOMWindowCSS* css();

        // Events
        // EventTarget API
        virtual bool addEventListener(const AtomicString& eventType, RefPtr<EventListener>&&, bool useCapture) override;
        virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;
        virtual void removeAllEventListeners() override;

        using EventTarget::dispatchEvent;
        bool dispatchEvent(Event&, EventTarget*);

        void dispatchLoadEvent();

        void captureEvents();
        void releaseEvents();

        void finishedLoading();

        using RefCounted<DOMWindow>::ref;
        using RefCounted<DOMWindow>::deref;

        // HTML 5 key/value storage
        Storage* sessionStorage(ExceptionCode&) const;
        Storage* localStorage(ExceptionCode&) const;
        Storage* optionalSessionStorage() const { return m_sessionStorage.get(); }
        Storage* optionalLocalStorage() const { return m_localStorage.get(); }

        DOMApplicationCache* applicationCache() const;
        DOMApplicationCache* optionalApplicationCache() const { return m_applicationCache.get(); }

#if ENABLE(ORIENTATION_EVENTS)
        // This is the interface orientation in degrees. Some examples are:
        //  0 is straight up; -90 is when the device is rotated 90 clockwise;
        //  90 is when rotated counter clockwise.
        int orientation() const;
#endif

#if ENABLE(WEB_TIMING)
        Performance* performance() const;
#endif

#if PLATFORM(IOS)
        void incrementScrollEventListenersCount();
        void decrementScrollEventListenersCount();
        unsigned scrollEventListenerCount() const { return m_scrollEventListenerCount; }
#endif

        void resetAllGeolocationPermission();

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
        bool hasTouchEventListeners() const { return m_touchEventListenerCount > 0; }
#endif

#if ENABLE(USER_MESSAGE_HANDLERS)
        bool shouldHaveWebKitNamespaceForWorld(DOMWrapperWorld&);
        WebKitNamespace* webkitNamespace() const;
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

        WeakPtr<DOMWindow> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    private:
        explicit DOMWindow(Document*);

        Page* page();
        bool allowedToChangeWindowGeometry() const;

        virtual void frameDestroyed() override;
        virtual void willDetachPage() override;

        virtual void refEventTarget() override { ref(); }
        virtual void derefEventTarget() override { deref(); }

        static RefPtr<Frame> createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures&, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, std::function<void (DOMWindow&)> prepareDialogFunction = nullptr);
        bool isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString);

        void resetDOMWindowProperties();
        void disconnectDOMWindowProperties();
        void reconnectDOMWindowProperties();
        void willDestroyDocumentInFrame();

        bool isSameSecurityOriginAsMainFrame() const;

#if ENABLE(GAMEPAD)
        void incrementGamepadEventListenerCount();
        void decrementGamepadEventListenerCount();
#endif

        bool m_shouldPrintWhenFinishedLoading;
        bool m_suspendedForDocumentSuspension;
        Optional<bool> m_canShowModalDialogOverride;

        HashSet<DOMWindowProperty*> m_properties;

        mutable RefPtr<Screen> m_screen;
        mutable RefPtr<History> m_history;
        mutable RefPtr<Crypto>  m_crypto;
        mutable RefPtr<BarProp> m_locationbar;
        mutable RefPtr<BarProp> m_menubar;
        mutable RefPtr<BarProp> m_personalbar;
        mutable RefPtr<BarProp> m_scrollbars;
        mutable RefPtr<BarProp> m_statusbar;
        mutable RefPtr<BarProp> m_toolbar;
        mutable RefPtr<Navigator> m_navigator;
        mutable RefPtr<Location> m_location;
        mutable RefPtr<StyleMedia> m_media;

        String m_status;
        String m_defaultStatus;

        enum PageStatus { PageStatusNone, PageStatusShown, PageStatusHidden };
        PageStatus m_lastPageStatus;

        WeakPtrFactory<DOMWindow> m_weakPtrFactory;

#if PLATFORM(IOS)
        unsigned m_scrollEventListenerCount;
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
        unsigned m_touchEventListenerCount;
#endif

#if ENABLE(GAMEPAD)
        unsigned m_gamepadEventListenerCount;
#endif
        mutable RefPtr<Storage> m_sessionStorage;
        mutable RefPtr<Storage> m_localStorage;
        mutable RefPtr<DOMApplicationCache> m_applicationCache;

#if ENABLE(WEB_TIMING)
        mutable RefPtr<Performance> m_performance;
#endif

        mutable RefPtr<DOMWindowCSS> m_css;

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

#endif // DOMWindow_h
