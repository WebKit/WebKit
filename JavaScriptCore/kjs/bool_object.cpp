/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "bool_object.h"

#include "JSGlobalObject.h"
#include "error_object.h"
#include "operations.h"
#include <wtf/Assertions.h>

namespace KJS {

// ------------------------------ BooleanInstance ---------------------------

const ClassInfo BooleanInstance::info = { "Boolean", 0, 0 };

BooleanInstance::BooleanInstance(JSObject* proto)
    : JSWrapperObject(proto)
{
}

// ------------------------------ BooleanPrototype --------------------------

// Functions
static JSValue* booleanProtoFuncToString(ExecState*, JSObject*, const List&);
static JSValue* booleanProtoFuncValueOf(ExecState*, JSObject*, const List&);

// ECMA 15.6.4

BooleanPrototype::BooleanPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : BooleanInstance(objectPrototype)
{
    setInternalValue(jsBoolean(false));

    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, booleanProtoFuncToString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().valueOf, booleanProtoFuncValueOf), DontEnum);
}


// ------------------------------ Functions --------------------------

// ECMA 15.6.4.2 + 15.6.4.3

JSValue* booleanProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&BooleanInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<BooleanInstance*>(thisObj)->internalValue();
    ASSERT(v);

    return jsString(v->toString(exec));
}
JSValue* booleanProtoFuncValueOf(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&BooleanInstance::info))
        return throwError(exec, TypeError);

    JSValue* v = static_cast<BooleanInstance*>(thisObj)->internalValue();
    ASSERT(v);

    // TODO: optimize for bool case
    return jsBoolean(v->toBoolean(exec));
}

// ------------------------------ BooleanObjectImp -----------------------------


BooleanObjectImp::BooleanObjectImp(ExecState* exec, FunctionPrototype* functionPrototype, BooleanPrototype* booleanPrototype)
    : InternalFunctionImp(functionPrototype, booleanPrototype->classInfo()->className)
{
    putDirect(exec->propertyNames().prototype, booleanPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly | DontDelete | DontEnum);
}


bool BooleanObjectImp::implementsConstruct() const
{
    return true;
}

// ECMA 15.6.2
JSObject* BooleanObjectImp::construct(ExecState* exec, const List& args)
{
    BooleanInstance* obj(new BooleanInstance(exec->lexicalGlobalObject()->booleanPrototype()));
    obj->setInternalValue(jsBoolean(args[0]->toBoolean(exec)));
    return obj;
}

// ECMA 15.6.1
JSValue* BooleanObjectImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    // TODO: optimize for bool case
    return jsBoolean(args[0]->toBoolean(exec));
}

} // namespace KJS
