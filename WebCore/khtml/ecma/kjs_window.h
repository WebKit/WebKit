// -*- c-basic-offset: 2 -*-
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

#ifndef _KJS_WINDOW_H_
#define _KJS_WINDOW_H_

#include <qobject.h>
#include <qguardedptr.h>
#include <qmap.h>
#include <qptrdict.h>

#include "kjs_binding.h"

class QTimer;
class KHTMLView;
class KHTMLPart;

namespace DOM {
    class NodeImpl;
}

namespace KJS {

  class WindowFunc;
  class WindowQObject;
  class Location;
  class Selection;
  class BarInfo;
  class History;
  class FrameArray;
  class JSEventListener;
  class JSUnprotectedEventListener;
  class JSLazyEventListener;

  class Screen : public ObjectImp {
  public:
    Screen(ExecState *exec);
    enum {
      Height, Width, ColorDepth, PixelDepth, AvailLeft, AvailTop, AvailHeight,
      AvailWidth
    };
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    Value getValueProperty(ExecState *exec, int token) const;
  private:
    KHTMLView *view;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  };

  class Window : public ObjectImp {
    friend QGuardedPtr<KHTMLPart> getInstance();
    friend class Location;
    friend class WindowFunc;
    friend class WindowQObject;
    friend class ScheduledAction;
  public:
    Window(KHTMLPart *p);
  public:
    ~Window();
    /**
     * Returns and registers a window object. In case there's already a Window
     * for the specified part p this will be returned in order to have unique
     * bindings.
     */
    static Value retrieve(KHTMLPart *p);
    /**
     * Returns the Window object for a given HTML part
     */
    static Window *retrieveWindow(KHTMLPart *p);
    /**
     * returns a pointer to the Window object this javascript interpreting instance
     * was called from.
     */
    static Window *retrieveActive(ExecState *exec);
    QGuardedPtr<KHTMLPart> part() const { return m_part; }
    virtual void mark();
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual bool hasOwnProperty(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr = None);
    virtual bool toBoolean(ExecState *exec) const;
    int installTimeout(const UString &handler, int t, bool singleShot);
    int installTimeout(const Value &function, List &args, int t, bool singleShot);
    void clearTimeout(int timerId);
#ifdef APPLE_CHANGES
    bool hasTimeouts();
    QMap<int, ScheduledAction*> *pauseTimeouts(const void *key);
    void resumeTimeouts(QMap<int, ScheduledAction*>*sa, const void *key);
    
    KJS::Interpreter *interpreter() const;

    static bool isSafeScript (const KJS::ScriptInterpreter *origin, const KJS::ScriptInterpreter *target);
#endif
    void scheduleClose();
        
    bool isSafeScript(ExecState *exec) const;
    Location *location() const;
    Selection *selection() const;
    BarInfo *locationbar(ExecState *exec) const;
    BarInfo *menubar(ExecState *exec) const;
    BarInfo *personalbar(ExecState *exec) const;
    BarInfo *scrollbars(ExecState *exec) const;
    BarInfo *statusbar(ExecState *exec) const;
    BarInfo *toolbar(ExecState *exec) const;
    JSEventListener *getJSEventListener(const Value &val, bool html = false);
    JSUnprotectedEventListener *getJSUnprotectedEventListener(const Value &val, bool html = false);
    JSLazyEventListener *getJSLazyEventListener(const QString &code, DOM::NodeImpl *node, int lineno = 0);
    void clear( ExecState *exec );
    virtual UString toString(ExecState *exec) const;

    // Set the current "event" object
    void setCurrentEvent(DOM::EventImpl *evt);

    // Set a place to put a dialog return value when the window is cleared.
    void setReturnValueSlot(ValueImp **slot) { m_returnValueSlot = slot; }

    QPtrDict<JSEventListener> jsEventListeners;
    QPtrDict<JSUnprotectedEventListener> jsUnprotectedEventListeners;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Closed, Crypto, DefaultStatus, Status, Document, Node, EventCtor, Range,
           NodeFilter, DOMException, CSSRule, Frames, _History, Event, InnerHeight,
           InnerWidth, Length, _Location, Locationbar, Name, _Navigator, _Konqueror, ClientInformation,
           Menubar, OffscreenBuffering, Opener, OuterHeight, OuterWidth, PageXOffset, PageYOffset,
           Parent, Personalbar, ScreenX, ScreenY, Scrollbars, Scroll, ScrollBy,
           ScreenTop, ScreenLeft,
           ScrollTo, ScrollX, ScrollY, MoveBy, MoveTo, ResizeBy, ResizeTo, Self, _Window, Top, _Screen,
           Image, Option, Alert, Confirm, Prompt, Open, Print, SetTimeout, ClearTimeout,
           Focus, GetSelection, Blur, Close, SetInterval, ClearInterval, CaptureEvents, 
           ReleaseEvents, AddEventListener, RemoveEventListener, XMLHttpRequest, XMLSerializer, DOMParser,
	   Onabort, Onblur, Onchange, Onclick, Ondblclick, Ondragdrop, Onerror, 
	   Onfocus, Onkeydown, Onkeypress, Onkeyup, Onload, Onmousedown, Onmousemove,
           Onmouseout, Onmouseover, Onmouseup, OnWindowMouseWheel, Onmove, Onreset, Onresize, Onscroll, Onsearch,
           Onselect, Onsubmit, Onunload,
           Statusbar, Toolbar, FrameElement, ShowModalDialog };
  protected:
    Value getListener(ExecState *exec, int eventId) const;
    void setListener(ExecState *exec, int eventId, Value func);
  private:
    void updateLayout() const;

    QGuardedPtr<KHTMLPart> m_part;
    Screen *screen;
    History *history;
    FrameArray *frames;
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
    ValueImp **m_returnValueSlot;
  };

  /**
   * An action (either function or string) to be executed after a specified
   * time interval, either once or repeatedly. Used for window.setTimeout()
   * and window.setInterval()
   */
  class ScheduledAction {
  public:
    ScheduledAction(Object _func, List _args, bool _singleShot);
    ScheduledAction(const QString &_code, bool _singleShot);
    ~ScheduledAction();
    void execute(Window *window);

    ProtectedObject func;
    List args;
    QString code;
    bool isFunction;
    bool singleShot;
  };

  class WindowQObject : public QObject {
    Q_OBJECT
  public:
    WindowQObject(Window *w);
    ~WindowQObject();
    int installTimeout(const UString &handler, int t, bool singleShot);
    int installTimeout(const Value &func, List args, int t, bool singleShot);
    void clearTimeout(int timerId, bool delAction = true);
#ifdef APPLE_CHANGES
    bool hasTimeouts();
    QMap<int, ScheduledAction*> *WindowQObject::pauseTimeouts(const void *key);
    void WindowQObject::resumeTimeouts(QMap<int, ScheduledAction*> *sa, const void *key);
#endif

  public slots:
    void timeoutClose();
  protected slots:
    void parentDestroyed();
  protected:
    void timerEvent(QTimerEvent *e);
  private:
    Window *parent;
    KHTMLPart *part;   		// not guarded, may be dangling
    QMap<int, ScheduledAction*> scheduledActions;
  };

  class Location : public ObjectImp {
  public:
    ~Location();
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr = None);
    virtual Value toPrimitive(ExecState *exec, Type preferred) const;
    virtual UString toString(ExecState *exec) const;
    enum { Hash, Href, Hostname, Host, Pathname, Port, Protocol, Search, EqualEqual,
           Replace, Reload, ToString };
    KHTMLPart *part() const { return m_part; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Location(KHTMLPart *p);
    QGuardedPtr<KHTMLPart> m_part;
  };

  class Selection : public ObjectImp {
  public:
    ~Selection();
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr = None);
    virtual Value toPrimitive(ExecState *exec, Type preferred) const;
    virtual UString toString(ExecState *exec) const;
    enum { AnchorNode, AnchorOffset, FocusNode, FocusOffset, BaseNode, BaseOffset, ExtentNode, ExtentOffset, 
           IsCollapsed, _Type, EqualEqual, Collapse, CollapseToEnd, CollapseToStart, Empty, ToString, 
           SetBaseAndExtent, SetPosition, Modify };
    KHTMLPart *part() const { return m_part; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    Selection(KHTMLPart *p);
    QGuardedPtr<KHTMLPart> m_part;
  };

  class BarInfo : public ObjectImp {
  public:
    ~BarInfo();
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr = None);
    enum { Visible };
    enum Type { Locationbar, Menubar, Personalbar, Scrollbars, Statusbar, Toolbar };
    KHTMLPart *part() const { return m_part; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    friend class Window;
    BarInfo(ExecState *exec, KHTMLPart *p, Type barType);
    QGuardedPtr<KHTMLPart> m_part;
    Type m_type;
  };

#ifdef Q_WS_QWS
  class Konqueror : public ObjectImp {
    friend class KonquerorFunc;
  public:
    Konqueror(KHTMLPart *p) : part(p) { }
    virtual Value get(ExecState *exec, const Identifier &propertyName) const;
    virtual bool hasOwnProperty(ExecState *exec, const Identifier &p) const;
    virtual UString toString(ExecState *exec) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
  private:
    KHTMLPart *part;
  };
#endif

} // namespace

#endif
