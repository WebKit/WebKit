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

#ifndef KJS_PROXY_H
#define KJS_PROXY_H

#include <qvariant.h>

class Frame;
class QString;

namespace KJS {
    class ScriptInterpreter;
}

namespace WebCore {

    class DOMString;
    class EventImpl;
    class EventListener;
    class NodeImpl;

class KJSProxyImpl {
public:
    KJSProxyImpl(Frame*);
    ~KJSProxyImpl();
    QVariant evaluate(const DOMString& filename, int baseLine, const DOMString& code, NodeImpl*);
    void clear();
    EventListener* createHTMLEventHandler(const DOMString& code, NodeImpl*);
#if SVG_SUPPORT
    EventListener* createSVGEventHandler(const DOMString& code, NodeImpl*);
#endif
    void finishedWithEvent(EventImpl*);
    KJS::ScriptInterpreter *interpreter();
    void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }

    void initScript();

private:
    KJS::ScriptInterpreter* m_script;
    Frame *m_frame;
    int m_handlerLineno;
};

}

#endif
