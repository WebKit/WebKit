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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "regexp_object.h"

#include "regexp_object.lut.h"

#include <stdio.h>
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "internal.h"
#include "regexp.h"
#include "error_object.h"
#include "lookup.h"

using namespace KJS;

// ------------------------------ RegExpPrototype ---------------------------

// ECMA 15.9.4

const ClassInfo RegExpPrototype::info = {"RegExpPrototype", 0, 0, 0};

RegExpPrototype::RegExpPrototype(ExecState *exec,
                                       ObjectPrototype *objProto,
                                       FunctionPrototype *funcProto)
  : JSObject(objProto)
{
  setInternalValue(jsString(""));

  // The constructor will be added later in RegExpObject's constructor (?)

  static const Identifier execPropertyName("exec");
  static const Identifier testPropertyName("test");
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::Exec, 0, execPropertyName), DontEnum);
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::Test, 0, testPropertyName), DontEnum);
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::ToString, 0, toStringPropertyName), DontEnum);
}

// ------------------------------ RegExpProtoFunc ---------------------------

RegExpProtoFunc::RegExpProtoFunc(ExecState*, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
   : InternalFunctionImp(funcProto, name), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool RegExpProtoFunc::implementsCall() const
{
  return true;
}

JSValue *RegExpProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&RegExpImp::info)) {
    if (thisObj->inherits(&RegExpPrototype::info)) {
      switch (id) {
        case ToString: return jsString("//");
      }
    }
    
    return throwError(exec, TypeError);
  }

  switch (id) {
  case Test:      // 15.10.6.2
  case Exec:
  {
    RegExp *regExp = static_cast<RegExpImp*>(thisObj)->regExp();
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalInterpreter()->builtinRegExp());

    UString input;
    if (args.isEmpty())
      input = regExpObj->get(exec, "input")->toString(exec);
    else
      input = args[0]->toString(exec);

    double lastIndex = thisObj->get(exec, "lastIndex")->toInteger(exec);

    bool globalFlag = thisObj->get(exec, "global")->toBoolean(exec);
    if (!globalFlag)
      lastIndex = 0;
    if (lastIndex < 0 || lastIndex > input.size()) {
      thisObj->put(exec, "lastIndex", jsNumber(0), DontDelete | DontEnum);
      return jsNull();
    }

    int foundIndex;
    UString match = regExpObj->performMatch(regExp, input, static_cast<int>(lastIndex), &foundIndex);
    bool didMatch = !match.isNull();

    // Test
    if (id == Test)
      return jsBoolean(didMatch);

    // Exec
    if (didMatch) {
      if (globalFlag)
        thisObj->put(exec, "lastIndex", jsNumber(foundIndex + match.size()), DontDelete | DontEnum);
      return regExpObj->arrayOfMatches(exec, match);
    } else {
      if (globalFlag)
        thisObj->put(exec, "lastIndex", jsNumber(0), DontDelete | DontEnum);
      return jsNull();
    }
  }
  break;
  case ToString:
    UString result = "/" + thisObj->get(exec, "source")->toString(exec) + "/";
    if (thisObj->get(exec, "global")->toBoolean(exec)) {
      result += "g";
    }
    if (thisObj->get(exec, "ignoreCase")->toBoolean(exec)) {
      result += "i";
    }
    if (thisObj->get(exec, "multiline")->toBoolean(exec)) {
      result += "m";
    }
    return jsString(result);
  }

  return jsUndefined();
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = {"RegExp", 0, 0, 0};

RegExpImp::RegExpImp(RegExpPrototype *regexpProto)
  : JSObject(regexpProto), reg(0L)
{
}

RegExpImp::~RegExpImp()
{
  delete reg;
}

// ------------------------------ RegExpObjectImp ------------------------------

const ClassInfo RegExpObjectImp::info = {"Function", &InternalFunctionImp::info, &RegExpTable, 0};

/* Source for regexp_object.lut.h
@begin RegExpTable 20
  input           RegExpObjectImp::Input          None
  $_              RegExpObjectImp::Input          DontEnum
  multiline       RegExpObjectImp::Multiline      None
  $*              RegExpObjectImp::Multiline      DontEnum
  lastMatch       RegExpObjectImp::LastMatch      DontDelete|ReadOnly
  $&              RegExpObjectImp::LastMatch      DontDelete|ReadOnly|DontEnum
  lastParen       RegExpObjectImp::LastParen      DontDelete|ReadOnly
  $+              RegExpObjectImp::LastParen      DontDelete|ReadOnly|DontEnum
  leftContext     RegExpObjectImp::LeftContext    DontDelete|ReadOnly
  $`              RegExpObjectImp::LeftContext    DontDelete|ReadOnly|DontEnum
  rightContext    RegExpObjectImp::RightContext   DontDelete|ReadOnly
  $'              RegExpObjectImp::RightContext   DontDelete|ReadOnly|DontEnum
  $1              RegExpObjectImp::Dollar1        DontDelete|ReadOnly
  $2              RegExpObjectImp::Dollar2        DontDelete|ReadOnly
  $3              RegExpObjectImp::Dollar3        DontDelete|ReadOnly
  $4              RegExpObjectImp::Dollar4        DontDelete|ReadOnly
  $5              RegExpObjectImp::Dollar5        DontDelete|ReadOnly
  $6              RegExpObjectImp::Dollar6        DontDelete|ReadOnly
  $7              RegExpObjectImp::Dollar7        DontDelete|ReadOnly
  $8              RegExpObjectImp::Dollar8        DontDelete|ReadOnly
  $9              RegExpObjectImp::Dollar9        DontDelete|ReadOnly
@end
*/

RegExpObjectImp::RegExpObjectImp(ExecState *exec,
                                 FunctionPrototype *funcProto,
                                 RegExpPrototype *regProto)

  : InternalFunctionImp(funcProto), multiline(false), lastInput(""), lastNumSubPatterns(0)
{
  // ECMA 15.10.5.1 RegExp.prototype
  putDirect(prototypePropertyName, regProto, DontEnum|DontDelete|ReadOnly);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(2), ReadOnly|DontDelete|DontEnum);
}

/* 
  To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
  expression matching through the performMatch function. We use cached results to calculate, 
  e.g., RegExp.lastMatch and RegExp.leftParen.
*/
UString RegExpObjectImp::performMatch(RegExp* r, const UString& s, int startOffset, int *endOffset, int **ovector)
{
  int tmpOffset;
  int *tmpOvector;
  UString match = r->match(s, startOffset, &tmpOffset, &tmpOvector);

  if (endOffset)
    *endOffset = tmpOffset;
  if (ovector)
    *ovector = tmpOvector;
  
  if (!match.isNull()) {
    ASSERT(tmpOvector);
    
    lastInput = s;
    lastOvector.set(tmpOvector);
    lastNumSubPatterns = r->subPatterns();
  }
  
  return match;
}

JSObject *RegExpObjectImp::arrayOfMatches(ExecState *exec, const UString &result) const
{
  List list;
  // The returned array contains 'result' as first item, followed by the list of matches
  list.append(jsString(result));
  if ( lastOvector )
    for ( unsigned i = 1 ; i < lastNumSubPatterns + 1 ; ++i )
    {
      int start = lastOvector[2*i];
      if (start == -1)
        list.append(jsUndefined());
      else {
        UString substring = lastInput.substr( start, lastOvector[2*i+1] - start );
        list.append(jsString(substring));
      }
    }
  JSObject *arr = exec->lexicalInterpreter()->builtinArray()->construct(exec, list);
  arr->put(exec, "index", jsNumber(lastOvector[0]));
  arr->put(exec, "input", jsString(lastInput));
  return arr;
}

JSValue *RegExpObjectImp::getBackref(unsigned i) const
{
  if (lastOvector && i < lastNumSubPatterns + 1) {
    UString substring = lastInput.substr(lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i] );
    return jsString(substring);
  } 

  return jsString("");
}

JSValue *RegExpObjectImp::getLastMatch() const
{
  if (lastOvector) {
    UString substring = lastInput.substr(lastOvector[0], lastOvector[1] - lastOvector[0]);
    return jsString(substring);
  }
  
  return jsString("");
}

JSValue *RegExpObjectImp::getLastParen() const
{
  int i = lastNumSubPatterns;
  if (i > 0) {
    ASSERT(lastOvector);
    UString substring = lastInput.substr(lastOvector[2*i], lastOvector[2*i+1] - lastOvector[2*i]);
    return jsString(substring);
  }
    
  return jsString("");
}

JSValue *RegExpObjectImp::getLeftContext() const
{
  if (lastOvector) {
    UString substring = lastInput.substr(0, lastOvector[0]);
    return jsString(substring);
  }
  
  return jsString("");
}

JSValue *RegExpObjectImp::getRightContext() const
{
  if (lastOvector) {
    UString s = lastInput;
    UString substring = s.substr(lastOvector[1], s.size() - lastOvector[1]);
    return jsString(substring);
  }
  
  return jsString("");
}

bool RegExpObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpObjectImp, InternalFunctionImp>(exec, &RegExpTable, this, propertyName, slot);
}

JSValue *RegExpObjectImp::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
    case Dollar1:
      return getBackref(1);
    case Dollar2:
      return getBackref(2);
    case Dollar3:
      return getBackref(3);
    case Dollar4:
      return getBackref(4);
    case Dollar5:
      return getBackref(5);
    case Dollar6:
      return getBackref(6);
    case Dollar7:
      return getBackref(7);
    case Dollar8:
      return getBackref(8);
    case Dollar9:
      return getBackref(9);
    case Input:
      return jsString(lastInput);
    case Multiline:
      return jsBoolean(multiline);
    case LastMatch:
      return getLastMatch();
    case LastParen:
      return getLastParen();
    case LeftContext:
      return getLeftContext();
    case RightContext:
      return getRightContext();
    default:
      ASSERT(0);
  }

  return jsString("");
}

void RegExpObjectImp::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  lookupPut<RegExpObjectImp, InternalFunctionImp>(exec, propertyName, value, attr, &RegExpTable, this);
}

void RegExpObjectImp::putValueProperty(ExecState *exec, int token, JSValue *value, int attr)
{
  switch (token) {
    case Input:
      lastInput = value->toString(exec);
      break;
    case Multiline:
      multiline = value->toBoolean(exec);
      break;
    default:
      ASSERT(0);
  }
}
  
bool RegExpObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.10.4
JSObject *RegExpObjectImp::construct(ExecState *exec, const List &args)
{
  JSObject *o = args[0]->getObject();
  if (o && o->inherits(&RegExpImp::info)) {
    if (!args[1]->isUndefined())
      return throwError(exec, TypeError);
    return o;
  }
  
  UString p = args[0]->isUndefined() ? UString("") : args[0]->toString(exec);
  UString flags = args[1]->isUndefined() ? UString("") : args[1]->toString(exec);

  RegExpPrototype *proto = static_cast<RegExpPrototype*>(exec->lexicalInterpreter()->builtinRegExpPrototype());
  RegExpImp *dat = new RegExpImp(proto);

  bool global = (flags.find("g") >= 0);
  bool ignoreCase = (flags.find("i") >= 0);
  bool multiline = (flags.find("m") >= 0);
  // TODO: throw a syntax error on invalid flags

  dat->putDirect("global", jsBoolean(global), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("ignoreCase", jsBoolean(ignoreCase), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("multiline", jsBoolean(multiline), DontDelete | ReadOnly | DontEnum);

  dat->putDirect("source", jsString(p), DontDelete | ReadOnly | DontEnum);
  dat->putDirect("lastIndex", jsNumber(0), DontDelete | DontEnum);

  int reflags = RegExp::None;
  if (global)
      reflags |= RegExp::Global;
  if (ignoreCase)
      reflags |= RegExp::IgnoreCase;
  if (multiline)
      reflags |= RegExp::Multiline;
  dat->setRegExp(new RegExp(p, reflags));

  return dat;
}

bool RegExpObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.10.3
JSValue *RegExpObjectImp::callAsFunction(ExecState *exec, JSObject * /*thisObj*/, const List &args)
{
  // TODO: handle RegExp argument case (15.10.3.1)

  return construct(exec, args);
}
