// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef _KJS_PROXY_H_
#define _KJS_PROXY_H_

#include <qvariant.h>
#include <qstring.h>

class KHTMLPart;
class KJSDebugWin;

namespace DOM {
  class EventImpl;
  class EventListener;
  class NodeImpl;
};

namespace KJS {
  class List;
  class Interpreter;
}

/**
 * @internal
 *
 * @short Proxy class serving as interface when being dlopen'ed.
 */
class KJSProxy {
public:
  KJSProxy() { m_handlerLineno = 0; }
  virtual ~KJSProxy() { }
  virtual QVariant evaluate(QString filename, int baseLine, const QString &, DOM::NodeImpl *n) = 0;
  virtual void clear() = 0;
  virtual DOM::EventListener *createHTMLEventHandler(QString sourceUrl, QString code, DOM::NodeImpl *node) = 0;
  virtual void finishedWithEvent(DOM::EventImpl *event) = 0;
  virtual KJS::Interpreter *interpreter() = 0;

  virtual void setDebugEnabled(bool enabled) = 0;
  virtual bool paused() const = 0;
  virtual void setSourceFile(QString url, QString code) = 0;
  virtual void appendSourceFile(QString url, QString code) = 0;

  void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }

  KHTMLPart *m_part;
  int m_handlerLineno;

  // Helper method, to access the private KHTMLPart::jScript()
  static KJSProxy *proxy( KHTMLPart *part );
};

#endif
