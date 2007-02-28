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

    Identifier::init();

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

    if (s_hook) {
        prev = s_hook;
        next = s_hook->next;
        s_hook->next->prev = this;
        s_hook->next = this;
    } else {
        // This is the first interpreter
        s_hook = next = prev = this;
    }
    interpreterMap().set(m_globalObject, this);

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
    if (s_hook == this) {
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
    // Clear before inititalizing, to avoid marking uninitialized (dangerous) or 
    // stale (wasteful) pointers during initialization.

    // Prototypes
    m_FunctionPrototype = 0;
    m_ObjectPrototype = 0;

    m_ArrayPrototype = 0;
    m_StringPrototype = 0;
    m_BooleanPrototype = 0;
    m_NumberPrototype = 0;
    m_DatePrototype = 0;
    m_RegExpPrototype = 0;
    m_ErrorPrototype = 0;
    
    m_EvalErrorPrototype = 0;
    m_RangeErrorPrototype = 0;
    m_ReferenceErrorPrototype = 0;
    m_SyntaxErrorPrototype = 0;
    m_TypeErrorPrototype = 0;
    m_UriErrorPrototype = 0;

    // Constructors
    m_Object = 0;
    m_Function = 0;
    m_Array = 0;
    m_String = 0;
    m_Boolean = 0;
    m_Number = 0;
    m_Date = 0;
    m_RegExp = 0;
    m_Error = 0;
    
    m_EvalError = 0;
    m_RangeError = 0;
    m_ReferenceError = 0;
    m_SyntaxError = 0;
    m_TypeError = 0;
    m_UriError = 0;

    // Prototypes
    m_FunctionPrototype = new FunctionPrototype(&m_globalExec);
    m_ObjectPrototype = new ObjectPrototype(&m_globalExec, m_FunctionPrototype);
    m_FunctionPrototype->setPrototype(m_ObjectPrototype);
    
    m_ArrayPrototype = new ArrayPrototype(&m_globalExec, m_ObjectPrototype);
    m_StringPrototype = new StringPrototype(&m_globalExec, m_ObjectPrototype);
    m_BooleanPrototype = new BooleanPrototype(&m_globalExec, m_ObjectPrototype, m_FunctionPrototype);
    m_NumberPrototype = new NumberPrototype(&m_globalExec, m_ObjectPrototype, m_FunctionPrototype);
    m_DatePrototype = new DatePrototype(&m_globalExec, m_ObjectPrototype);
    m_RegExpPrototype = new RegExpPrototype(&m_globalExec, m_ObjectPrototype, m_FunctionPrototype);;
    m_ErrorPrototype = new ErrorPrototype(&m_globalExec, m_ObjectPrototype, m_FunctionPrototype);
    
    m_EvalErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, EvalError, "EvalError", "EvalError");
    m_RangeErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, RangeError, "RangeError", "RangeError");
    m_ReferenceErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, ReferenceError, "ReferenceError", "ReferenceError");
    m_SyntaxErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, SyntaxError, "SyntaxError", "SyntaxError");
    m_TypeErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, TypeError, "TypeError", "TypeError");
    m_UriErrorPrototype = new NativeErrorPrototype(&m_globalExec, m_ErrorPrototype, URIError, "URIError", "URIError");

    // Constructors
    m_Object = new ObjectObjectImp(&m_globalExec, m_ObjectPrototype, m_FunctionPrototype);
    m_Function = new FunctionObjectImp(&m_globalExec, m_FunctionPrototype);
    m_Array = new ArrayObjectImp(&m_globalExec, m_FunctionPrototype, m_ArrayPrototype);
    m_String = new StringObjectImp(&m_globalExec, m_FunctionPrototype, m_StringPrototype);
    m_Boolean = new BooleanObjectImp(&m_globalExec, m_FunctionPrototype, m_BooleanPrototype);
    m_Number = new NumberObjectImp(&m_globalExec, m_FunctionPrototype, m_NumberPrototype);
    m_Date = new DateObjectImp(&m_globalExec, m_FunctionPrototype, m_DatePrototype);
    m_RegExp = new RegExpObjectImp(&m_globalExec, m_FunctionPrototype, m_RegExpPrototype);
    m_Error = new ErrorObjectImp(&m_globalExec, m_FunctionPrototype, m_ErrorPrototype);
    
    m_EvalError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_EvalErrorPrototype);
    m_RangeError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_RangeErrorPrototype);
    m_ReferenceError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_ReferenceErrorPrototype);
    m_SyntaxError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_SyntaxErrorPrototype);
    m_TypeError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_TypeErrorPrototype);
    m_UriError = new NativeErrorImp(&m_globalExec, m_FunctionPrototype, m_UriErrorPrototype);
    
    m_FunctionPrototype->put(&m_globalExec, constructorPropertyName, m_Function, DontEnum);
    m_ObjectPrototype->put(&m_globalExec, constructorPropertyName, m_Object, DontEnum | DontDelete | ReadOnly);
    m_FunctionPrototype->put(&m_globalExec, constructorPropertyName, m_Function, DontEnum | DontDelete | ReadOnly);
    m_ArrayPrototype->put(&m_globalExec, constructorPropertyName, m_Array, DontEnum | DontDelete | ReadOnly);
    m_BooleanPrototype->put(&m_globalExec, constructorPropertyName, m_Boolean, DontEnum | DontDelete | ReadOnly);
    m_StringPrototype->put(&m_globalExec, constructorPropertyName, m_String, DontEnum | DontDelete | ReadOnly);
    m_NumberPrototype->put(&m_globalExec, constructorPropertyName, m_Number, DontEnum | DontDelete | ReadOnly);
    m_DatePrototype->put(&m_globalExec, constructorPropertyName, m_Date, DontEnum | DontDelete | ReadOnly);
    m_RegExpPrototype->put(&m_globalExec, constructorPropertyName, m_RegExp, DontEnum | DontDelete | ReadOnly);
    m_ErrorPrototype->put(&m_globalExec, constructorPropertyName, m_Error, DontEnum | DontDelete | ReadOnly);
    m_EvalErrorPrototype->put(&m_globalExec, constructorPropertyName, m_EvalError, DontEnum | DontDelete | ReadOnly);
    m_RangeErrorPrototype->put(&m_globalExec, constructorPropertyName, m_RangeError, DontEnum | DontDelete | ReadOnly);
    m_ReferenceErrorPrototype->put(&m_globalExec, constructorPropertyName, m_ReferenceError, DontEnum | DontDelete | ReadOnly);
    m_SyntaxErrorPrototype->put(&m_globalExec, constructorPropertyName, m_SyntaxError, DontEnum | DontDelete | ReadOnly);
    m_TypeErrorPrototype->put(&m_globalExec, constructorPropertyName, m_TypeError, DontEnum | DontDelete | ReadOnly);
    m_UriErrorPrototype->put(&m_globalExec, constructorPropertyName, m_UriError, DontEnum | DontDelete | ReadOnly);

    // Set global object prototype
    JSObject* o = m_globalObject;
    while (o->prototype()->isObject())
        o = static_cast<JSObject*>(o->prototype());
    o->setPrototype(m_ObjectPrototype);

    // Set global constructors
    // FIXME: kjs_window.cpp checks Internal/DontEnum as a performance hack, to
    // see that these values can be put directly without a check for override
    // properties. Maybe we should call putDirect instead, for better encapsulation.
    m_globalObject->put(&m_globalExec, "Object", m_Object, DontEnum);
    m_globalObject->put(&m_globalExec, "Function", m_Function, DontEnum);
    m_globalObject->put(&m_globalExec, "Array", m_Array, DontEnum);
    m_globalObject->put(&m_globalExec, "Boolean", m_Boolean, DontEnum);
    m_globalObject->put(&m_globalExec, "String", m_String, DontEnum);
    m_globalObject->put(&m_globalExec, "Number", m_Number, DontEnum);
    m_globalObject->put(&m_globalExec, "Date", m_Date, DontEnum);
    m_globalObject->put(&m_globalExec, "RegExp", m_RegExp, DontEnum);
    m_globalObject->put(&m_globalExec, "Error", m_Error, DontEnum);
    m_globalObject->put(&m_globalExec, "EvalError",m_EvalError, Internal);
    m_globalObject->put(&m_globalExec, "RangeError",m_RangeError, Internal);
    m_globalObject->put(&m_globalExec, "ReferenceError",m_ReferenceError, Internal);
    m_globalObject->put(&m_globalExec, "SyntaxError",m_SyntaxError, Internal);
    m_globalObject->put(&m_globalExec, "TypeError",m_TypeError, Internal);
    m_globalObject->put(&m_globalExec, "URIError",m_UriError, Internal);

    // Set global values
    m_globalObject->put(&m_globalExec, "Math", new MathObjectImp(&m_globalExec, m_ObjectPrototype), DontEnum);
    m_globalObject->put(&m_globalExec, "NaN", jsNaN(), DontEnum|DontDelete);
    m_globalObject->put(&m_globalExec, "Infinity", jsNumber(Inf), DontEnum|DontDelete);
    m_globalObject->put(&m_globalExec, "undefined", jsUndefined(), DontEnum|DontDelete);
    
    // Set global functions
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::Eval, 1, "eval"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::ParseInt, 2, "parseInt"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::ParseFloat, 1, "parseFloat"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::IsNaN, 1, "isNaN"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::IsFinite, 1, "isFinite"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::Escape, 1, "escape"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::UnEscape, 1, "unescape"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::DecodeURI, 1, "decodeURI"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::DecodeURIComponent, 1, "decodeURIComponent"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::EncodeURI, 1, "encodeURI"), DontEnum);
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::EncodeURIComponent, 1, "encodeURIComponent"), DontEnum);
#ifndef NDEBUG
    m_globalObject->putDirectFunction(new GlobalFuncImp(&m_globalExec, m_FunctionPrototype, GlobalFuncImp::KJSPrint, 1, "kjsprint"), DontEnum);
#endif
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

    if (m_globalExec.exception() && !m_globalExec.exception()->marked())
        m_globalExec.exception()->mark();

    if (m_globalObject && !m_globalObject->marked())
        m_globalObject->mark();

    if (m_Object && !m_Object->marked())
        m_Object->mark();
    if (m_Function && !m_Function->marked())
        m_Function->mark();
    if (m_Array && !m_Array->marked())
        m_Array->mark();
    if (m_Boolean && !m_Boolean->marked())
        m_Boolean->mark();
    if (m_String && !m_String->marked())
        m_String->mark();
    if (m_Number && !m_Number->marked())
        m_Number->mark();
    if (m_Date && !m_Date->marked())
        m_Date->mark();
    if (m_RegExp && !m_RegExp->marked())
        m_RegExp->mark();
    if (m_Error && !m_Error->marked())
        m_Error->mark();
    
    if (m_ObjectPrototype && !m_ObjectPrototype->marked())
        m_ObjectPrototype->mark();
    if (m_FunctionPrototype && !m_FunctionPrototype->marked())
        m_FunctionPrototype->mark();
    if (m_ArrayPrototype && !m_ArrayPrototype->marked())
        m_ArrayPrototype->mark();
    if (m_BooleanPrototype && !m_BooleanPrototype->marked())
        m_BooleanPrototype->mark();
    if (m_StringPrototype && !m_StringPrototype->marked())
        m_StringPrototype->mark();
    if (m_NumberPrototype && !m_NumberPrototype->marked())
        m_NumberPrototype->mark();
    if (m_DatePrototype && !m_DatePrototype->marked())
        m_DatePrototype->mark();
    if (m_RegExpPrototype && !m_RegExpPrototype->marked())
        m_RegExpPrototype->mark();
    if (m_ErrorPrototype && !m_ErrorPrototype->marked())
        m_ErrorPrototype->mark();
    
    if (m_EvalError && !m_EvalError->marked())
        m_EvalError->mark();
    if (m_RangeError && !m_RangeError->marked())
        m_RangeError->mark();
    if (m_ReferenceError && !m_ReferenceError->marked())
        m_ReferenceError->mark();
    if (m_SyntaxError && !m_SyntaxError->marked())
        m_SyntaxError->mark();
    if (m_TypeError && !m_TypeError->marked())
        m_TypeError->mark();
    if (m_UriError && !m_UriError->marked())
        m_UriError->mark();
    
    if (m_EvalErrorPrototype && !m_EvalErrorPrototype->marked())
        m_EvalErrorPrototype->mark();
    if (m_RangeErrorPrototype && !m_RangeErrorPrototype->marked())
        m_RangeErrorPrototype->mark();
    if (m_ReferenceErrorPrototype && !m_ReferenceErrorPrototype->marked())
        m_ReferenceErrorPrototype->mark();
    if (m_SyntaxErrorPrototype && !m_SyntaxErrorPrototype->marked())
        m_SyntaxErrorPrototype->mark();
    if (m_TypeErrorPrototype && !m_TypeErrorPrototype->marked())
        m_TypeErrorPrototype->mark();
    if (m_UriErrorPrototype && !m_UriErrorPrototype->marked())
        m_UriErrorPrototype->mark();
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
