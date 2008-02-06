// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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
#include "function.h"

#include "Activation.h"
#include "ExecState.h"
#include "JSGlobalObject.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "debugger.h"
#include "dtoa.h"
#include "function_object.h"
#include "internal.h"
#include "lexer.h"
#include "nodes.h"
#include "operations.h"
#include "scope_chain_mark.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/UTF8.h>

using namespace WTF;
using namespace Unicode;

namespace KJS {

// ----------------------------- FunctionImp ----------------------------------

const ClassInfo FunctionImp::info = { "Function", &InternalFunctionImp::info, 0 };

FunctionImp::FunctionImp(ExecState* exec, const Identifier& name, FunctionBodyNode* b, const ScopeChain& sc)
  : InternalFunctionImp(exec->lexicalGlobalObject()->functionPrototype(), name)
  , body(b)
  , _scope(sc)
{
}

void FunctionImp::mark()
{
    InternalFunctionImp::mark();
    _scope.mark();
}

JSValue* FunctionImp::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    FunctionExecState newExec(exec->dynamicGlobalObject(), thisObj, body.get(), exec, this, args);
    JSValue* result = body->execute(&newExec);
    if (newExec.completionType() == Throw) {
        exec->setException(result);
        return result;
    }
    if (newExec.completionType() == ReturnValue)
        return result;
    return jsUndefined();
}

JSValue* FunctionImp::argumentsGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
  FunctionImp* thisObj = static_cast<FunctionImp*>(slot.slotBase());
  
  for (ExecState* e = exec; e; e = e->callingExecState())
    if (e->function() == thisObj) {
      e->dynamicGlobalObject()->tearOffActivation(e, e != exec);
      return e->activationObject()->get(exec, propertyName);
    }
  
  return jsNull();
}

JSValue* FunctionImp::callerGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
    FunctionImp* thisObj = static_cast<FunctionImp*>(slot.slotBase());
    ExecState* e = exec;
    while (e) {
        if (e->function() == thisObj)
            break;
        e = e->callingExecState();
    }

    if (!e)
        return jsNull();
    
    ExecState* callingExecState = e->callingExecState();
    if (!callingExecState)
        return jsNull();
    
    FunctionImp* callingFunction = callingExecState->function();
    if (!callingFunction)
        return jsNull();

    return callingFunction;
}

JSValue* FunctionImp::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    FunctionImp* thisObj = static_cast<FunctionImp*>(slot.slotBase());
    return jsNumber(thisObj->body->parameters().size());
}

bool FunctionImp::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Find the arguments from the closest context.
    if (propertyName == exec->propertyNames().arguments) {
        slot.setCustom(this, argumentsGetter);
        return true;
    }

    // Compute length of parameters.
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    if (propertyName == exec->propertyNames().caller) {
        slot.setCustom(this, callerGetter);
        return true;
    }

    return InternalFunctionImp::getOwnPropertySlot(exec, propertyName, slot);
}

void FunctionImp::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return;
    InternalFunctionImp::put(exec, propertyName, value, attr);
}

bool FunctionImp::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return false;
    return InternalFunctionImp::deleteProperty(exec, propertyName);
}

/* Returns the parameter name corresponding to the given index. eg:
 * function f1(x, y, z): getParameterName(0) --> x
 *
 * If a name appears more than once, only the last index at which
 * it appears associates with it. eg:
 * function f2(x, x): getParameterName(0) --> null
 */
Identifier FunctionImp::getParameterName(int index)
{
    Vector<Identifier>& parameters = body->parameters();

    if (static_cast<size_t>(index) >= body->parameters().size())
        return CommonIdentifiers::shared()->nullIdentifier;
  
    Identifier name = parameters[index];

    // Are there any subsequent parameters with the same name?
    size_t size = parameters.size();
    for (size_t i = index + 1; i < size; ++i)
        if (parameters[i] == name)
            return CommonIdentifiers::shared()->nullIdentifier;

    return name;
}

// ECMA 13.2.2 [[Construct]]
JSObject* FunctionImp::construct(ExecState* exec, const List& args)
{
  JSObject* proto;
  JSValue* p = get(exec, exec->propertyNames().prototype);
  if (p->isObject())
    proto = static_cast<JSObject*>(p);
  else
    proto = exec->lexicalGlobalObject()->objectPrototype();

  JSObject* obj(new JSObject(proto));

  JSValue* res = call(exec,obj,args);

  if (res->isObject())
    return static_cast<JSObject*>(res);
  else
    return obj;
}

// ------------------------------ IndexToNameMap ---------------------------------

// We map indexes in the arguments array to their corresponding argument names. 
// Example: function f(x, y, z): arguments[0] = x, so we map 0 to Identifier("x"). 

// Once we have an argument name, we can get and set the argument's value in the 
// activation object.

// We use Identifier::null to indicate that a given argument's value
// isn't stored in the activation object.

IndexToNameMap::IndexToNameMap(FunctionImp* func, const List& args)
{
  _map = new Identifier[args.size()];
  this->size = args.size();
  
  int i = 0;
  List::const_iterator end = args.end();
  for (List::const_iterator it = args.begin(); it != end; ++i, ++it)
    _map[i] = func->getParameterName(i); // null if there is no corresponding parameter
}

IndexToNameMap::~IndexToNameMap() {
  delete [] _map;
}

bool IndexToNameMap::isMapped(const Identifier& index) const
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);
  
  if (!indexIsNumber)
    return false;
  
  if (indexAsNumber >= size)
    return false;

  if (_map[indexAsNumber].isNull())
    return false;
  
  return true;
}

void IndexToNameMap::unMap(const Identifier& index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  ASSERT(indexIsNumber && indexAsNumber < size);
  
  _map[indexAsNumber] = CommonIdentifiers::shared()->nullIdentifier;
}

Identifier& IndexToNameMap::operator[](int index)
{
  return _map[index];
}

Identifier& IndexToNameMap::operator[](const Identifier& index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  ASSERT(indexIsNumber && indexAsNumber < size);
  
  return (*this)[indexAsNumber];
}

// ------------------------------ Arguments ---------------------------------

const ClassInfo Arguments::info = { "Arguments", 0, 0 };

// ECMA 10.1.8
Arguments::Arguments(ExecState* exec, FunctionImp* func, const List& args, ActivationImp* act)
    : JSObject(exec->lexicalGlobalObject()->objectPrototype())
    , _activationObject(act)
    , indexToNameMap(func, args)
{
    putDirect(exec->propertyNames().callee, func, DontEnum);
    putDirect(exec->propertyNames().length, args.size(), DontEnum);
  
    int i = 0;
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it, ++i) {
        Identifier name = Identifier::from(i);
        if (!indexToNameMap.isMapped(name))
            putDirect(name, *it, DontEnum);
    }
}

void Arguments::mark() 
{
  JSObject::mark();
  if (_activationObject && !_activationObject->marked())
    _activationObject->mark();
}

JSValue* Arguments::mappedIndexGetter(ExecState* exec, JSObject*, const Identifier& propertyName, const PropertySlot& slot)
{
  Arguments* thisObj = static_cast<Arguments*>(slot.slotBase());
  return thisObj->_activationObject->get(exec, thisObj->indexToNameMap[propertyName]);
}

bool Arguments::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
  if (indexToNameMap.isMapped(propertyName)) {
    slot.setCustom(this, mappedIndexGetter);
    return true;
  }

  return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void Arguments::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
  if (indexToNameMap.isMapped(propertyName)) {
    _activationObject->put(exec, indexToNameMap[propertyName], value, attr);
  } else {
    JSObject::put(exec, propertyName, value, attr);
  }
}

bool Arguments::deleteProperty(ExecState* exec, const Identifier& propertyName) 
{
  if (indexToNameMap.isMapped(propertyName)) {
    indexToNameMap.unMap(propertyName);
    return true;
  } else {
    return JSObject::deleteProperty(exec, propertyName);
  }
}

// ------------------------------ ActivationImp --------------------------------

const ClassInfo ActivationImp::info = { "Activation", 0, 0 };

ActivationImp::ActivationImp(const ActivationData& oldData, bool leaveRelic)
{
    JSVariableObject::d = new ActivationData(oldData);
    d()->leftRelic = leaveRelic;
}

ActivationImp::~ActivationImp()
{
    if (!d()->isOnStack)
        delete d();
}

void ActivationImp::init(ExecState* exec)
{
    d()->symbolTable = &exec->function()->body->symbolTable();
    d()->exec = exec;
    d()->function = exec->function();
    d()->argumentsObject = 0;
}

JSValue* ActivationImp::argumentsGetter(ExecState* exec, JSObject*, const Identifier&, const PropertySlot& slot)
{
  ActivationImp* thisObj = static_cast<ActivationImp*>(slot.slotBase());
  
  if (!thisObj->d()->argumentsObject)
    thisObj->createArgumentsObject(exec);
  
  return thisObj->d()->argumentsObject;
}

PropertySlot::GetValueFunc ActivationImp::getArgumentsGetter()
{
  return ActivationImp::argumentsGetter;
}

bool ActivationImp::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (symbolTableGet(propertyName, slot))
        return true;

    if (JSValue** location = getDirectLocation(propertyName)) {
        slot.setValueSlot(this, location);
        return true;
    }

    // Only return the built-in arguments object if it wasn't overridden above.
    if (propertyName == exec->propertyNames().arguments) {
        for (ExecState* e = exec; e; e = e->callingExecState())
            if (e->function() == d()->function) {
                e->dynamicGlobalObject()->tearOffActivation(e, e != exec);
                ActivationImp* newActivation = e->activationObject();
                slot.setCustom(newActivation, newActivation->getArgumentsGetter());
                return true;
            }
        
        slot.setCustom(this, getArgumentsGetter());
        return true;
    }

    // We don't call through to JSObject because there's no way to give an 
    // activation object getter properties or a prototype.
    ASSERT(!_prop.hasGetterSetterProperties());
    ASSERT(prototype() == jsNull());
    return false;
}

bool ActivationImp::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments)
        return false;

    return JSVariableObject::deleteProperty(exec, propertyName);
}

void ActivationImp::put(ExecState*, const Identifier& propertyName, JSValue* value, int attr)
{
    // If any bits other than DontDelete are set, then we bypass the read-only check.
    bool checkReadOnly = !(attr & ~DontDelete);
    if (symbolTablePut(propertyName, value, checkReadOnly))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!_prop.hasGetterSetterProperties());
    _prop.put(propertyName, value, attr, checkReadOnly);
}

void ActivationImp::markChildren()
{
    LocalStorage& localStorage = d()->localStorage;
    size_t size = localStorage.size();
    
    for (size_t i = 0; i < size; ++i) {
        JSValue* value = localStorage[i].value;
        
        if (!value->marked())
            value->mark();
    }
    
    if (!d()->function->marked())
        d()->function->mark();
    
    if (d()->argumentsObject && !d()->argumentsObject->marked())
        d()->argumentsObject->mark();    
}

void ActivationImp::mark()
{
    JSObject::mark();
    markChildren();
}

void ActivationImp::createArgumentsObject(ExecState* exec)
{
    // Since "arguments" is only accessible while a function is being called,
    // we can retrieve our argument list from the ExecState for our function 
    // call instead of storing the list ourselves.
    d()->argumentsObject = new Arguments(exec, d()->exec->function(), *d()->exec->arguments(), this);
}

ActivationImp::ActivationData::ActivationData(const ActivationData& old)
    : JSVariableObjectData(old)
    , exec(old.exec)
    , function(old.function)
    , argumentsObject(old.argumentsObject)
    , isOnStack(false)
{
}

// ------------------------------ Global Functions -----------------------------------

static JSValue* encode(ExecState* exec, const List& args, const char* do_not_escape)
{
  UString r = "", s, str = args[0]->toString(exec);
  CString cstr = str.UTF8String(true);
  if (!cstr.c_str())
    return throwError(exec, URIError, "String contained an illegal UTF-16 sequence.");
  const char* p = cstr.c_str();
  for (size_t k = 0; k < cstr.size(); k++, p++) {
    char c = *p;
    if (c && strchr(do_not_escape, c)) {
      r.append(c);
    } else {
      char tmp[4];
      sprintf(tmp, "%%%02X", (unsigned char)c);
      r += tmp;
    }
  }
  return jsString(r);
}

static JSValue* decode(ExecState* exec, const List& args, const char* do_not_unescape, bool strict)
{
  UString s = "", str = args[0]->toString(exec);
  int k = 0, len = str.size();
  const UChar* d = str.data();
  UChar u;
  while (k < len) {
    const UChar* p = d + k;
    UChar c = *p;
    if (c == '%') {
      int charLen = 0;
      if (k <= len - 3 && isASCIIHexDigit(p[1].uc) && isASCIIHexDigit(p[2].uc)) {
        const char b0 = Lexer::convertHex(p[1].uc, p[2].uc);
        const int sequenceLen = UTF8SequenceLength(b0);
        if (sequenceLen != 0 && k <= len - sequenceLen * 3) {
          charLen = sequenceLen * 3;
          char sequence[5];
          sequence[0] = b0;
          for (int i = 1; i < sequenceLen; ++i) {
            const UChar* q = p + i * 3;
            if (q[0] == '%' && isASCIIHexDigit(q[1].uc) && isASCIIHexDigit(q[2].uc))
              sequence[i] = Lexer::convertHex(q[1].uc, q[2].uc);
            else {
              charLen = 0;
              break;
            }
          }
          if (charLen != 0) {
            sequence[sequenceLen] = 0;
            const int character = decodeUTF8Sequence(sequence);
            if (character < 0 || character >= 0x110000) {
              charLen = 0;
            } else if (character >= 0x10000) {
              // Convert to surrogate pair.
              s.append(static_cast<unsigned short>(0xD800 | ((character - 0x10000) >> 10)));
              u = static_cast<unsigned short>(0xDC00 | ((character - 0x10000) & 0x3FF));
            } else {
              u = static_cast<unsigned short>(character);
            }
          }
        }
      }
      if (charLen == 0) {
        if (strict)
          return throwError(exec, URIError);
        // The only case where we don't use "strict" mode is the "unescape" function.
        // For that, it's good to support the wonky "%u" syntax for compatibility with WinIE.
        if (k <= len - 6 && p[1] == 'u'
            && isASCIIHexDigit(p[2].uc) && isASCIIHexDigit(p[3].uc)
            && isASCIIHexDigit(p[4].uc) && isASCIIHexDigit(p[5].uc)) {
          charLen = 6;
          u = Lexer::convertUnicode(p[2].uc, p[3].uc, p[4].uc, p[5].uc);
        }
      }
      if (charLen && (u.uc == 0 || u.uc >= 128 || !strchr(do_not_unescape, u.low()))) {
        c = u;
        k += charLen - 1;
      }
    }
    k++;
    s.append(c);
  }
  return jsString(s);
}

static bool isStrWhiteSpace(unsigned short c)
{
    switch (c) {
        case 0x0009:
        case 0x000A:
        case 0x000B:
        case 0x000C:
        case 0x000D:
        case 0x0020:
        case 0x00A0:
        case 0x2028:
        case 0x2029:
            return true;
        default:
            return isSeparatorSpace(c);
    }
}

static int parseDigit(unsigned short c, int radix)
{
    int digit = -1;

    if (c >= '0' && c <= '9') {
        digit = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        digit = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'z') {
        digit = c - 'a' + 10;
    }

    if (digit >= radix)
        return -1;
    return digit;
}

double parseIntOverflow(const char* s, int length, int radix)
{
    double number = 0.0;
    double radixMultiplier = 1.0;

    for (const char* p = s + length - 1; p >= s; p--) {
        if (radixMultiplier == Inf) {
            if (*p != '0') {
                number = Inf;
                break;
            }
        } else {
            int digit = parseDigit(*p, radix);
            number += digit * radixMultiplier;
        }

        radixMultiplier *= radix;
    }

    return number;
}

static double parseInt(const UString& s, int radix)
{
    int length = s.size();
    int p = 0;

    while (p < length && isStrWhiteSpace(s[p].uc)) {
        ++p;
    }

    double sign = 1;
    if (p < length) {
        if (s[p] == '+') {
            ++p;
        } else if (s[p] == '-') {
            sign = -1;
            ++p;
        }
    }

    if ((radix == 0 || radix == 16) && length - p >= 2 && s[p] == '0' && (s[p + 1] == 'x' || s[p + 1] == 'X')) {
        radix = 16;
        p += 2;
    } else if (radix == 0) {
        if (p < length && s[p] == '0')
            radix = 8;
        else
            radix = 10;
    }

    if (radix < 2 || radix > 36)
        return NaN;

    int firstDigitPosition = p;
    bool sawDigit = false;
    double number = 0;
    while (p < length) {
        int digit = parseDigit(s[p].uc, radix);
        if (digit == -1)
            break;
        sawDigit = true;
        number *= radix;
        number += digit;
        ++p;
    }

    if (number >= mantissaOverflowLowerBound) {
        if (radix == 10)
            number = kjs_strtod(s.substr(firstDigitPosition, p - firstDigitPosition).ascii(), 0);
        else if (radix == 2 || radix == 4 || radix == 8 || radix == 16 || radix == 32)
            number = parseIntOverflow(s.substr(firstDigitPosition, p - firstDigitPosition).ascii(), p - firstDigitPosition, radix);
    }

    if (!sawDigit)
        return NaN;

    return sign * number;
}

static double parseFloat(const UString& s)
{
    // Check for 0x prefix here, because toDouble allows it, but we must treat it as 0.
    // Need to skip any whitespace and then one + or - sign.
    int length = s.size();
    int p = 0;
    while (p < length && isStrWhiteSpace(s[p].uc)) {
        ++p;
    }
    if (p < length && (s[p] == '+' || s[p] == '-')) {
        ++p;
    }
    if (length - p >= 2 && s[p] == '0' && (s[p + 1] == 'x' || s[p + 1] == 'X')) {
        return 0;
    }

    return s.toDouble( true /*tolerant*/, false /* NaN for empty string */ );
}

JSValue* globalFuncEval(ExecState* exec, JSObject* thisObj, const List& args)
{
    JSValue* x = args[0];
    if (!x->isString())
        return x;

    UString s = x->toString(exec);

    int sourceId;
    int errLine;
    UString errMsg;
    RefPtr<EvalNode> evalNode = parser().parse<EvalNode>(UString(), 0, s.data(), s.size(), &sourceId, &errLine, &errMsg);

    Debugger* dbg = exec->dynamicGlobalObject()->debugger();
    if (dbg) {
        bool cont = dbg->sourceParsed(exec, sourceId, UString(), s, 0, errLine, errMsg);
        if (!cont)
            return jsUndefined();
    }

    // No program node means a syntax occurred
    if (!evalNode)
        return throwError(exec, SyntaxError, errMsg, errLine, sourceId, NULL);

    bool switchGlobal = thisObj && thisObj != exec->dynamicGlobalObject() && thisObj->isGlobalObject();

    // enter a new execution context
    exec->dynamicGlobalObject()->tearOffActivation(exec);
    JSGlobalObject* globalObject = switchGlobal ? static_cast<JSGlobalObject*>(thisObj) : exec->dynamicGlobalObject();
    EvalExecState newExec(globalObject, evalNode.get(), exec);

    if (switchGlobal) {
        newExec.pushScope(thisObj);
        newExec.setVariableObject(static_cast<JSGlobalObject*>(thisObj));
    }
    JSValue* value = evalNode->execute(&newExec);
    if (switchGlobal)
        newExec.popScope();

    if (newExec.completionType() == Throw) {
        exec->setException(value);
        return value;
    }

    return value ? value : jsUndefined();
}

JSValue* globalFuncParseInt(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(parseInt(args[0]->toString(exec), args[1]->toInt32(exec)));
}

JSValue* globalFuncParseFloat(ExecState* exec, JSObject*, const List& args)
{
    return jsNumber(parseFloat(args[0]->toString(exec)));
}

JSValue* globalFuncIsNaN(ExecState* exec, JSObject*, const List& args)
{
    return jsBoolean(isnan(args[0]->toNumber(exec)));
}

JSValue* globalFuncIsFinite(ExecState* exec, JSObject*, const List& args)
{
    double n = args[0]->toNumber(exec);
    return jsBoolean(!isnan(n) && !isinf(n));
}

JSValue* globalFuncDecodeURI(ExecState* exec, JSObject*, const List& args)
{
    static const char do_not_unescape_when_decoding_URI[] =
        "#$&+,/:;=?@";

    return decode(exec, args, do_not_unescape_when_decoding_URI, true);
}

JSValue* globalFuncDecodeURIComponent(ExecState* exec, JSObject*, const List& args)
{
    return decode(exec, args, "", true);
}

JSValue* globalFuncEncodeURI(ExecState* exec, JSObject*, const List& args)
{
    static const char do_not_escape_when_encoding_URI[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!#$&'()*+,-./:;=?@_~";

    return encode(exec, args, do_not_escape_when_encoding_URI);
}

JSValue* globalFuncEncodeURIComponent(ExecState* exec, JSObject*, const List& args)
{
    static const char do_not_escape_when_encoding_URI_component[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "!'()*-._~";

    return encode(exec, args, do_not_escape_when_encoding_URI_component);
}

JSValue* globalFuncEscape(ExecState* exec, JSObject*, const List& args)
{
    static const char do_not_escape[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "*+-./@_";

    UString r = "", s, str = args[0]->toString(exec);
    const UChar* c = str.data();
    for (int k = 0; k < str.size(); k++, c++) {
        int u = c->uc;
        if (u > 255) {
            char tmp[7];
            sprintf(tmp, "%%u%04X", u);
            s = UString(tmp);
        } else if (u != 0 && strchr(do_not_escape, (char)u))
            s = UString(c, 1);
        else {
            char tmp[4];
            sprintf(tmp, "%%%02X", u);
            s = UString(tmp);
        }
        r += s;
    }

    return jsString(r);
}

JSValue* globalFuncUnescape(ExecState* exec, JSObject*, const List& args)
{
    UString s = "", str = args[0]->toString(exec);
    int k = 0, len = str.size();
    while (k < len) {
        const UChar* c = str.data() + k;
        UChar u;
        if (*c == UChar('%') && k <= len - 6 && *(c + 1) == UChar('u')) {
            if (Lexer::isHexDigit((c + 2)->uc) && Lexer::isHexDigit((c + 3)->uc) && Lexer::isHexDigit((c + 4)->uc) && Lexer::isHexDigit((c + 5)->uc)) {
                u = Lexer::convertUnicode((c + 2)->uc, (c + 3)->uc, (c + 4)->uc, (c + 5)->uc);
                c = &u;
                k += 5;
            }
        } else if (*c == UChar('%') && k <= len - 3 && Lexer::isHexDigit((c + 1)->uc) && Lexer::isHexDigit((c + 2)->uc)) {
            u = UChar(Lexer::convertHex((c+1)->uc, (c+2)->uc));
            c = &u;
            k += 2;
        }
        k++;
        s += UString(c, 1);
    }

    return jsString(s);
}

#ifndef NDEBUG
JSValue* globalFuncKJSPrint(ExecState* exec, JSObject*, const List& args)
{
    puts(args[0]->toString(exec).ascii());
    return jsUndefined();
}
#endif

// ------------------------------ PrototypeFunction -------------------------------

PrototypeFunction::PrototypeFunction(ExecState* exec, int len, const Identifier& name, JSMemberFunction function)
    : InternalFunctionImp(exec->lexicalGlobalObject()->functionPrototype(), name)
    , m_function(function)
{
    ASSERT_ARG(function, function);
    putDirect(exec->propertyNames().length, jsNumber(len), DontDelete | ReadOnly | DontEnum);
}

PrototypeFunction::PrototypeFunction(ExecState* exec, FunctionPrototype* functionPrototype, int len, const Identifier& name, JSMemberFunction function)
    : InternalFunctionImp(functionPrototype, name)
    , m_function(function)
{
    ASSERT_ARG(function, function);
    putDirect(exec->propertyNames().length, jsNumber(len), DontDelete | ReadOnly | DontEnum);
}

JSValue* PrototypeFunction::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    return m_function(exec, thisObj, args);
}

} // namespace KJS
