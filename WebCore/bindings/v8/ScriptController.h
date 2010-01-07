/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptController_h
#define ScriptController_h

#include "ScriptInstance.h"
#include "ScriptValue.h"

#include "V8Proxy.h"

#include <v8.h>

#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMWrapperWorld;
class Event;
class Frame;
class HTMLPlugInElement;
class ScriptSourceCode;
class ScriptState;
class String;
class V8DOMWindow;
class Widget;
class XSSAuditor;

class ScriptController {
    typedef WTF::HashMap< RefPtr<DOMWrapperWorld>, RefPtr<V8DOMWindowShell> > ShellMap;

public:
    ScriptController(Frame*);
    ~ScriptController();

    static PassRefPtr<DOMWrapperWorld> createWorld();

    V8DOMWindowShell* windowShell(DOMWrapperWorld* world)
    {
        ShellMap::iterator iter = m_windowShells.find(world);
        return (iter != m_windowShells.end()) ? iter->second.get() : initScript(world);
    }
    V8DOMWindowShell* existingWindowShell(DOMWrapperWorld* world) const
    {
        ShellMap::const_iterator iter = m_windowShells.find(world);
        return (iter != m_windowShells.end()) ? iter->second.get() : 0;
    }
    V8DOMWindow* globalObject(DOMWrapperWorld* world)
    {
        notImplemented();
        return 0;
    }

    static void getAllWorlds(Vector<DOMWrapperWorld*>&);

    ScriptValue executeScript(const ScriptSourceCode&);
    ScriptValue executeScript(const String& script, bool forceUserGesture = false);
    ScriptValue executeScriptInWorld(DOMWrapperWorld* world, const String& script, bool forceUserGesture = false);

    // Returns true if argument is a JavaScript URL.
    bool executeIfJavaScriptURL(const KURL&, bool userGesture = false, bool replaceDocument = true);

    // This function must be called from the main thread. It is safe to call it repeatedly.
    static void initializeThreading();

    ScriptValue evaluate(const ScriptSourceCode&);
    ScriptValue evaluateInWorld(const ScriptSourceCode&, DOMWrapperWorld*);

    // ==== End identical match with JSC's ScriptController === //

    // FIXME: V8Proxy should either be folded into ScriptController
    // or this accessor should be made JSProxy*
    V8Proxy* proxy() { return m_proxy.get(); }

    // FIXME: We should eventually remove all clients of this method. The
    // problem is that some of them are in very hot code paths.
    V8DOMWindowShell* mainWorldWindowShell();

    void evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>&);

    // Executes JavaScript in an isolated world. The script gets its own global scope,
    // its own prototypes for intrinsic JavaScript objects (String, Array, and so-on),
    // and its own wrappers for all DOM nodes and DOM constructors.
    //
    // If an isolated world with the specified ID already exists, it is reused.
    // Otherwise, a new world is created.
    //
    // If the worldID is 0, a new world is always created.
    //
    // FIXME: Get rid of extensionGroup here.
    void evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>&, int extensionGroup);

    XSSAuditor* xssAuditor() { return m_XSSAuditor.get(); }

    void collectGarbage();

    // Notify V8 that the system is running low on memory.
    void lowMemoryNotification();

    // Creates a property of the global object of a frame.
    void bindToWindowObject(Frame*, const String& key, NPObject*);

    PassScriptInstance createScriptInstanceForWidget(Widget*);

    bool isEnabled() const;

    // FIXME: void* is a compile hack.
    void attachDebugger(void*);

    // --- Static methods assume we are running VM in single thread, ---
    // --- and there is only one VM instance.                        ---

    // Returns the frame for the entered context. See comments in
    // V8Proxy::retrieveFrameForEnteredContext() for more information.
    static Frame* retrieveFrameForEnteredContext();

    // Returns the frame for the current context. See comments in
    // V8Proxy::retrieveFrameForEnteredContext() for more information.
    static Frame* retrieveFrameForCurrentContext();

    // Check whether it is safe to access a frame in another domain.
    static bool isSafeScript(Frame*);

    // Pass command-line flags to the JS engine.
    static void setFlags(const char* string, int length);

    // Protect and unprotect the JS wrapper from garbage collected.
    static void gcProtectJSWrapper(void*);
    static void gcUnprotectJSWrapper(void*);

    void finishedWithEvent(Event*);
    void setEventHandlerLineNumber(int lineNumber);

    void setProcessingTimerCallback(bool processingTimerCallback) { m_processingTimerCallback = processingTimerCallback; }
    bool processingUserGesture() const;
    bool anyPageIsProcessingUserGesture() const;

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }

    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script.

    void clearWindowShell();
    void updateDocument();

    void clearForClose();

    // This is very destructive (e.g., out of memory).
    void destroyWindowShell();

    void updateSecurityOrigin();
    void clearScriptObjects();
    void updatePlatformScriptObjects();
    void cleanupScriptObjectsForPlugin(Widget*);

#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* createScriptObjectForPluginElement(HTMLPlugInElement*);
    NPObject* windowScriptNPObject();
#endif

    // Script state for the main world context.
    ScriptState* mainWorldScriptState();

    // Returns ScriptState for current context.
    static ScriptState* currentScriptState();

private:
    V8DOMWindowShell* initScript(DOMWrapperWorld*);

    ShellMap m_windowShells;
    Frame* m_frame;

    // This is a cache of the main world's windowShell.  We have this
    // because we need access to it during some wrapper operations that
    // are performance sensitive.  Those call sites are wrong, but I'm
    // waiting to remove them until the next patch.
    RefPtr<V8DOMWindowShell> m_mainWorldWindowShell;

    const String* m_sourceURL;

    bool m_inExecuteScript;

    bool m_processingTimerCallback;
    bool m_paused;

    // FIXME: V8Proxy should eventually be removed.
    OwnPtr<V8Proxy> m_proxy;

    typedef HashMap<Widget*, NPObject*> PluginObjectMap;

    // A mapping between Widgets and their corresponding script object.
    // This list is used so that when the plugin dies, we can immediately
    // invalidate all sub-objects which are associated with that plugin.
    // The frame keeps a NPObject reference for each item on the list.
    PluginObjectMap m_pluginObjects;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_windowScriptNPObject;
#endif
    // The XSSAuditor associated with this ScriptController.
    OwnPtr<XSSAuditor> m_XSSAuditor;

    // Script state for the main world context.
    OwnPtr<ScriptState> m_mainWorldScriptState;
};

} // namespace WebCore

#endif // ScriptController_h
