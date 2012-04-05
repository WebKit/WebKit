/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSDOMWindowBase.h"

#include "Chrome.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "InspectorController.h"
#include "JSDOMWindowCustom.h"
#include "JSNode.h"
#include "Logging.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include <wtf/MainThread.h>

using namespace JSC;

namespace WebCore {

const ClassInfo JSDOMWindowBase::s_info = { "Window", &JSDOMGlobalObject::s_info, 0, 0, CREATE_METHOD_TABLE(JSDOMWindowBase) };

const GlobalObjectMethodTable JSDOMWindowBase::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsProfiling, &supportsRichSourceInfo, &shouldInterruptScript };

JSDOMWindowBase::JSDOMWindowBase(JSGlobalData& globalData, Structure* structure, PassRefPtr<DOMWindow> window, JSDOMWindowShell* shell)
    : JSDOMGlobalObject(globalData, structure, shell->world(), &s_globalObjectMethodTable)
    , m_impl(window)
    , m_shell(shell)
{
}

void JSDOMWindowBase::finishCreation(JSGlobalData& globalData, JSDOMWindowShell* shell)
{
    Base::finishCreation(globalData, shell);
    ASSERT(inherits(&s_info));

    GlobalPropertyInfo staticGlobals[] = {
        GlobalPropertyInfo(Identifier(globalExec(), "document"), jsNull(), DontDelete | ReadOnly),
        GlobalPropertyInfo(Identifier(globalExec(), "window"), m_shell, DontDelete | ReadOnly)
    };
    
    addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
}

void JSDOMWindowBase::destroy(JSCell* cell)
{
    jsCast<JSDOMWindowBase*>(cell)->JSDOMWindowBase::~JSDOMWindowBase();
}

void JSDOMWindowBase::updateDocument()
{
    ASSERT(m_impl->document());
    ExecState* exec = globalExec();
    symbolTablePutWithAttributes(exec->globalData(), Identifier(exec, "document"), toJS(exec, this, m_impl->document()), DontDelete | ReadOnly);
}

ScriptExecutionContext* JSDOMWindowBase::scriptExecutionContext() const
{
    return m_impl->document();
}

String JSDOMWindowBase::crossDomainAccessErrorMessage(const JSGlobalObject* other) const
{
    return m_shell->window()->impl()->crossDomainAccessErrorMessage(asJSDOMWindow(other)->impl());
}

void JSDOMWindowBase::printErrorMessage(const String& message) const
{
    printErrorMessageForFrame(impl()->frame(), message);
}

// This method checks whether accesss to *this* global object is permitted from
// the given context; this differs from allowsAccessFromPrivate, since that
// method checks whether the given context is permitted to access the current
// window the shell is referencing (which may come from a different security
// origin to this global object).
bool JSDOMWindowBase::allowsAccessFrom(const JSGlobalObject* thisObject, ExecState* exec)
{
    JSGlobalObject* otherObject = exec->lexicalGlobalObject();

    const JSDOMWindow* originWindow = asJSDOMWindow(otherObject);
    const JSDOMWindow* targetWindow = asJSDOMWindow(thisObject);

    if (originWindow == targetWindow)
        return true;

    const SecurityOrigin* originSecurityOrigin = originWindow->impl()->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->impl()->securityOrigin();

    if (originSecurityOrigin->canAccess(targetSecurityOrigin))
        return true;

    targetWindow->printErrorMessage(targetWindow->crossDomainAccessErrorMessage(otherObject));
    return false;
}

bool JSDOMWindowBase::supportsProfiling(const JSGlobalObject* object)
{
#if !ENABLE(JAVASCRIPT_DEBUGGER) || !ENABLE(INSPECTOR)
    return false;
#else
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->impl()->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return page->inspectorController()->profilerEnabled();
#endif
}

bool JSDOMWindowBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
#if !ENABLE(JAVASCRIPT_DEBUGGER) || !ENABLE(INSPECTOR)
    return false;
#else
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    Frame* frame = thisObject->impl()->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    bool enabled = page->inspectorController()->enabled();
    ASSERT(enabled || !thisObject->debugger());
    ASSERT(enabled || !supportsProfiling(thisObject));
    return enabled;
#endif
}

bool JSDOMWindowBase::shouldInterruptScript(const JSGlobalObject* object)
{
    const JSDOMWindowBase* thisObject = static_cast<const JSDOMWindowBase*>(object);
    ASSERT(thisObject->impl()->frame());
    Page* page = thisObject->impl()->frame()->page();

    // See <rdar://problem/5479443>. We don't think that page can ever be NULL
    // in this case, but if it is, we've gotten into a state where we may have
    // hung the UI, with no way to ask the client whether to cancel execution.
    // For now, our solution is just to cancel execution no matter what,
    // ensuring that we never hang. We might want to consider other solutions
    // if we discover problems with this one.
    ASSERT(page);
    if (!page)
        return true;

    return page->chrome()->shouldInterruptJavaScript();
}

void JSDOMWindowBase::willRemoveFromWindowShell()
{
    setCurrentEvent(0);
}

JSObject* JSDOMWindowBase::toThisObject(JSCell* cell, ExecState*)
{
    return jsCast<JSDOMWindowBase*>(cell)->shell();
}

JSDOMWindowShell* JSDOMWindowBase::shell() const
{
    return m_shell;
}

JSGlobalData* JSDOMWindowBase::commonJSGlobalData()
{
    ASSERT(isMainThread());

    static JSGlobalData* globalData = 0;
    if (!globalData) {
        globalData = JSGlobalData::createLeaked(ThreadStackTypeLarge, LargeHeap).leakRef();
        globalData->timeoutChecker.setTimeoutInterval(10000); // 10 seconds
#ifndef NDEBUG
        globalData->exclusiveThread = currentThread();
#endif
        initNormalWorldClientData(globalData);
    }

    return globalData;
}

// JSDOMGlobalObject* is ignored, accessing a window in any context will
// use that DOMWindow's prototype chain.
JSValue toJS(ExecState* exec, JSDOMGlobalObject*, DOMWindow* domWindow)
{
    return toJS(exec, domWindow);
}

JSValue toJS(ExecState* exec, DOMWindow* domWindow)
{
    if (!domWindow)
        return jsNull();
    Frame* frame = domWindow->frame();
    if (!frame)
        return jsNull();
    return frame->script()->windowShell(currentWorld(exec));
}

JSDOMWindow* toJSDOMWindow(Frame* frame, DOMWrapperWorld* world)
{
    if (!frame)
        return 0;
    return frame->script()->windowShell(world)->window();
}

JSDOMWindow* toJSDOMWindow(JSValue value)
{
    if (!value.isObject())
        return 0;
    const ClassInfo* classInfo = asObject(value)->classInfo();
    if (classInfo == &JSDOMWindow::s_info)
        return jsCast<JSDOMWindow*>(asObject(value));
    if (classInfo == &JSDOMWindowShell::s_info)
        return jsCast<JSDOMWindowShell*>(asObject(value))->window();
    return 0;
}

} // namespace WebCore
