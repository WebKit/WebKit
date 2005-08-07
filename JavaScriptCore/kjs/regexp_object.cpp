// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "internal.h"
#include "regexp.h"
#include "regexp_object.h"
#include "error_object.h"

using namespace KJS;

// ------------------------------ RegExpPrototypeImp ---------------------------

// ECMA 15.9.4

const ClassInfo RegExpPrototypeImp::info = {"RegExpPrototype", 0, 0, 0};

RegExpPrototypeImp::RegExpPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto,
                                       FunctionPrototypeImp *funcProto)
  : ObjectImp(objProto)
{
  Value protect(this);
  setInternalValue(String(""));

  // The constructor will be added later in RegExpObject's constructor (?)

  static const Identifier execPropertyName("exec");
  putDirect(execPropertyName,     new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Exec,     0), DontEnum);
  static const Identifier testPropertyName("test");
  putDirect(testPropertyName,     new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Test,     0), DontEnum);
  putDirect(toStringPropertyName, new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::ToString, 0), DontEnum);
}

// ------------------------------ RegExpProtoFuncImp ---------------------------

RegExpProtoFuncImp::RegExpProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool RegExpProtoFuncImp::implementsCall() const
{
  return true;
}

Value RegExpProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&RegExpImp::info)) {
    if (thisObj.inherits(&RegExpPrototypeImp::info)) {
      switch (id) {
        case ToString: return String("//");
      }
    }
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }

  RegExpImp *reimp = static_cast<RegExpImp*>(thisObj.imp());
  RegExp *re = reimp->regExp();
  String s;
  UString str;
  switch (id) {
  case Exec:      // 15.10.6.2
  case Test:
  {
    s = args[0].toString(exec);
    int length = s.value().size();
    Value lastIndex = thisObj.get(exec,"lastIndex");
    int i = lastIndex.isNull() ? 0 : lastIndex.toInt32(exec);
    bool globalFlag = thisObj.get(exec,"global").toBoolean(exec);
    if (!globalFlag)
      i = 0;
    if (i < 0 || i > length) {
      thisObj.put(exec,"lastIndex", Number(0), DontDelete | DontEnum);
      if (id == Test)
        return Boolean(false);
      else
        return Null();
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalInterpreter()->builtinRegExp().imp());
    int **ovector = regExpObj->registerRegexp( re, s.value() );

    str = re->match(s.value(), i, 0L, ovector);
    regExpObj->setSubPatterns(re->subPatterns());

    if (id == Test)
      return Boolean(!str.isNull());

    if (str.isNull()) // no match
    {
      if (globalFlag)
        thisObj.put(exec,"lastIndex",Number(0), DontDelete | DontEnum);
      return Null();
    }
    else // success
    {
      if (globalFlag)
        thisObj.put(exec,"lastIndex",Number( (*ovector)[1] ), DontDelete | DontEnum);
      return regExpObj->arrayOfMatches(exec,str);
    }
  }
  break;
  case ToString:
    s = thisObj.get(exec,"source").toString(exec);
    str = "/";
    str += s.value();
    str += "/";
    if (thisObj.get(exec,"global").toBoolean(exec)) {
      str += "g";
    }
    if (thisObj.get(exec,"ignoreCase").toBoolean(exec)) {
      str += "i";
    }
    if (thisObj.get(exec,"multiline").toBoolean(exec)) {
      str += "m";
    }
    return String(str);
  }

  return Undefined();
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = {"RegExp", 0, 0, 0};

RegExpImp::RegExpImp(RegExpPrototypeImp *regexpProto)
  : ObjectImp(regexpProto), reg(0L)
{
}

RegExpImp::~RegExpImp()
{
  delete reg;
}

// ------------------------------ RegExpObjectImp ------------------------------

RegExpObjectImp::RegExpObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 RegExpPrototypeImp *regProto)

  : InternalFunctionImp(funcProto), lastOvector(0L), lastNrSubPatterns(0)
{
  Value protect(this);
  // ECMA 15.10.5.1 RegExp.prototype
  putDirect(prototypePropertyName, regProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, NumberImp::two(), ReadOnly|DontDelete|DontEnum);
}

RegExpObjectImp::~RegExpObjectImp()
{
  delete [] lastOvector;
}

int **RegExpObjectImp::registerRegexp( const RegExp* re, const UString& s )
{
  lastString = s;
  delete [] lastOvector;
  lastOvector = 0;
  lastNrSubPatterns = re->subPatterns();
  return &lastOvector;
}

Object RegExpObjectImp::arrayOfMatches(ExecState *exec, const UString &result) const
{
  List list;
  // The returned array contains 'result' as first item, followed by the list of matches
  list.append(String(result));
  if ( lastOvector )
    for ( uint i = 1 ; i < lastNrSubPatterns + 1 ; ++i )
    {
      int start = lastOvector[2*i];
      if (start == -1)
        list.append(UndefinedImp::staticUndefined);
      else {
        UString substring = lastString.substr( start, lastOvector[2*i+1] - start );
        list.append(String(substring));
      }
    }
  Object arr = exec->lexicalInterpreter()->builtinArray().construct(exec, list);
  arr.put(exec, "index", Number(lastOvector[0]));
  arr.put(exec, "input", String(lastString));
  return arr;
}

Value RegExpObjectImp::backrefGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot& slot)
{
  RegExpObjectImp *thisObj = static_cast<RegExpObjectImp *>(slot.slotBase());
  unsigned long i = slot.index();

  if (i < thisObj->lastNrSubPatterns + 1) {
    int *lastOvector = thisObj->lastOvector;
    UString substring = thisObj->lastString.substr(lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i] );
    return String(substring);
  } 

  return String("");
}

bool RegExpObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  UString s = propertyName.ustring();
  if (s[0] == '$' && lastOvector)
  {
    bool ok;
    unsigned long i = s.substr(1).toULong(&ok);
    if (ok)
    {
      slot.setCustomIndex(this, i, backrefGetter);
      return true;
    }
  }

  return InternalFunctionImp::getOwnPropertySlot(exec, propertyName, slot);
}

bool RegExpObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.10.4
Object RegExpObjectImp::construct(ExecState *exec, const List &args)
{
  Object o = Object::dynamicCast(args[0]);
  if (!o.isNull() && o.inherits(&RegExpImp::info)) {
    if (args[1].type() != UndefinedType) {
      Object err = Error::create(exec,TypeError);
      exec->setException(err);
      return err;
    }
    return o;
  }
  
  UString p = args[0].type() == UndefinedType ? UString("") : args[0].toString(exec);
  UString flags = args[1].type() == UndefinedType ? UString("") : args[1].toString(exec);

  RegExpPrototypeImp *proto = static_cast<RegExpPrototypeImp*>(exec->lexicalInterpreter()->builtinRegExpPrototype().imp());
  RegExpImp *dat = new RegExpImp(proto);
  Object obj(dat); // protect from GC

  bool global = (flags.find("g") >= 0);
  bool ignoreCase = (flags.find("i") >= 0);
  bool multiline = (flags.find("m") >= 0);
  // TODO: throw a syntax error on invalid flags

  dat->putDirect("global", global ? BooleanImp::staticTrue : BooleanImp::staticFalse, DontDelete | ReadOnly | DontEnum);
  dat->putDirect("ignoreCase", ignoreCase ? BooleanImp::staticTrue : BooleanImp::staticFalse, DontDelete | ReadOnly | DontEnum);
  dat->putDirect("multiline", multiline ? BooleanImp::staticTrue : BooleanImp::staticFalse, DontDelete | ReadOnly | DontEnum);

  dat->putDirect("source", new StringImp(p), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("lastIndex", NumberImp::zero(), DontDelete | DontEnum);

  int reflags = RegExp::None;
  if (global)
      reflags |= RegExp::Global;
  if (ignoreCase)
      reflags |= RegExp::IgnoreCase;
  if (multiline)
      reflags |= RegExp::Multiline;
  dat->setRegExp(new RegExp(p, reflags));

  return obj;
}

bool RegExpObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.10.3
Value RegExpObjectImp::call(ExecState *exec, Object &/*thisObj*/,
			    const List &args)
{
  // TODO: handle RegExp argument case (15.10.3.1)

  return construct(exec, args);
}
