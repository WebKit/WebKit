// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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

#include "operations.h"
#include "function_object.h"
#include <stdio.h>

using namespace KJS;

// ------------------------------ ObjectPrototype --------------------------------

ObjectPrototype::ObjectPrototype(ExecState* exec, FunctionPrototype* funcProto)
  : JSObject() // [[Prototype]] is null
{
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ToString, 0, toStringPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ToLocaleString, 0, toLocaleStringPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::ValueOf, 0, valueOfPropertyName), DontEnum);
    static Identifier hasOwnPropertyPropertyName("hasOwnProperty");
    static Identifier propertyIsEnumerablePropertyName("propertyIsEnumerable");
    static Identifier isPrototypeOfPropertyName("isPrototypeOf");
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::HasOwnProperty, 1, hasOwnPropertyPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::PropertyIsEnumerable, 1, propertyIsEnumerablePropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::IsPrototypeOf, 1, isPrototypeOfPropertyName), DontEnum);

    // Mozilla extensions
    static const Identifier defineGetterPropertyName("__defineGetter__");
    static const Identifier defineSetterPropertyName("__defineSetter__");
    static const Identifier lookupGetterPropertyName("__lookupGetter__");
    static const Identifier lookupSetterPropertyName("__lookupSetter__");
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::DefineGetter, 2, defineGetterPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::DefineSetter, 2, defineSetterPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::LookupGetter, 1, lookupGetterPropertyName), DontEnum);
    putDirectFunction(new ObjectProtoFunc(exec, funcProto, ObjectProtoFunc::LookupSetter, 1, lookupSetterPropertyName), DontEnum);
}


// ------------------------------ ObjectProtoFunc --------------------------------

ObjectProtoFunc::ObjectProtoFunc(ExecState*, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
  : InternalFunctionImp(funcProto, name)
  , id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}


// ECMA 15.2.4.2, 15.2.4.4, 15.2.4.5, 15.2.4.7

JSValue *ObjectProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    switch (id) {
        case ValueOf:
            return thisObj;
        case HasOwnProperty: {
            PropertySlot slot;
            return jsBoolean(thisObj->getOwnPropertySlot(exec, Identifier(args[0]->toString(exec)), slot));
        }
        case IsPrototypeOf: {
            if (!args[0]->isObject())
                return jsBoolean(false);
         
            JSValue *v = static_cast<JSObject *>(args[0])->prototype();

            while (true) {
                if (!v->isObject())
                    return jsBoolean(false);
                
                if (thisObj == static_cast<JSObject *>(v))
                    return jsBoolean(true);
                
                v = static_cast<JSObject *>(v)->prototype();
            }
        }
        case DefineGetter: 
        case DefineSetter: {
            if (!args[1]->isObject() ||
                !static_cast<JSObject *>(args[1])->implementsCall()) {
                if (id == DefineGetter)
                    return throwError(exec, SyntaxError, "invalid getter usage");
                else
                    return throwError(exec, SyntaxError, "invalid setter usage");
            }

            if (id == DefineGetter)
                thisObj->defineGetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
            else
                thisObj->defineSetter(exec, Identifier(args[0]->toString(exec)), static_cast<JSObject *>(args[1]));
            return jsUndefined();
        }
        case LookupGetter:
        case LookupSetter: {
            Identifier propertyName = Identifier(args[0]->toString(exec));
            
            JSObject *obj = thisObj;
            while (true) {
                JSValue *v = obj->getDirect(propertyName);
                
                if (v) {
                    if (v->type() != GetterSetterType)
                        return jsUndefined();

                    JSObject *funcObj;
                        
                    if (id == LookupGetter)
                        funcObj = static_cast<GetterSetterImp *>(v)->getGetter();
                    else
                        funcObj = static_cast<GetterSetterImp *>(v)->getSetter();
                
                    if (!funcObj)
                        return jsUndefined();
                    else
                        return funcObj;
                }
                
                if (!obj->prototype() || !obj->prototype()->isObject())
                    return jsUndefined();
                
                obj = static_cast<JSObject *>(obj->prototype());
            }
        }
        case PropertyIsEnumerable:
            return jsBoolean(thisObj->propertyIsEnumerable(exec, Identifier(args[0]->toString(exec))));
        case ToLocaleString:
            return jsString(thisObj->toString(exec));
        case ToString:
        default:
            return jsString("[object " + thisObj->className() + "]");
    }
}

// ------------------------------ ObjectObjectImp --------------------------------

ObjectObjectImp::ObjectObjectImp(ExecState*, ObjectPrototype* objProto, FunctionPrototype* funcProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.2.3.1
  putDirect(prototypePropertyName, objProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
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
      return new JSObject(exec->lexicalInterpreter()->builtinObjectPrototype());
  default:
      ASSERT_NOT_REACHED();
      return 0;
  }
}

JSValue* ObjectObjectImp::callAsFunction(ExecState* exec, JSObject* /*thisObj*/, const List &args)
{
    return construct(exec, args);
}

