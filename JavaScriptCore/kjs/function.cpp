// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "function.h"

#include "internal.h"
#include "function_object.h"
#include "lexer.h"
#include "nodes.h"
#include "operations.h"
#include "debugger.h"
#include "context.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

using namespace KJS;

// ----------------------------- FunctionImp ----------------------------------

const ClassInfo FunctionImp::info = {"Function", &InternalFunctionImp::info, 0, 0};

namespace KJS {
  class Parameter {
  public:
    Parameter(const Identifier &n) : name(n), next(0L) { }
    ~Parameter() { delete next; }
    Identifier name;
    Parameter *next;
  };
};

FunctionImp::FunctionImp(ExecState *exec, const Identifier &n)
  : InternalFunctionImp(
      static_cast<FunctionPrototypeImp*>(exec->interpreter()->builtinFunctionPrototype().imp())
      ), param(0L), ident(n)
{
}

FunctionImp::~FunctionImp()
{
  delete param;
}

bool FunctionImp::implementsCall() const
{
  return true;
}

Value FunctionImp::call(ExecState *exec, Object &thisObj, const List &args)
{
  Object &globalObj = exec->interpreter()->globalObject();

  Debugger *dbg = exec->interpreter()->imp()->debugger();
  int sid = -1;
  int lineno = -1;
  if (dbg) {
    if (inherits(&DeclaredFunctionImp::info)) {
      sid = static_cast<DeclaredFunctionImp*>(this)->body->sourceId();
      lineno = static_cast<DeclaredFunctionImp*>(this)->body->firstLine();
    }

    Object func(this);
    bool cont = dbg->callEvent(exec,sid,lineno,func,args);
    if (!cont) {
      dbg->imp()->abort();
      return Undefined();
    }
  }

  // enter a new execution context
  ContextImp ctx(globalObj, exec->interpreter()->imp(), thisObj, codeType(),
                 exec->context().imp(), this, &args);
  ExecState newExec(exec->interpreter(), &ctx);
  newExec.setException(exec->exception()); // could be null

  // assign user supplied arguments to parameters
  processParameters(&newExec, args);
  // add variable declarations (initialized to undefined)
  processVarDecls(&newExec);

  Completion comp = execute(&newExec);

  // if an exception occured, propogate it back to the previous execution object
  if (newExec.hadException())
    exec->setException(newExec.exception());

#ifdef KJS_VERBOSE
  if (comp.complType() == Throw)
    printInfo(exec,"throwing", comp.value());
  else if (comp.complType() == ReturnValue)
    printInfo(exec,"returning", comp.value());
  else
    fprintf(stderr, "returning: undefined\n");
#endif

  if (dbg) {
    Object func(this);
    int cont = dbg->returnEvent(exec,sid,lineno,func);
    if (!cont) {
      dbg->imp()->abort();
      return Undefined();
    }
  }

  if (comp.complType() == Throw) {
    exec->setException(comp.value());
    return comp.value();
  }
  else if (comp.complType() == ReturnValue)
    return comp.value();
  else
    return Undefined();
}

void FunctionImp::addParameter(const Identifier &n)
{
  Parameter **p = &param;
  while (*p)
    p = &(*p)->next;

  *p = new Parameter(n);
}

UString FunctionImp::parameterString() const
{
  UString s;
  const Parameter *p = param;
  while (p) {
    if (!s.isEmpty())
        s += ", ";
    s += p->name.ustring();
    p = p->next;
  }

  return s;
}


// ECMA 10.1.3q
void FunctionImp::processParameters(ExecState *exec, const List &args)
{
  Object variable = exec->context().imp()->variableObject();

#ifdef KJS_VERBOSE
  fprintf(stderr, "---------------------------------------------------\n"
	  "processing parameters for %s call\n",
	  name().isEmpty() ? "(internal)" : name().ascii());
#endif

  if (param) {
    ListIterator it = args.begin();
    Parameter *p = param;
    while (p) {
      if (it != args.end()) {
#ifdef KJS_VERBOSE
	fprintf(stderr, "setting parameter %s ", p->name.ascii());
	printInfo(exec,"to", *it);
#endif
	variable.put(exec, p->name, *it);
	it++;
      } else
	variable.put(exec, p->name, Undefined());
      p = p->next;
    }
  }
#ifdef KJS_VERBOSE
  else {
    for (int i = 0; i < args.size(); i++)
      printInfo(exec,"setting argument", args[i]);
  }
#endif
}

void FunctionImp::processVarDecls(ExecState */*exec*/)
{
}

Value FunctionImp::get(ExecState *exec, const Identifier &propertyName) const
{
    // Find the arguments from the closest context.
    if (propertyName == argumentsPropertyName) {
        ContextImp *context = exec->_context;
        while (context) {
            if (context->function() == this)
                return static_cast<ActivationImp *>
                    (context->activationObject())->get(exec, propertyName);
            context = context->callingContext();
        }
        return Undefined();
    }
    
    // Compute length of parameters.
    if (propertyName == lengthPropertyName) {
        const Parameter * p = param;
        int count = 0;
        while (p) {
            ++count;
            p = p->next;
        }
        return Number(count);
    }
    
    return InternalFunctionImp::get(exec, propertyName);
}

void FunctionImp::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
{
    if (propertyName == argumentsPropertyName || propertyName == lengthPropertyName)
        return;
    InternalFunctionImp::put(exec, propertyName, value, attr);
}

bool FunctionImp::hasProperty(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == argumentsPropertyName || propertyName == lengthPropertyName)
        return true;
    return InternalFunctionImp::hasProperty(exec, propertyName);
}

bool FunctionImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    if (propertyName == argumentsPropertyName || propertyName == lengthPropertyName)
        return false;
    return InternalFunctionImp::deleteProperty(exec, propertyName);
}

// ------------------------------ DeclaredFunctionImp --------------------------

// ### is "Function" correct here?
const ClassInfo DeclaredFunctionImp::info = {"Function", &FunctionImp::info, 0, 0};

DeclaredFunctionImp::DeclaredFunctionImp(ExecState *exec, const Identifier &n,
					 FunctionBodyNode *b, const ScopeChain &sc)
  : FunctionImp(exec,n), body(b)
{
  Value protect(this);
  body->ref();
  setScope(sc);
}

DeclaredFunctionImp::~DeclaredFunctionImp()
{
  if ( body->deref() )
    delete body;
}

bool DeclaredFunctionImp::implementsConstruct() const
{
  return true;
}

// ECMA 13.2.2 [[Construct]]
Object DeclaredFunctionImp::construct(ExecState *exec, const List &args)
{
  Object proto;
  Value p = get(exec,prototypePropertyName);
  if (p.type() == ObjectType)
    proto = Object(static_cast<ObjectImp*>(p.imp()));
  else
    proto = exec->interpreter()->builtinObjectPrototype();

  Object obj(new ObjectImp(proto));

  Value res = call(exec,obj,args);

  if (res.type() == ObjectType)
    return Object::dynamicCast(res);
  else
    return obj;
}

Completion DeclaredFunctionImp::execute(ExecState *exec)
{
  Completion result = body->execute(exec);

  if (result.complType() == Throw || result.complType() == ReturnValue)
      return result;
  return Completion(Normal, Undefined()); // TODO: or ReturnValue ?
}

void DeclaredFunctionImp::processVarDecls(ExecState *exec)
{
  body->processVarDecls(exec);
}

// ------------------------------ ArgumentsImp ---------------------------------

const ClassInfo ArgumentsImp::info = {"Arguments", 0, 0, 0};

// ECMA 10.1.8
ArgumentsImp::ArgumentsImp(ExecState *exec, FunctionImp *func)
  : ArrayInstanceImp(exec->interpreter()->builtinObjectPrototype().imp(), 0)
{
  Value protect(this);
  putDirect(calleePropertyName, func, DontEnum);
}

ArgumentsImp::ArgumentsImp(ExecState *exec, FunctionImp *func, const List &args)
  : ArrayInstanceImp(exec->interpreter()->builtinObjectPrototype().imp(), args)
{
  Value protect(this);
  putDirect(calleePropertyName, func, DontEnum);
}

// ------------------------------ ActivationImp --------------------------------

const ClassInfo ActivationImp::info = {"Activation", 0, 0, 0};

// ECMA 10.1.6
ActivationImp::ActivationImp(FunctionImp *function, const List &arguments)
    : _function(function), _arguments(true), _argumentsObject(0)
{
  _arguments = arguments.copy();
  // FIXME: Do we need to support enumerating the arguments property?
}

Value ActivationImp::get(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == argumentsPropertyName) {
        if (!_argumentsObject)
            createArgumentsObject(exec);
        return Value(_argumentsObject);
    }
    return ObjectImp::get(exec, propertyName);
}

void ActivationImp::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
{
    if (propertyName == argumentsPropertyName) {
        // FIXME: Do we need to allow overwriting this?
        return;
    }
    ObjectImp::put(exec, propertyName, value, attr);
}

bool ActivationImp::hasProperty(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == argumentsPropertyName)
        return true;
    return ObjectImp::hasProperty(exec, propertyName);
}

bool ActivationImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    if (propertyName == argumentsPropertyName)
        return false;
    return ObjectImp::deleteProperty(exec, propertyName);
}

void ActivationImp::mark()
{
    if (_function && !_function->marked()) 
        _function->mark();
    _arguments.mark();
    if (_argumentsObject && !_argumentsObject->marked())
        _argumentsObject->mark();
    ObjectImp::mark();
}

void ActivationImp::createArgumentsObject(ExecState *exec) const
{
  _argumentsObject = new ArgumentsImp(exec, _function, _arguments);
}

// ------------------------------ GlobalFunc -----------------------------------


GlobalFuncImp::GlobalFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto, int i, int len)
  : InternalFunctionImp(funcProto), id(i)
{
  Value protect(this);
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

CodeType GlobalFuncImp::codeType() const
{
  return id == Eval ? EvalCode : codeType();
}

bool GlobalFuncImp::implementsCall() const
{
  return true;
}

static Value encode(ExecState *exec, const List &args, const char *do_not_escape)
{
  UString r = "", s, str = args[0].toString(exec);
  CString cstr = str.UTF8String();
  const char *p = cstr.c_str();
  for (int k = 0; k < cstr.size(); k++, p++) {
    char c = *p;
    if (c && strchr(do_not_escape, c)) {
      r.append(c);
    } else {
      char tmp[4];
      sprintf(tmp, "%%%02X", (unsigned char)c);
      r += tmp;
    }
  }
  return String(r);
}

static Value decode(ExecState *exec, const List &args, const char *do_not_unescape, bool strict)
{
  UString s = "", str = args[0].toString(exec);
  int k = 0, len = str.size();
  const UChar *d = str.data();
  UChar u;
  while (k < len) {
    const UChar *p = d + k;
    UChar c = *p;
    if (c == '%') {
      int charLen = 0;
      if (k <= len - 3 && isxdigit(p[1].uc) && isxdigit(p[2].uc)) {
        const char b0 = Lexer::convertHex(p[1].uc, p[2].uc);
        const int sequenceLen = UTF8SequenceLength(b0);
        if (sequenceLen != 0 && k <= len - sequenceLen * 3) {
          charLen = sequenceLen * 3;
          char sequence[5];
          sequence[0] = b0;
          for (int i = 1; i < sequenceLen; ++i) {
            const UChar *q = p + i * 3;
            if (q[0] == '%' && isxdigit(q[1].uc) && isxdigit(q[2].uc))
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
        if (strict) {
	  Object error = Error::create(exec, URIError);
          exec->setException(error);
          return error;
        }
        // The only case where we don't use "strict" mode is the "unescape" function.
        // For that, it's good to support the wonky "%u" syntax for compatibility with WinIE.
        if (k <= len - 6 && p[1] == 'u'
            && isxdigit(p[2].uc) && isxdigit(p[3].uc)
            && isxdigit(p[4].uc) && isxdigit(p[5].uc)) {
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
  return String(s);
}

Value GlobalFuncImp::call(ExecState *exec, Object &/*thisObj*/, const List &args)
{
  Value res;

  static const char do_not_escape[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "*+-./@_";
  static const char do_not_escape_when_encoding_URI_component[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!'()*-._~";
  static const char do_not_escape_when_encoding_URI[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!#$&'()*+,-./:;=?@_~";
  static const char do_not_unescape_when_decoding_URI[] =
    "#$&+,/:;=?@";

  switch (id) {
  case Eval: { // eval()
    Value x = args[0];
    if (x.type() != StringType)
      return x;
    else {
      UString s = x.toString(exec);

      int sid;
      int errLine;
      UString errMsg;
      ProgramNode *progNode = Parser::parse(s.data(),s.size(),&sid,&errLine,&errMsg);

      // no program node means a syntax occurred
      if (!progNode) {
	Object err = Error::create(exec,SyntaxError,errMsg.ascii(),errLine);
        err.put(exec,"sid",Number(sid));
        exec->setException(err);
        return err;
      }

      progNode->ref();

      // enter a new execution context
      Object thisVal(Object::dynamicCast(exec->context().thisValue()));
      ContextImp ctx(exec->interpreter()->globalObject(),
                     exec->interpreter()->imp(),
                     thisVal,
                     EvalCode,
                     exec->context().imp());

      ExecState newExec(exec->interpreter(), &ctx);
      newExec.setException(exec->exception()); // could be null

      // execute the code
      Completion c = progNode->execute(&newExec);

      // if an exception occured, propogate it back to the previous execution object
      if (newExec.hadException())
        exec->setException(newExec.exception());

      if ( progNode->deref() )
          delete progNode;
      if (c.complType() == ReturnValue)
	  return c.value();
      // ### setException() on throw?
      else if (c.complType() == Normal) {
	  if (c.isValueCompletion())
	      return c.value();
	  else
	      return Undefined();
      } else {
	  return Undefined();
      }
    }
    break;
  }
  case ParseInt: {
    CString cstr = args[0].toString(exec).cstring();
    int radix = args[1].toInt32(exec);

    char* endptr;
    errno = 0;
#ifdef HAVE_FUNC_STRTOLL
    long long llValue = strtoll(cstr.c_str(), &endptr, radix);
    double value = llValue;
#else
    long value = strtol(cstr.c_str(), &endptr, radix);
#endif
    if (errno != 0 || endptr == cstr.c_str())
      res = Number(NaN);
    else
      res = Number(value);
    break;
  }
  case ParseFloat:
    res = Number(args[0].toString(exec).toDouble( true /*tolerant*/ ));
    break;
  case IsNaN:
    res = Boolean(isNaN(args[0].toNumber(exec)));
    break;
  case IsFinite: {
    double n = args[0].toNumber(exec);
    res = Boolean(!isNaN(n) && !isInf(n));
    break;
  }
  case DecodeURI:
    res = decode(exec, args, do_not_unescape_when_decoding_URI, true);
    break;
  case DecodeURIComponent:
    res = decode(exec, args, "", true);
    break;
  case EncodeURI:
    res = encode(exec, args, do_not_escape_when_encoding_URI);
    break;
  case EncodeURIComponent:
    res = encode(exec, args, do_not_escape_when_encoding_URI_component);
    break;
  case Escape:
    res = encode(exec, args, do_not_escape);
    break;
  case UnEscape:
    res = decode(exec, args, "", false);
    break;
#ifndef NDEBUG
  case KJSPrint:
    puts(args[0].toString(exec).ascii());
    break;
#endif
  }

  return res;
}
