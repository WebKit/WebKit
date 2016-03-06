/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2006, 2008, 2009, 2012-2015 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSObjectInlines_h
#define JSObjectInlines_h

#include "Error.h"
#include "JSObject.h"
#include "Lookup.h"

namespace JSC {

ALWAYS_INLINE bool JSObject::canPerformFastPutInline(ExecState* exec, VM& vm, PropertyName propertyName)
{
    if (UNLIKELY(propertyName == exec->propertyNames().underscoreProto))
        return false;

    // Check if there are any setters or getters in the prototype chain
    JSValue prototype;
    JSObject* obj = this;
    while (true) {
        if (obj->structure(vm)->hasReadOnlyOrGetterSetterPropertiesExcludingProto() || obj->type() == ProxyObjectType)
            return false;

        prototype = obj->getPrototypeDirect();
        if (prototype.isNull())
            return true;

        obj = asObject(prototype);
    }

    ASSERT_NOT_REACHED();
}

// ECMA 8.6.2.2
ALWAYS_INLINE void JSObject::putInline(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSObject* thisObject = jsCast<JSObject*>(cell);
    ASSERT(value);
    ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(thisObject));
    VM& vm = exec->vm();
    
    // Try indexed put first. This is required for correctness, since loads on property names that appear like
    // valid indices will never look in the named property storage.
    if (Optional<uint32_t> index = parseIndex(propertyName)) {
        putByIndex(thisObject, exec, index.value(), value, slot.isStrictMode());
        return;
    }

    if (thisObject->canPerformFastPutInline(exec, vm, propertyName)) {
        ASSERT(!thisObject->structure(vm)->prototypeChainMayInterceptStoreTo(exec->vm(), propertyName));
        if (!thisObject->putDirectInternal<PutModePut>(vm, propertyName, value, 0, slot)
            && slot.isStrictMode())
            throwTypeError(exec, ASCIILiteral(StrictModeReadonlyPropertyWriteError));
    } else
        thisObject->putInlineSlow(exec, propertyName, value, slot);
}

} // namespace JSC

#endif // JSObjectInlines_h

