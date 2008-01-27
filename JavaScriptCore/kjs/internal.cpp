/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "internal.h"

#include "ExecState.h"
#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "lexer.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "string_object.h"
#include <math.h>
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {

// ------------------------------ StringImp ------------------------------------

JSValue* StringImp::toPrimitive(ExecState*, JSType) const
{
  return const_cast<StringImp*>(this);
}

bool StringImp::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    value = this;
    number = val.toDouble();
    return false;
}

bool StringImp::toBoolean(ExecState *) const
{
  return (val.size() > 0);
}

double StringImp::toNumber(ExecState *) const
{
  return val.toDouble();
}

UString StringImp::toString(ExecState *) const
{
  return val;
}

JSObject* StringImp::toObject(ExecState *exec) const
{
    return new StringInstance(exec->lexicalGlobalObject()->stringPrototype(), const_cast<StringImp*>(this));
}

// ------------------------------ NumberImp ------------------------------------

JSValue* NumberImp::toPrimitive(ExecState*, JSType) const
{
    return const_cast<NumberImp*>(this);
}

bool NumberImp::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    number = val;
    value = this;
    return true;
}

bool NumberImp::toBoolean(ExecState *) const
{
  return val < 0.0 || val > 0.0; // false for NaN
}

double NumberImp::toNumber(ExecState *) const
{
  return val;
}

UString NumberImp::toString(ExecState *) const
{
  if (val == 0.0) // +0.0 or -0.0
    return "0";
  return UString::from(val);
}

JSObject *NumberImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<NumberImp*>(this));
  return static_cast<JSObject *>(exec->lexicalGlobalObject()->numberConstructor()->construct(exec,args));
}

bool NumberImp::getUInt32(uint32_t& uint32) const
{
    uint32 = static_cast<uint32_t>(val);
    return uint32 == val;
}

bool NumberImp::getTruncatedInt32(int32_t& int32) const
{
    if (!(val >= -2147483648.0 && val < 2147483648.0))
        return false;
    int32 = static_cast<int32_t>(val);
    return true;
}

bool NumberImp::getTruncatedUInt32(uint32_t& uint32) const
{
    if (!(val >= 0.0 && val < 4294967296.0))
        return false;
    uint32 = static_cast<uint32_t>(val);
    return true;
}

// --------------------------- GetterSetterImp ---------------------------------
void GetterSetterImp::mark()
{
    JSCell::mark();
    
    if (getter && !getter->marked())
        getter->mark();
    if (setter && !setter->marked())
        setter->mark();
}

JSValue* GetterSetterImp::toPrimitive(ExecState*, JSType) const
{
    ASSERT(false);
    return jsNull();
}

bool GetterSetterImp::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    ASSERT_NOT_REACHED();
    number = 0;
    value = 0;
    return true;
}

bool GetterSetterImp::toBoolean(ExecState*) const
{
    ASSERT(false);
    return false;
}

double GetterSetterImp::toNumber(ExecState *) const
{
    ASSERT(false);
    return 0.0;
}

UString GetterSetterImp::toString(ExecState *) const
{
    ASSERT(false);
    return UString::null();
}

JSObject *GetterSetterImp::toObject(ExecState *exec) const
{
    ASSERT(false);
    return jsNull()->toObject(exec);
}

// ------------------------------ LabelStack -----------------------------------

bool LabelStack::push(const Identifier &id)
{
  if (contains(id))
    return false;

  StackElem *newtos = new StackElem;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const Identifier &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElem *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

// ------------------------------ InternalFunctionImp --------------------------

const ClassInfo InternalFunctionImp::info = { "Function", 0, 0 };

InternalFunctionImp::InternalFunctionImp()
{
}

InternalFunctionImp::InternalFunctionImp(FunctionPrototype* funcProto, const Identifier& name)
  : JSObject(funcProto)
  , m_name(name)
{
}

bool InternalFunctionImp::implementsCall() const
{
  return true;
}

bool InternalFunctionImp::implementsHasInstance() const
{
  return true;
}

// ------------------------------ global functions -----------------------------

#ifndef NDEBUG
#include <stdio.h>
void printInfo(ExecState *exec, const char *s, JSValue *o, int lineno)
{
  if (!o)
    fprintf(stderr, "KJS: %s: (null)", s);
  else {
    JSValue *v = o;

    UString name;
    switch (v->type()) {
    case UnspecifiedType:
      name = "Unspecified";
      break;
    case UndefinedType:
      name = "Undefined";
      break;
    case NullType:
      name = "Null";
      break;
    case BooleanType:
      name = "Boolean";
      break;
    case StringType:
      name = "String";
      break;
    case NumberType:
      name = "Number";
      break;
    case ObjectType:
      name = static_cast<JSObject *>(v)->className();
      if (name.isNull())
        name = "(unknown class)";
      break;
    case GetterSetterType:
      name = "GetterSetter";
      break;
    }
    UString vString = v->toString(exec);
    if ( vString.size() > 50 )
      vString = vString.substr( 0, 50 ) + "...";
    // Can't use two UString::ascii() in the same fprintf call
    CString tempString( vString.cstring() );

    fprintf(stderr, "KJS: %s: %s : %s (%p)",
            s, tempString.c_str(), name.ascii(), (void*)v);

    if (lineno >= 0)
      fprintf(stderr, ", line %d\n",lineno);
    else
      fprintf(stderr, "\n");
  }
}
#endif

}
