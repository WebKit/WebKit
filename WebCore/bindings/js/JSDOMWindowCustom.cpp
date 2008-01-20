/*
 * Copyright (C) 2007 Apple, Inc.
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

#include "Document.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "kjs_window.h"
#include "kjs/object.h"
#include "kjs/value.h"

namespace WebCore {

bool JSDOMWindow::customGetOwnPropertySlot(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::PropertySlot& slot)
{
    // we don't want any properties other than "closed" on a closed window
    if (!impl()->frame()) {
        if (propertyName == "closed") {
            const KJS::HashEntry* entry = KJS::Lookup::findEntry(classInfo()->propHashTable, propertyName);
            ASSERT(entry);
            if (entry) {
                slot.setStaticEntry(this, entry, KJS::staticValueGetter<JSDOMWindow>);
                return true;
            }
        }
        if (propertyName == "close") {
            KJS::JSValue* proto = prototype();
            if (proto->isObject()) {
                const KJS::HashEntry* entry = KJS::Lookup::findEntry(static_cast<KJS::JSObject*>(proto)->classInfo()->propHashTable, propertyName);
                ASSERT(entry);
                if (entry) {
                    slot.setStaticEntry(this, entry, KJS::staticFunctionGetter);
                    return true;
                }
            }
        }

        slot.setUndefined(this);
        return true;
    }

    // Look for overrides first
    if (JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        if (!allowsAccessFrom(exec))
            slot.setUndefined(this);

        return true;
    }

    // FIXME: We need this to work around the blanket same origin (allowsAccessFrom) check in KJS::Window.  Once we remove that, we
    // can move this to JSDOMWindowPrototype.
    KJS::JSValue* proto = prototype();
    if (proto->isObject()) {
        const KJS::HashEntry* entry = KJS::Lookup::findEntry(static_cast<KJS::JSObject*>(proto)->classInfo()->propHashTable, propertyName);
        if (entry) {
            if (entry->attr & KJS::Function) {
                if (entry->value.functionValue == jsDOMWindowPrototypeFunctionFocus
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionBlur
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionClose
                    || entry->value.functionValue == jsDOMWindowPrototypeFunctionPostMessage)
                        slot.setStaticEntry(this, entry, KJS::staticFunctionGetter);
                else {
                    if (!allowsAccessFrom(exec))
                        slot.setUndefined(this);
                    else
                        slot.setStaticEntry(this, entry, KJS::staticFunctionGetter);
                }
                return true;
            }
        }
    }

    return false;
}

bool JSDOMWindow::customPut(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value, int attr)
{
    if (!impl()->frame())
        return true;

    // Called by an internal KJS, save time and jump directly to JSGlobalObject.
    if (attr != KJS::None && attr != KJS::DontDelete) {
        KJS::JSGlobalObject::put(exec, propertyName, value, attr);
        return true;
    }

    // We have a local override (e.g. "var location"), save time and jump directly to JSGlobalObject.
    KJS::PropertySlot slot;
    if (KJS::JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot)) {
        if (allowsAccessFrom(exec))
            KJS::JSGlobalObject::put(exec, propertyName, value, attr);
        return true;
    }

    return false;
}

KJS::JSValue* JSDOMWindow::postMessage(KJS::ExecState* exec, const KJS::List& args)
{
    DOMWindow* window = impl();
    
    DOMWindow* source = static_cast<JSDOMWindow*>(exec->dynamicGlobalObject())->impl();
    String domain = source->frame()->loader()->url().host();
    String uri = source->frame()->loader()->url().string();
    String message = args[0]->toString(exec);
    
    if (exec->hadException())
        return KJS::jsUndefined();
    
    window->postMessage(message, domain, uri, source);
    
    return KJS::jsUndefined();
}

} // namespace WebCore
