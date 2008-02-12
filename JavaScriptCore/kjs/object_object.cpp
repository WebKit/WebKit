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
#include "function_object.h"
#include <stdio.h>

namespace KJS {

// ------------------------------ ObjectPrototype --------------------------------

static JSValue* objectProtoFuncValueOf(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncHasOwnProperty(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncIsPrototypeOf(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncDefineGetter(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncDefineSetter(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncLookupGetter(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncLookupSetter(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncPropertyIsEnumerable(ExecState*, JSObject*, const List&);
static JSValue* objectProtoFuncToLocaleString(ExecState*, JSObject*, const List&);

ObjectPrototype::ObjectPrototype(ExecState* exec, FunctionPrototype* functionPrototype)
    : JSObject() // [[Prototype]] is null
{
    static const Identifier* hasOwnPropertyPropertyName = new Identifier("hasOwnProperty");
    static const Identifier* propertyIsEnumerablePropertyName = new Identifier("propertyIsEnumerable");
    static const Identifier* isPrototypeOfPropertyName = new Identifier("isPrototypeOf");
    static const Identifier* defineGetterPropertyName = new Identifier("__defineGetter__");
    static const Identifier* defineSetterPropertyName = new Identifier("__defineSetter__");
    static const Identifier* lookupGetterPropertyName = new Identifier("__lookupGetter__");
    static const Identifier* lookupSetterPropertyName = new Identifier("__lookupSetter__");

    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, objectProtoFuncToString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toLocaleString, objectProtoFuncToLocaleString), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().valueOf, objectProtoFuncValueOf), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, *hasOwnPropertyPropertyName, objectProtoFuncHasOwnProperty), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, *propertyIsEnumerablePropertyName, objectProtoFuncPropertyIsEnumerable), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, *isPrototypeOfPropertyName, objectProtoFuncIsPrototypeOf), DontEnum);

    // Mozilla extensions
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 2, *defineGetterPropertyName, objectProtoFuncDefineGetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 2, *defineSetterPropertyName, objectProtoFuncDefineSetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, *lookupGetterPropertyName, objectProtoFuncLookupGetter), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 1, *lookupSetterPropertyName, objectProtoFuncLookupSetter), DontEnum);
}


// ------------------------------ Functions --------------------------------

// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValue* objectProtoFuncValueOf(ExecState*, JSObject* thisObj, const List&)
{
    return thisObj;
}

JSValue* objectProtoFuncHasOwnProperty(ExecState* exec, JSObject* thisObj, const List& args)
{
    return jsBoolean(thisObj->hasOwnProperty(exec, Identifier(args[0]->toString(exec))));
}

JSValue* objectProtoFuncIsPrototypeOf(ExecState*, JSObject* thisObj, const List& args)
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

JSValue* objectProtoFuncDefineGetter(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!args[1]->isObject() || !static_cast<JSObject*>(args[1])->implementsCall())
        return throwError(exec, SyntaxError, "invalid getter usage");

    thisObj->defineGetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncDefineSetter(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!args[1]->isObject() || !static_cast<JSObject*>(args[1])->implementsCall())
        return throwError(exec, SyntaxError, "invalid setter usage");

    thisObj->defineSetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
    return jsUndefined();
}

JSValue* objectProtoFuncLookupGetter(ExecState* exec, JSObject* thisObj, const List& args)
{
    Identifier propertyName = Identifier(args[0]->toString(exec));
    JSObject* obj = thisObj;
    while (true) {
        JSValue* v = obj->getDirect(propertyName);
        if (v) {
            if (v->type() != GetterSetterType)
                return jsUndefined();
            JSObject* funcObj = static_cast<GetterSetterImp*>(v)->getGetter();
            if (!funcObj)
                return jsUndefined();
            return funcObj;
        }

        if (!obj->prototype() || !obj->prototype()->isObject())
            return jsUndefined();
        obj = static_cast<JSObject*>(obj->prototype());
    }
}

JSValue* objectProtoFuncLookupSetter(ExecState* exec, JSObject* thisObj, const List& args)
{
    Identifier propertyName = Identifier(args[0]->toString(exec));
    JSObject* obj = thisObj;
    while (true) {
        JSValue* v = obj->getDirect(propertyName);
        if (v) {
            if (v->type() != GetterSetterType)
                return jsUndefined();
            JSObject* funcObj = static_cast<GetterSetterImp*>(v)->getSetter();
            if (!funcObj)
                return jsUndefined();
            return funcObj;
        }

        if (!obj->prototype() || !obj->prototype()->isObject())
            return jsUndefined();
        obj = static_cast<JSObject*>(obj->prototype());
    }
}

JSValue* objectProtoFuncPropertyIsEnumerable(ExecState* exec, JSObject* thisObj, const List& args)
{
    return jsBoolean(thisObj->propertyIsEnumerable(exec, Identifier(args[0]->toString(exec))));
}

JSValue* objectProtoFuncToLocaleString(ExecState* exec, JSObject* thisObj, const List&)
{
    return jsString(thisObj->toString(exec));
}

JSValue* objectProtoFuncToString(ExecState*, JSObject* thisObj, const List&)
{
    return jsString("[object " + thisObj->className() + "]");
}

// ------------------------------ ObjectObjectImp --------------------------------

ObjectObjectImp::ObjectObjectImp(ExecState* exec, ObjectPrototype* objProto, FunctionPrototype* funcProto)
  : InternalFunctionImp(funcProto, "Object")
{
  // ECMA 15.2.3.1
  putDirect(exec->propertyNames().prototype, objProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool ObjectObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.2.2
JSObject* ObjectObjectImp::construct(ExecState* exec, const List& args)
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

JSValue* ObjectObjectImp::callAsFunction(ExecState* exec, JSObject* /*thisObj*/, const List &args)
{
    return construct(exec, args);
}

} // namespace KJS
