/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "object_object.h"

#include "JSGlobalObject.h"
#include "operations.h"
#include "FunctionPrototype.h"
#include <stdio.h>

namespace KJS {

// ------------------------------ ObjectPrototype --------------------------------

static JSValue* objectProtoFuncValueOf(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncHasOwnProperty(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncIsPrototypeOf(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncDefineGetter(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncDefineSetter(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncLookupGetter(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncLookupSetter(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncPropertyIsEnumerable(ExecState*, JSObject*, const ArgList&);
static JSValue* objectProtoFuncToLocaleString(ExecState*, JSObject*, const ArgList&);

ObjectPrototype::ObjectPrototype(ExecState* exec, FunctionPrototype* functionPrototype)
    : JSObject() // [[Prototype]] is null
{
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, objectProtoFuncToString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toLocaleString, objectProtoFuncToLocaleString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().valueOf, objectProtoFuncValueOf), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().hasOwnProperty, objectProtoFuncHasOwnProperty), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().propertyIsEnumerable, objectProtoFuncPropertyIsEnumerable), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().isPrototypeOf, objectProtoFuncIsPrototypeOf), DontEnum);

    // Mozilla extensions
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 2, exec->propertyNames().__defineGetter__, objectProtoFuncDefineGetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 2, exec->propertyNames().__defineSetter__, objectProtoFuncDefineSetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().__lookupGetter__, objectProtoFuncLookupGetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, exec->propertyNames().__lookupSetter__, objectProtoFuncLookupSetter), DontEnum);
}


// ------------------------------ Functions --------------------------------

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValue* objectProtoFuncValueOf(ExecState*, JSObject* thisObj, const ArgList&)
{
    return thisObj;
}

JSValue* objectProtoFuncHasOwnProperty(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    return jsBoolean(thisObj->hasOwnProperty(exec, Identifier(args[0]->toString(exec))));
}

JSValue* objectProtoFuncIsPrototypeOf(ExecState*, JSObject* thisObj, const ArgList& args)
{
    if (!args[0]->isObject())
        return jsBoolean(false);

    JSValue* v = static_cast<JSObject*>(args[0])->prototype();

    while (true) {
        if (!v->isObject())
            return jsBoolean(false);
        if (thisObj == static_cast<JSObject*>(v))
            return jsBoolean(true);
        v = static_cast<JSObject*>(v)->prototype();
    }
}

JSValue* objectProtoFuncDefineGetter(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!args[1]->isObject() || !static_cast<JSObject*>(args[1])->implementsCall())
        return throwError(exec, SyntaxError, "invalid getter usage");

    thisObj->defineGetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncDefineSetter(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!args[1]->isObject() || !static_cast<JSObject*>(args[1])->implementsCall())
        return throwError(exec, SyntaxError, "invalid setter usage");

    thisObj->defineSetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncLookupGetter(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    return thisObj->lookupGetter(exec, Identifier(args[0]->toString(exec)));
}

JSValue* objectProtoFuncLookupSetter(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    return thisObj->lookupSetter(exec, Identifier(args[0]->toString(exec)));
}

JSValue* objectProtoFuncPropertyIsEnumerable(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    return jsBoolean(thisObj->propertyIsEnumerable(exec, Identifier(args[0]->toString(exec))));
}

JSValue* objectProtoFuncToLocaleString(ExecState* exec, JSObject* thisObj, const ArgList&)
{
    return jsString(thisObj->toString(exec));
}

JSValue* objectProtoFuncToString(ExecState*, JSObject* thisObj, const ArgList&)
{
    return jsString("[object " + thisObj->className() + "]");
}

// ------------------------------ ObjectConstructor --------------------------------

ObjectConstructor::ObjectConstructor(ExecState* exec, ObjectPrototype* objProto, FunctionPrototype* funcProto)
  : InternalFunction(funcProto, "Object")
{
  // ECMA 15.2.3.1
  putDirect(exec->propertyNames().prototype, objProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}

ConstructType ObjectConstructor::getConstructData(ConstructData&)
{
    return ConstructTypeNative;
}

// ECMA 15.2.2
JSObject* ObjectConstructor::construct(ExecState* exec, const ArgList& args)
{
  JSValue* arg = args[0];
  switch (arg->type()) {
  case StringType:
  case BooleanType:
  case NumberType:
  case ObjectType:
      return arg->toObject(exec);
  case NullType:
  case UndefinedType:
      return new JSObject(exec->lexicalGlobalObject()->objectPrototype());
  default:
      ASSERT_NOT_REACHED();
      return 0;
  }
}

JSValue* ObjectConstructor::callAsFunction(ExecState* exec, JSObject* /*thisObj*/, const ArgList &args)
{
    return construct(exec, args);
}

} // namespace KJS
