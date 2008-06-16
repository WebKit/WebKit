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
#include "RegExpObject.h"
#include "RegExpObject.lut.h"

#include "JSArray.h"
#include "ArrayPrototype.h"
#include "error_object.h"
#include "JSString.h"
#include "JSObject.h"
#include "operations.h"
#include "regexp.h"
#include "types.h"
#include "JSValue.h"
#include "UnusedParam.h"

#include <stdio.h>

namespace KJS {

// ------------------------------ RegExpPrototype ---------------------------

static JSValue* regExpProtoFuncTest(ExecState*, JSObject*, const ArgList&);
static JSValue* regExpProtoFuncExec(ExecState*, JSObject*, const ArgList&);
static JSValue* regExpProtoFuncCompile(ExecState*, JSObject*, const ArgList&);
static JSValue* regExpProtoFuncToString(ExecState*, JSObject*, const ArgList&);

// ECMA 15.10.5

const ClassInfo RegExpPrototype::info = { "RegExpPrototype", 0, 0, 0 };

RegExpPrototype::RegExpPrototype(ExecState* exec, ObjectPrototype* objectPrototype, FunctionPrototype* functionPrototype)
    : JSObject(objectPrototype)
{
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().compile, regExpProtoFuncCompile), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().exec, regExpProtoFuncExec), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().test, regExpProtoFuncTest), DontEnum);
    putDirectFunction(new PrototypeFunction(exec, functionPrototype, 0, exec->propertyNames().toString, regExpProtoFuncToString), DontEnum);
}

// ------------------------------ Functions ---------------------------
    
JSValue* regExpProtoFuncTest(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!thisObj->inherits(&RegExpObject::info))
        return throwError(exec, TypeError);

    return static_cast<RegExpObject*>(thisObj)->test(exec, args);
}

JSValue* regExpProtoFuncExec(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!thisObj->inherits(&RegExpObject::info))
        return throwError(exec, TypeError);

    return static_cast<RegExpObject*>(thisObj)->exec(exec, args);
}

JSValue* regExpProtoFuncCompile(ExecState* exec, JSObject* thisObj, const ArgList& args)
{
    if (!thisObj->inherits(&RegExpObject::info))
        return throwError(exec, TypeError);

    RefPtr<RegExp> regExp;
    JSValue* arg0 = args[0];
    JSValue* arg1 = args[1];
    
    if (arg0->isObject(&RegExpObject::info)) {
        if (!arg1->isUndefined())
            return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
        regExp = static_cast<RegExpObject*>(arg0)->regExp();
    } else {
        UString pattern = args.isEmpty() ? UString("") : arg0->toString(exec);
        UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
        regExp = RegExp::create(pattern, flags);
    }

    if (!regExp->isValid())
        return throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));

    static_cast<RegExpObject*>(thisObj)->setRegExp(regExp.release());
    static_cast<RegExpObject*>(thisObj)->setLastIndex(0);
    return jsUndefined();
}

JSValue* regExpProtoFuncToString(ExecState* exec, JSObject* thisObj, const ArgList&)
{
    if (!thisObj->inherits(&RegExpObject::info)) {
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

// ------------------------------ RegExpObject ------------------------------------

const ClassInfo RegExpObject::info = { "RegExp", 0, 0, ExecState::regExpTable };

/* Source for RegExpObject.lut.h
@begin regExpTable 5
    global        RegExpObject::Global       DontDelete|ReadOnly|DontEnum
    ignoreCase    RegExpObject::IgnoreCase   DontDelete|ReadOnly|DontEnum
    multiline     RegExpObject::Multiline    DontDelete|ReadOnly|DontEnum
    source        RegExpObject::Source       DontDelete|ReadOnly|DontEnum
    lastIndex     RegExpObject::LastIndex    DontDelete|DontEnum
@end
*/

RegExpObject::RegExpObject(RegExpPrototype* regexpProto, PassRefPtr<RegExp> regExp)
  : JSObject(regexpProto)
  , m_regExp(regExp)
  , m_lastIndex(0)
{
}

RegExpObject::~RegExpObject()
{
}

bool RegExpObject::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpObject, JSObject>(exec, ExecState::regExpTable(exec), this, propertyName, slot);
}

JSValue* RegExpObject::getValueProperty(ExecState*, int token) const
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

void RegExpObject::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    lookupPut<RegExpObject, JSObject>(exec, propertyName, value, ExecState::regExpTable(exec), this);
}

void RegExpObject::putValueProperty(ExecState* exec, int token, JSValue* value)
{
    UNUSED_PARAM(token);
    ASSERT(token == LastIndex);
    m_lastIndex = value->toInteger(exec);
}

bool RegExpObject::match(ExecState* exec, const ArgList& args)
{
    RegExpConstructor* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();

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

JSValue* RegExpObject::test(ExecState* exec, const ArgList& args)
{
    return jsBoolean(match(exec, args));
}

JSValue* RegExpObject::exec(ExecState* exec, const ArgList& args)
{
    return match(exec, args)
        ? exec->lexicalGlobalObject()->regExpConstructor()->arrayOfMatches(exec)
        :  jsNull();
}

CallType RegExpObject::getCallData(CallData&)
{
    return CallTypeNative;
}

JSValue* RegExpObject::callAsFunction(ExecState* exec, JSObject*, const ArgList& args)
{
    return RegExpObject::exec(exec, args);
}

// ------------------------------ RegExpConstructor ------------------------------

const ClassInfo RegExpConstructor::info = { "Function", &InternalFunction::info, 0, ExecState::regExpConstructorTable };

/* Source for RegExpObject.lut.h
@begin regExpConstructorTable 21
  input           RegExpConstructor::Input          None
  $_              RegExpConstructor::Input          DontEnum
  multiline       RegExpConstructor::Multiline      None
  $*              RegExpConstructor::Multiline      DontEnum
  lastMatch       RegExpConstructor::LastMatch      DontDelete|ReadOnly
  $&              RegExpConstructor::LastMatch      DontDelete|ReadOnly|DontEnum
  lastParen       RegExpConstructor::LastParen      DontDelete|ReadOnly
  $+              RegExpConstructor::LastParen      DontDelete|ReadOnly|DontEnum
  leftContext     RegExpConstructor::LeftContext    DontDelete|ReadOnly
  $`              RegExpConstructor::LeftContext    DontDelete|ReadOnly|DontEnum
  rightContext    RegExpConstructor::RightContext   DontDelete|ReadOnly
  $'              RegExpConstructor::RightContext   DontDelete|ReadOnly|DontEnum
  $1              RegExpConstructor::Dollar1        DontDelete|ReadOnly
  $2              RegExpConstructor::Dollar2        DontDelete|ReadOnly
  $3              RegExpConstructor::Dollar3        DontDelete|ReadOnly
  $4              RegExpConstructor::Dollar4        DontDelete|ReadOnly
  $5              RegExpConstructor::Dollar5        DontDelete|ReadOnly
  $6              RegExpConstructor::Dollar6        DontDelete|ReadOnly
  $7              RegExpConstructor::Dollar7        DontDelete|ReadOnly
  $8              RegExpConstructor::Dollar8        DontDelete|ReadOnly
  $9              RegExpConstructor::Dollar9        DontDelete|ReadOnly
@end
*/

struct RegExpConstructorPrivate {
  // Global search cache / settings
  RegExpConstructorPrivate() : lastNumSubPatterns(0), multiline(false) { }
  UString lastInput;
  OwnArrayPtr<int> lastOvector;
  unsigned lastNumSubPatterns : 31;
  bool multiline              : 1;
};

RegExpConstructor::RegExpConstructor(ExecState* exec, FunctionPrototype* funcProto, RegExpPrototype* regProto)
  : InternalFunction(funcProto, "RegExp")
  , d(new RegExpConstructorPrivate)
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
void RegExpConstructor::performMatch(RegExp* r, const UString& s, int startOffset, int& position, int& length, int** ovector)
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

class RegExpMatchesArray : public JSArray {
public:
    RegExpMatchesArray(ExecState*, RegExpConstructorPrivate*);

    virtual ~RegExpMatchesArray();

    virtual bool getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::getOwnPropertySlot(exec, propertyName, slot); }
    virtual bool getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::getOwnPropertySlot(exec, propertyName, slot); }
    virtual void put(ExecState* exec, const Identifier& propertyName, JSValue* v) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::put(exec, propertyName, v); }
    virtual void put(ExecState* exec, unsigned propertyName, JSValue* v) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::put(exec, propertyName, v); }
    virtual bool deleteProperty(ExecState* exec, const Identifier& propertyName) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::deleteProperty(exec, propertyName); }
    virtual bool deleteProperty(ExecState* exec, unsigned propertyName) { if (lazyCreationData()) fillArrayInstance(exec); return JSArray::deleteProperty(exec, propertyName); }
    virtual void getPropertyNames(ExecState* exec, PropertyNameArray& arr) { if (lazyCreationData()) fillArrayInstance(exec); JSArray::getPropertyNames(exec, arr); }

private:
    void fillArrayInstance(ExecState*);
};

RegExpMatchesArray::RegExpMatchesArray(ExecState* exec, RegExpConstructorPrivate* data)
    : JSArray(exec->lexicalGlobalObject()->arrayPrototype(), data->lastNumSubPatterns + 1)
{
    RegExpConstructorPrivate* d = new RegExpConstructorPrivate;
    d->lastInput = data->lastInput;
    d->lastNumSubPatterns = data->lastNumSubPatterns;
    unsigned offsetVectorSize = (data->lastNumSubPatterns + 1) * 2; // only copying the result part of the vector
    d->lastOvector.set(new int[offsetVectorSize]);
    memcpy(d->lastOvector.get(), data->lastOvector.get(), offsetVectorSize * sizeof(int));
    // d->multiline is not needed, and remains uninitialized

    setLazyCreationData(d);
}

RegExpMatchesArray::~RegExpMatchesArray()
{
    delete static_cast<RegExpConstructorPrivate*>(lazyCreationData());
}

void RegExpMatchesArray::fillArrayInstance(ExecState* exec)
{
    RegExpConstructorPrivate* d = static_cast<RegExpConstructorPrivate*>(lazyCreationData());
    ASSERT(d);

    unsigned lastNumSubpatterns = d->lastNumSubPatterns;

    for (unsigned i = 0; i <= lastNumSubpatterns; ++i) {
        int start = d->lastOvector[2 * i];
        if (start >= 0)
            JSArray::put(exec, i, jsString(d->lastInput.substr(start, d->lastOvector[2 * i + 1] - start)));
    }
    JSArray::put(exec, exec->propertyNames().index, jsNumber(d->lastOvector[0]));
    JSArray::put(exec, exec->propertyNames().input, jsString(d->lastInput));

    delete d;
    setLazyCreationData(0);
}

JSObject* RegExpConstructor::arrayOfMatches(ExecState* exec) const
{
    return new RegExpMatchesArray(exec, d.get());
}

JSValue* RegExpConstructor::getBackref(unsigned i) const
{
  if (d->lastOvector && i <= d->lastNumSubPatterns)
    return jsString(d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  return jsString("");
}

JSValue* RegExpConstructor::getLastParen() const
{
  unsigned i = d->lastNumSubPatterns;
  if (i > 0) {
    ASSERT(d->lastOvector);
    return jsString(d->lastInput.substr(d->lastOvector[2 * i], d->lastOvector[2 * i + 1] - d->lastOvector[2 * i]));
  }
  return jsString("");
}

JSValue *RegExpConstructor::getLeftContext() const
{
  if (d->lastOvector)
    return jsString(d->lastInput.substr(0, d->lastOvector[0]));
  return jsString("");
}

JSValue *RegExpConstructor::getRightContext() const
{
  if (d->lastOvector) {
    UString s = d->lastInput;
    return jsString(s.substr(d->lastOvector[1], s.size() - d->lastOvector[1]));
  }
  return jsString("");
}

bool RegExpConstructor::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<RegExpConstructor, InternalFunction>(exec, ExecState::regExpConstructorTable(exec), this, propertyName, slot);
}

JSValue *RegExpConstructor::getValueProperty(ExecState*, int token) const
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

void RegExpConstructor::put(ExecState *exec, const Identifier &propertyName, JSValue *value)
{
    lookupPut<RegExpConstructor, InternalFunction>(exec, propertyName, value, ExecState::regExpConstructorTable(exec), this);
}

void RegExpConstructor::putValueProperty(ExecState *exec, int token, JSValue *value)
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
  
ConstructType RegExpConstructor::getConstructData(ConstructData&)
{
    return ConstructTypeNative;
}

// ECMA 15.10.4
JSObject *RegExpConstructor::construct(ExecState *exec, const ArgList &args)
{
  JSValue* arg0 = args[0];
  JSValue* arg1 = args[1];
  
  if (arg0->isObject(&RegExpObject::info)) {
    if (!arg1->isUndefined())
      return throwError(exec, TypeError, "Cannot supply flags when constructing one RegExp from another.");
    return static_cast<JSObject*>(arg0);
  }
  
  UString pattern = arg0->isUndefined() ? UString("") : arg0->toString(exec);
  UString flags = arg1->isUndefined() ? UString("") : arg1->toString(exec);
  
  RefPtr<RegExp> regExp = RegExp::create(pattern, flags);
  return regExp->isValid()
    ? new RegExpObject(exec->lexicalGlobalObject()->regExpPrototype(), regExp.release())
    : throwError(exec, SyntaxError, UString("Invalid regular expression: ").append(regExp->errorMessage()));
}

// ECMA 15.10.3
JSValue *RegExpConstructor::callAsFunction(ExecState *exec, JSObject * /*thisObj*/, const ArgList &args)
{
  return construct(exec, args);
}

const UString& RegExpConstructor::input() const
{
    // Can detect a distinct initial state that is invisible to JavaScript, by checking for null
    // state (since jsString turns null strings to empty strings).
    return d->lastInput;
}

}
