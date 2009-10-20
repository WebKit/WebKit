/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScriptController.h"

#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "GCController.h"
#include "HTMLPlugInElement.h"
#include "JSDocument.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "Settings.h"
#include "StorageNamespace.h"
#include "XSSAuditor.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <debugger/Debugger.h>
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

ScriptController::ScriptController(Frame* frame)
    : m_frame(frame)
    , m_handlerLineNumber(0)
    , m_sourceURL(0)
    , m_inExecuteScript(false)
    , m_processingTimerCallback(false)
    , m_paused(false)
    , m_allowPopupsFromPlugin(false)
#if ENABLE(NETSCAPE_PLUGIN_API)
    , m_windowScriptNPObject(0)
#endif
#if PLATFORM(MAC)
    , m_windowScriptObject(0)
#endif
    , m_XSSAuditor(new XSSAuditor(frame))
{
#if PLATFORM(MAC) && ENABLE(MAC_JAVA_BRIDGE)
    static bool initializedJavaJSBindings;
    if (!initializedJavaJSBindings) {
        initializedJavaJSBindings = true;
        initJavaJSBindings();
    }
#endif
}

ScriptController::~ScriptController()
{
    if (m_windowShell) {
        m_windowShell = 0;
    
        // It's likely that releasing the global object has created a lot of garbage.
        gcController().garbageCollectSoon();
    }

    disconnectPlatformScriptObjects();
}

ScriptValue ScriptController::evaluate(const ScriptSourceCode& sourceCode) 
{
    const SourceCode& jsSourceCode = sourceCode.jsSourceCode();
    String sourceURL = jsSourceCode.provider()->url();

    if (!m_XSSAuditor->canEvaluate(sourceCode.source())) {
        // This script is not safe to be evaluated.
        return JSValue();
    }

    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occured or the type couldn't be converted.

    initScriptIfNeeded();
    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    ExecState* exec = m_windowShell->window()->globalExec();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    JSLock lock(SilenceAssertionsOnly);

    RefPtr<Frame> protect = m_frame;

    m_windowShell->window()->globalData()->timeoutChecker.start();
    Completion comp = JSC::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), jsSourceCode, m_windowShell);
    m_windowShell->window()->globalData()->timeoutChecker.stop();

    // Evaluating the JavaScript could cause the frame to be deallocated
    // so we start the keep alive timer here.
    m_frame->keepAlive();

    if (comp.complType() == Normal || comp.complType() == ReturnValue) {
        m_sourceURL = savedSourceURL;
        return comp.value();
    }

    if (comp.complType() == Throw || comp.complType() == Interrupted)
        reportException(exec, comp.value());

    m_sourceURL = savedSourceURL;
    return JSValue();
}

void ScriptController::evaluateInIsolatedWorld(unsigned /* worldID */, const Vector<ScriptSourceCode>& sourceCode) 
{
    // FIXME: Actually support isolated worlds!
    unsigned size = sourceCode.size();
    for (unsigned i = 0; i < size; ++i)
        evaluate(sourceCode[i]);
}

void ScriptController::clearWindowShell()
{
    if (!m_windowShell)
        return;

    JSLock lock(SilenceAssertionsOnly);

    // Clear the debugger from the current window before setting the new window.
    attachDebugger(0);

    m_windowShell->window()->willRemoveFromWindowShell();
    m_windowShell->setWindow(m_frame->domWindow());

    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setProfileGroup(page->group().identifier());
    }

    // There is likely to be a lot of garbage now.
    gcController().garbageCollectSoon();
}

void ScriptController::initScript()
{
    if (m_windowShell)
        return;

    JSLock lock(SilenceAssertionsOnly);

    m_windowShell = new JSDOMWindowShell(m_frame->domWindow());
    m_windowShell->window()->updateDocument();

    if (Page* page = m_frame->page()) {
        attachDebugger(page->debugger());
        m_windowShell->window()->setProfileGroup(page->group().identifier());
    }

    m_frame->loader()->dispatchWindowObjectAvailable();
}

bool ScriptController::processingUserGesture() const
{
    return m_allowPopupsFromPlugin || processingUserGestureEvent() || isJavaScriptAnchorNavigation();
}

bool ScriptController::processingUserGestureEvent() const
{
    if (!m_windowShell)
        return false;

    if (Event* event = m_windowShell->window()->currentEvent()) {
        if (event->createdByDOM())
            return false;

        const AtomicString& type = event->type();
        if ( // mouse events
            type == eventNames().clickEvent || type == eventNames().mousedownEvent ||
            type == eventNames().mouseupEvent || type == eventNames().dblclickEvent ||
            // keyboard events
            type == eventNames().keydownEvent || type == eventNames().keypressEvent ||
            type == eventNames().keyupEvent ||
            // other accepted events
            type == eventNames().selectEvent || type == eventNames().changeEvent ||
            type == eventNames().focusEvent || type == eventNames().blurEvent ||
            type == eventNames().submitEvent)
            return true;
    }
    
    return false;
}

// FIXME: This seems like an insufficient check to verify a click on a javascript: anchor.
bool ScriptController::isJavaScriptAnchorNavigation() const
{
    // This is the <a href="javascript:window.open('...')> case -> we let it through
    if (m_sourceURL && m_sourceURL->isNull() && !m_processingTimerCallback)
        return true;

    // This is the <script>window.open(...)</script> case or a timer callback -> block it
    return false;
}

bool ScriptController::anyPageIsProcessingUserGesture() const
{
    Page* page = m_frame->page();
    if (!page)
        return false;

    const HashSet<Page*>& pages = page->group().pages();
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->script()->processingUserGesture())
                return true;
        }
    }

    return false;
}

bool ScriptController::isEnabled()
{
    Settings* settings = m_frame->settings();
    return (settings && settings->isJavaScriptEnabled());
}

void ScriptController::attachDebugger(JSC::Debugger* debugger)
{
    if (!m_windowShell)
        return;

    if (debugger)
        debugger->attach(m_windowShell->window());
    else if (JSC::Debugger* currentDebugger = m_windowShell->window()->debugger())
        currentDebugger->detach(m_windowShell->window());
}

void ScriptController::updateDocument()
{
    if (!m_frame->document())
        return;

    JSLock lock(SilenceAssertionsOnly);
    if (m_windowShell)
        m_windowShell->window()->updateDocument();
}

void ScriptController::updateSecurityOrigin()
{
    // Our bindings do not do anything in this case.
}

Bindings::RootObject* ScriptController::bindingRootObject()
{
    if (!isEnabled())
        return 0;

    if (!m_bindingRootObject) {
        JSLock lock(SilenceAssertionsOnly);
        m_bindingRootObject = Bindings::RootObject::create(0, globalObject());
    }
    return m_bindingRootObject.get();
}

PassRefPtr<Bindings::RootObject> ScriptController::createRootObject(void* nativeHandle)
{
    RootObjectMap::iterator it = m_rootObjects.find(nativeHandle);
    if (it != m_rootObjects.end())
        return it->second;

    RefPtr<Bindings::RootObject> rootObject = Bindings::RootObject::create(nativeHandle, globalObject());

    m_rootObjects.set(nativeHandle, rootObject);
    return rootObject.release();
}

#if ENABLE(NETSCAPE_PLUGIN_API)

NPObject* ScriptController::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (isEnabled()) {
            // JavaScript is enabled, so there is a JavaScript window object.
            // Return an NPObject bound to the window object.
            JSC::JSLock lock(SilenceAssertionsOnly);
            JSObject* win = windowShell()->window();
            ASSERT(win);
            Bindings::RootObject* root = bindingRootObject();
            m_windowScriptNPObject = _NPN_CreateScriptObject(0, win, root);
        } else {
            // JavaScript is not enabled, so we cannot bind the NPObject to the JavaScript window object.
            // Instead, we create an NPObject of a different class, one which is not bound to a JavaScript object.
            m_windowScriptNPObject = _NPN_CreateNoScriptObject();
        }
    }

    return m_windowScriptNPObject;
}

NPObject* ScriptController::createScriptObjectForPluginElement(HTMLPlugInElement* plugin)
{
    JSObject* object = jsObjectForPluginElement(plugin);
    if (!object)
        return _NPN_CreateNoScriptObject();

    // Wrap the JSObject in an NPObject
    return _NPN_CreateScriptObject(0, object, bindingRootObject());
}

#endif

JSObject* ScriptController::jsObjectForPluginElement(HTMLPlugInElement* plugin)
{
    // Can't create JSObjects when JavaScript is disabled
    if (!isEnabled())
        return 0;

    // Create a JSObject bound to this element
    JSLock lock(SilenceAssertionsOnly);
    ExecState* exec = globalObject()->globalExec();
    JSValue jsElementValue = toJS(exec, globalObject(), plugin);
    if (!jsElementValue || !jsElementValue.isObject())
        return 0;
    
    return jsElementValue.getObject();
}

#if !PLATFORM(MAC)

void ScriptController::updatePlatformScriptObjects()
{
}

void ScriptController::disconnectPlatformScriptObjects()
{
}

#endif

void ScriptController::cleanupScriptObjectsForPlugin(void* nativeHandle)
{
    RootObjectMap::iterator it = m_rootObjects.find(nativeHandle);

    if (it == m_rootObjects.end())
        return;

    it->second->invalidate();
    m_rootObjects.remove(it);
}

void ScriptController::clearScriptObjects()
{
    JSLock lock(SilenceAssertionsOnly);

    RootObjectMap::const_iterator end = m_rootObjects.end();
    for (RootObjectMap::const_iterator it = m_rootObjects.begin(); it != end; ++it)
        it->second->invalidate();

    m_rootObjects.clear();

    if (m_bindingRootObject) {
        m_bindingRootObject->invalidate();
        m_bindingRootObject = 0;
    }

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

} // namespace WebCore
