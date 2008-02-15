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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef kjs_proxy_h
#define kjs_proxy_h

#include "JSDOMWindow.h"
#include <kjs/protect.h>
#include <wtf/RefPtr.h>

namespace KJS {
    class JSGlobalObject;
    class JSValue;
}

namespace WebCore {

class Event;
class EventListener;
class Frame;
class Node;
class String;

// FIXME: Rename this class to JSController and the Frame function to javaScript().

class KJSProxy {
public:
    KJSProxy(Frame*);
    ~KJSProxy();

    bool haveGlobalObject() const { return m_globalObject; }
    JSDOMWindow* globalObject()
    {
        initScriptIfNeeded();
        return m_globalObject;
    }

    KJS::JSValue* evaluate(const String& filename, int baseLine, const String& code);
    void clear();
    EventListener* createHTMLEventHandler(const String& functionName, const String& code, Node*);
#if ENABLE(SVG)
    EventListener* createSVGEventHandler(const String& functionName, const String& code, Node*);
#endif
    void finishedWithEvent(Event*);
    void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }

    void clearDocumentWrapper();

    void setProcessingTimerCallback(bool b) { m_processingTimerCallback = b; }
    bool processingUserGesture() const;

    bool isEnabled();

private:
    void initScriptIfNeeded()
    {
        if (!m_globalObject)
            initScript();
    }
    void initScript();

    KJS::ProtectedPtr<JSDOMWindow> m_globalObject;
    Frame* m_frame;
    int m_handlerLineno;
    
    bool m_processingTimerCallback;
    bool m_processingInlineCode;
};

}

#endif
