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

#include "ScriptControllerBase.h"
#include "ScriptInstance.h"
#include "ScriptValue.h"

#include "V8Proxy.h"

#include <v8.h>

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

struct NPObject;

namespace WebCore {

class DOMWrapperWorld;
class Event;
class Frame;
class HTMLDocument;
class HTMLPlugInElement;
class PagePopupClient;
class ScriptSourceCode;
class ScriptState;
class V8DOMWindowShell;
class Widget;

typedef WTF::Vector<v8::Extension*> V8Extensions;

class ScriptController {
public:
    ScriptController(Frame*);
    ~ScriptController();

    // FIXME: V8Proxy should either be folded into ScriptController
    // or this accessor should be made JSProxy*
    V8Proxy* proxy() { return m_proxy.get(); }
    V8DOMWindowShell* windowShell() const { return m_windowShell.get(); }

    ScriptValue executeScript(const ScriptSourceCode&);
    ScriptValue executeScript(const String& script, bool forceUserGesture = false);

    // Call the function with the given receiver and arguments.
    v8::Local<v8::Value> callFunction(v8::Handle<v8::Function>, v8::Handle<v8::Object>, int argc, v8::Handle<v8::Value> argv[]);

    // Call the function with the given receiver and arguments and report times to DevTools.
    static v8::Local<v8::Value> callFunctionWithInstrumentation(ScriptExecutionContext*, v8::Handle<v8::Function>, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[]);

    ScriptValue callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function>, v8::Handle<v8::Object>, int argc, v8::Handle<v8::Value> argv[]);

    // Returns true if argument is a JavaScript URL.
    bool executeIfJavaScriptURL(const KURL&, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL = ReplaceDocumentIfJavaScriptURL);

    // This function must be called from the main thread. It is safe to call it repeatedly.
    static void initializeThreading();

    // Evaluate a script file in the environment of this proxy.
    ScriptValue evaluate(const ScriptSourceCode&);

    // Evaluate JavaScript in a new isolated world. The script gets its own
    // global scope, its own prototypes for intrinsic JavaScript objects (String,
    // Array, and so-on), and its own wrappers for all DOM nodes and DOM
    // constructors.
    void evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources, Vector<ScriptValue>* results);

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
    void evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup, Vector<ScriptValue>* results);

    // Associates an isolated world (see above for description) with a security
    // origin. XMLHttpRequest instances used in that world will be considered
    // to come from that origin, not the frame's.
    void setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin>);

    // Masquerade 'this' as the windowShell.
    // This is a bit of a hack, but provides reasonable compatibility
    // with what JSC does as well.
    ScriptController* windowShell(DOMWrapperWorld*) { return this; }
    ScriptController* existingWindowShell(DOMWrapperWorld*) { return this; }

    // Creates a property of the global object of a frame.
    void bindToWindowObject(Frame*, const String& key, NPObject*);

    PassScriptInstance createScriptInstanceForWidget(Widget*);

    // Check if the javascript engine has been initialized.
    bool haveInterpreter() const;

    void enableEval();
    void disableEval();

    static bool canAccessFromCurrentOrigin(Frame*);

#if ENABLE(INSPECTOR)
    static void setCaptureCallStackForUncaughtExceptions(bool);
    void collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*> >&);
#endif

    bool canExecuteScripts(ReasonForCallingCanExecuteScripts);

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

    // Returns V8 Context of a frame. If none exists, creates
    // a new context. It is potentially slow and consumes memory.
    static v8::Local<v8::Context> mainWorldContext(Frame*);
    v8::Local<v8::Context> mainWorldContext();

    // Pass command-line flags to the JS engine.
    static void setFlags(const char* string, int length);

    void finishedWithEvent(Event*);

    TextPosition eventHandlerPosition() const;

    static bool processingUserGesture();

    void setPaused(bool paused) { m_paused = paused; }
    bool isPaused() const { return m_paused; }

    const String* sourceURL() const { return m_sourceURL; } // 0 if we are not evaluating any script.

    void clearWindowShell(DOMWindow*, bool);
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&);
    void namedItemRemoved(HTMLDocument*, const AtomicString&);

    void updateSecurityOrigin();
    void clearScriptObjects();
    void updatePlatformScriptObjects();
    void cleanupScriptObjectsForPlugin(Widget*);

    void clearForNavigation();
    void clearForClose();

    NPObject* createScriptObjectForPluginElement(HTMLPlugInElement*);
    NPObject* windowScriptNPObject();

    // Dummy method to avoid a bunch of ifdef's in WebCore.
    void evaluateInWorld(const ScriptSourceCode&, DOMWrapperWorld*);
    static void getAllWorlds(Vector<RefPtr<DOMWrapperWorld> >& worlds);

    // Registers a v8 extension to be available on webpages. Will only
    // affect v8 contexts initialized after this call. Takes ownership of
    // the v8::Extension object passed.
    static void registerExtensionIfNeeded(v8::Extension*);
    static V8Extensions& registeredExtensions();

    bool setContextDebugId(int);
    static int contextDebugId(v8::Handle<v8::Context>);

private:
    void resetIsolatedWorlds();

    Frame* m_frame;
    const String* m_sourceURL;

    // For the moment, we have one of these. Soon we will have one per DOMWrapperWorld.
    RefPtr<V8DOMWindowShell> m_windowShell;

    bool m_paused;

    OwnPtr<V8Proxy> m_proxy;
    typedef HashMap<Widget*, NPObject*> PluginObjectMap;

    // A mapping between Widgets and their corresponding script object.
    // This list is used so that when the plugin dies, we can immediately
    // invalidate all sub-objects which are associated with that plugin.
    // The frame keeps a NPObject reference for each item on the list.
    PluginObjectMap m_pluginObjects;
    // The window script object can get destroyed while there are outstanding
    // references to it. Please refer to ScriptController::clearScriptObjects
    // for more information as to why this is necessary. To avoid crashes due
    // to calls on the destroyed window object, we return a proxy NPObject
    // which wraps the underlying window object. The wrapped window object
    // pointer in this object is cleared out when the window object is
    // destroyed.
    NPObject* m_wrappedWindowScriptNPObject;

    // All of the extensions registered with the context.
    static V8Extensions m_extensions;
};

} // namespace WebCore

#endif // ScriptController_h
