/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
#include "ScriptInstance.h"
#include <runtime/Protect.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class WebScriptObject;
#else
class WebScriptObject;
#endif
#endif

struct NPObject;

namespace JSC {
    class JSGlobalObject;

    namespace Bindings {
        class RootObject;
    }
}

namespace WebCore {

class Event;
class EventListener;
class HTMLPlugInElement;
class Frame;
class Node;
class ScriptSourceCode;
class ScriptValue;
class String;
class Widget;

typedef HashMap<void*, RefPtr<JSC::Bindings::RootObject> > RootObjectMap;

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

    ScriptValue evaluate(const ScriptSourceCode&);

    void setEventHandlerLineNumber(int lineno) { m_handlerLineNumber = lineno; }
    int eventHandlerLineNumber() { return m_handlerLineNumber; }

    void setProcessingTimerCallback(bool b) { m_processingTimerCallback = b; }
    bool processingUserGesture() const;
    bool anyPageIsProcessingUserGesture() const;

    bool isEnabled();

    void attachDebugger(JSC::Debugger*);

    void setPaused(bool b) { m_paused = b; }
    bool isPaused() const { return m_paused; }

    void setAllowPopupsFromPlugin(bool allowPopupsFromPlugin) { m_allowPopupsFromPlugin = allowPopupsFromPlugin; }
    bool allowPopupsFromPlugin() const { return m_allowPopupsFromPlugin; }
    
    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script

    void clearWindowShell();
    void updateDocument();

    // Notifies the ScriptController that the securityOrigin of the current
    // document was modified.  For example, this method is called when
    // document.domain is set.  This method is *not* called when a new document
    // is attached to a frame because updateDocument() is called instead.
    void updateSecurityOrigin();

    void clearScriptObjects();
    void cleanupScriptObjectsForPlugin(void*);

    void updatePlatformScriptObjects();

    PassScriptInstance createScriptInstanceForWidget(Widget*);
    JSC::Bindings::RootObject* bindingRootObject();

    PassRefPtr<JSC::Bindings::RootObject> createRootObject(void* nativeHandle);

#if PLATFORM(MAC)
#if ENABLE(MAC_JAVA_BRIDGE)
    static void initJavaJSBindings();
#endif
    WebScriptObject* windowScriptObject();
#endif

    JSC::JSObject* jsObjectForPluginElement(HTMLPlugInElement*);
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* createScriptObjectForPluginElement(HTMLPlugInElement*);
    NPObject* windowScriptNPObject();
#endif

private:
    void initScriptIfNeeded()
    {
        if (!m_windowShell)
            initScript();
    }
    void initScript();

    void disconnectPlatformScriptObjects();

    bool processingUserGestureEvent() const;
    bool isJavaScriptAnchorNavigation() const;

    JSC::ProtectedPtr<JSDOMWindowShell> m_windowShell;
    Frame* m_frame;
    int m_handlerLineNumber;
    const String* m_sourceURL;

    bool m_processingTimerCallback;
    bool m_paused;
    bool m_allowPopupsFromPlugin;

    // The root object used for objects bound outside the context of a plugin.
    RefPtr<JSC::Bindings::RootObject> m_bindingRootObject;
    RootObjectMap m_rootObjects;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_windowScriptNPObject;
#endif
#if PLATFORM(MAC)
    RetainPtr<WebScriptObject> m_windowScriptObject;
#endif
};

} // namespace WebCore

#endif // ScriptController_h
