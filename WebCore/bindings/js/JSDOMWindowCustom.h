/*
 *  Copyright (C) 2008 Apple Inc. All rights reseved.
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

#ifndef JSDOMWindowCustom_h
#define JSDOMWindowCustom_h

#include "JSDOMWindow.h"
#include "JSDOMWindowShell.h"
#include <wtf/AlwaysInline.h>

namespace WebCore {

inline JSDOMWindow* asJSDOMWindow(JSC::JSGlobalObject* globalObject)
{
    return static_cast<JSDOMWindow*>(globalObject);
}
 
inline const JSDOMWindow* asJSDOMWindow(const JSC::JSGlobalObject* globalObject)
{
    return static_cast<const JSDOMWindow*>(globalObject);
}

JSC::JSValuePtr nonCachingStaticCloseFunctionGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
JSC::JSValuePtr nonCachingStaticBlurFunctionGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
JSC::JSValuePtr nonCachingStaticFocusFunctionGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);
JSC::JSValuePtr nonCachingStaticPostMessageFunctionGetter(JSC::ExecState*, const JSC::Identifier&, const JSC::PropertySlot&);

ALWAYS_INLINE bool JSDOMWindow::customGetOwnPropertySlot(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
{
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.

    const JSC::HashEntry* entry;

    // We don't want any properties other than "close" and "closed" on a closed window.
    if (!impl()->frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        entry = s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && !(entry->attributes() & JSC::Function) && entry->propertyGetter() == jsDOMWindowClosed) {
            slot.setCustom(this, entry->propertyGetter());
            return true;
        }
        entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && (entry->attributes() & JSC::Function) && entry->function() == jsDOMWindowPrototypeFunctionClose) {
            slot.setCustom(this, nonCachingStaticCloseFunctionGetter);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined();
        return true;
    }

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    bool allowsAccess = allowsAccessFromNoErrorMessage(exec);

    // Look for overrides before looking at any of our own properties, but ignore overrides completely
    // if this is cross-domain access.
    if (allowsAccess && JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot))
        return true;

    // We need this code here because otherwise JSC::Window will stop the search before we even get to the
    // prototype due to the blanket same origin (allowsAccessFrom) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & JSC::Function) {
            if (entry->function() == jsDOMWindowPrototypeFunctionBlur) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticBlurFunctionGetter);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionClose) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticCloseFunctionGetter);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionFocus) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticFocusFunctionGetter);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionPostMessage) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticPostMessageFunctionGetter);
                    return true;
                }
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

inline bool JSDOMWindow::customPut(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSValuePtr value, JSC::PutPropertySlot& slot)
{
    if (!impl()->frame())
        return true;

    // We have a local override (e.g. "var location"), save time and jump directly to JSGlobalObject.
    JSC::PropertySlot getSlot;
    bool slotIsWriteable;
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, getSlot, slotIsWriteable)) {
        if (allowsAccessFrom(exec)) {
            if (slotIsWriteable) {
                getSlot.putValue(value);
                if (getSlot.isCacheable())
                    slot.setExistingProperty(this, getSlot.cachedOffset());
            } else
                JSGlobalObject::put(exec, propertyName, value, slot);
        }
        return true;
    }

    return false;
}



inline bool JSDOMWindowBase::allowsAccessFrom(const JSGlobalObject* other) const
{
    if (allowsAccessFromPrivate(other))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(other));
    return false;
}

    inline bool JSDOMWindowBase::allowsAccessFrom(JSC::ExecState* exec) const
{
    if (allowsAccessFromPrivate(exec->lexicalGlobalObject()))
        return true;
    printErrorMessage(crossDomainAccessErrorMessage(exec->lexicalGlobalObject()));
    return false;
}
    
inline bool JSDOMWindowBase::allowsAccessFromNoErrorMessage(JSC::ExecState* exec) const
{
    return allowsAccessFromPrivate(exec->lexicalGlobalObject());
}
    
inline bool JSDOMWindowBase::allowsAccessFrom(JSC::ExecState* exec, String& message) const
{
    if (allowsAccessFromPrivate(exec->lexicalGlobalObject()))
        return true;
    message = crossDomainAccessErrorMessage(exec->lexicalGlobalObject());
    return false;
}
    
ALWAYS_INLINE bool JSDOMWindowBase::allowsAccessFromPrivate(const JSGlobalObject* other) const
{
    const JSDOMWindow* originWindow = asJSDOMWindow(other);
    const JSDOMWindow* targetWindow = d()->shell->window();

    if (originWindow == targetWindow)
        return true;

    // JS may be attempting to access the "window" object, which should be valid,
    // even if the document hasn't been constructed yet.  If the document doesn't
    // exist yet allow JS to access the window object.
    if (!originWindow->impl()->document())
        return true;

    const SecurityOrigin* originSecurityOrigin = originWindow->impl()->securityOrigin();
    const SecurityOrigin* targetSecurityOrigin = targetWindow->impl()->securityOrigin();

    return originSecurityOrigin->canAccess(targetSecurityOrigin);
}

}

#endif // JSDOMWindowCustom_h
