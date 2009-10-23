/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScriptController.h"

#include "ChromiumBridge.h"
#include "CString.h"
#include "Document.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "Node.h"
#include "NotImplemented.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "NPV8Object.h"
#include "ScriptSourceCode.h"
#include "Widget.h"
#include "XSSAuditor.h"

#include "V8Binding.h"
#include "V8NPObject.h"
#include "V8Proxy.h"

namespace WebCore {

void ScriptController::setFlags(const char* string, int length)
{
    v8::V8::SetFlagsFromString(string, length);
}

Frame* ScriptController::retrieveFrameForEnteredContext()
{
    return V8Proxy::retrieveFrameForEnteredContext();
}

Frame* ScriptController::retrieveFrameForCurrentContext()
{
    return V8Proxy::retrieveFrameForCurrentContext();
}

bool ScriptController::isSafeScript(Frame* target)
{
    return V8Proxy::canAccessFrame(target, true);
}

void ScriptController::gcProtectJSWrapper(void* domObject)
{
    V8GCController::gcProtect(domObject);
}

void ScriptController::gcUnprotectJSWrapper(void* domObject)
{
    V8GCController::gcUnprotect(domObject);
}

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_sourceURL(0)
    , m_inExecuteScript(false)
    , m_processingTimerCallback(false)
    , m_paused(false)
    , m_proxy(new V8Proxy(frame))
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_windowScriptNPObject(0)
#endif
    , m_XSSAuditor(new XSSAuditor(frame))
{
}

ScriptController::~ScriptController()
{
    m_proxy->disconnectFrame();
}

void ScriptController::clearScriptObjects()
{
    PluginObjectMap::iterator it = m_pluginObjects.begin();
    for (; it != m_pluginObjects.end(); ++it) {
        _NPN_UnregisterObject(it->second);
        _NPN_ReleaseObject(it->second);
    }
    m_pluginObjects.clear();

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (m_windowScriptNPObject) {
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(m_windowScriptNPObject);
        m_windowScriptNPObject = 0;
    }
#endif
}

void ScriptController::updateSecurityOrigin()
{
    m_proxy->updateSecurityOrigin();
}

void ScriptController::updatePlatformScriptObjects()
{
    notImplemented();
}

bool ScriptController::processingUserGesture() const
{
    Frame* activeFrame = V8Proxy::retrieveFrameForEnteredContext();
    // No script is running, so it must be run by users.
    if (!activeFrame)
        return true;

    V8Proxy* activeProxy = activeFrame->script()->proxy();

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(activeFrame);
    // FIXME: find all cases context can be empty:
    //  1) JS is disabled;
    //  2) page is NULL;
    if (v8Context.IsEmpty())
        return true;

    v8::Context::Scope scope(v8Context);

    v8::Handle<v8::Object> global = v8Context->Global();
    v8::Handle<v8::Value> jsEvent = global->Get(v8::String::NewSymbol("event"));
    Event* event = V8DOMWrapper::convertToNativeEvent(jsEvent);

    // Based on code from kjs_bindings.cpp.
    // Note: This is more liberal than Firefox's implementation.
    if (event) {
        if (event->createdByDOM())
            return false;

        const AtomicString& type = event->type();
        bool eventOk =
            // mouse events
            type == eventNames().clickEvent || type == eventNames().mousedownEvent || type == eventNames().mouseupEvent || type == eventNames().dblclickEvent
            // keyboard events
            || type == eventNames().keydownEvent || type == eventNames().keypressEvent || type == eventNames().keyupEvent
            // other accepted events
            || type == eventNames().selectEvent || type == eventNames().changeEvent || type == eventNames().focusEvent || type == eventNames().blurEvent || type == eventNames().submitEvent;

        if (eventOk)
            return true;
    } else if (activeProxy->inlineCode() && !activeProxy->timerCallback()) {
        // This is the <a href="javascript:window.open('...')> case -> we let it through.
        return true;
    }

    // This is the <script>window.open(...)</script> case or a timer callback -> block it.
    return false;
}

void ScriptController::evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources)
{
    m_proxy->evaluateInIsolatedWorld(worldID, sources, 0);
}

void ScriptController::evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    m_proxy->evaluateInIsolatedWorld(worldID, sources, extensionGroup);
}

void ScriptController::evaluateInNewContext(const Vector<ScriptSourceCode>& sources, int extensionGroup)
{
    m_proxy->evaluateInNewContext(sources, extensionGroup);
}

// Evaluate a script file in the environment of this proxy.
ScriptValue ScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    String sourceURL = sourceCode.url();
    
    if (!m_XSSAuditor->canEvaluate(sourceCode.source())) {
        // This script is not safe to be evaluated.
        return ScriptValue();
    }

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(m_proxy->frame());
    if (v8Context.IsEmpty())
        return ScriptValue();

    v8::Context::Scope scope(v8Context);

    RefPtr<Frame> protect(m_frame);

    v8::Local<v8::Value> object = m_proxy->evaluate(sourceCode, 0);

    // Evaluating the JavaScript could cause the frame to be deallocated
    // so we start the keep alive timer here.
    m_frame->keepAlive();

    if (object.IsEmpty() || object->IsUndefined())
        return ScriptValue();

    return ScriptValue(object);
}

void ScriptController::setEventHandlerLineNumber(int lineNumber)
{
    m_proxy->setEventHandlerLineNumber(lineNumber);
}

void ScriptController::finishedWithEvent(Event* event)
{
    m_proxy->finishedWithEvent(event);
}

// Create a V8 object with an interceptor of NPObjectPropertyGetter.
void ScriptController::bindToWindowObject(Frame* frame, const String& key, NPObject* object)
{
    v8::HandleScope handleScope;

    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(frame);
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    v8::Handle<v8::Object> value = createV8ObjectForNPObject(object, 0);

    // Attach to the global object.
    v8::Handle<v8::Object> global = v8Context->Global();
    global->Set(v8String(key), value);
}

void ScriptController::collectGarbage()
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(m_proxy->frame());
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    m_proxy->evaluate(ScriptSourceCode("if (window.gc) void(gc());"), 0);
}

void ScriptController::lowMemoryNotification()
{
    v8::V8::LowMemoryNotification();
}

bool ScriptController::haveInterpreter() const
{
    return m_proxy->isContextInitialized();
}

bool ScriptController::isEnabled() const
{
    return m_proxy->isEnabled();
}

PassScriptInstance ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    ASSERT(widget);

    if (widget->isFrameView())
        return 0;

    NPObject* npObject = ChromiumBridge::pluginScriptableObject(widget);
    if (!npObject)
        return 0;

    // Frame Memory Management for NPObjects
    // -------------------------------------
    // NPObjects are treated differently than other objects wrapped by JS.
    // NPObjects can be created either by the browser (e.g. the main
    // window object) or by the plugin (the main plugin object
    // for a HTMLEmbedElement). Further, unlike most DOM Objects, the frame
    // is especially careful to ensure NPObjects terminate at frame teardown because
    // if a plugin leaks a reference, it could leak its objects (or the browser's objects).
    //
    // The Frame maintains a list of plugin objects (m_pluginObjects)
    // which it can use to quickly find the wrapped embed object.
    //
    // Inside the NPRuntime, we've added a few methods for registering
    // wrapped NPObjects. The purpose of the registration is because
    // javascript garbage collection is non-deterministic, yet we need to
    // be able to tear down the plugin objects immediately. When an object
    // is registered, javascript can use it. When the object is destroyed,
    // or when the object's "owning" object is destroyed, the object will
    // be un-registered, and the javascript engine must not use it.
    //
    // Inside the javascript engine, the engine can keep a reference to the
    // NPObject as part of its wrapper. However, before accessing the object
    // it must consult the _NPN_Registry.

    v8::Local<v8::Object> wrapper = createV8ObjectForNPObject(npObject, 0);

    // Track the plugin object. We've been given a reference to the object.
    m_pluginObjects.set(widget, npObject);

    return V8ScriptInstance::create(wrapper);
}

void ScriptController::cleanupScriptObjectsForPlugin(Widget* nativeHandle)
{
    PluginObjectMap::iterator it = m_pluginObjects.find(nativeHandle);
    if (it == m_pluginObjects.end())
        return;
    _NPN_UnregisterObject(it->second);
    _NPN_ReleaseObject(it->second);
    m_pluginObjects.remove(it);
}

static NPObject* createNoScriptObject()
{
    notImplemented();
    return 0;
}

static NPObject* createScriptObject(Frame* frame)
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(frame);
    if (v8Context.IsEmpty())
        return createNoScriptObject();

    v8::Context::Scope scope(v8Context);
    DOMWindow* window = frame->domWindow();
    v8::Handle<v8::Value> global = V8DOMWrapper::convertToV8Object(V8ClassIndex::DOMWINDOW, window);
    ASSERT(global->IsObject());
    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(global), window);
}

NPObject* ScriptController::windowScriptNPObject()
{
    if (m_windowScriptNPObject)
        return m_windowScriptNPObject;

    if (isEnabled()) {
        // JavaScript is enabled, so there is a JavaScript window object.
        // Return an NPObject bound to the window object.
        m_windowScriptNPObject = createScriptObject(m_frame);
        _NPN_RegisterObject(m_windowScriptNPObject, 0);
    } else {
        // JavaScript is not enabled, so we cannot bind the NPObject to the
        // JavaScript window object. Instead, we create an NPObject of a
        // different class, one which is not bound to a JavaScript object.
        m_windowScriptNPObject = createNoScriptObject();
    }
    return m_windowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create NPObjects when JavaScript is disabled.
    if (!isEnabled())
        return createNoScriptObject();

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(m_frame);
    if (v8Context.IsEmpty())
        return createNoScriptObject();
    v8::Context::Scope scope(v8Context);

    DOMWindow* window = m_frame->domWindow();
    v8::Handle<v8::Value> v8plugin = V8DOMWrapper::convertToV8Object(V8ClassIndex::HTMLEMBEDELEMENT, plugin);
    if (!v8plugin->IsObject())
        return createNoScriptObject();

    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(v8plugin), window);
}


void ScriptController::clearWindowShell()
{
    // V8 binding expects ScriptController::clearWindowShell only be called
    // when a frame is loading a new page. V8Proxy::clearForNavigation
    // creates a new context for the new page.
    m_proxy->clearForNavigation();
}

void ScriptController::attachDebugger(void*)
{
    notImplemented();
}

void ScriptController::updateDocument()
{
    m_proxy->updateDocument();
}

// FIXME: Stub method so we compile.  Currently called from FrameLoader.cpp.
DOMWrapperWorld* mainThreadNormalWorld()
{
    return 0;
}

} // namespace WebCore
