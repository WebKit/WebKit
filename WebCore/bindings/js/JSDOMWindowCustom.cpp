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
#include "JSDOMWindow.h"

#include "AtomicString.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "JSDOMWindowWrapper.h"
#include "Settings.h"
#include "kjs_proxy.h"
#include <kjs/object.h>

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
}

bool JSDOMWindow::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.

    const HashEntry* entry;

    // We don't want any properties other than "close" and "closed" on a closed window.
    if (!impl()->frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        entry = s_info.propHashTable->entry(propertyName);
        if (entry && !(entry->attributes & Function) && entry->integerValue == ClosedAttrNum) {
            slot.setStaticEntry(this, entry, staticValueGetter<JSDOMWindow>);
            return true;
        }
        entry = JSDOMWindowPrototype::s_info.propHashTable->entry(propertyName);
        if (entry && (entry->attributes & Function) && entry->functionValue == jsDOMWindowPrototypeFunctionClose) {
            slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined(this);
        return true;
    }

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    bool allowsAccess = allowsAccessFromNoErrorMessage(exec);

    // Look for overrides before looking at any of our own properties.
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        // But ignore overrides completely if this is cross-domain access.
        if (allowsAccess)
            return true;
    }

    // We need this code here because otherwise KJS::Window will stop the search before we even get to the
    // prototype due to the blanket same origin (allowsAccessFrom) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    entry = JSDOMWindowPrototype::s_info.propHashTable->entry(propertyName);
    if (entry) {
        if ((entry->attributes & Function)
                && (entry->functionValue == jsDOMWindowPrototypeFunctionBlur
                    || entry->functionValue == jsDOMWindowPrototypeFunctionClose
                    || entry->functionValue == jsDOMWindowPrototypeFunctionFocus
#if ENABLE(CROSS_DOCUMENT_MESSAGING)
                    || entry->functionValue == jsDOMWindowPrototypeFunctionPostMessage
#endif
                    )) {
            if (!allowsAccess) {
                slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
                return true;
            }
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.prototype.toString.
        if (propertyName == exec->propertyNames().toString) {
            if (!allowsAccess) {
                slot.setCustom(this, objectToStringFunctionGetter);
                return true;
            }
        }
    }

    return false;
}

bool JSDOMWindow::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (!impl()->frame())
        return true;

    // We have a local override (e.g. "var location"), save time and jump directly to JSGlobalObject.
    PropertySlot slot;
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        if (allowsAccessFrom(exec))
            JSGlobalObject::put(exec, propertyName, value);
        return true;
    }

    return false;
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
    Frame* activeFrame = toJSDOMWindow(exec->dynamicGlobalObject())->impl()->frame();
    if (!activeFrame)
        return;

    // To avoid breaking old widgets, make "var location =" in a top-level frame create
    // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
    if (Settings* settings = activeFrame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode() && !activeFrame->tree()->parent()) {
            if (allowsAccessFrom(exec))
                putDirect("location", value);
            return;
        }
    }

    if (!activeFrame->loader()->shouldAllowNavigation(impl()->frame()))
        return;
    String dstUrl = activeFrame->loader()->completeURL(value->toString(exec)).string();
    if (!protocolIs(dstUrl, "javascript") || allowsAccessFrom(exec)) {
        bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
        // We want a new history item if this JS was called via a user gesture
        impl()->frame()->loader()->scheduleLocationChange(dstUrl, activeFrame->loader()->outgoingReferrer(), false, userGesture);
    }
}

#if ENABLE(CROSS_DOCUMENT_MESSAGING)
JSValue* JSDOMWindow::postMessage(ExecState* exec, const List& args)
{
    DOMWindow* window = impl();

    DOMWindow* source = toJSDOMWindow(exec->dynamicGlobalObject())->impl();
    String domain = source->frame()->loader()->url().host();
    String uri = source->frame()->loader()->url().string();
    String message = args[0]->toString(exec);

    if (exec->hadException())
        return jsUndefined();

    window->postMessage(message, domain, uri, source);

    return jsUndefined();
}
#endif

DOMWindow* toDOMWindow(JSValue* val)
{
    if (val->isObject(&JSDOMWindow::s_info))
        return static_cast<JSDOMWindow*>(val)->impl();
    if (val->isObject(&JSDOMWindowWrapper::s_info))
        return static_cast<JSDOMWindowWrapper*>(val)->impl();
    return 0;
}

} // namespace WebCore
