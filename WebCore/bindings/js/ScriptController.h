/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ScriptController_h
#define ScriptController_h

#include "JSDOMWindowShell.h"
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

class ScriptController {
public:
    ScriptController(Frame*);
    ~ScriptController();

    bool haveWindowShell() const { return m_windowShell; }
    JSDOMWindowShell* windowShell()
    {
        initScriptIfNeeded();
        return m_windowShell;
    }

    JSDOMWindow* globalObject()
    {
        initScriptIfNeeded();
        return m_windowShell->window();
    }

    KJS::JSValue* evaluate(const String& sourceURL, int baseLine, const String& code);
    void clear();
    PassRefPtr<EventListener> createHTMLEventHandler(const String& functionName, const String& code, Node*);
#if ENABLE(SVG)
    PassRefPtr<EventListener> createSVGEventHandler(const String& functionName, const String& code, Node*);
#endif
    void finishedWithEvent(Event*);
    void setEventHandlerLineno(int lineno) { m_handlerLineno = lineno; }

    void setProcessingTimerCallback(bool b) { m_processingTimerCallback = b; }
    bool processingUserGesture() const;

    bool isEnabled();

    void attachDebugger(KJS::Debugger*);

    void setPaused(bool b) { m_paused = b; }
    bool isPaused() const { return m_paused; }

    void clearFormerWindow(JSDOMWindow* window) { m_liveFormerWindows.remove(window); }
    void updateDocument();

    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script

private:
    void initScriptIfNeeded()
    {
        if (!m_windowShell)
            initScript();
    }
    void initScript();

    KJS::ProtectedPtr<JSDOMWindowShell> m_windowShell;
    HashSet<JSDOMWindow*> m_liveFormerWindows;
    Frame* m_frame;
    int m_handlerLineno;
    const String* m_sourceURL;

    bool m_processingTimerCallback;
    bool m_paused;
};

} // namespace WebCore

#endif // ScriptController_h
