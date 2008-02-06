/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All Rights Reserved.
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
#include "array_object.h"
#include "error_object.h"
#include "internal.h"
#include "object.h"
#include "operations.h"
#include "regexp.h"
#include "types.h"
#include "value.h"
#include "UnusedParam.h"

#include <stdio.h>

namespace KJS {

// ------------------------------ RegExpPrototype ---------------------------

static JSValue* regExpProtoFuncTest(ExecState*, JSObject*, const List&);
static JSValue* regExpProtoFuncExec(ExecState*, JSObject*, const List&);
static JSValue* regExpProtoFuncCompile(ExecState*, JSObject*, const List&);
static JSValue* regExpProtoFuncToString(ExecState*, JSObject*, const List&);

// ECMA 15.10.5

const ClassInfo RegExpPrototype::info = { "RegExpPrototype", 0, 0 };

RegExpPrototype::RegExpPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : JSObject(objectPrototype)
{
    static const Identifier* compilePropertyName = new Identifier("compile");
    static const Identifier* execPropertyName = new Identifier("exec");
    static const Identifier* testPropertyName = new Identifier("test");

    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, *compilePropertyName, regExpProtoFuncCompile), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, *execPropertyName, regExpProtoFuncExec), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, *testPropertyName, regExpProtoFuncTest), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, regExpProtoFuncToString), DontEnum);
}

// ------------------------------ Functions ---------------------------
    
JSValue* regExpProtoFuncTest(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&RegExpImp::info))
        return throwError(exec, TypeError);

    return static_cast<RegExpImp*>(thisObj)->test(exec, args);
}

JSValue* regExpProtoFuncExec(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&RegExpImp::info))
        return throwError(exec, TypeError);

    return static_cast<RegExpImp*>(thisObj)->exec(exec, args);
}

JSValue* regExpProtoFuncCompile(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&RegExpImp::info))
        return throwError(exec, TypeError);

    RefPtr<RegExp> regExp;
    JSValue* arg0 = args[0];
    JSValue* arg1 = args[1];
    
    if (arg0->isObject(&RegExpImp::info)) {
        if (!arg1->isUndefined())
            return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
        regExp = static_cast<RegExpImp*>(arg0)->regExp();
    } else {
        UString pattern = args.isEmpty() ? UString("") : arg0->toString(exec);
        UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
        regExp = new RegExp(pattern, flags);
    }

    if (!regExp->isValid())
        return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));

    static_cast<RegExpImp*>(thisObj)->setRegExp(regExp.release());
    static_cast<RegExpImp*>(thisObj)->setLastIndex(0);
    return jsUndefined();
}

JSValue* regExpProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&RegExpImp::info)) {
        if (thisObj->inherits(&RegExpPrototype::info))
            return jsString("//");
        return throwError(exec, TypeError);
    }

    UString result = "/" + thisObj->get(exec, exec->propertyNames().source)->toString(exec) + "/";
    if (thisObj->get(exec, exec->propertyNames().global)->toBoolean(exec))
        result += "g";
    if (thisObj->get(exec, exec->propertyNames().ignoreCase)->toBoolean(exec))
        result += "i";
    if (thisObj->get(exec, exec->propertyNames().multiline)->toBoolean(exec))
        result += "m";
    return jsString(result);
}

// ------------------------------ RegExpImp ------------------------------------

const ClassInfo RegExpImp::info = { "RegExp", 0, &RegExpImpTable };

/* Source for regexp_object.lut.h
@begin RegExpImpTable 5
    global        RegExpImp::Global       DontDelete|ReadOnly|DontEnum
    ignoreCase    RegExpImp::IgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     RegExpImp::Multiline    DontDelete|ReadOnly|DontEnum
    source        RegExpImp::Source       DontDelete|ReadOnly|DontEnum
    lastIndex     RegExpImp::LastIndex    DontDelete|DontEnum
@end
*/

RegExpImp::RegExpImp(RegExpPrototype* regexpProto, PassRefPtr<RegExp> regExp)
  : JSObject(regexpProto)
  , m_regExp(regExp)
  , m_lastIndex(0)
{
}

RegExpImp::~RegExpImp()
{
}

bool RegExpImp::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpImp, JSObject>(exec, &RegExpImpTable, this, propertyName, slot);
}

JSValue* RegExpImp::getValueProperty(ExecState*, int token) const
{
    switch (token) {
        case Global:
            return jsBoolean(m_regExp->global());
        case IgnoreCase:
            return jsBoolean(m_regExp->ignoreCase());
        case Multiline:
            return jsBoolean(m_regExp->multiline());
        case Source:
            return jsString(m_regExp->pattern());
        case LastIndex:
            return jsNumber(m_lastIndex);
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

void RegExpImp::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attributes)
{
    lookupPut<RegExpImp, JSObject>(exec, propertyName, value, attributes, &RegExpImpTable, this);
}

void RegExpImp::putValueProperty(ExecState* exec, int token, JSValue* value, int)
{
    UNUSED_PARAM(token);
    ASSERT(token == LastIndex);
    m_lastIndex = value->toInteger(exec);
}

bool RegExpImp::match(ExecState* exec, const List& args)
{
    RegExpObjectImp* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();

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
        if (m_lastIndex < 0 || m_lastIndex > input.size()) {
            m_lastIndex = 0;
            return false;
        }
        lastIndex = static_cast<int>(m_lastIndex);
    }

    int foundIndex;
    int foundLength;
    regExpObj->performMatch(m_regExp.get(), input, lastIndex, foundIndex, foundLength);

    if (global) {
        lastIndex = foundIndex < 0 ? 0 : foundIndex + foundLength;
        m_lastIndex = lastIndex;
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
        ? exec->lexicalGlobalObject()->regExpConstructor()->arrayOfMatches(exec)
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

const ClassInfo RegExpObjectImp::info = { "Function", &InternalFunctionImp::info, &RegExpObjectImpTable };

/* Source for regexp_object.lut.h
@begin RegExpObjectImpTable 21
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
  : InternalFunctionImp(funcProto, "RegExp")
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
    d->lastNumSubPatterns = r->numSubpatterns();
  }
}

JSObject* RegExpObjectImp::arrayOfMatches(ExecState* exec) const
{
  unsigned lastNumSubpatterns = d->lastNumSubPatterns;
  ArrayInstance* arr = new ArrayInstance(exec->lexicalGlobalObject()->arrayPrototype(), lastNumSubpatterns + 1);
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
  return getStaticValueSlot<RegExpObjectImp, InternalFunctionImp>(exec, &RegExpObjectImpTable, this, propertyName, slot);
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
  lookupPut<RegExpObjectImp, InternalFunctionImp>(exec, propertyName, value, attr, &RegExpObjectImpTable, this);
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
  
  if (arg0->isObject(&RegExpImp::info)) {
    if (!arg1->isUndefined())
      return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
    return static_cast<JSObject*>(arg0);
  }
  
  UString pattern = arg0->isUndefined() ? UString("") : arg0->toString(exec);
  UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
  
  return createRegExpImp(exec, new RegExp(pattern, flags));
}

JSObject* RegExpObjectImp::createRegExpImp(ExecState* exec, PassRefPtr<RegExp> regExp)
{
    return regExp->isValid()
        ? new RegExpImp(static_cast<RegExpPrototype*>(exec->lexicalGlobalObject()->regExpPrototype()), regExp)
        : throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));
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
