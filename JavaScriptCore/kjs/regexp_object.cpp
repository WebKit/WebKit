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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

RegExpPrototypeImp::RegExpPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto,
                                       FunctionPrototypeImp *funcProto)
  : ObjectImp(Object(objProto))
{
  Value protect(this);
  setInternalValue(String(""));

  // The constructor will be added later in RegExpObject's constructor (?)

  put(exec, "exec",     Object(new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Exec,     0)), DontEnum);
  put(exec, "test",     Object(new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::Test,     0)), DontEnum);
  put(exec, "toString", Object(new RegExpProtoFuncImp(exec,funcProto,RegExpProtoFuncImp::ToString, 0)), DontEnum);
}

// ------------------------------ RegExpProtoFuncImp ---------------------------

RegExpProtoFuncImp::RegExpProtoFuncImp(ExecState *exec,
                                       FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  put(exec,"length",Number(len),DontDelete|ReadOnly|DontEnum);
}

bool RegExpProtoFuncImp::implementsCall() const
{
  return true;
}

Value RegExpProtoFuncImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&RegExpImp::info)) {
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
        Null();
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->interpreter()->builtinRegExp().imp());
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
    // TODO append the flags
    return String(str);
  }

  return Undefined();
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = {"RegExp", 0, 0, 0};

RegExpImp::RegExpImp(RegExpPrototypeImp *regexpProto)
  : ObjectImp(Object(regexpProto)), reg(0L)
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
  put(exec,"prototype", Object(regProto), DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  put(exec,"length", Number(2), ReadOnly|DontDelete|DontEnum);
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
      UString substring = lastString.substr( lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i] );
      list.append(String(substring));
    }
  Object arr = exec->interpreter()->builtinArray().construct(exec, list);
  arr.put(exec, "index", Number(lastOvector[0]));
  arr.put(exec, "input", String(lastString));
  return arr;
}

Value RegExpObjectImp::get(ExecState *exec, const UString &p) const
{
  if (p[0] == '$' && lastOvector)
  {
    bool ok;
    unsigned long i = p.substr(1).toULong(&ok);
    if (ok)
    {
      if (i < lastNrSubPatterns + 1)
      {
        UString substring = lastString.substr( lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i] );
        return String(substring);
      }
      return String("");
    }
  }
  return InternalFunctionImp::get(exec, p);
}

bool RegExpObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.10.4
Object RegExpObjectImp::construct(ExecState *exec, const List &args)
{
  String p = args.isEmpty() ? UString("") : args[0].toString(exec);
  UString flags = args[1].toString(exec);

  RegExpPrototypeImp *proto = static_cast<RegExpPrototypeImp*>(exec->interpreter()->builtinRegExpPrototype().imp());
  RegExpImp *dat = new RegExpImp(proto);
  Object obj(dat); // protect from GC

  bool global = (flags.find("g") >= 0);
  bool ignoreCase = (flags.find("i") >= 0);
  bool multiline = (flags.find("m") >= 0);
  // TODO: throw a syntax error on invalid flags

  dat->put(exec, "global", Boolean(global));
  dat->put(exec, "ignoreCase", Boolean(ignoreCase));
  dat->put(exec, "multiline", Boolean(multiline));

  dat->put(exec, "source", p);
  dat->put(exec, "lastIndex", Number(0), DontDelete | DontEnum);

  int reflags = RegExp::None;
  if (global)
      reflags |= RegExp::Global;
  if (ignoreCase)
      reflags |= RegExp::IgnoreCase;
  if (multiline)
      reflags |= RegExp::Multiline;
  dat->setRegExp(new RegExp(p.value(), reflags));

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
