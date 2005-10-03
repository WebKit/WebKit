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
#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#if APPLE_CHANGES
#include "runtime.h"
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "internal.h"
#include "collector.h"
#include "operations.h"
#include "error_object.h"
#include "nodes.h"
#include "context.h"

using namespace KJS;

// ------------------------------ Context --------------------------------------

const ScopeChain &Context::scopeChain() const
{
  return rep->scopeChain();
}

ObjectImp *Context::variableObject() const
{
  return rep->variableObject();
}

ObjectImp *Context::thisValue() const
{
  return rep->thisValue();
}

const Context Context::callingContext() const
{
  return rep->callingContext();
}

// ------------------------------ Interpreter ----------------------------------

Interpreter::Interpreter(ObjectImp *global) 
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
  rep = new InterpreterImp(this, new ObjectImp);
}

Interpreter::~Interpreter()
{
  delete rep;
}

ObjectImp *Interpreter::globalObject() const
{
  return rep->globalObject();
}

void Interpreter::initGlobalObject()
{
  rep->initGlobalObject();
}

void Interpreter::lock()
{
  InterpreterImp::lock();
}

void Interpreter::unlock()
{
  InterpreterImp::unlock();
}

int Interpreter::lockCount()
{
  return InterpreterImp::lockCount();
}

ExecState *Interpreter::globalExec()
{
  return rep->globalExec();
}

bool Interpreter::checkSyntax(const UString &code)
{
  return rep->checkSyntax(code);
}

Completion Interpreter::evaluate(const UString &code, ValueImp *thisV, const UString &)
{
  return evaluate(UString(), 0, code, thisV);
}

Completion Interpreter::evaluate(const UString &sourceURL, int startingLineNumber, const UString &code, ValueImp *thisV)
{
  Completion comp = rep->evaluate(code,thisV, sourceURL, startingLineNumber);

#if APPLE_CHANGES
  if (shouldPrintExceptions() && comp.complType() == Throw) {
    InterpreterLock lock;
    ExecState *exec = rep->globalExec();
    char *f = strdup(sourceURL.ascii());
    const char *message = comp.value()->toObject(exec)->toString(exec).ascii();
    printf("[%d] %s:%s\n", getpid(), f, message);

    free(f);
  }
#endif

  return comp;
}

ObjectImp *Interpreter::builtinObject() const
{
  return rep->builtinObject();
}

ObjectImp *Interpreter::builtinFunction() const
{
  return rep->builtinFunction();
}

ObjectImp *Interpreter::builtinArray() const
{
  return rep->builtinArray();
}

ObjectImp *Interpreter::builtinBoolean() const
{
  return rep->builtinBoolean();
}

ObjectImp *Interpreter::builtinString() const
{
  return rep->builtinString();
}

ObjectImp *Interpreter::builtinNumber() const
{
  return rep->builtinNumber();
}

ObjectImp *Interpreter::builtinDate() const
{
  return rep->builtinDate();
}

ObjectImp *Interpreter::builtinRegExp() const
{
  return rep->builtinRegExp();
}

ObjectImp *Interpreter::builtinError() const
{
  return rep->builtinError();
}

ObjectImp *Interpreter::builtinObjectPrototype() const
{
  return rep->builtinObjectPrototype();
}

ObjectImp *Interpreter::builtinFunctionPrototype() const
{
  return rep->builtinFunctionPrototype();
}

ObjectImp *Interpreter::builtinArrayPrototype() const
{
  return rep->builtinArrayPrototype();
}

ObjectImp *Interpreter::builtinBooleanPrototype() const
{
  return rep->builtinBooleanPrototype();
}

ObjectImp *Interpreter::builtinStringPrototype() const
{
  return rep->builtinStringPrototype();
}

ObjectImp *Interpreter::builtinNumberPrototype() const
{
  return rep->builtinNumberPrototype();
}

ObjectImp *Interpreter::builtinDatePrototype() const
{
  return rep->builtinDatePrototype();
}

ObjectImp *Interpreter::builtinRegExpPrototype() const
{
  return rep->builtinRegExpPrototype();
}

ObjectImp *Interpreter::builtinErrorPrototype() const
{
  return rep->builtinErrorPrototype();
}

ObjectImp *Interpreter::builtinEvalError() const
{
  return rep->builtinEvalError();
}

ObjectImp *Interpreter::builtinRangeError() const
{
  return rep->builtinRangeError();
}

ObjectImp *Interpreter::builtinReferenceError() const
{
  return rep->builtinReferenceError();
}

ObjectImp *Interpreter::builtinSyntaxError() const
{
  return rep->builtinSyntaxError();
}

ObjectImp *Interpreter::builtinTypeError() const
{
  return rep->builtinTypeError();
}

ObjectImp *Interpreter::builtinURIError() const
{
  return rep->builtinURIError();
}

ObjectImp *Interpreter::builtinEvalErrorPrototype() const
{
  return rep->builtinEvalErrorPrototype();
}

ObjectImp *Interpreter::builtinRangeErrorPrototype() const
{
  return rep->builtinRangeErrorPrototype();
}

ObjectImp *Interpreter::builtinReferenceErrorPrototype() const
{
  return rep->builtinReferenceErrorPrototype();
}

ObjectImp *Interpreter::builtinSyntaxErrorPrototype() const
{
  return rep->builtinSyntaxErrorPrototype();
}

ObjectImp *Interpreter::builtinTypeErrorPrototype() const
{
  return rep->builtinTypeErrorPrototype();
}

ObjectImp *Interpreter::builtinURIErrorPrototype() const
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

#if APPLE_CHANGES
static bool printExceptions = false;

bool Interpreter::shouldPrintExceptions()
{
  return printExceptions;
}

void Interpreter::setShouldPrintExceptions(bool print)
{
  printExceptions = print;
}


void *Interpreter::createLanguageInstanceForValue(ExecState *exec, int language, ObjectImp *value, const Bindings::RootObject *origin, const Bindings::RootObject *current)
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
