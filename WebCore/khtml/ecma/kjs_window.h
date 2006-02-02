/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef KJS_WINDOW_H_
#define KJS_WINDOW_H_

#include "kjs_binding.h"
#include <kjs/protect.h>
#include <kxmlcore/HashMap.h>
#include <qguardedptr.h>
#include <qobject.h>
#include <qstring.h>

namespace WebCore {
    class AtomicString;
    class Frame;
    class FrameView;
    class NodeImpl;
}

namespace KJS {

    class BarInfo;
    class FrameArray;
    class History;
    class JSEventListener;
    class JSLazyEventListener;
    class JSUnprotectedEventListener;
    class Location;
    class PausedTimeout;
    class ScheduledAction;
    class Selection;
    class Window;
    class WindowFunc;

    class PausedTimeouts {
    public:
        PausedTimeouts(PausedTimeout *a, size_t length) : m_array(a), m_length(length) { }
        ~PausedTimeouts();

        size_t numTimeouts() const { return m_length; }
        PausedTimeout *takeTimeouts()
            { PausedTimeout *a = m_array; m_array = 0; return a; }

    private:
        PausedTimeout *m_array;
        size_t m_length;

        PausedTimeouts(const PausedTimeouts&);
        PausedTimeouts& operator=(const PausedTimeouts&);
    };

    class DOMWindowTimer;

    class WindowQObject : public QObject {
        Q_OBJECT
    public:
        WindowQObject(Window *);
        ~WindowQObject() { parentDestroyed(); }

        int installTimeout(const UString &handler, int interval, bool singleShot);
        int installTimeout(JSValue *function, const List &, int interval, bool singleShot);
        void clearTimeout(int timerId, bool delAction = true);

        PausedTimeouts *pauseTimeouts();
        void resumeTimeouts(PausedTimeouts *);

        void timerFired(DOMWindowTimer*);

    private slots:
        void parentDestroyed();
    private:
        int installTimeout(ScheduledAction*, int interval, bool singleShot);

        Window* m_parent;
        typedef HashMap<int, DOMWindowTimer*> TimeoutsMap;
        TimeoutsMap m_timeouts;
    };

  class Screen : public JSObject {
  public:
    enum { Height, Width, ColorDepth, PixelDepth, AvailLeft, AvailTop, AvailHeight, AvailWidth };
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Screen(ExecState*, WebCore::Frame*);
    QGuardedPtr<WebCore::Frame> m_frame;
  };

  class Window : public JSObject {
    friend QGuardedPtr<WebCore::Frame> getInstance();
    friend class Location;
    friend class WindowFunc;
    friend class WindowQObject;
    friend class ScheduledAction;
  public:
    Window(WebCore::Frame *p);
  public:
    ~Window();
    /**
     * Returns and registers a window object. In case there's already a Window
     * for the specified frame p this will be returned in order to have unique
     * bindings.
     */
    static JSValue* retrieve(WebCore::Frame *p);
    /**
     * Returns the Window object for a given HTML frame
     */
    static Window* retrieveWindow(WebCore::Frame *p);
    /**
     * returns a pointer to the Window object this javascript interpreting instance
     * was called from.
     */
    static Window *retrieveActive(ExecState*);
    QGuardedPtr<WebCore::Frame> frame() const { return m_frame; }
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool toBoolean(ExecState*) const;

    int installTimeout(const UString& handler, int t, bool singleShot) { return winq->installTimeout(handler, t, singleShot); }
    int installTimeout(JSValue* function, List& args, int t, bool singleShot) { return winq->installTimeout(function, args, t, singleShot); }
    void clearTimeout(int timerId) { winq->clearTimeout(timerId); }
    PausedTimeouts* pauseTimeouts() { return winq->pauseTimeouts(); }
    void resumeTimeouts(PausedTimeouts* t) { winq->resumeTimeouts(t); }
    
    KJS::ScriptInterpreter *interpreter() const;

    void scheduleClose();
        
    bool isSafeScript(ExecState*) const;
    static bool isSafeScript(const ScriptInterpreter *origin, const ScriptInterpreter *target);
    Location *location() const;
    Selection *selection() const;
    BarInfo *locationbar(ExecState*) const;
    BarInfo *menubar(ExecState*) const;
    BarInfo *personalbar(ExecState*) const;
    BarInfo *scrollbars(ExecState*) const;
    BarInfo *statusbar(ExecState*) const;
    BarInfo *toolbar(ExecState*) const;
    JSEventListener *getJSEventListener(JSValue*, bool html = false);
    JSUnprotectedEventListener *getJSUnprotectedEventListener(JSValue*, bool html = false);
    void clear();
    virtual UString toString(ExecState *) const;

    // Set the current "event" object
    void setCurrentEvent(DOM::EventImpl *evt);

    // Set a place to put a dialog return value when the window is cleared.
    void setReturnValueSlot(JSValue **slot) { m_returnValueSlot = slot; }

    typedef HashMap<JSObject*, JSEventListener*> ListenersMap;
    typedef HashMap<JSObject*, JSUnprotectedEventListener*> UnprotectedListenersMap;
    ListenersMap jsEventListeners;
    UnprotectedListenersMap jsUnprotectedEventListeners;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Closed, Crypto, DefaultStatus, Status, Document, Node, EventCtor, MutationEventCtor, Range,
           NodeFilter, DOMException, CSSRule, Frames, _History, Event, InnerHeight,
           InnerWidth, Length, _Location, Locationbar, Name, _Navigator, ClientInformation,
           Menubar, OffscreenBuffering, Opener, OuterHeight, OuterWidth, PageXOffset, PageYOffset,
           Parent, Personalbar, ScreenX, ScreenY, Scrollbars, Scroll, ScrollBy,
           ScreenTop, ScreenLeft,
           ScrollTo, ScrollX, ScrollY, MoveBy, MoveTo, ResizeBy, ResizeTo, Self, _Window, Top, _Screen,
           Image, Option, Alert, Confirm, Prompt, Open, Print, SetTimeout, ClearTimeout,
           Focus, GetSelection, Blur, Close, SetInterval, ClearInterval, CaptureEvents, 
           ReleaseEvents, AddEventListener, RemoveEventListener,
           XMLHttpRequest, XMLSerializer, DOMParser, XSLTProcessor,
           Onabort, Onblur, Onchange, Onclick, Ondblclick, Ondragdrop, Onerror, 
           Onfocus, Onkeydown, Onkeypress, Onkeyup, Onload, Onmousedown, Onmousemove,
           Onmouseout, Onmouseover, Onmouseup, OnWindowMouseWheel, Onmove, Onreset, Onresize, Onscroll, Onsearch,
           Onselect, Onsubmit, Onunload, Onbeforeunload,
           Statusbar, Toolbar, FrameElement, ShowModalDialog };
  protected:
    JSValue *getListener(ExecState *exec, const DOM::AtomicString &eventType) const;
    void setListener(ExecState *exec, const DOM::AtomicString &eventType, JSValue *func);
  private:
    static JSValue *childFrameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *namedFrameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *namedItemGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);

    void updateLayout() const;

    QGuardedPtr<WebCore::Frame> m_frame;
    mutable Screen *screen;
    mutable History *history;
    mutable FrameArray *frames;
    Location *loc;
    Selection *m_selection;
    BarInfo *m_locationbar;
    BarInfo *m_menubar;
    BarInfo *m_personalbar;
    BarInfo *m_scrollbars;
    BarInfo *m_statusbar;
    BarInfo *m_toolbar;
    WindowQObject *winq;
    DOM::EventImpl *m_evt;
    JSValue **m_returnValueSlot;
  };

  /**
   * An action (either function or string) to be executed after a specified
   * time interval, either once or repeatedly. Used for window.setTimeout()
   * and window.setInterval()
   */
    class ScheduledAction {
    public:
        ScheduledAction(JSValue *func, const List& args)
            : m_func(func), m_args(args) { }
        ScheduledAction(const QString& code)
            : m_code(code) { }
        void execute(Window *);

    private:
        ProtectedPtr<JSValue> m_func;
        List m_args;
        QString m_code;
    };

  class Location : public JSObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual JSValue *toPrimitive(ExecState *exec, Type preferred) const;
    virtual UString toString(ExecState*) const;
    enum { Hash, Href, Hostname, Host, Pathname, Port, Protocol, Search, EqualEqual,
           Replace, Reload, ToString, Assign };
    WebCore::Frame *frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Location(WebCore::Frame *p);
    QGuardedPtr<WebCore::Frame> m_frame;
  };

  class Selection : public JSObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual JSValue *toPrimitive(ExecState *exec, Type preferred) const;
    virtual UString toString(ExecState*) const;
    enum { AnchorNode, AnchorOffset, FocusNode, FocusOffset, BaseNode, BaseOffset, ExtentNode, ExtentOffset, 
           IsCollapsed, _Type, EqualEqual, Collapse, CollapseToEnd, CollapseToStart, Empty, ToString, 
           SetBaseAndExtent, SetPosition, Modify, GetRangeAt };
    WebCore::Frame *frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Selection(WebCore::Frame *p);
    QGuardedPtr<WebCore::Frame> m_frame;
  };

  class BarInfo : public JSObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    enum { Visible };
    enum Type { Locationbar, Menubar, Personalbar, Scrollbars, Statusbar, Toolbar };
    WebCore::Frame *frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    BarInfo(ExecState *exec, WebCore::Frame *p, Type barType);
    QGuardedPtr<WebCore::Frame> m_frame;
    Type m_type;
  };

} // namespace

#endif
