// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
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
#include <qlist.h>
#include <kjs/object.h>
#include <kjs/function.h>

#include "kjs_binding.h"

class QTimer;
class KHTMLView;
class KHTMLPart;

namespace KJS {

  class WindowFunc;
  class WindowQObject;
  class Location;
  class History;
  class FrameArray;
  class JSEventListener;

  class Screen : public ObjectImp {
  public:
    enum {
      height, width, colorDepth, pixelDepth, availLeft, availTop, availHeight,
      availWidth
    };

    Screen() : ObjectImp( UndefClass ) { }
    KJSO get(const UString &p) const;
  private:
    KHTMLView *view;
  };

  class Window : public HostImp {
    friend QGuardedPtr<KHTMLPart> getInstance();
    friend class Location;
    friend class WindowFunc;
    friend class WindowQObject;
  public:
    Window(KHTMLPart *p);
  public:
    ~Window();
    /**
     * Returns and registers a window object. In case there's already a Window
     * for the specified part p this will be returned in order to have unique
     * bindings.
     */
    static Imp *retrieve(KHTMLPart *p);
    static Window *retrieveWindow(KHTMLPart *p);
    /**
     * returns a pointer to the Window object this javascript interpreting instance
     * was called from.
     */
    static Window *retrieveActive();
    QGuardedPtr<KHTMLPart> part() const { return m_part; }
    virtual void mark(Imp *imp = 0L);
    virtual bool hasProperty(const UString &p, bool recursive = true) const;
    virtual KJSO get(const UString &p) const;
    virtual void put(const UString &p, const KJSO& v);
    virtual Boolean toBoolean() const;
    int installTimeout(const UString &handler, int t, bool singleShot);
    void clearTimeout(int timerId);
    void scheduleClose();
    bool isSafeScript() const;
    Location *location() const;
    void setListener(int eventId, KJSO func);
    KJSO getListener(int eventId) const;
    JSEventListener *getJSEventListener(const KJSO &obj, bool html = false);
    void clear();
    virtual String toString() const;

    QList<JSEventListener> jsEventListeners;
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  private:
    QGuardedPtr<KHTMLPart> m_part;
    Screen *screen;
    History *history;
    FrameArray *frames;
    Location *loc;
    WindowQObject *winq;
  };

  class WindowFunc : public DOMFunction {
  public:
    WindowFunc(const Window *w, int i) : window(w), id(i) { };
    Completion tryExecute(const List &);
    enum { Alert, Confirm, Prompt, Open, SetTimeout, ClearTimeout, Focus, Blur, Close,
          MoveBy, MoveTo, ResizeBy, ResizeTo, ScrollBy, ScrollTo, SetInterval, ClearInterval,
          ToString };

  private:
    const Window *window;
    int id;
  };

  class WindowQObject : public QObject {
    Q_OBJECT
  public:
    WindowQObject(Window *w);
    ~WindowQObject();
    int installTimeout(const UString &handler, int t, bool singleShot);
    void clearTimeout(int timerId);
  public slots:
    void timeoutClose();
  protected slots:
    void parentDestroyed();
  protected:
    void timerEvent(QTimerEvent *e);
  private:
    Window *parent;
    KHTMLPart *part;   		// not guarded, may be dangling
    QMap<int, QString> map;
  };

  class Location : public HostImp {
  public:
    virtual KJSO get(const UString &p) const;
    virtual void put(const UString &p, const KJSO &v);
    virtual KJSO toPrimitive(Type preferred) const;
    virtual String toString() const;
    KHTMLPart *part() const { return m_part; }
  private:
    friend class Window;
    Location(KHTMLPart *p);
    QGuardedPtr<KHTMLPart> m_part;
  };

  class LocationFunc : public DOMFunction {
  public:
    LocationFunc(const Location *loc, int i);
    Completion tryExecute(const List &);
    enum { Replace, Reload, ToString };
  private:
    const Location *location;
    int id;
  };

}; // namespace

#endif
