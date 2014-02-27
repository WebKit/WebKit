/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "config.h"
#include "GetterSetter.h"

#include "Error.h"
#include "JSObject.h"
#include "JSCInlines.h"
#include <wtf/Assertions.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(GetterSetter);

const ClassInfo GetterSetter::s_info = { "GetterSetter", 0, 0, 0, CREATE_METHOD_TABLE(GetterSetter) };

void GetterSetter::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    GetterSetter* thisObject = jsCast<GetterSetter*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    JSCell::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_getter);
    visitor.append(&thisObject->m_setter);
}

JSValue callGetter(ExecState* exec, JSValue base, JSValue getterSetter)
{
    // FIXME: Some callers may invoke get() without checking for an exception first.
    // We work around that by checking here.
    if (exec->hadException())
        return exec->exception();

    JSObject* getter = jsCast<GetterSetter*>(getterSetter)->getter();
    if (!getter)
        return jsUndefined();

    CallData callData;
    CallType callType = getter->methodTable(exec->vm())->getCallData(getter, callData);
    return call(exec, getter, callType, callData, base, ArgList());
}

void callSetter(ExecState* exec, JSValue base, JSValue getterSetter, JSValue value, ECMAMode ecmaMode)
{
    JSObject* setter = jsCast<GetterSetter*>(getterSetter)->setter();
    if (!setter) {
        if (ecmaMode == StrictMode)
            throwTypeError(exec, StrictModeReadonlyPropertyWriteError);
        return;
    }

    MarkedArgumentBuffer args;
    args.append(value);

    CallData callData;
    CallType callType = setter->methodTable(exec->vm())->getCallData(setter, callData);
    call(exec, setter, callType, callData, base, args);
}

} // namespace JSC
