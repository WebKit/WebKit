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
#include "FrameLoaderClient.h"
#include "GCController.h"
#include "HTMLPlugInElement.h"
#include "InspectorTimelineAgent.h"
#include "JSDocument.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "Settings.h"
#include "StorageNamespace.h"
#include "UserGestureIndicator.h"
#include "WebCoreJSClientData.h"
#include "XSSAuditor.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include <debugger/Debugger.h>
#include <runtime/InitializeThreading.h>
#include <runtime/JSLock.h>
#include <wtf/Threading.h>

using namespace JSC;
using namespace std;

namespace WebCore {

void ScriptController::initializeThreading()
{
    JSC::initializeThreading();
    WTF::initializeMainThread();
}

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
#if PLATFORM(MAC) && ENABLE(JAVA_BRIDGE)
    static bool initializedJavaJSBindings;
    if (!initializedJavaJSBindings) {
        initializedJavaJSBindings = true;
        initJavaJSBindings();
    }
#endif
}

ScriptController::~ScriptController()
{
    disconnectPlatformScriptObjects();

    // It's likely that destroying m_windowShells will create a lot of garbage.
    if (!m_windowShells.isEmpty()) {
        while (!m_windowShells.isEmpty())
            destroyWindowShell(m_windowShells.begin()->first.get());
        gcController().garbageCollectSoon();
    }
}

void ScriptController::destroyWindowShell(DOMWrapperWorld* world)
{
    ASSERT(m_windowShells.contains(world));
    m_windowShells.remove(world);
    world->didDestroyWindowShell(this);
}

JSDOMWindowShell* ScriptController::createWindowShell(DOMWrapperWorld* world)
{
    ASSERT(!m_windowShells.contains(world));
    JSDOMWindowShell* windowShell = new JSDOMWindowShell(m_frame->domWindow(), world);
    m_windowShells.add(world, windowShell);
    world->didCreateWindowShell(this);
    return windowShell;
}

ScriptValue ScriptController::evaluateInWorld(const ScriptSourceCode& sourceCode, DOMWrapperWorld* world, ShouldAllowXSS shouldAllowXSS)
{
    const SourceCode& jsSourceCode = sourceCode.jsSourceCode();
    String sourceURL = ustringToString(jsSourceCode.provider()->url());

    if (shouldAllowXSS == DoNotAllowXSS && !m_XSSAuditor->canEvaluate(sourceCode.source())) {
        // This script is not safe to be evaluated.
        return JSValue();
    }

    // evaluate code. Returns the JS return value or 0
    // if there was none, an error occurred or the type couldn't be converted.

    // inlineCode is true for <a href="javascript:doSomething()">
    // and false for <script>doSomething()</script>. Check if it has the
    // expected value in all cases.
    // See smart window.open policy for where this is used.
    JSDOMWindowShell* shell = windowShell(world);
    ExecState* exec = shell->window()->globalExec();
    const String* savedSourceURL = m_sourceURL;
    m_sourceURL = &sourceURL;

    JSLock lock(SilenceAssertionsOnly);

    RefPtr<Frame> protect = m_frame;

#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_frame->page() ? m_frame->page()->inspectorTimelineAgent() : 0)
        timelineAgent->willEvaluateScript(sourceURL, sourceCode.startLine());
#endif

    exec->globalData().timeoutChecker.start();
    Completion comp = JSC::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), jsSourceCode, shell);
    exec->globalData().timeoutChecker.stop();

#if ENABLE(INSPECTOR)
    if (InspectorTimelineAgent* timelineAgent = m_frame->page() ? m_frame->page()->inspectorTimelineAgent() : 0)
        timelineAgent->didEvaluateScript();
#endif

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

ScriptValue ScriptController::evaluate(const ScriptSourceCode& sourceCode, ShouldAllowXSS shouldAllowXSS) 
{
    return evaluateInWorld(sourceCode, mainThreadNormalWorld(), shouldAllowXSS);
}

PassRefPtr<DOMWrapperWorld> ScriptController::createWorld()
{
    return DOMWrapperWorld::create(JSDOMWindow::commonJSGlobalData());
}

void ScriptController::getAllWorlds(Vector<DOMWrapperWorld*>& worlds)
{
    static_cast<WebCoreJSClientData*>(JSDOMWindow::commonJSGlobalData()->clientData)->getAllWorlds(worlds);
}

void ScriptController::clearWindowShell()
{
    if (m_windowShells.isEmpty())
        return;

    JSLock lock(SilenceAssertionsOnly);

    for (ShellMap::iterator iter = m_windowShells.begin(); iter != m_windowShells.end(); ++iter) {
        JSDOMWindowShell* windowShell = iter->second;

        // Clear the debugger from the current window before setting the new window.
        attachDebugger(windowShell, 0);

        windowShell->window()->willRemoveFromWindowShell();
        windowShell->setWindow(m_frame->domWindow());

        if (Page* page = m_frame->page()) {
            attachDebugger(windowShell, page->debugger());
            windowShell->window()->setProfileGroup(page->group().identifier());
        }
    }

    // It's likely that resetting our windows created a lot of garbage.
    gcController().garbageCollectSoon();
}

JSDOMWindowShell* ScriptController::initScript(DOMWrapperWorld* world)
{
    ASSERT(!m_windowShells.contains(world));

    JSLock lock(SilenceAssertionsOnly);

    JSDOMWindowShell* windowShell = createWindowShell(world);

    windowShell->window()->updateDocument();

    if (Page* page = m_frame->page()) {
        attachDebugger(windowShell, page->debugger());
        windowShell->window()->setProfileGroup(page->group().identifier());
    }

    m_frame->loader()->dispatchDidClearWindowObjectInWorld(world);

    return windowShell;
}

bool ScriptController::processingUserGesture(DOMWrapperWorld* world) const
{
    if (m_allowPopupsFromPlugin || isJavaScriptAnchorNavigation())
        return true;

    // If a DOM event is being processed, check that it was initiated by the user
    // and that it is in the whitelist of event types allowed to generate pop-ups.
    if (JSDOMWindowShell* shell = existingWindowShell(world))
        if (Event* event = shell->window()->currentEvent())
            return event->fromUserGesture();

    return UserGestureIndicator::processingUserGesture();
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
            ScriptController* script = frame->script();

            if (script->m_allowPopupsFromPlugin)
                return true;

            const ShellMap::const_iterator iterEnd = m_windowShells.end();
            for (ShellMap::const_iterator iter = m_windowShells.begin(); iter != iterEnd; ++iter) {
                JSDOMWindowShell* shell = iter->second.get();
                Event* event = shell->window()->currentEvent();
                if (event && event->fromUserGesture())
                    return true;
            }

            if (isJavaScriptAnchorNavigation())
                return true;
        }
    }

    return false;
}

void ScriptController::attachDebugger(JSC::Debugger* debugger)
{
    for (ShellMap::iterator iter = m_windowShells.begin(); iter != m_windowShells.end(); ++iter)
        attachDebugger(iter->second, debugger);
}

void ScriptController::attachDebugger(JSDOMWindowShell* shell, JSC::Debugger* debugger)
{
    if (!shell)
        return;

    JSDOMWindow* globalObject = shell->window();
    if (debugger)
        debugger->attach(globalObject);
    else if (JSC::Debugger* currentDebugger = globalObject->debugger())
        currentDebugger->detach(globalObject);
}

void ScriptController::updateDocument()
{
    if (!m_frame->document())
        return;

    JSLock lock(SilenceAssertionsOnly);
    for (ShellMap::iterator iter = m_windowShells.begin(); iter != m_windowShells.end(); ++iter)
        iter->second->window()->updateDocument();
}

void ScriptController::updateSecurityOrigin()
{
    // Our bindings do not do anything in this case.
}

Bindings::RootObject* ScriptController::bindingRootObject()
{
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return 0;

    if (!m_bindingRootObject) {
        JSLock lock(SilenceAssertionsOnly);
        m_bindingRootObject = Bindings::RootObject::create(0, globalObject(pluginWorld()));
    }
    return m_bindingRootObject.get();
}

PassRefPtr<Bindings::RootObject> ScriptController::createRootObject(void* nativeHandle)
{
    RootObjectMap::iterator it = m_rootObjects.find(nativeHandle);
    if (it != m_rootObjects.end())
        return it->second;

    RefPtr<Bindings::RootObject> rootObject = Bindings::RootObject::create(nativeHandle, globalObject(pluginWorld()));

    m_rootObjects.set(nativeHandle, rootObject);
    return rootObject.release();
}

#if ENABLE(NETSCAPE_PLUGIN_API)

NPObject* ScriptController::windowScriptNPObject()
{
    if (!m_windowScriptNPObject) {
        if (canExecuteScripts(NotAboutToExecuteScript)) {
            // JavaScript is enabled, so there is a JavaScript window object.
            // Return an NPObject bound to the window object.
            JSC::JSLock lock(SilenceAssertionsOnly);
            JSObject* win = windowShell(pluginWorld())->window();
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
    if (!canExecuteScripts(NotAboutToExecuteScript))
        return 0;

    // Create a JSObject bound to this element
    JSLock lock(SilenceAssertionsOnly);
    JSDOMWindow* globalObj = globalObject(pluginWorld());
    // FIXME: is normal okay? - used for NP plugins?
    JSValue jsElementValue = toJS(globalObj->globalExec(), globalObj, plugin);
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

ScriptValue ScriptController::executeScriptInWorld(DOMWrapperWorld* world, const String& script, bool forceUserGesture, ShouldAllowXSS shouldAllowXSS)
{
    ScriptSourceCode sourceCode(script, forceUserGesture ? KURL() : m_frame->loader()->url());

    if (!canExecuteScripts(AboutToExecuteScript) || isPaused())
        return ScriptValue();

    bool wasInExecuteScript = m_inExecuteScript;
    m_inExecuteScript = true;

    ScriptValue result = evaluateInWorld(sourceCode, world, shouldAllowXSS);

    if (!wasInExecuteScript) {
        m_inExecuteScript = false;
        Document::updateStyleForAllDocuments();
    }

    return result;
}

} // namespace WebCore
