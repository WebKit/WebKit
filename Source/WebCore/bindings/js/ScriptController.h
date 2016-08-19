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

#include "FrameLoaderTypes.h"
#include "JSDOMWindowShell.h"
#include <JavaScriptCore/JSBase.h>
#include <heap/Strong.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/text/TextPosition.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
OBJC_CLASS JSContext;
OBJC_CLASS WebScriptObject;
#endif

struct NPObject;

namespace Deprecated {
class ScriptValue;
}

namespace JSC {
class JSGlobalObject;
class ExecState;

namespace Bindings {
class Instance;
class RootObject;
}
}

namespace WebCore {

class Frame;
class HTMLDocument;
class HTMLPlugInElement;
class ScriptSourceCode;
class SecurityOrigin;
class Widget;

typedef HashMap<void*, RefPtr<JSC::Bindings::RootObject>> RootObjectMap;

enum ReasonForCallingCanExecuteScripts {
    AboutToExecuteScript,
    NotAboutToExecuteScript
};

class ScriptController {
    WTF_MAKE_FAST_ALLOCATED;

    typedef HashMap<RefPtr<DOMWrapperWorld>, JSC::Strong<JSDOMWindowShell>> ShellMap;

public:
    explicit ScriptController(Frame&);
    ~ScriptController();

    WEBCORE_EXPORT static Ref<DOMWrapperWorld> createWorld();

    JSDOMWindowShell& createWindowShell(DOMWrapperWorld&);
    void destroyWindowShell(DOMWrapperWorld&);

    Vector<JSC::Strong<JSDOMWindowShell>> windowShells();

    JSDOMWindowShell* windowShell(DOMWrapperWorld& world)
    {
        ShellMap::iterator iter = m_windowShells.find(&world);
        return (iter != m_windowShells.end()) ? iter->value.get() : initScript(world);
    }
    JSDOMWindowShell* existingWindowShell(DOMWrapperWorld& world) const
    {
        ShellMap::const_iterator iter = m_windowShells.find(&world);
        return (iter != m_windowShells.end()) ? iter->value.get() : 0;
    }
    JSDOMWindow* globalObject(DOMWrapperWorld& world)
    {
        return windowShell(world)->window();
    }

    static void getAllWorlds(Vector<Ref<DOMWrapperWorld>>&);

    JSC::JSValue executeScript(const ScriptSourceCode&, ExceptionDetails* = nullptr);
    WEBCORE_EXPORT JSC::JSValue executeScript(const String& script, bool forceUserGesture = false, ExceptionDetails* = nullptr);
    WEBCORE_EXPORT JSC::JSValue executeScriptInWorld(DOMWrapperWorld&, const String& script, bool forceUserGesture = false);

    // Returns true if argument is a JavaScript URL.
    bool executeIfJavaScriptURL(const URL&, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL = ReplaceDocumentIfJavaScriptURL);

    // This function must be called from the main thread. It is safe to call it repeatedly.
    // Darwin is an exception to this rule: it is OK to call this function from any thread, even reentrantly.
    static void initializeThreading();

    JSC::JSValue evaluate(const ScriptSourceCode&, ExceptionDetails* = nullptr);
    JSC::JSValue evaluateInWorld(const ScriptSourceCode&, DOMWrapperWorld&, ExceptionDetails* = nullptr);

    WTF::TextPosition eventHandlerPosition() const;

    void enableEval();
    void disableEval(const String& errorMessage);

    WEBCORE_EXPORT static bool processingUserGesture();
    WEBCORE_EXPORT static bool processingUserGestureForMedia();

    static bool canAccessFromCurrentOrigin(Frame*);
    WEBCORE_EXPORT bool canExecuteScripts(ReasonForCallingCanExecuteScripts);

    // Debugger can be 0 to detach any existing Debugger.
    void attachDebugger(JSC::Debugger*); // Attaches/detaches in all worlds/window shells.
    void attachDebugger(JSDOMWindowShell*, JSC::Debugger*);

    void setPaused(bool b) { m_paused = b; }
    bool isPaused() const { return m_paused; }

    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script

    void clearWindowShell(DOMWindow* newDOMWindow, bool goingIntoPageCache);
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&) { }
    void namedItemRemoved(HTMLDocument*, const AtomicString&) { }

    void clearScriptObjects();
    WEBCORE_EXPORT void cleanupScriptObjectsForPlugin(void*);

    void updatePlatformScriptObjects();

    RefPtr<JSC::Bindings::Instance>  createScriptInstanceForWidget(Widget*);
    WEBCORE_EXPORT JSC::Bindings::RootObject* bindingRootObject();
    JSC::Bindings::RootObject* cacheableBindingRootObject();

    WEBCORE_EXPORT RefPtr<JSC::Bindings::RootObject> createRootObject(void* nativeHandle);

    void collectIsolatedContexts(Vector<std::pair<JSC::ExecState*, SecurityOrigin*>>&);

#if PLATFORM(COCOA)
    WEBCORE_EXPORT WebScriptObject* windowScriptObject();
    WEBCORE_EXPORT JSContext *javaScriptContext();
#endif

    WEBCORE_EXPORT JSC::JSObject* jsObjectForPluginElement(HTMLPlugInElement*);
    
#if ENABLE(NETSCAPE_PLUGIN_API)
    WEBCORE_EXPORT NPObject* windowScriptNPObject();
#endif

private:
    WEBCORE_EXPORT JSDOMWindowShell* initScript(DOMWrapperWorld&);

    void disconnectPlatformScriptObjects();

    ShellMap m_windowShells;
    Frame& m_frame;
    const String* m_sourceURL;

    bool m_paused;

    // The root object used for objects bound outside the context of a plugin, such
    // as NPAPI plugins. The plugins using these objects prevent a page from being cached so they
    // are safe to invalidate() when WebKit navigates away from the page that contains them.
    RefPtr<JSC::Bindings::RootObject> m_bindingRootObject;
    // Unlike m_bindingRootObject these objects are used in pages that are cached, so they are not invalidate()'d.
    // This ensures they are still available when the page is restored.
    RefPtr<JSC::Bindings::RootObject> m_cacheableBindingRootObject;
    RootObjectMap m_rootObjects;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_windowScriptNPObject;
#endif
#if PLATFORM(COCOA)
    RetainPtr<WebScriptObject> m_windowScriptObject;
#endif
};

} // namespace WebCore

#endif // ScriptController_h
