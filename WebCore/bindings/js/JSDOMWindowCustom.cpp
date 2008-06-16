/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSDOMWindowCustom.h"

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "JSDOMWindowShell.h"
#include "Settings.h"
#include "ScriptController.h"
#include <kjs/JSObject.h>

using namespace KJS;

namespace WebCore {

static void markDOMObjectWrapper(void* object)
{
    if (!object)
        return;
    DOMObject* wrapper = ScriptInterpreter::getDOMObject(object);
    if (!wrapper || wrapper->marked())
        return;
    wrapper->mark();
}

void JSDOMWindow::mark()
{
    Base::mark();
    markDOMObjectWrapper(impl()->optionalConsole());
    markDOMObjectWrapper(impl()->optionalHistory());
    markDOMObjectWrapper(impl()->optionalLocationbar());
    markDOMObjectWrapper(impl()->optionalMenubar());
    markDOMObjectWrapper(impl()->optionalNavigator());
    markDOMObjectWrapper(impl()->optionalPersonalbar());
    markDOMObjectWrapper(impl()->optionalScreen());
    markDOMObjectWrapper(impl()->optionalScrollbars());
    markDOMObjectWrapper(impl()->optionalSelection());
    markDOMObjectWrapper(impl()->optionalStatusbar());
    markDOMObjectWrapper(impl()->optionalToolbar());
    markDOMObjectWrapper(impl()->optionalLocation());
#if ENABLE(DOM_STORAGE)
    markDOMObjectWrapper(impl()->optionalSessionStorage());
    markDOMObjectWrapper(impl()->optionalLocalStorage());
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    markDOMObjectWrapper(impl()->optionalApplicationCache());
#endif
}

bool JSDOMWindow::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting properties by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

bool JSDOMWindow::customGetPropertyNames(ExecState* exec, PropertyNameArray&)
{
    // Only allow the window to enumerated by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return true;
    return false;
}

void JSDOMWindow::setLocation(ExecState* exec, JSValue* value)
{
    Frame* activeFrame = asJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return;

#if ENABLE(DASHBOARD_SUPPORT)
    // To avoid breaking old widgets, make "var location =" in a top-level frame create
    // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
    if (Settings* settings = activeFrame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode() && !activeFrame->tree()->parent()) {
            if (allowsAccessFrom(exec))
                putDirect(Identifier(exec, "location"), value);
            return;
        }
    }
#endif

    if (!activeFrame->loader()->shouldAllowNavigation(impl()->frame()))
        return;
    String dstUrl = activeFrame->loader()->completeURL(value->toString(exec)).string();
    if (!protocolIs(dstUrl, "javascript") || allowsAccessFrom(exec)) {
        bool userGesture = activeFrame->script()->processingUserGesture();
        // We want a new history item if this JS was called via a user gesture
        impl()->frame()->loader()->scheduleLocationChange(dstUrl, activeFrame->loader()->outgoingReferrer(), false, userGesture);
    }
}

JSValue* JSDOMWindow::postMessage(ExecState* exec, const ArgList& args)
{
    DOMWindow* window = impl();

    DOMWindow* source = asJSDOMWindow(exec->dynamicGlobalObject())->impl();
    String message = args[0]->toString(exec);

    if (exec->hadException())
        return jsUndefined();

    String targetOrigin = valueToStringWithUndefinedOrNullCheck(exec, args[1]);
    if (exec->hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    window->postMessage(message, targetOrigin, source, ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

DOMWindow* toDOMWindow(JSValue* val)
{
    if (val->isObject(&JSDOMWindow::s_info))
        return static_cast<JSDOMWindow*>(val)->impl();
    if (val->isObject(&JSDOMWindowShell::s_info))
        return static_cast<JSDOMWindowShell*>(val)->impl();
    return 0;
}

} // namespace WebCore
