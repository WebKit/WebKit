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

#include "DeprecatedString.h"
#include "kjs_binding.h"
#include <kxmlcore/HashMap.h>

namespace WebCore {
    class AtomicString;
    class DOMWindow;
    class Frame;
    class FrameView;
    class JSDOMWindow;
    class Node;
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

  class Screen : public DOMObject {
  public:
    enum { Height, Width, ColorDepth, PixelDepth, AvailLeft, AvailTop, AvailHeight, AvailWidth };
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Screen(ExecState*, WebCore::Frame*);
    WebCore::Frame* m_frame;
  };

  class Window : public DOMObject {
    friend class Location;
    friend class WindowFunc;
    friend class ScheduledAction;
  protected:
    Window(WebCore::DOMWindow*);
  public:
    ~Window();
    WebCore::DOMWindow* impl() const;
    void disconnectFrame();
    /**
     * Returns and registers a window object. In case there's already a Window
     * for the specified frame p this will be returned in order to have unique
     * bindings.
     */
    static JSValue* retrieve(WebCore::Frame*);
    /**
     * Returns the Window object for a given HTML frame
     */
    static Window* retrieveWindow(WebCore::Frame*);
    /**
     * returns a pointer to the Window object this javascript interpreting instance
     * was called from.
     */
    static Window* retrieveActive(ExecState*);
    WebCore::Frame* frame() const { return m_frame; }
    virtual void mark();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual bool toBoolean(ExecState*) const;

    int installTimeout(const UString& handler, int t, bool singleShot);
    int installTimeout(JSValue* function, const List& args, int t, bool singleShot);
    void clearTimeout(int timerId, bool delAction = true);
    PausedTimeouts* pauseTimeouts();
    void resumeTimeouts(PausedTimeouts*);

    void timerFired(DOMWindowTimer*);
    
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
    void setCurrentEvent(WebCore::Event*);

    // Set a place to put a dialog return value when the window is cleared.
    void setReturnValueSlot(JSValue **slot) { m_returnValueSlot = slot; }

    typedef HashMap<JSObject*, JSEventListener*> ListenersMap;
    typedef HashMap<JSObject*, JSUnprotectedEventListener*> UnprotectedListenersMap;
    ListenersMap jsEventListeners;
    UnprotectedListenersMap jsUnprotectedEventListeners;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Closed, Crypto, DefaultStatus, Status, Node, EventCtor, MutationEventCtor, Range,
           NodeFilter, DOMException, CSSRule, CSSValue, Frames, History_, Event_, InnerHeight,
           InnerWidth, Length, Location_, Locationbar, Name, Navigator_, ClientInformation,
           Menubar, OffscreenBuffering, Opener, OuterHeight, OuterWidth, PageXOffset, PageYOffset,
           Parent, Personalbar, ScreenX, ScreenY, Scrollbars, Scroll, ScrollBy,
           ScreenTop, ScreenLeft,
           ScrollTo, ScrollX, ScrollY, MoveBy, MoveTo, ResizeBy, ResizeTo, Self, Window_, Top, Screen_,
           Image, Option, Alert, Confirm, Prompt, Open, Print, SetTimeout, ClearTimeout,
           Focus, GetSelection, Blur, Close, SetInterval, ClearInterval, CaptureEvents, 
           ReleaseEvents, AddEventListener, RemoveEventListener,
           XMLHttpRequest, XMLSerializer, DOMParser_, XSLTProcessor_,
           Onabort, Onblur, Onchange, Onclick, Ondblclick, Ondragdrop, Onerror, 
           Onfocus, Onkeydown, Onkeypress, Onkeyup, Onload, Onmousedown, Onmousemove,
           Onmouseout, Onmouseover, Onmouseup, OnWindowMouseWheel, Onmove, Onreset, Onresize, Onscroll, Onsearch,
           Onselect, Onsubmit, Onunload, Onbeforeunload,
           Statusbar, Toolbar, FrameElement, ShowModalDialog };

  private:
    JSValue* getListener(ExecState*, const WebCore::AtomicString& eventType) const;
    void setListener(ExecState*, const WebCore::AtomicString& eventType, JSValue* func);

    static JSValue *childFrameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *namedFrameGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *indexGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);
    static JSValue *namedItemGetter(ExecState *exec, JSObject *, const Identifier&, const PropertySlot& slot);

    void updateLayout() const;

    void clearAllTimeouts();
    int installTimeout(ScheduledAction*, int interval, bool singleShot);

    WebCore::Frame* m_frame;
    mutable Screen* screen;
    mutable History* history;
    mutable FrameArray* frames;
    mutable Location* loc;
    mutable Selection* m_selection;
    mutable BarInfo* m_locationbar;
    mutable BarInfo* m_menubar;
    mutable BarInfo* m_personalbar;
    mutable BarInfo* m_scrollbars;
    mutable BarInfo* m_statusbar;
    mutable BarInfo* m_toolbar;
    WebCore::Event *m_evt;
    JSValue **m_returnValueSlot;
    typedef HashMap<int, DOMWindowTimer*> TimeoutsMap;
    TimeoutsMap m_timeouts;
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
        ScheduledAction(const DeprecatedString& code)
            : m_code(code) { }
        void execute(Window *);

    private:
        ProtectedPtr<JSValue> m_func;
        List m_args;
        DeprecatedString m_code;
    };

  class Location : public DOMObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    virtual JSValue *toPrimitive(ExecState *exec, JSType preferred) const;
    virtual UString toString(ExecState*) const;
    enum { Hash, Href, Hostname, Host, Pathname, Port, Protocol, Search, EqualEqual,
           Replace, Reload, ToString, Assign };
    WebCore::Frame* frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Location(WebCore::Frame*);
    WebCore::Frame* m_frame;
  };

  class Selection : public DOMObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual JSValue *toPrimitive(ExecState *exec, JSType preferred) const;
    virtual UString toString(ExecState*) const;
    enum { AnchorNode, AnchorOffset, FocusNode, FocusOffset, BaseNode, BaseOffset, ExtentNode, ExtentOffset, 
           IsCollapsed, _Type, EqualEqual, Collapse, CollapseToEnd, CollapseToStart, Empty, ToString, 
           SetBaseAndExtent, SetPosition, Modify, GetRangeAt };
    WebCore::Frame* frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Selection(WebCore::Frame*);
    WebCore::Frame* m_frame;
  };

  class BarInfo : public DOMObject {
  public:
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    enum { Visible };
    enum Type { Locationbar, Menubar, Personalbar, Scrollbars, Statusbar, Toolbar };
    WebCore::Frame* frame() const { return m_frame; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    BarInfo(ExecState*, WebCore::Frame*, Type);
    WebCore::Frame* m_frame;
    Type m_type;
  };

} // namespace

namespace WebCore {
    KJS::JSValue* toJS(KJS::ExecState*, DOMWindow*);
    DOMWindow* toDOMWindow(KJS::JSValue*);
} // namespace WebCore

#endif
