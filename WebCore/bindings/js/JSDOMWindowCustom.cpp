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

#include "kjs_window.h"
#include "DOMWindow.h"

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
                    slot.setStaticEntry(this, entry, KJS::staticFunctionGetter<JSDOMWindowPrototypeFunction>);
                    return true;
                }
            }
        }

        slot.setUndefined(this);
        return true;
    }

    // Look for overrides first
    KJS::JSValue** val = getDirectLocation(propertyName);
    if (val) {
        if (!isSafeScript(exec)) {
            slot.setUndefined(this);
            return true;
        }

        // FIXME: Come up with a way of having JavaScriptCore handle getters/setters in this case
        if (_prop.hasGetterSetterProperties() && val[0]->type() == KJS::GetterSetterType)
            fillGetterPropertySlot(slot, val);
        else
            slot.setValueSlot(this, val);
        return true;
    }

    // FIXME: We need this to work around the blanket isSafeScript check in KJS::Window.  Once we remove that, we
    // can move this to JSDOMWindowPrototype.
    KJS::JSValue* proto = prototype();
    if (proto->isObject()) {
        const KJS::HashEntry* entry = KJS::Lookup::findEntry(static_cast<KJS::JSObject*>(proto)->classInfo()->propHashTable, propertyName);
        if (entry) {
            if (entry->attr & KJS::Function) {
                switch (entry->value) {
                    case FocusFuncNum:
                    case BlurFuncNum:
                    case CloseFuncNum:
                        slot.setStaticEntry(this, entry, KJS::staticFunctionGetter<JSDOMWindowPrototypeFunction>);
                        return true;
                    default:
                        if (!isSafeScript(exec))
                            slot.setUndefined(this);
                        else
                            slot.setStaticEntry(this, entry, KJS::staticFunctionGetter<JSDOMWindowPrototypeFunction>);
                        return true;
                }
            }
        }
    }

    return false;
}

bool JSDOMWindow::customPut(KJS::ExecState* exec, const KJS::Identifier& propertyName, KJS::JSValue* value, int attr)
{
    if (!impl()->frame())
        return true;

    // Called by an internal KJS, save time and jump directly to JSObject.
    if (attr != KJS::None && attr != KJS::DontDelete) {
        KJS::JSObject::put(exec, propertyName, value, attr);
        return true;
    }

    // We have a local override (e.g. "var location"), save time and jump directly to JSObject.
    if (KJS::JSObject::getDirect(propertyName)) {
        if (isSafeScript(exec))
            KJS::JSObject::put(exec, propertyName, value, attr);
        return true;
    }

    return false;
}

} // namespace WebCore
