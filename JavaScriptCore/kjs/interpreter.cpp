// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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
#include "interpreter.h"

#include "SavedBuiltins.h"
#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "context.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "internal.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "string_object.h"
#include "types.h"
#include "value.h"

#include "runtime.h"

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

namespace KJS {

// Default number of ticks before a timeout check should be done.
static const int initialTickCountThreshold = 255;

// Preferred number of milliseconds between each timeout check
static const int preferredScriptCheckTimeInterval = 1000;

Interpreter* Interpreter::s_hook = 0;
    
typedef HashMap<JSObject*, Interpreter*> InterpreterMap;
static inline InterpreterMap &interpreterMap()
{
    static InterpreterMap* map = new InterpreterMap;
    return* map;
}
    
Interpreter::Interpreter(JSObject* globalObject)
    : m_globalExec(this, 0)
    , m_globalObject(globalObject)
{
    init();
}

Interpreter::Interpreter()
    : m_globalExec(this, 0)
    , m_globalObject(new JSObject())
{
    init();
}

void Interpreter::init()
{
    JSLock lock;

    m_refCount = 0;
    m_timeoutTime = 0;
    m_recursion = 0;
    m_debugger= 0;
    m_context = 0;

    resetTimeoutCheck();
    m_timeoutCheckCount = 0;
    
    m_compatMode = NativeMode;
    m_argumentsPropertyName = &argumentsPropertyName;
    m_specialPrototypePropertyName = &specialPrototypePropertyName;

    interpreterMap().set(m_globalObject, this);

    if (s_hook) {
        prev = s_hook;
        next = s_hook->next;
        s_hook->next->prev = this;
        s_hook->next = this;
    } else {
        // This is the first interpreter
        s_hook = next = prev = this;
    }
    
    initGlobalObject();
}

Interpreter::~Interpreter()
{
    JSLock lock;
    
    if (m_debugger)
        m_debugger->detach(this);

    next->prev = prev;
    prev->next = next;
    s_hook = next;
    if (s_hook == this)
    {
        // This was the last interpreter
        s_hook = 0;
    }
    
    interpreterMap().remove(m_globalObject);
}

JSObject* Interpreter::globalObject() const
{
  return m_globalObject;
}

void Interpreter::initGlobalObject()
{
    Identifier::init();

    FunctionPrototype *funcProto = new FunctionPrototype(&m_globalExec);
    m_FunctionPrototype = funcProto;
    ObjectPrototype *objProto = new ObjectPrototype(&m_globalExec, funcProto);
    m_ObjectPrototype = objProto;
    funcProto->setPrototype(m_ObjectPrototype);
    
    ArrayPrototype *arrayProto = new ArrayPrototype(&m_globalExec, objProto);
    m_ArrayPrototype = arrayProto;
    StringPrototype *stringProto = new StringPrototype(&m_globalExec, objProto);
    m_StringPrototype = stringProto;
    BooleanPrototype *booleanProto = new BooleanPrototype(&m_globalExec, objProto, funcProto);
    m_BooleanPrototype = booleanProto;
    NumberPrototype *numberProto = new NumberPrototype(&m_globalExec, objProto, funcProto);
    m_NumberPrototype = numberProto;
    DatePrototype *dateProto = new DatePrototype(&m_globalExec, objProto);
    m_DatePrototype = dateProto;
    RegExpPrototype *regexpProto = new RegExpPrototype(&m_globalExec, objProto, funcProto);
    m_RegExpPrototype = regexpProto;
    ErrorPrototype *errorProto = new ErrorPrototype(&m_globalExec, objProto, funcProto);
    m_ErrorPrototype = errorProto;
    
    JSObject* o = m_globalObject;
    while (o->prototype()->isObject())
        o = static_cast<JSObject*>(o->prototype());
    o->setPrototype(m_ObjectPrototype);
    
    // Constructors (Object, Array, etc.)
    m_Object = new ObjectObjectImp(&m_globalExec, objProto, funcProto);
    m_Function = new FunctionObjectImp(&m_globalExec, funcProto);
    m_Array = new ArrayObjectImp(&m_globalExec, funcProto, arrayProto);
    m_String = new StringObjectImp(&m_globalExec, funcProto, stringProto);
    m_Boolean = new BooleanObjectImp(&m_globalExec, funcProto, booleanProto);
    m_Number = new NumberObjectImp(&m_globalExec, funcProto, numberProto);
    m_Date = new DateObjectImp(&m_globalExec, funcProto, dateProto);
    m_RegExp = new RegExpObjectImp(&m_globalExec, funcProto, regexpProto);
    m_Error = new ErrorObjectImp(&m_globalExec, funcProto, errorProto);
    
    // Error object prototypes
    m_EvalErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, EvalError, "EvalError", "EvalError");
    m_RangeErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, RangeError, "RangeError", "RangeError");
    m_ReferenceErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, ReferenceError, "ReferenceError", "ReferenceError");
    m_SyntaxErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, SyntaxError, "SyntaxError", "SyntaxError");
    m_TypeErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, TypeError, "TypeError", "TypeError");
    m_UriErrorPrototype = new NativeErrorPrototype(&m_globalExec, errorProto, URIError, "URIError", "URIError");
    
    // Error objects
    m_EvalError = new NativeErrorImp(&m_globalExec, funcProto, m_EvalErrorPrototype);
    m_RangeError = new NativeErrorImp(&m_globalExec, funcProto, m_RangeErrorPrototype);
    m_ReferenceError = new NativeErrorImp(&m_globalExec, funcProto, m_ReferenceErrorPrototype);
    m_SyntaxError = new NativeErrorImp(&m_globalExec, funcProto, m_SyntaxErrorPrototype);
    m_TypeError = new NativeErrorImp(&m_globalExec, funcProto, m_TypeErrorPrototype);
    m_UriError = new NativeErrorImp(&m_globalExec, funcProto, m_UriErrorPrototype);
    
    // ECMA 15.3.4.1
    funcProto->put(&m_globalExec, constructorPropertyName, m_Function, DontEnum);
    
    m_globalObject->put(&m_globalExec, "Object", m_Object, DontEnum);
    m_globalObject->put(&m_globalExec, "Function", m_Function, DontEnum);
    m_globalObject->put(&m_globalExec, "Array", m_Array, DontEnum);
    m_globalObject->put(&m_globalExec, "Boolean", m_Boolean, DontEnum);
    m_globalObject->put(&m_globalExec, "String", m_String, DontEnum);
    m_globalObject->put(&m_globalExec, "Number", m_Number, DontEnum);
    m_globalObject->put(&m_globalExec, "Date", m_Date, DontEnum);
    m_globalObject->put(&m_globalExec, "RegExp", m_RegExp, DontEnum);
    m_globalObject->put(&m_globalExec, "Error", m_Error, DontEnum);
    // Using Internal for those to have something != 0
    // (see kjs_window). Maybe DontEnum would be ok too ?
    m_globalObject->put(&m_globalExec, "EvalError",m_EvalError, Internal);
    m_globalObject->put(&m_globalExec, "RangeError",m_RangeError, Internal);
    m_globalObject->put(&m_globalExec, "ReferenceError",m_ReferenceError, Internal);
    m_globalObject->put(&m_globalExec, "SyntaxError",m_SyntaxError, Internal);
    m_globalObject->put(&m_globalExec, "TypeError",m_TypeError, Internal);
    m_globalObject->put(&m_globalExec, "URIError",m_UriError, Internal);
    
    // Set the constructorPropertyName property of all builtin constructors
    objProto->put(&m_globalExec, constructorPropertyName, m_Object, DontEnum | DontDelete | ReadOnly);
    funcProto->put(&m_globalExec, constructorPropertyName, m_Function, DontEnum | DontDelete | ReadOnly);
    arrayProto->put(&m_globalExec, constructorPropertyName, m_Array, DontEnum | DontDelete | ReadOnly);
    booleanProto->put(&m_globalExec, constructorPropertyName, m_Boolean, DontEnum | DontDelete | ReadOnly);
    stringProto->put(&m_globalExec, constructorPropertyName, m_String, DontEnum | DontDelete | ReadOnly);
    numberProto->put(&m_globalExec, constructorPropertyName, m_Number, DontEnum | DontDelete | ReadOnly);
    dateProto->put(&m_globalExec, constructorPropertyName, m_Date, DontEnum | DontDelete | ReadOnly);
    regexpProto->put(&m_globalExec, constructorPropertyName, m_RegExp, DontEnum | DontDelete | ReadOnly);
    errorProto->put(&m_globalExec, constructorPropertyName, m_Error, DontEnum | DontDelete | ReadOnly);
    m_EvalErrorPrototype->put(&m_globalExec, constructorPropertyName, m_EvalError, DontEnum | DontDelete | ReadOnly);
    m_RangeErrorPrototype->put(&m_globalExec, constructorPropertyName, m_RangeError, DontEnum | DontDelete | ReadOnly);
    m_ReferenceErrorPrototype->put(&m_globalExec, constructorPropertyName, m_ReferenceError, DontEnum | DontDelete | ReadOnly);
    m_SyntaxErrorPrototype->put(&m_globalExec, constructorPropertyName, m_SyntaxError, DontEnum | DontDelete | ReadOnly);
    m_TypeErrorPrototype->put(&m_globalExec, constructorPropertyName, m_TypeError, DontEnum | DontDelete | ReadOnly);
    m_UriErrorPrototype->put(&m_globalExec, constructorPropertyName, m_UriError, DontEnum | DontDelete | ReadOnly);
    
    // built-in values
    m_globalObject->put(&m_globalExec, "NaN",        jsNaN(), DontEnum|DontDelete);
    m_globalObject->put(&m_globalExec, "Infinity",   jsNumber(Inf), DontEnum|DontDelete);
    m_globalObject->put(&m_globalExec, "undefined",  jsUndefined(), DontEnum|DontDelete);
    
    // built-in functions
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::Eval, 1, "eval"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::ParseInt, 2, "parseInt"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::ParseFloat, 1, "parseFloat"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::IsNaN, 1, "isNaN"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::IsFinite, 1, "isFinite"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::Escape, 1, "escape"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::UnEscape, 1, "unescape"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::DecodeURI, 1, "decodeURI"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::DecodeURIComponent, 1, "decodeURIComponent"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::EncodeURI, 1, "encodeURI"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::EncodeURIComponent, 1, "encodeURIComponent"), DontEnum);
#ifndef NDEBUG
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, funcProto, GlobalFuncImp::KJSPrint, 1, "kjsprint"), DontEnum);
#endif
    
    // built-in objects
    m_globalObject->put(&m_globalExec, "Math", new MathObjectImp(&m_globalExec, objProto), DontEnum);
}

ExecState* Interpreter::globalExec()
{
  return &m_globalExec;
}

Completion Interpreter::checkSyntax(const UString& sourceURL, int startingLineNumber, const UString& code)
{
    return checkSyntax(sourceURL, startingLineNumber, code.data(), code.size());
}

Completion Interpreter::checkSyntax(const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength)
{
    JSLock lock;

    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> progNode = Parser::parse(sourceURL, startingLineNumber, code, codeLength, 0, &errLine, &errMsg);
    if (!progNode)
        return Completion(Throw, Error::create(&m_globalExec, SyntaxError, errMsg, errLine, 0, sourceURL));
    return Completion(Normal);
}

Completion Interpreter::evaluate(const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV)
{
    return evaluate(sourceURL, startingLineNumber, code.data(), code.size(), thisV);
}

Completion Interpreter::evaluate(const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength, JSValue* thisV)
{
    JSLock lock;
    
    // prevent against infinite recursion
    if (m_recursion >= 20)
        return Completion(Throw, Error::create(&m_globalExec, GeneralError, "Recursion too deep"));
    
    // parse the source code
    int sid;
    int errLine;
    UString errMsg;
    RefPtr<ProgramNode> progNode = Parser::parse(sourceURL, startingLineNumber, code, codeLength, &sid, &errLine, &errMsg);
    
    // notify debugger that source has been parsed
    if (m_debugger) {
        bool cont = m_debugger->sourceParsed(&m_globalExec, sid, sourceURL, UString(code, codeLength), startingLineNumber, errLine, errMsg);
        if (!cont)
            return Completion(Break);
    }
    
    // no program node means a syntax error occurred
    if (!progNode)
        return Completion(Throw, Error::create(&m_globalExec, SyntaxError, errMsg, errLine, sid, sourceURL));
    
    m_globalExec.clearException();
    
    m_recursion++;
    
    JSObject* globalObj = m_globalObject;
    JSObject* thisObj = globalObj;
    
    // "this" must be an object... use same rules as Function.prototype.apply()
    if (thisV && !thisV->isUndefinedOrNull())
        thisObj = thisV->toObject(&m_globalExec);
    
    Completion res;
    if (m_globalExec.hadException())
        // the thisV->toObject() conversion above might have thrown an exception - if so, propagate it
        res = Completion(Throw, m_globalExec.exception());
    else {
        // execute the code
        Context ctx(globalObj, this, thisObj, progNode.get());
        ExecState newExec(this, &ctx);
        ctx.setExecState(&newExec);
        progNode->processVarDecls(&newExec);
        res = progNode->execute(&newExec);
    }
    
    m_recursion--;
    
    if (shouldPrintExceptions() && res.complType() == Throw) {
        JSLock lock;
        ExecState* exec = globalExec();
        CString f = sourceURL.UTF8String();
        CString message = res.value()->toObject(exec)->toString(exec).UTF8String();
        int line = res.value()->toObject(exec)->get(exec, "line")->toUInt32(exec);
#if PLATFORM(WIN_OS)
        printf("%s line %d: %s\n", f.c_str(), line, message.c_str());
#else
        printf("[%d] %s line %d: %s\n", getpid(), f.c_str(), line, message.c_str());
#endif
    }

    return res;
}

JSObject *Interpreter::builtinObject() const
{
  return m_Object;
}

JSObject *Interpreter::builtinFunction() const
{
  return m_Function;
}

JSObject *Interpreter::builtinArray() const
{
  return m_Array;
}

JSObject *Interpreter::builtinBoolean() const
{
  return m_Boolean;
}

JSObject *Interpreter::builtinString() const
{
  return m_String;
}

JSObject *Interpreter::builtinNumber() const
{
  return m_Number;
}

JSObject *Interpreter::builtinDate() const
{
  return m_Date;
}

JSObject *Interpreter::builtinRegExp() const
{
  return m_RegExp;
}

JSObject *Interpreter::builtinError() const
{
  return m_Error;
}

JSObject *Interpreter::builtinObjectPrototype() const
{
  return m_ObjectPrototype;
}

JSObject *Interpreter::builtinFunctionPrototype() const
{
  return m_FunctionPrototype;
}

JSObject *Interpreter::builtinArrayPrototype() const
{
  return m_ArrayPrototype;
}

JSObject *Interpreter::builtinBooleanPrototype() const
{
  return m_BooleanPrototype;
}

JSObject *Interpreter::builtinStringPrototype() const
{
  return m_StringPrototype;
}

JSObject *Interpreter::builtinNumberPrototype() const
{
  return m_NumberPrototype;
}

JSObject *Interpreter::builtinDatePrototype() const
{
  return m_DatePrototype;
}

JSObject *Interpreter::builtinRegExpPrototype() const
{
  return m_RegExpPrototype;
}

JSObject *Interpreter::builtinErrorPrototype() const
{
  return m_ErrorPrototype;
}

JSObject *Interpreter::builtinEvalError() const
{
  return m_EvalError;
}

JSObject *Interpreter::builtinRangeError() const
{
  return m_RangeError;
}

JSObject *Interpreter::builtinReferenceError() const
{
  return m_ReferenceError;
}

JSObject *Interpreter::builtinSyntaxError() const
{
  return m_SyntaxError;
}

JSObject *Interpreter::builtinTypeError() const
{
  return m_TypeError;
}

JSObject *Interpreter::builtinURIError() const
{
  return m_UriError;
}

JSObject *Interpreter::builtinEvalErrorPrototype() const
{
  return m_EvalErrorPrototype;
}

JSObject *Interpreter::builtinRangeErrorPrototype() const
{
  return m_RangeErrorPrototype;
}

JSObject *Interpreter::builtinReferenceErrorPrototype() const
{
  return m_ReferenceErrorPrototype;
}

JSObject *Interpreter::builtinSyntaxErrorPrototype() const
{
  return m_SyntaxErrorPrototype;
}

JSObject *Interpreter::builtinTypeErrorPrototype() const
{
  return m_TypeErrorPrototype;
}

JSObject *Interpreter::builtinURIErrorPrototype() const
{
  return m_UriErrorPrototype;
}

void Interpreter::mark(bool)
{
    if (m_context)
        m_context->mark();
    if (m_globalObject && !m_globalObject->marked())
        m_globalObject->mark();
    if (m_globalExec.exception() && !m_globalExec.exception()->marked())
        m_globalExec.exception()->mark();
}

Interpreter* Interpreter::interpreterWithGlobalObject(JSObject* globalObject)
{
    return interpreterMap().get(globalObject);
}

#ifdef KJS_DEBUG_MEM
#include "lexer.h"
void Interpreter::finalCheck()
{
  fprintf(stderr,"Interpreter::finalCheck()\n");
  Collector::collect();

  Node::finalCheck();
  Collector::finalCheck();
  Lexer::globalClear();
  UString::globalClear();
}
#endif

static bool printExceptions = false;

bool Interpreter::shouldPrintExceptions()
{
  return printExceptions;
}

void Interpreter::setShouldPrintExceptions(bool print)
{
  printExceptions = print;
}

void Interpreter::saveBuiltins (SavedBuiltins& builtins) const
{
    if (!builtins._internal)
        builtins._internal = new SavedBuiltinsInternal;
    
    builtins._internal->m_Object = m_Object;
    builtins._internal->m_Function = m_Function;
    builtins._internal->m_Array = m_Array;
    builtins._internal->m_Boolean = m_Boolean;
    builtins._internal->m_String = m_String;
    builtins._internal->m_Number = m_Number;
    builtins._internal->m_Date = m_Date;
    builtins._internal->m_RegExp = m_RegExp;
    builtins._internal->m_Error = m_Error;
    
    builtins._internal->m_ObjectPrototype = m_ObjectPrototype;
    builtins._internal->m_FunctionPrototype = m_FunctionPrototype;
    builtins._internal->m_ArrayPrototype = m_ArrayPrototype;
    builtins._internal->m_BooleanPrototype = m_BooleanPrototype;
    builtins._internal->m_StringPrototype = m_StringPrototype;
    builtins._internal->m_NumberPrototype = m_NumberPrototype;
    builtins._internal->m_DatePrototype = m_DatePrototype;
    builtins._internal->m_RegExpPrototype = m_RegExpPrototype;
    builtins._internal->m_ErrorPrototype = m_ErrorPrototype;
    
    builtins._internal->m_EvalError = m_EvalError;
    builtins._internal->m_RangeError = m_RangeError;
    builtins._internal->m_ReferenceError = m_ReferenceError;
    builtins._internal->m_SyntaxError = m_SyntaxError;
    builtins._internal->m_TypeError = m_TypeError;
    builtins._internal->m_UriError = m_UriError;
    
    builtins._internal->m_EvalErrorPrototype = m_EvalErrorPrototype;
    builtins._internal->m_RangeErrorPrototype = m_RangeErrorPrototype;
    builtins._internal->m_ReferenceErrorPrototype = m_ReferenceErrorPrototype;
    builtins._internal->m_SyntaxErrorPrototype = m_SyntaxErrorPrototype;
    builtins._internal->m_TypeErrorPrototype = m_TypeErrorPrototype;
    builtins._internal->m_UriErrorPrototype = m_UriErrorPrototype;
}

void Interpreter::restoreBuiltins (const SavedBuiltins& builtins)
{
    if (!builtins._internal)
        return;

    m_Object = builtins._internal->m_Object;
    m_Function = builtins._internal->m_Function;
    m_Array = builtins._internal->m_Array;
    m_Boolean = builtins._internal->m_Boolean;
    m_String = builtins._internal->m_String;
    m_Number = builtins._internal->m_Number;
    m_Date = builtins._internal->m_Date;
    m_RegExp = builtins._internal->m_RegExp;
    m_Error = builtins._internal->m_Error;
    
    m_ObjectPrototype = builtins._internal->m_ObjectPrototype;
    m_FunctionPrototype = builtins._internal->m_FunctionPrototype;
    m_ArrayPrototype = builtins._internal->m_ArrayPrototype;
    m_BooleanPrototype = builtins._internal->m_BooleanPrototype;
    m_StringPrototype = builtins._internal->m_StringPrototype;
    m_NumberPrototype = builtins._internal->m_NumberPrototype;
    m_DatePrototype = builtins._internal->m_DatePrototype;
    m_RegExpPrototype = builtins._internal->m_RegExpPrototype;
    m_ErrorPrototype = builtins._internal->m_ErrorPrototype;
    
    m_EvalError = builtins._internal->m_EvalError;
    m_RangeError = builtins._internal->m_RangeError;
    m_ReferenceError = builtins._internal->m_ReferenceError;
    m_SyntaxError = builtins._internal->m_SyntaxError;
    m_TypeError = builtins._internal->m_TypeError;
    m_UriError = builtins._internal->m_UriError;
    
    m_EvalErrorPrototype = builtins._internal->m_EvalErrorPrototype;
    m_RangeErrorPrototype = builtins._internal->m_RangeErrorPrototype;
    m_ReferenceErrorPrototype = builtins._internal->m_ReferenceErrorPrototype;
    m_SyntaxErrorPrototype = builtins._internal->m_SyntaxErrorPrototype;
    m_TypeErrorPrototype = builtins._internal->m_TypeErrorPrototype;
    m_UriErrorPrototype = builtins._internal->m_UriErrorPrototype;
}

void Interpreter::startTimeoutCheck()
{
    if (m_timeoutCheckCount == 0)
        resetTimeoutCheck();
    
    m_timeoutCheckCount++;
}

void Interpreter::stopTimeoutCheck()
{
    m_timeoutCheckCount--;
}

void Interpreter::resetTimeoutCheck()
{
    m_tickCount = 0;
    m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;
    m_timeAtLastCheckTimeout = 0;
    m_timeExecuting = 0;
}

// Returns the current time in milliseconds
// It doesn't matter what "current time" is here, just as long as
// it's possible to measure the time difference correctly.
static inline unsigned getCurrentTime() {
#if HAVE(SYS_TIME_H)
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif PLATFORM(WIN_OS)
    return timeGetTime();
#else
#error Platform does not have getCurrentTime function
#endif
}

bool Interpreter::checkTimeout()
{    
    m_tickCount = 0;
    
    unsigned currentTime = getCurrentTime();

    if (!m_timeAtLastCheckTimeout) {
        // Suspicious amount of looping in a script -- start timing it
        m_timeAtLastCheckTimeout = currentTime;
        return false;
    }

    unsigned timeDiff = currentTime - m_timeAtLastCheckTimeout;

    if (timeDiff == 0)
        timeDiff = 1;
    
    m_timeExecuting += timeDiff;
    m_timeAtLastCheckTimeout = currentTime;
    
    // Adjust the tick threshold so we get the next checkTimeout call in the interval specified in 
    // preferredScriptCheckTimeInterval
    m_ticksUntilNextTimeoutCheck = (unsigned)((float)preferredScriptCheckTimeInterval / timeDiff) * m_ticksUntilNextTimeoutCheck;

    // If the new threshold is 0 reset it to the default threshold. This can happen if the timeDiff is higher than the
    // preferred script check time interval.
    if (m_ticksUntilNextTimeoutCheck == 0)
        m_ticksUntilNextTimeoutCheck = initialTickCountThreshold;

    if (m_timeoutTime && m_timeExecuting > m_timeoutTime) {
        if (shouldInterruptScript())
            return true;
        
        resetTimeoutCheck();
    }
    
    return false;
}


SavedBuiltins::SavedBuiltins() : 
  _internal(0)
{
}

SavedBuiltins::~SavedBuiltins()
{
  delete _internal;
}

}
