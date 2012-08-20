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

#include "BindingState.h"
#include "Document.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptableDocumentParser.h"
#include "DOMWindow.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "Node.h"
#include "NotImplemented.h"
#include "NPObjectWrapper.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "NPV8Object.h"
#include "PlatformSupport.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include "V8Binding.h"
#include "V8DOMWindow.h"
#include "V8Event.h"
#include "V8HiddenPropertyName.h"
#include "V8HTMLEmbedElement.h"
#include "V8IsolatedContext.h"
#include "V8NPObject.h"
#include "V8Proxy.h"
#include "V8RecursionScope.h"
#include "Widget.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

void ScriptController::initializeThreading()
{
    static bool initializedThreading = false;
    if (!initializedThreading) {
        WTF::initializeThreading();
        WTF::initializeMainThread();
        initializedThreading = true;
    }
}

void ScriptController::setFlags(const char* string, int length)
{
    v8::V8::SetFlagsFromString(string, length);
}

Frame* ScriptController::retrieveFrameForEnteredContext()
{
    return firstFrame(BindingState::instance());
}

Frame* ScriptController::retrieveFrameForCurrentContext()
{
    return currentFrame(BindingState::instance());
}

bool ScriptController::canAccessFromCurrentOrigin(Frame *frame)
{
    return !v8::Context::InContext() || BindingSecurity::shouldAllowAccessToFrame(BindingState::instance(), frame);
}

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_sourceURL(0)
    , m_windowShell(V8DOMWindowShell::create(frame))
    , m_paused(false)
    , m_proxy(adoptPtr(new V8Proxy(frame)))
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_wrappedWindowScriptNPObject(0)
#endif
{
}

ScriptController::~ScriptController()
{
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
    if (m_wrappedWindowScriptNPObject) {
        NPObjectWrapper* windowScriptObjectWrapper = NPObjectWrapper::getWrapper(m_wrappedWindowScriptNPObject);
        ASSERT(windowScriptObjectWrapper);

        NPObject* windowScriptNPObject = NPObjectWrapper::getUnderlyingNPObject(m_wrappedWindowScriptNPObject);
        ASSERT(windowScriptNPObject);
        // Call _NPN_DeallocateObject() instead of _NPN_ReleaseObject() so that we don't leak if a plugin fails to release the window
        // script object properly.
        // This shouldn't cause any problems for plugins since they should have already been stopped and destroyed at this point.
        _NPN_DeallocateObject(windowScriptNPObject);

        // Clear out the wrapped window script object pointer held by the wrapper.
        windowScriptObjectWrapper->clear();
        _NPN_ReleaseObject(m_wrappedWindowScriptNPObject);
        m_wrappedWindowScriptNPObject = 0;
    }
#endif
}

void ScriptController::updateSecurityOrigin()
{
    m_proxy->windowShell()->updateSecurityOrigin();
}

void ScriptController::updatePlatformScriptObjects()
{
    notImplemented();
}

bool ScriptController::processingUserGesture()
{
    return UserGestureIndicator::processingUserGesture();
}

ScriptValue ScriptController::callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> argv[])
{
    // FIXME: This should probably perform the same isPaused check that happens in ScriptController::executeScript.
    return ScriptValue(m_proxy->callFunction(function, receiver, argc, argv));
}

void ScriptController::evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources, Vector<ScriptValue>* results)
{
    evaluateInIsolatedWorld(worldID, sources, 0, results);
}

void ScriptController::evaluateInIsolatedWorld(unsigned worldID, const Vector<ScriptSourceCode>& sources, int extensionGroup, Vector<ScriptValue>* results)
{
    v8::HandleScope handleScope;

    // FIXME: This will need to get reorganized once we have a windowShell for the isolated world.
    if (!windowShell()->initContextIfNeeded())
        return;

    v8::Local<v8::Array> v8Results;
    {
        v8::HandleScope evaluateHandleScope;
        V8IsolatedContext* isolatedContext = 0;
        if (worldID > 0) {
            IsolatedWorldMap::iterator iter = m_proxy->isolatedWorlds().find(worldID);
            if (iter != m_proxy->isolatedWorlds().end())
                isolatedContext = iter->second;
            else {
                isolatedContext = new V8IsolatedContext(proxy(), extensionGroup, worldID);
                if (isolatedContext->context().IsEmpty()) {
                    delete isolatedContext;
                    return;
                }

                // FIXME: We should change this to using window shells to match JSC.
                m_proxy->isolatedWorlds().set(worldID, isolatedContext);
            }

            IsolatedWorldSecurityOriginMap::iterator securityOriginIter = m_proxy->isolatedWorldSecurityOrigins().find(worldID);
            if (securityOriginIter != m_proxy->isolatedWorldSecurityOrigins().end())
                isolatedContext->setSecurityOrigin(securityOriginIter->second);
        } else {
            isolatedContext = new V8IsolatedContext(proxy(), extensionGroup, worldID);
            if (isolatedContext->context().IsEmpty()) {
                delete isolatedContext;
                return;
            }
        }

        v8::Local<v8::Context> context = v8::Local<v8::Context>::New(isolatedContext->context());
        v8::Context::Scope contextScope(context);
        v8::Local<v8::Array> resultArray = v8::Array::New(sources.size());

        for (size_t i = 0; i < sources.size(); ++i) {
            v8::Local<v8::Value> evaluationResult = m_proxy->evaluate(sources[i], 0);
            if (evaluationResult.IsEmpty())
                evaluationResult = v8::Local<v8::Value>::New(v8::Undefined());
            resultArray->Set(i, evaluationResult);
        }

        if (!worldID)
            isolatedContext->destroy();

        v8Results = evaluateHandleScope.Close(resultArray);
    }

    if (results && !v8Results.IsEmpty()) {
        for (size_t i = 0; i < v8Results->Length(); ++i)
            results->append(ScriptValue(v8Results->Get(i)));
    }
}

void ScriptController::setIsolatedWorldSecurityOrigin(int worldID, PassRefPtr<SecurityOrigin> securityOrigin)
{
    ASSERT(worldID);
    m_proxy->isolatedWorldSecurityOrigins().set(worldID, securityOrigin);
    IsolatedWorldMap::iterator iter = m_proxy->isolatedWorlds().find(worldID);
    if (iter != m_proxy->isolatedWorlds().end())
        iter->second->setSecurityOrigin(securityOrigin);
}

// Evaluate a script file in the environment of this proxy.
ScriptValue ScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    String sourceURL = sourceCode.url();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(m_proxy->frame());
    if (v8Context.IsEmpty())
        return ScriptValue();

    v8::Context::Scope scope(v8Context);

    RefPtr<Frame> protect(m_frame);

    v8::Local<v8::Value> object = m_proxy->evaluate(sourceCode, 0);

    m_sourceURL = savedSourceURL;

    if (object.IsEmpty())
        return ScriptValue();

    return ScriptValue(object);
}

TextPosition ScriptController::eventHandlerPosition() const
{
    ScriptableDocumentParser* parser = m_frame->document()->scriptableDocumentParser();
    if (parser)
        return parser->textPosition();
    return TextPosition::minimumPosition();
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

    v8::Persistent<v8::Context> v8Context = v8::Context::New();
    if (v8Context.IsEmpty())
        return;
    {
        v8::Context::Scope scope(v8Context);
        v8::Local<v8::String> source = v8::String::New("if (gc) gc();");
        v8::Local<v8::String> name = v8::String::New("gc");
        v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
        if (!script.IsEmpty()) {
            V8RecursionScope::MicrotaskSuppression scope;
            script->Run();
        }
    }
    v8Context.Dispose();
}

void ScriptController::lowMemoryNotification()
{
    v8::V8::LowMemoryNotification();
}

bool ScriptController::haveInterpreter() const
{
    return m_proxy->windowShell()->isContextInitialized();
}

void ScriptController::enableEval()
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = proxy()->windowShell()->context();
    if (v8Context.IsEmpty())
        return;

    v8Context->AllowCodeGenerationFromStrings(true);
}

void ScriptController::disableEval()
{
    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = proxy()->windowShell()->context();
    if (v8Context.IsEmpty())
        return;

    v8Context->AllowCodeGenerationFromStrings(false);
}

PassScriptInstance ScriptController::createScriptInstanceForWidget(Widget* widget)
{
    ASSERT(widget);

    if (widget->isFrameView())
        return 0;

    NPObject* npObject = PlatformSupport::pluginScriptableObject(widget);

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

void ScriptController::getAllWorlds(Vector<RefPtr<DOMWrapperWorld> >& worlds)
{
    worlds.append(mainThreadNormalWorld());
}

void ScriptController::evaluateInWorld(const ScriptSourceCode& source,
                                       DOMWrapperWorld* world)
{
    Vector<ScriptSourceCode> sources;
    sources.append(source);
    // FIXME: Get an ID from the world param.
    evaluateInIsolatedWorld(0, sources, 0);
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
    DOMWindow* window = frame->document()->domWindow();
    v8::Handle<v8::Value> global = toV8(window);
    ASSERT(global->IsObject());
    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(global), window);
}

NPObject* ScriptController::windowScriptNPObject()
{
    if (m_wrappedWindowScriptNPObject)
        return m_wrappedWindowScriptNPObject;

    NPObject* windowScriptNPObject = 0;
    if (canExecuteScripts(NotAboutToExecuteScript)) {
        // JavaScript is enabled, so there is a JavaScript window object.
        // Return an NPObject bound to the window object.
        windowScriptNPObject = createScriptObject(m_frame);
        _NPN_RegisterObject(windowScriptNPObject, 0);
    } else {
        // JavaScript is not enabled, so we cannot bind the NPObject to the
        // JavaScript window object. Instead, we create an NPObject of a
        // different class, one which is not bound to a JavaScript object.
        windowScriptNPObject = createNoScriptObject();
    }

    m_wrappedWindowScriptNPObject = NPObjectWrapper::create(windowScriptNPObject);
    return m_wrappedWindowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create NPObjects when JavaScript is disabled.
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return createNoScriptObject();

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> v8Context = V8Proxy::mainWorldContext(m_frame);
    if (v8Context.IsEmpty())
        return createNoScriptObject();
    v8::Context::Scope scope(v8Context);

    DOMWindow* window = m_frame->document()->domWindow();
    v8::Handle<v8::Value> v8plugin = toV8(static_cast<HTMLEmbedElement*>(plugin));
    if (!v8plugin->IsObject())
        return createNoScriptObject();

    return npCreateV8ScriptObject(0, v8::Handle<v8::Object>::Cast(v8plugin), window);
}


void ScriptController::clearWindowShell(DOMWindow*, bool)
{
    // V8 binding expects ScriptController::clearWindowShell only be called
    // when a frame is loading a new page. V8Proxy::clearForNavigation
    // creates a new context for the new page.
    m_proxy->clearForNavigation();
}

#if ENABLE(INSPECTOR)
void ScriptController::setCaptureCallStackForUncaughtExceptions(bool value)
{
    v8::V8::SetCaptureStackTraceForUncaughtExceptions(value, ScriptCallStack::maxCallStackSizeToCapture, stackTraceOptions);
}

void ScriptController::collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*> >& result)
{
    v8::HandleScope handleScope;
    for (IsolatedWorldMap::iterator it = m_proxy->isolatedWorlds().begin(); it != m_proxy->isolatedWorlds().end(); ++it) {
        V8IsolatedContext* isolatedContext = it->second;
        if (!isolatedContext->securityOrigin())
            continue;
        v8::Handle<v8::Context> v8Context = isolatedContext->context();
        if (v8Context.IsEmpty())
            continue;
        ScriptState* scriptState = ScriptState::forContext(v8::Local<v8::Context>::New(v8Context));
        result.append(std::pair<ScriptState*, SecurityOrigin*>(scriptState, isolatedContext->securityOrigin()));
    }
}
#endif

void ScriptController::attachDebugger(void*)
{
    notImplemented();
}

void ScriptController::updateDocument()
{
    m_proxy->windowShell()->updateDocument();
}

void ScriptController::namedItemAdded(HTMLDocument* doc, const AtomicString& name)
{
    m_proxy->windowShell()->namedItemAdded(doc, name);
}

void ScriptController::namedItemRemoved(HTMLDocument* doc, const AtomicString& name)
{
    m_proxy->windowShell()->namedItemRemoved(doc, name);
}

} // namespace WebCore
