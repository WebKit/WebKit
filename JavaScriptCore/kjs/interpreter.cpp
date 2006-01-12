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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "interpreter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "collector.h"
#include "context.h"
#include "error_object.h"
#include "internal.h"
#include "nodes.h"
#include "object.h"
#include "operations.h"
#include "runtime.h"
#include "types.h"
#include "value.h"

namespace KJS {

// ------------------------------ Context --------------------------------------

const ScopeChain &Context::scopeChain() const
{
  return rep->scopeChain();
}

JSObject *Context::variableObject() const
{
  return rep->variableObject();
}

JSObject *Context::thisValue() const
{
  return rep->thisValue();
}

const Context Context::callingContext() const
{
  return rep->callingContext();
}

// ------------------------------ Interpreter ----------------------------------

Interpreter::Interpreter(JSObject *global) 
  : rep(0)
  , m_argumentsPropertyName(&argumentsPropertyName)
  , m_specialPrototypePropertyName(&specialPrototypePropertyName)
{
  rep = new InterpreterImp(this, global);
}

Interpreter::Interpreter()
  : rep(0)
  , m_argumentsPropertyName(&argumentsPropertyName)
  , m_specialPrototypePropertyName(&specialPrototypePropertyName)
{
  rep = new InterpreterImp(this, new JSObject);
}

Interpreter::~Interpreter()
{
  delete rep;
}

JSObject *Interpreter::globalObject() const
{
  return rep->globalObject();
}

void Interpreter::initGlobalObject()
{
  rep->initGlobalObject();
}

ExecState *Interpreter::globalExec()
{
  return rep->globalExec();
}

bool Interpreter::checkSyntax(const UString &code)
{
  return rep->checkSyntax(code);
}

Completion Interpreter::evaluate(const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV)
{
    return evaluate(sourceURL, startingLineNumber, code.data(), code.size());
}

Completion Interpreter::evaluate(const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength, JSValue* thisV)
{
    Completion comp = rep->evaluate(code, codeLength, thisV, sourceURL, startingLineNumber);

    if (shouldPrintExceptions() && comp.complType() == Throw) {
        JSLock lock;
        ExecState *exec = rep->globalExec();
        CString f = sourceURL.UTF8String();
        CString message = comp.value()->toObject(exec)->toString(exec).UTF8String();
#ifdef WIN32
		printf("%s:%s\n", f.c_str(), message.c_str());
#else
		printf("[%d] %s:%s\n", getpid(), f.c_str(), message.c_str());
#endif
    }

    return comp;
}

JSObject *Interpreter::builtinObject() const
{
  return rep->builtinObject();
}

JSObject *Interpreter::builtinFunction() const
{
  return rep->builtinFunction();
}

JSObject *Interpreter::builtinArray() const
{
  return rep->builtinArray();
}

JSObject *Interpreter::builtinBoolean() const
{
  return rep->builtinBoolean();
}

JSObject *Interpreter::builtinString() const
{
  return rep->builtinString();
}

JSObject *Interpreter::builtinNumber() const
{
  return rep->builtinNumber();
}

JSObject *Interpreter::builtinDate() const
{
  return rep->builtinDate();
}

JSObject *Interpreter::builtinRegExp() const
{
  return rep->builtinRegExp();
}

JSObject *Interpreter::builtinError() const
{
  return rep->builtinError();
}

JSObject *Interpreter::builtinObjectPrototype() const
{
  return rep->builtinObjectPrototype();
}

JSObject *Interpreter::builtinFunctionPrototype() const
{
  return rep->builtinFunctionPrototype();
}

JSObject *Interpreter::builtinArrayPrototype() const
{
  return rep->builtinArrayPrototype();
}

JSObject *Interpreter::builtinBooleanPrototype() const
{
  return rep->builtinBooleanPrototype();
}

JSObject *Interpreter::builtinStringPrototype() const
{
  return rep->builtinStringPrototype();
}

JSObject *Interpreter::builtinNumberPrototype() const
{
  return rep->builtinNumberPrototype();
}

JSObject *Interpreter::builtinDatePrototype() const
{
  return rep->builtinDatePrototype();
}

JSObject *Interpreter::builtinRegExpPrototype() const
{
  return rep->builtinRegExpPrototype();
}

JSObject *Interpreter::builtinErrorPrototype() const
{
  return rep->builtinErrorPrototype();
}

JSObject *Interpreter::builtinEvalError() const
{
  return rep->builtinEvalError();
}

JSObject *Interpreter::builtinRangeError() const
{
  return rep->builtinRangeError();
}

JSObject *Interpreter::builtinReferenceError() const
{
  return rep->builtinReferenceError();
}

JSObject *Interpreter::builtinSyntaxError() const
{
  return rep->builtinSyntaxError();
}

JSObject *Interpreter::builtinTypeError() const
{
  return rep->builtinTypeError();
}

JSObject *Interpreter::builtinURIError() const
{
  return rep->builtinURIError();
}

JSObject *Interpreter::builtinEvalErrorPrototype() const
{
  return rep->builtinEvalErrorPrototype();
}

JSObject *Interpreter::builtinRangeErrorPrototype() const
{
  return rep->builtinRangeErrorPrototype();
}

JSObject *Interpreter::builtinReferenceErrorPrototype() const
{
  return rep->builtinReferenceErrorPrototype();
}

JSObject *Interpreter::builtinSyntaxErrorPrototype() const
{
  return rep->builtinSyntaxErrorPrototype();
}

JSObject *Interpreter::builtinTypeErrorPrototype() const
{
  return rep->builtinTypeErrorPrototype();
}

JSObject *Interpreter::builtinURIErrorPrototype() const
{
  return rep->builtinURIErrorPrototype();
}

void Interpreter::setCompatMode(CompatMode mode)
{
  rep->setCompatMode(mode);
}

Interpreter::CompatMode Interpreter::compatMode() const
{
  return rep->compatMode();
}

bool Interpreter::collect()
{
  return Collector::collect();
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

#if __APPLE__
void *Interpreter::createLanguageInstanceForValue(ExecState *exec, int language, JSObject *value, const Bindings::RootObject *origin, const Bindings::RootObject *current)
{
    return Bindings::Instance::createLanguageInstanceForValue (exec, (Bindings::Instance::BindingLanguage)language, value, origin, current);
}

#endif

void Interpreter::saveBuiltins (SavedBuiltins &builtins) const
{
  rep->saveBuiltins(builtins);
}

void Interpreter::restoreBuiltins (const SavedBuiltins &builtins)
{
  rep->restoreBuiltins(builtins);
}

SavedBuiltins::SavedBuiltins() : 
  _internal(0)
{
}

SavedBuiltins::~SavedBuiltins()
{
  delete _internal;
}


void Interpreter::virtual_hook( int, void* )
{ /*BASE::virtual_hook( id, data );*/ }


Interpreter *ExecState::lexicalInterpreter() const
{
  if (!_context) {
    return dynamicInterpreter();
  }

  InterpreterImp *result = InterpreterImp::interpreterWithGlobalObject(_context->scopeChain().bottom());

  if (!result) {
    return dynamicInterpreter();
  }

  return result->interpreter();
}

}
