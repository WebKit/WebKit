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

namespace DOM {
  class EventImpl;
  class EventListener;
  class NodeImpl;
};

namespace KJS {
  class List;
  class ScriptInterpreter;
}

class KJSProxyImpl {
public:
    KJSProxyImpl(KHTMLPart *part);
    ~KJSProxyImpl();
    QVariant evaluate(QString filename, int baseLine, const QString& str, DOM::NodeImpl *n);
    void clear();
    DOM::EventListener *createHTMLEventHandler(QString code, DOM::NodeImpl *node);
    void finishedWithEvent(DOM::EventImpl *event);
    KJS::ScriptInterpreter *interpreter();
    void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }

    void initScript();

private:
    KJS::ScriptInterpreter* m_script;
    KHTMLPart *m_part;
    int m_handlerLineno;
#ifndef NDEBUG
    static int s_count;
#endif
};


#endif
