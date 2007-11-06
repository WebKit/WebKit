/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007 Apple Inc. All Rights Reserved.
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

#include "array_instance.h"
#include "error_object.h"
#include "internal.h"
#include "interpreter.h"
#include "object.h"
#include "operations.h"
#include "regexp.h"
#include "types.h"
#include "value.h"

#include <stdio.h>

namespace KJS {

// ------------------------------ RegExpPrototype ---------------------------

// ECMA 15.10.5

const ClassInfo RegExpPrototype::info = { "RegExpPrototype", 0, 0 };

RegExpPrototype::RegExpPrototype(ExecState *exec,
                                       ObjectPrototype *objProto,
                                       FunctionPrototype *funcProto)
  : JSObject(objProto)
{
  static const Identifier* compilePropertyName = new Identifier("compile");
  static const Identifier* execPropertyName = new Identifier("exec");
  static const Identifier* testPropertyName = new Identifier("test");

  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::Compile, 0, *compilePropertyName), DontEnum);
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::Exec, 0, *execPropertyName), DontEnum);
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::Test, 0, *testPropertyName), DontEnum);
  putDirectFunction(new RegExpProtoFunc(exec, funcProto, RegExpProtoFunc::ToString, 0, exec->propertyNames().toString), DontEnum);
}

// ------------------------------ RegExpProtoFunc ---------------------------

RegExpProtoFunc::RegExpProtoFunc(ExecState* exec, FunctionPrototype* funcProto, int i, int len, const Identifier& name)
   : InternalFunctionImp(funcProto, name), id(i)
{
  putDirect(exec->propertyNames().length, len, DontDelete | ReadOnly | DontEnum);
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
        case Test:
            return static_cast<RegExpImp*>(thisObj)->test(exec, args);
        case Exec:
            return static_cast<RegExpImp*>(thisObj)->exec(exec, args);
  case Compile:
  {
    UString source;
    bool global = false;
    bool ignoreCase = false;
    bool multiline = false;
    if (args.size() > 0) {
      if (args[0]->isObject(&RegExpImp::info)) {
        if (args.size() != 1)
          return throwError(exec, TypeError, "cannot supply flags when constructing one RegExp from another.");

        // Flags are mirrored on the JS object and in the implementation, while source is only preserved on the JS object.
        RegExp* rhsRegExp = static_cast<RegExpImp*>(args[0])->regExp();
        global = rhsRegExp->flags() & RegExp::Global;
        ignoreCase = rhsRegExp->flags() & RegExp::IgnoreCase;
        multiline = rhsRegExp->flags() & RegExp::Multiline;
        source = static_cast<RegExpImp*>(args[0])->get(exec, exec->propertyNames().source)->toString(exec);
      } else
        source = args[0]->toString(exec);

      if (!args[1]->isUndefined()) {
        UString flags = args[1]->toString(exec);

        global = (flags.find("g") >= 0);
        ignoreCase = (flags.find("i") >= 0);
        multiline = (flags.find("m") >= 0);
      }
    }

    int reflags = RegExp::None;
    if (global)
        reflags |= RegExp::Global;
    if (ignoreCase)
        reflags |= RegExp::IgnoreCase;
    if (multiline)
        reflags |= RegExp::Multiline;

    OwnPtr<RegExp> newRegExp(new RegExp(source, reflags));
    if (!newRegExp->isValid())
        return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(newRegExp->errorMessage()));

    thisObj->putDirect(exec->propertyNames().global, jsBoolean(global), DontDelete | ReadOnly | DontEnum);
    thisObj->putDirect(exec->propertyNames().ignoreCase, jsBoolean(ignoreCase), DontDelete | ReadOnly | DontEnum);
    thisObj->putDirect(exec->propertyNames().multiline, jsBoolean(multiline), DontDelete | ReadOnly | DontEnum);
    thisObj->putDirect(exec->propertyNames().source, jsString(source), DontDelete | ReadOnly | DontEnum);
    thisObj->putDirect(exec->propertyNames().lastIndex, jsNumber(0), DontDelete | DontEnum);

    static_cast<RegExpImp*>(thisObj)->setRegExp(newRegExp.release());
    return jsUndefined();
  }
  case ToString:
    UString result = "/" + thisObj->get(exec, exec->propertyNames().source)->toString(exec) + "/";
    if (thisObj->get(exec, exec->propertyNames().global)->toBoolean(exec)) {
      result += "g";
    }
    if (thisObj->get(exec, exec->propertyNames().ignoreCase)->toBoolean(exec)) {
      result += "i";
    }
    if (thisObj->get(exec, exec->propertyNames().multiline)->toBoolean(exec)) {
      result += "m";
    }
    return jsString(result);
  }

  return jsUndefined();
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = { "RegExp", 0, 0 };

RegExpImp::RegExpImp(RegExpPrototype* regexpProto, RegExp* exp)
  : JSObject(regexpProto)
  , m_regExp(exp)
{
}

RegExpImp::~RegExpImp()
{
}

bool RegExpImp::match(ExecState* exec, const List& args)
{
    RegExpObjectImp* regExpObj = exec->lexicalInterpreter()->builtinRegExp();

    UString input;
    if (!args.isEmpty())
        input = args[0]->toString(exec);
    else {
        input = regExpObj->input();
        if (input.isNull()) {
            throwError(exec, GeneralError, "No input.");
            return false;
        }
    }

    bool global = get(exec, exec->propertyNames().global)->toBoolean(exec);
    int lastIndex = 0;
    if (global) {
        double lastIndexDouble = get(exec, exec->propertyNames().lastIndex)->toInteger(exec);
        if (lastIndexDouble < 0 || lastIndexDouble > input.size()) {
            put(exec, exec->propertyNames().lastIndex, jsNumber(0), DontDelete | DontEnum);
            return false;
        }
        lastIndex = static_cast<int>(lastIndexDouble);
    }

    int foundIndex;
    int foundLength;
    regExpObj->performMatch(m_regExp.get(), input, lastIndex, foundIndex, foundLength);

    if (global) {
        lastIndex = foundIndex < 0 ? 0 : foundIndex + foundLength;
        put(exec, exec->propertyNames().lastIndex, jsNumber(lastIndex), DontDelete | DontEnum);
    }

    return foundIndex >= 0;
}

JSValue* RegExpImp::test(ExecState* exec, const List& args)
{
    return jsBoolean(match(exec, args));
}

JSValue* RegExpImp::exec(ExecState* exec, const List& args)
{
    return match(exec, args)
        ? exec->lexicalInterpreter()->builtinRegExp()->arrayOfMatches(exec)
        :  jsNull();
}

bool RegExpImp::implementsCall() const
{
    return true;
}

JSValue* RegExpImp::callAsFunction(ExecState* exec, JSObject*, const List& args)
{
    return RegExpImp::exec(exec, args);
}

// ------------------------------ RegExpObjectImp ------------------------------

const ClassInfo RegExpObjectImp::info = { "Function", &InternalFunctionImp::info, &RegExpTable };

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

struct RegExpObjectImpPrivate {
  // Global search cache / settings
  RegExpObjectImpPrivate() : lastNumSubPatterns(0), multiline(false) { }
  UString lastInput;
  OwnArrayPtr<int> lastOvector;
  unsigned lastNumSubPatterns : 31;
  bool multiline              : 1;
};

RegExpObjectImp::RegExpObjectImp(ExecState* exec, FunctionPrototype* funcProto, RegExpPrototype* regProto)
  : InternalFunctionImp(funcProto)
  , d(new RegExpObjectImpPrivate)
{
  // ECMA 15.10.5.1 RegExp.prototype
  putDirect(exec->propertyNames().prototype, regProto, DontEnum | DontDelete | ReadOnly);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(2), ReadOnly | DontDelete | DontEnum);
}

/* 
  To facilitate result caching, exec(), test(), match(), search(), and replace() dipatch regular
  expression matching through the performMatch function. We use cached results to calculate, 
  e.g., RegExp.lastMatch and RegExp.leftParen.
*/
void RegExpObjectImp::performMatch(RegExp* r, const UString& s, int startOffset, int& position, int& length, int** ovector)
{
  OwnArrayPtr<int> tmpOvector;
  position = r->match(s, startOffset, &tmpOvector);

  if (ovector)
    *ovector = tmpOvector.get();
  
  if (position != -1) {
    ASSERT(tmpOvector);

    length = tmpOvector[1] - tmpOvector[0];

    d->lastInput = s;
    d->lastOvector.set(tmpOvector.release());
    d->lastNumSubPatterns = r->subPatterns();
  }
}

JSObject* RegExpObjectImp::arrayOfMatches(ExecState* exec) const
{
  unsigned lastNumSubpatterns = d->lastNumSubPatterns;
  ArrayInstance* arr = new ArrayInstance(exec->lexicalInterpreter()->builtinArrayPrototype(), lastNumSubpatterns + 1);
  for (unsigned i = 0; i <= lastNumSubpatterns; ++i) {
    int start = d->lastOvector[2 * i];
    if (start >= 0)
      arr->put(exec, i, jsString(d->lastInput.substr(start, d->lastOvector[2 * i + 1] - start)));
  }
  arr->put(exec, exec->propertyNames().index, jsNumber(d->lastOvector[0]));
  arr->put(exec, exec->propertyNames().input, jsString(d->lastInput));
  return arr;
}

JSValue* RegExpObjectImp::getBackref(unsigned i) const
{
  if (d->lastOvector && i <= d->lastNumSubPatterns)
    return jsString(d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  return jsString("");
}

JSValue* RegExpObjectImp::getLastParen() const
{
  unsigned i = d->lastNumSubPatterns;
  if (i > 0) {
    ASSERT(d->lastOvector);
    return jsString(d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  }
  return jsString("");
}

JSValue *RegExpObjectImp::getLeftContext() const
{
  if (d->lastOvector)
    return jsString(d->lastInput.substr(0, d->lastOvector[0]));
  return jsString("");
}

JSValue *RegExpObjectImp::getRightContext() const
{
  if (d->lastOvector) {
    UString s = d->lastInput;
    return jsString(s.substr(d->lastOvector[1], s.size() - d->lastOvector[1]));
  }
  return jsString("");
}

bool RegExpObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpObjectImp, InternalFunctionImp>(exec, &RegExpTable, this, propertyName, slot);
}

JSValue *RegExpObjectImp::getValueProperty(ExecState*, int token) const
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
      return jsString(d->lastInput);
    case Multiline:
      return jsBoolean(d->multiline);
    case LastMatch:
      return getBackref(0);
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

void RegExpObjectImp::putValueProperty(ExecState *exec, int token, JSValue *value, int)
{
  switch (token) {
    case Input:
      d->lastInput = value->toString(exec);
      break;
    case Multiline:
      d->multiline = value->toBoolean(exec);
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
  JSValue* arg0 = args[0];
  JSValue* arg1 = args[1];
  
  JSObject* o = arg0->getObject();
  if (o && o->inherits(&RegExpImp::info)) {
    if (!arg1->isUndefined())
      return throwError(exec, TypeError);
    return o;
  }
  
  UString p = arg0->isUndefined() ? UString("") : arg0->toString(exec);
  UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);

  RegExpPrototype* proto = static_cast<RegExpPrototype*>(exec->lexicalInterpreter()->builtinRegExpPrototype());

  bool global = (flags.find('g') >= 0);
  bool ignoreCase = (flags.find('i') >= 0);
  bool multiline = (flags.find('m') >= 0);

  int reflags = RegExp::None;
  if (global)
      reflags |= RegExp::Global;
  if (ignoreCase)
      reflags |= RegExp::IgnoreCase;
  if (multiline)
      reflags |= RegExp::Multiline;

  OwnPtr<RegExp> re(new RegExp(p, reflags));
  if (!re->isValid())
      return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(re->errorMessage()));

  RegExpImp* dat = new RegExpImp(proto, re.release());

  dat->putDirect(exec->propertyNames().global, jsBoolean(global), DontDelete | ReadOnly | DontEnum);
  dat->putDirect(exec->propertyNames().ignoreCase, jsBoolean(ignoreCase), DontDelete | ReadOnly | DontEnum);
  dat->putDirect(exec->propertyNames().multiline, jsBoolean(multiline), DontDelete | ReadOnly | DontEnum);

  dat->putDirect(exec->propertyNames().source, jsString(p), DontDelete | ReadOnly | DontEnum);
  dat->putDirect(exec->propertyNames().lastIndex, jsNumber(0), DontDelete | DontEnum);

  return dat;
}

// ECMA 15.10.3
JSValue *RegExpObjectImp::callAsFunction(ExecState *exec, JSObject * /*thisObj*/, const List &args)
{
  return construct(exec, args);
}

const UString& RegExpObjectImp::input() const
{
    // Can detect a distinct initial state that is invisible to JavaScript, by checking for null
    // state (since jsString turns null strings to empty strings).
    return d->lastInput;
}

}
