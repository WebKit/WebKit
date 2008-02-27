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
#include "kjs_window.h"
#include <kjs/object.h>

using namespace KJS;

namespace WebCore {

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
        entry = Lookup::findEntry(info.propHashTable, propertyName);
        if (entry && !(entry->attr & Function) && entry->value.intValue == ClosedAttrNum) {
            slot.setStaticEntry(this, entry, staticValueGetter<JSDOMWindow>);
            return true;
        }
        entry = Lookup::findEntry(JSDOMWindowPrototype::info.propHashTable, propertyName);
        if (entry && (entry->attr & Function) && entry->value.functionValue == jsDOMWindowPrototypeFunctionClose) {
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
    entry = Lookup::findEntry(JSDOMWindowPrototype::info.propHashTable, propertyName);
    if (entry) {
        if ((entry->attr & Function)
                && (entry->value.functionValue == jsDOMWindowPrototypeFunctionBlur
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionClose
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionFocus
#if ENABLE(CROSS_DOCUMENT_MESSAGING)
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionPostMessage
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

#if ENABLE(CROSS_DOCUMENT_MESSAGING)
JSValue* JSDOMWindow::postMessage(ExecState* exec, const List& args)
{
    DOMWindow* window = impl();
    
    DOMWindow* source = static_cast<JSDOMWindow*>(exec->dynamicGlobalObject())->impl();
    String domain = source->frame()->loader()->url().host();
    String uri = source->frame()->loader()->url().string();
    String message = args[0]->toString(exec);
    
    if (exec->hadException())
        return jsUndefined();
    
    window->postMessage(message, domain, uri, source);
    
    return jsUndefined();
}
#endif

} // namespace WebCore
