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

#if APPLE_CHANGES
#include <unicode/uchar.h>
#endif

namespace KJS {

// ----------------------------- FunctionImp ----------------------------------

const ClassInfo FunctionImp::info = {"Function", &InternalFunctionImp::info, 0, 0};

  class Parameter {
  public:
    Parameter(const Identifier &n) : name(n), next(0L) { }
    ~Parameter() { delete next; }
    Identifier name;
    Parameter *next;
  };

FunctionImp::FunctionImp(ExecState *exec, const Identifier &n)
  : InternalFunctionImp(
      static_cast<FunctionPrototypeImp*>(exec->lexicalInterpreter()->builtinFunctionPrototype().imp())
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
  Object &globalObj = exec->dynamicInterpreter()->globalObject();

  Debugger *dbg = exec->dynamicInterpreter()->imp()->debugger();
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
  ContextImp ctx(globalObj, exec->dynamicInterpreter()->imp(), thisObj, codeType(),
                 exec->context().imp(), this, &args);
  ExecState newExec(exec->dynamicInterpreter(), &ctx);
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
        return Null();
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

bool FunctionImp::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == argumentsPropertyName || propertyName == lengthPropertyName)
        return true;
    return InternalFunctionImp::hasOwnProperty(exec, propertyName);
}

bool FunctionImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
    if (propertyName == argumentsPropertyName || propertyName == lengthPropertyName)
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
  int i = 0;
  Parameter *p = param;
  
  if(!p)
    return Identifier::null();
  
  // skip to the parameter we want
  while (i++ < index && (p = p->next))
    ;
  
  if (!p)
    return Identifier::null();
  
  Identifier name = p->name;

  // Are there any subsequent parameters with the same name?
  while ((p = p->next))
    if (p->name == name)
      return Identifier::null();
  
  return name;
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
    proto = exec->lexicalInterpreter()->builtinObjectPrototype();

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

// ------------------------------ IndexToNameMap ---------------------------------

// We map indexes in the arguments array to their corresponding argument names. 
// Example: function f(x, y, z): arguments[0] = x, so we map 0 to Identifier("x"). 

// Once we have an argument name, we can get and set the argument's value in the 
// activation object.

// We use Identifier::null to indicate that a given argument's value
// isn't stored in the activation object.

IndexToNameMap::IndexToNameMap(FunctionImp *func, const List &args)
{
  _map = new Identifier[args.size()];
  this->size = args.size();
  
  int i = 0;
  ListIterator iterator = args.begin(); 
  for (; iterator != args.end(); i++, iterator++)
    _map[i] = func->getParameterName(i); // null if there is no corresponding parameter
}

IndexToNameMap::~IndexToNameMap() {
  delete [] _map;
}

bool IndexToNameMap::isMapped(const Identifier &index) const
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

void IndexToNameMap::unMap(const Identifier &index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  assert(indexIsNumber && indexAsNumber < size);
  
  _map[indexAsNumber] = Identifier::null();
}

Identifier& IndexToNameMap::operator[](int index)
{
  return _map[index];
}

Identifier& IndexToNameMap::operator[](const Identifier &index)
{
  bool indexIsNumber;
  int indexAsNumber = index.toUInt32(&indexIsNumber);

  assert(indexIsNumber && indexAsNumber < size);
  
  return (*this)[indexAsNumber];
}

// ------------------------------ ArgumentsImp ---------------------------------

const ClassInfo ArgumentsImp::info = {"Arguments", 0, 0, 0};

// ECMA 10.1.8
ArgumentsImp::ArgumentsImp(ExecState *exec, FunctionImp *func, const List &args, ActivationImp *act)
: ObjectImp(exec->lexicalInterpreter()->builtinObjectPrototype()), 
_activationObject(act),
indexToNameMap(func, args)
{
  Value protect(this);
  putDirect(calleePropertyName, func, DontEnum);
  putDirect(lengthPropertyName, args.size(), DontEnum);
  
  int i = 0;
  ListIterator iterator = args.begin(); 
  for (; iterator != args.end(); i++, iterator++) {
    if (!indexToNameMap.isMapped(Identifier::from(i))) {
      ObjectImp::put(exec, Identifier::from(i), *iterator, DontEnum);
    }
  }
}

void ArgumentsImp::mark() 
{
  ObjectImp::mark();
  if (_activationObject && !_activationObject->marked())
    _activationObject->mark();
}

Value ArgumentsImp::get(ExecState *exec, const Identifier &propertyName) const
{
  if (indexToNameMap.isMapped(propertyName)) {
    return _activationObject->get(exec, indexToNameMap[propertyName]);
  } else {
    return ObjectImp::get(exec, propertyName);
  }
}

void ArgumentsImp::put(ExecState *exec, const Identifier &propertyName, const Value &value, int attr)
{
  if (indexToNameMap.isMapped(propertyName)) {
    _activationObject->put(exec, indexToNameMap[propertyName], value, attr);
  } else {
    ObjectImp::put(exec, propertyName, value, attr);
  }
}

bool ArgumentsImp::deleteProperty(ExecState *exec, const Identifier &propertyName) 
{
  if (indexToNameMap.isMapped(propertyName)) {
    indexToNameMap.unMap(propertyName);
    return true;
  } else {
    return ObjectImp::deleteProperty(exec, propertyName);
  }
}

bool ArgumentsImp::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
{
  if (indexToNameMap.isMapped(propertyName))
    return true;
  
  return ObjectImp::hasOwnProperty(exec, propertyName);
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
        // check for locally declared arguments property
        ValueImp *v = getDirect(propertyName);
        if (v)
            return Value(v);

        // default: return builtin arguments array
        if (!_argumentsObject)
                createArgumentsObject(exec);
        return Value(_argumentsObject);
    }
    return ObjectImp::get(exec, propertyName);
}

bool ActivationImp::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
{
    if (propertyName == argumentsPropertyName)
        return true;
    return ObjectImp::hasOwnProperty(exec, propertyName);
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
  _argumentsObject = new ArgumentsImp(exec, _function, _arguments, const_cast<ActivationImp *>(this));
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
#if APPLE_CHANGES
            return u_charType(c) == U_SPACE_SEPARATOR;
#else
            // ### properly support other Unicode Zs characters
            return false;
#endif
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

static double parseInt(const UString &s, int radix)
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

    if (!sawDigit)
        return NaN;

    return sign * number;
}

static double parseFloat(const UString &s)
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
      ProgramNode *progNode = Parser::parse(UString(), 0, s.data(),s.size(),&sid,&errLine,&errMsg);

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
      ContextImp ctx(exec->dynamicInterpreter()->globalObject(),
                     exec->dynamicInterpreter()->imp(),
                     thisVal,
                     EvalCode,
                     exec->context().imp());

      ExecState newExec(exec->dynamicInterpreter(), &ctx);
      newExec.setException(exec->exception()); // could be null

      // execute the code
      progNode->processVarDecls(&newExec);
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
  case ParseInt:
    res = Number(parseInt(args[0].toString(exec), args[1].toInt32(exec)));
    break;
  case ParseFloat:
    res = Number(parseFloat(args[0].toString(exec)));
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
    {
      UString r = "", s, str = args[0].toString(exec);
      const UChar *c = str.data();
      for (int k = 0; k < str.size(); k++, c++) {
        int u = c->uc;
        if (u > 255) {
          char tmp[7];
          sprintf(tmp, "%%u%04X", u);
          s = UString(tmp);
        } else if (u != 0 && strchr(do_not_escape, (char)u)) {
          s = UString(c, 1);
        } else {
          char tmp[4];
          sprintf(tmp, "%%%02X", u);
          s = UString(tmp);
        }
        r += s;
      }
      res = String(r);
      break;
    }
  case UnEscape:
    {
      UString s = "", str = args[0].toString(exec);
      int k = 0, len = str.size();
      while (k < len) {
        const UChar *c = str.data() + k;
        UChar u;
        if (*c == UChar('%') && k <= len - 6 && *(c+1) == UChar('u')) {
          if (Lexer::isHexDigit((c+2)->uc) && Lexer::isHexDigit((c+3)->uc) &&
              Lexer::isHexDigit((c+4)->uc) && Lexer::isHexDigit((c+5)->uc)) {
	  u = Lexer::convertUnicode((c+2)->uc, (c+3)->uc,
				    (c+4)->uc, (c+5)->uc);
	  c = &u;
	  k += 5;
          }
        } else if (*c == UChar('%') && k <= len - 3 &&
                   Lexer::isHexDigit((c+1)->uc) && Lexer::isHexDigit((c+2)->uc)) {
          u = UChar(Lexer::convertHex((c+1)->uc, (c+2)->uc));
          c = &u;
          k += 2;
        }
        k++;
        s += UString(c, 1);
      }
      res = String(s);
      break;
    }
#ifndef NDEBUG
  case KJSPrint:
    puts(args[0].toString(exec).ascii());
    break;
#endif
  }

  return res;
}

} // namespace
