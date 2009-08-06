/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "EventTarget.h"
#include "KURL.h"
#include "PlatformString.h"
#include "RegisteredEventListener.h"
#include "SecurityOrigin.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class BarInfo;
    class BeforeUnloadEvent;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class Console;
    class DOMSelection;
    class Database;
    class Document;
    class Element;
    class Event;
    class EventListener;
    class FloatRect;
    class Frame;
    class History;
    class Location;
    class MessagePort;
    class Navigator;
    class Node;
    class PostMessageTimer;
    class ScheduledAction;
    class Screen;
    class WebKitPoint;

#if ENABLE(DOM_STORAGE)
    class Storage;
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    class DOMApplicationCache;
#endif

    typedef int ExceptionCode;

    class DOMWindow : public RefCounted<DOMWindow>, public EventTarget {
    public:
        static PassRefPtr<DOMWindow> create(Frame* frame) { return adoptRef(new DOMWindow(frame)); }
        virtual ~DOMWindow();

        virtual DOMWindow* toDOMWindow() { return this; }
        virtual ScriptExecutionContext* scriptExecutionContext() const;

        Frame* frame() const { return m_frame; }
        void disconnectFrame();

        void clear();

        void setSecurityOrigin(SecurityOrigin* securityOrigin) { m_securityOrigin = securityOrigin; }
        SecurityOrigin* securityOrigin() const { return m_securityOrigin.get(); }

        void setURL(const KURL& url) { m_url = url; }
        KURL url() const { return m_url; }

        unsigned pendingUnloadEventListeners() const;

        static bool dispatchAllPendingBeforeUnloadEvents();
        static void dispatchAllPendingUnloadEvents();

        static void adjustWindowRect(const FloatRect& screen, FloatRect& window, const FloatRect& pendingChanges);
        static void parseModalDialogFeatures(const String& featuresArg, HashMap<String, String>& map);

        static bool allowPopUp(Frame* activeFrame);
        static bool canShowModalDialog(const Frame*);
        static bool canShowModalDialogNow(const Frame*);

        // DOM Level 0
        Screen* screen() const;
        History* history() const;
        BarInfo* locationbar() const;
        BarInfo* menubar() const;
        BarInfo* personalbar() const;
        BarInfo* scrollbars() const;
        BarInfo* statusbar() const;
        BarInfo* toolbar() const;
        Navigator* navigator() const;
        Navigator* clientInformation() const { return navigator(); }
        Location* location() const;

        DOMSelection* getSelection();

        Element* frameElement() const;

        void focus();
        void blur();
        void close();
        void print();
        void stop();

        void alert(const String& message);
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
        // This attribute is an alias of defaultStatus and is necessary for legacy uses.
        String defaultstatus() const { return defaultStatus(); }
        void setDefaultstatus(const String& status) { setDefaultStatus(status); }

        // Self referential attributes
        DOMWindow* self() const;
        DOMWindow* window() const { return self(); }
        DOMWindow* frames() const { return self(); }

        DOMWindow* opener() const;
        DOMWindow* parent() const;
        DOMWindow* top() const;

        // DOM Level 2 AbstractView Interface
        Document* document() const;

        // DOM Level 2 Style Interface
        PassRefPtr<CSSStyleDeclaration> getComputedStyle(Element*, const String& pseudoElt) const;

        // WebKit extensions
        PassRefPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt, bool authorOnly = true) const;
        double devicePixelRatio() const;

        PassRefPtr<WebKitPoint> webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* p) const;
        PassRefPtr<WebKitPoint> webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* p) const;        

#if ENABLE(DATABASE)
        // HTML 5 client-side database
        PassRefPtr<Database> openDatabase(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, ExceptionCode&);
#endif

#if ENABLE(DOM_STORAGE)
        // HTML 5 key/value storage
        Storage* sessionStorage() const;
        Storage* localStorage() const;
#endif

        Console* console() const;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        DOMApplicationCache* applicationCache() const;
#endif

        void postMessage(const String& message, MessagePort*, const String& targetOrigin, DOMWindow* source, ExceptionCode&);
        void postMessageTimerFired(PostMessageTimer*);

        void scrollBy(int x, int y) const;
        void scrollTo(int x, int y) const;
        void scroll(int x, int y) const { scrollTo(x, y); }

        void moveBy(float x, float y) const;
        void moveTo(float x, float y) const;

        void resizeBy(float x, float y) const;
        void resizeTo(float width, float height) const;

        // Timers
        int setTimeout(ScheduledAction*, int timeout);
        void clearTimeout(int timeoutId);
        int setInterval(ScheduledAction*, int timeout);
        void clearInterval(int timeoutId);

        // Events
        // EventTarget API
        virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
        virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
        virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

        void handleEvent(Event*, bool useCapture, RegisteredEventListenerVector* = 0);

        void dispatchEvent(const AtomicString& eventType, bool canBubble, bool cancelable);
        void dispatchLoadEvent();
        void dispatchUnloadEvent(RegisteredEventListenerVector* = 0);
        PassRefPtr<BeforeUnloadEvent> dispatchBeforeUnloadEvent(RegisteredEventListenerVector* = 0);

        // Used for legacy "onEvent" property APIs.
        void setAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
        void clearAttributeEventListener(const AtomicString& eventType);
        EventListener* getAttributeEventListener(const AtomicString& eventType) const;

        const RegisteredEventListenerVector& eventListeners() const { return m_eventListeners; }
        bool hasEventListener(const AtomicString& eventType);
        void removeAllEventListeners();

        EventListener* onabort() const;
        void setOnabort(PassRefPtr<EventListener>);
        EventListener* onblur() const;
        void setOnblur(PassRefPtr<EventListener>);
        EventListener* onchange() const;
        void setOnchange(PassRefPtr<EventListener>);
        EventListener* onclick() const;
        void setOnclick(PassRefPtr<EventListener>);
        EventListener* ondblclick() const;
        void setOndblclick(PassRefPtr<EventListener>);
        EventListener* ondrag() const;
        void setOndrag(PassRefPtr<EventListener>);
        EventListener* ondragend() const;
        void setOndragend(PassRefPtr<EventListener>);
        EventListener* ondragenter() const;
        void setOndragenter(PassRefPtr<EventListener>);
        EventListener* ondragleave() const;
        void setOndragleave(PassRefPtr<EventListener>);
        EventListener* ondragover() const;
        void setOndragover(PassRefPtr<EventListener>);
        EventListener* ondragstart() const;
        void setOndragstart(PassRefPtr<EventListener>);
        EventListener* ondrop() const;
        void setOndrop(PassRefPtr<EventListener>);
        EventListener* onerror() const;
        void setOnerror(PassRefPtr<EventListener>);
        EventListener* onfocus() const;
        void setOnfocus(PassRefPtr<EventListener>);
        EventListener* onkeydown() const;
        void setOnkeydown(PassRefPtr<EventListener>);
        EventListener* onkeypress() const;
        void setOnkeypress(PassRefPtr<EventListener>);
        EventListener* onkeyup() const;
        void setOnkeyup(PassRefPtr<EventListener>);
        EventListener* onload() const;
        void setOnload(PassRefPtr<EventListener>);
        EventListener* onmousedown() const;
        void setOnmousedown(PassRefPtr<EventListener>);
        EventListener* onmousemove() const;
        void setOnmousemove(PassRefPtr<EventListener>);
        EventListener* onmouseout() const;
        void setOnmouseout(PassRefPtr<EventListener>);
        EventListener* onmouseover() const;
        void setOnmouseover(PassRefPtr<EventListener>);
        EventListener* onmouseup() const;
        void setOnmouseup(PassRefPtr<EventListener>);
        EventListener* onmousewheel() const;
        void setOnmousewheel(PassRefPtr<EventListener>);
        EventListener* onoffline() const;
        void setOnoffline(PassRefPtr<EventListener>);
        EventListener* ononline() const;
        void setOnonline(PassRefPtr<EventListener>);
        EventListener* onreset() const;
        void setOnreset(PassRefPtr<EventListener>);
        EventListener* onresize() const;
        void setOnresize(PassRefPtr<EventListener>);
        EventListener* onscroll() const;
        void setOnscroll(PassRefPtr<EventListener>);
        EventListener* onsearch() const;
        void setOnsearch(PassRefPtr<EventListener>);
        EventListener* onselect() const;
        void setOnselect(PassRefPtr<EventListener>);
        EventListener* onstorage() const;
        void setOnstorage(PassRefPtr<EventListener>);
        EventListener* onsubmit() const;
        void setOnsubmit(PassRefPtr<EventListener>);
        EventListener* onunload() const;
        void setOnunload(PassRefPtr<EventListener>);
        EventListener* onbeforeunload() const;
        void setOnbeforeunload(PassRefPtr<EventListener>);
        EventListener* onwebkitanimationstart() const;
        void setOnwebkitanimationstart(PassRefPtr<EventListener>);
        EventListener* onwebkitanimationiteration() const;
        void setOnwebkitanimationiteration(PassRefPtr<EventListener>);
        EventListener* onwebkitanimationend() const;
        void setOnwebkitanimationend(PassRefPtr<EventListener>);
        EventListener* onwebkittransitionend() const;
        void setOnwebkittransitionend(PassRefPtr<EventListener>);
        EventListener* oncanplay() const;
        void setOncanplay(PassRefPtr<EventListener>);
        EventListener* oncanplaythrough() const;
        void setOncanplaythrough(PassRefPtr<EventListener>);
        EventListener* ondurationchange() const;
        void setOndurationchange(PassRefPtr<EventListener>);
        EventListener* onemptied() const;
        void setOnemptied(PassRefPtr<EventListener>);
        EventListener* onended() const;
        void setOnended(PassRefPtr<EventListener>);
        EventListener* onloadeddata() const;
        void setOnloadeddata(PassRefPtr<EventListener>);
        EventListener* onloadedmetadata() const;
        void setOnloadedmetadata(PassRefPtr<EventListener>);
        EventListener* onpause() const;
        void setOnpause(PassRefPtr<EventListener>);
        EventListener* onplay() const;
        void setOnplay(PassRefPtr<EventListener>);
        EventListener* onplaying() const;
        void setOnplaying(PassRefPtr<EventListener>);
        EventListener* onratechange() const;
        void setOnratechange(PassRefPtr<EventListener>);
        EventListener* onseeked() const;
        void setOnseeked(PassRefPtr<EventListener>);
        EventListener* onseeking() const;
        void setOnseeking(PassRefPtr<EventListener>);
        EventListener* ontimeupdate() const;
        void setOntimeupdate(PassRefPtr<EventListener>);
        EventListener* onvolumechange() const;
        void setOnvolumechange(PassRefPtr<EventListener>);
        EventListener* onwaiting() const;
        void setOnwaiting(PassRefPtr<EventListener>);
        EventListener* onloadstart() const;
        void setOnloadstart(PassRefPtr<EventListener>);
        EventListener* onprogress() const;
        void setOnprogress(PassRefPtr<EventListener>);
        EventListener* onstalled() const;
        void setOnstalled(PassRefPtr<EventListener>);
        EventListener* onsuspend() const;
        void setOnsuspend(PassRefPtr<EventListener>);
        EventListener* oninput() const;
        void setOninput(PassRefPtr<EventListener>);
        EventListener* onmessage() const;
        void setOnmessage(PassRefPtr<EventListener>);
        EventListener* oncontextmenu() const;
        void setOncontextmenu(PassRefPtr<EventListener>);

        void captureEvents();
        void releaseEvents();

        // These methods are used for GC marking. See JSDOMWindow::mark() in
        // JSDOMWindowCustom.cpp.
        Screen* optionalScreen() const { return m_screen.get(); }
        DOMSelection* optionalSelection() const { return m_selection.get(); }
        History* optionalHistory() const { return m_history.get(); }
        BarInfo* optionalLocationbar() const { return m_locationbar.get(); }
        BarInfo* optionalMenubar() const { return m_menubar.get(); }
        BarInfo* optionalPersonalbar() const { return m_personalbar.get(); }
        BarInfo* optionalScrollbars() const { return m_scrollbars.get(); }
        BarInfo* optionalStatusbar() const { return m_statusbar.get(); }
        BarInfo* optionalToolbar() const { return m_toolbar.get(); }
        Console* optionalConsole() const { return m_console.get(); }
        Navigator* optionalNavigator() const { return m_navigator.get(); }
        Location* optionalLocation() const { return m_location.get(); }
#if ENABLE(DOM_STORAGE)
        Storage* optionalSessionStorage() const { return m_sessionStorage.get(); }
        Storage* optionalLocalStorage() const { return m_localStorage.get(); }
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        DOMApplicationCache* optionalApplicationCache() const { return m_applicationCache.get(); }
#endif

        using RefCounted<DOMWindow>::ref;
        using RefCounted<DOMWindow>::deref;

    private:
        DOMWindow(Frame*);

        virtual void refEventTarget() { ref(); }
        virtual void derefEventTarget() { deref(); }

        void dispatchEventWithDocumentAsTarget(PassRefPtr<Event>, RegisteredEventListenerVector* = 0);

        RefPtr<SecurityOrigin> m_securityOrigin;
        KURL m_url;

        Frame* m_frame;
        mutable RefPtr<Screen> m_screen;
        mutable RefPtr<DOMSelection> m_selection;
        mutable RefPtr<History> m_history;
        mutable RefPtr<BarInfo> m_locationbar;
        mutable RefPtr<BarInfo> m_menubar;
        mutable RefPtr<BarInfo> m_personalbar;
        mutable RefPtr<BarInfo> m_scrollbars;
        mutable RefPtr<BarInfo> m_statusbar;
        mutable RefPtr<BarInfo> m_toolbar;
        mutable RefPtr<Console> m_console;
        mutable RefPtr<Navigator> m_navigator;
        mutable RefPtr<Location> m_location;
#if ENABLE(DOM_STORAGE)
        mutable RefPtr<Storage> m_sessionStorage;
        mutable RefPtr<Storage> m_localStorage;
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        mutable RefPtr<DOMApplicationCache> m_applicationCache;
#endif

        RegisteredEventListenerVector m_eventListeners;
    };

} // namespace WebCore

#endif // DOMWindow_h
