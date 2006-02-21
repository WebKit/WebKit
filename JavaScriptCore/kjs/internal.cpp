/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
#include "internal.h"

#include "array_object.h"
#include "bool_object.h"
#include "collector.h"
#include "context.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "function_object.h"
#include "lexer.h"
#include "math_object.h"
#include "nodes.h"
#include "number_object.h"
#include "object.h"
#include "object_object.h"
#include "operations.h"
#include "regexp_object.h"
#include "string_object.h"
#include <assert.h>
#include <kxmlcore/HashMap.h>
#include <kxmlcore/HashSet.h>
#include <kxmlcore/Vector.h>
#include <math.h>
#include <stdio.h>

extern int kjsyyparse();

namespace KJS {

#if !__APPLE__
 
#ifdef WORDS_BIGENDIAN
  const unsigned char NaN_Bytes[] = { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 };
  const unsigned char Inf_Bytes[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#elif defined(arm)
  const unsigned char NaN_Bytes[] = { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 };
  const unsigned char Inf_Bytes[] = { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 };
#else
  const unsigned char NaN_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f };
  const unsigned char Inf_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif
 
  const double NaN = *(const double*) NaN_Bytes;
  const double Inf = *(const double*) Inf_Bytes;
 
#endif

#if WIN32
#define copysign _copysign
#endif

// ------------------------------ StringImp ------------------------------------

JSValue *StringImp::toPrimitive(ExecState *, JSType) const
{
  return const_cast<StringImp *>(this);
}

bool StringImp::toBoolean(ExecState *) const
{
  return (val.size() > 0);
}

double StringImp::toNumber(ExecState *) const
{
  return val.toDouble();
}

UString StringImp::toString(ExecState *) const
{
  return val;
}

JSObject *StringImp::toObject(ExecState *exec) const
{
    return new StringInstance(exec->lexicalInterpreter()->builtinStringPrototype(), val);
}

// ------------------------------ NumberImp ------------------------------------

JSValue *NumberImp::toPrimitive(ExecState *, JSType) const
{
  return const_cast<NumberImp *>(this);
}

bool NumberImp::toBoolean(ExecState *) const
{
  return val < 0.0 || val > 0.0; // false for NaN
}

double NumberImp::toNumber(ExecState *) const
{
  return val;
}

UString NumberImp::toString(ExecState *) const
{
  if (val == 0.0) // +0.0 or -0.0
    return "0";
  return UString::from(val);
}

JSObject *NumberImp::toObject(ExecState *exec) const
{
  List args;
  args.append(const_cast<NumberImp*>(this));
  return static_cast<JSObject *>(exec->lexicalInterpreter()->builtinNumber()->construct(exec,args));
}

// FIXME: We can optimize this to work like JSValue::getUInt32. I'm ignoring it for now
// because it never shows up on profiles.
bool NumberImp::getUInt32(uint32_t& uint32) const
{
  uint32 = (uint32_t)val;
  return (double)uint32 == val;
}

// --------------------------- GetterSetterImp ---------------------------------
void GetterSetterImp::mark()
{
    if (getter && !getter->marked())
        getter->mark();
    if (setter && !setter->marked())
        setter->mark();
}

JSValue *GetterSetterImp::toPrimitive(ExecState *exec, JSType) const
{
    assert(false);
    return jsNull();
}

bool GetterSetterImp::toBoolean(ExecState *) const
{
    assert(false);
    return false;
}

double GetterSetterImp::toNumber(ExecState *) const
{
    assert(false);
    return 0.0;
}

UString GetterSetterImp::toString(ExecState *) const
{
    assert(false);
    return UString::null();
}

JSObject *GetterSetterImp::toObject(ExecState *exec) const
{
    assert(false);
    return jsNull()->toObject(exec);
}

// ------------------------------ LabelStack -----------------------------------

bool LabelStack::push(const Identifier &id)
{
  if (contains(id))
    return false;

  StackElem *newtos = new StackElem;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const Identifier &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElem *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

// ------------------------------ ContextImp -----------------------------------

// ECMA 10.2
ContextImp::ContextImp(JSObject *glob, InterpreterImp *interpreter, JSObject *thisV, CodeType type,
                       ContextImp *callingCon, FunctionImp *func, const List *args)
    : _interpreter(interpreter), _function(func), _arguments(args)
{
  m_codeType = type;
  _callingContext = callingCon;

  // create and initialize activation object (ECMA 10.1.6)
  if (type == FunctionCode || type == AnonymousCode ) {
    activation = new ActivationImp(func, *args);
    variable = activation;
  } else {
    activation = NULL;
    variable = glob;
  }

  // ECMA 10.2
  switch(type) {
    case EvalCode:
      if (_callingContext) {
        scope = _callingContext->scopeChain();
        variable = _callingContext->variableObject();
        thisVal = _callingContext->thisValue();
        break;
      } // else same as GlobalCode
    case GlobalCode:
      scope.clear();
      scope.push(glob);
      thisVal = static_cast<JSObject*>(glob);
      break;
    case FunctionCode:
    case AnonymousCode:
      if (type == FunctionCode) {
        scope = func->scope();
        scope.push(activation);
      } else {
        scope.clear();
        scope.push(glob);
        scope.push(activation);
      }
      variable = activation; // TODO: DontDelete ? (ECMA 10.2.3)
      thisVal = thisV;
      break;
    }

  _interpreter->setContext(this);
}

ContextImp::~ContextImp()
{
  _interpreter->setContext(_callingContext);
}

void ContextImp::mark()
{
  for (ContextImp *context = this; context; context = context->_callingContext) {
    context->scope.mark();
  }
}

// ------------------------------ Parser ---------------------------------------

static RefPtr<ProgramNode> *progNode;
int Parser::sid = 0;

static Vector<RefPtr<Node> >* newNodes;
static HashSet<Node*>* nodeCycles;

void Parser::saveNewNode(Node *node)
{
    if (!newNodes)
        newNodes = new Vector<RefPtr<Node> >;

    newNodes->append(node);
}

void Parser::noteNodeCycle(Node *node)
{
    if (!nodeCycles)
        nodeCycles = new HashSet<Node*>;
    nodeCycles->add(node);
}

void Parser::removeNodeCycle(Node *node)
{
    ASSERT(nodeCycles);
    nodeCycles->remove(node);
}

static void clearNewNodes()
{
    if (nodeCycles) {
        for (HashSet<Node*>::iterator it = nodeCycles->begin(); it != nodeCycles->end(); ++it)
            (*it)->breakCycle();
        delete nodeCycles;
        nodeCycles = 0;
    }

    delete newNodes;
    newNodes = 0;
}

RefPtr<ProgramNode> Parser::parse(const UString &sourceURL, int startingLineNumber,
                                     const UChar *code, unsigned int length, int *sourceId,
                                     int *errLine, UString *errMsg)
{
  if (errLine)
    *errLine = -1;
  if (errMsg)
    *errMsg = 0;
  if (!progNode)
    progNode = new RefPtr<ProgramNode>;

  Lexer::curr()->setCode(sourceURL, startingLineNumber, code, length);
  *progNode = 0;
  sid++;
  if (sourceId)
    *sourceId = sid;
  // Enable this (and the #define YYDEBUG in grammar.y) to debug a parse error
  //extern int kjsyydebug;
  //kjsyydebug=1;
  int parseError = kjsyyparse();
  bool lexError = Lexer::curr()->sawError();
  Lexer::curr()->doneParsing();
  RefPtr<ProgramNode> prog = *progNode;
  *progNode = 0;

  clearNewNodes();

  if (parseError || lexError) {
    int eline = Lexer::curr()->lineNo();
    if (errLine)
      *errLine = eline;
    if (errMsg)
      *errMsg = "Parse error";
    return RefPtr<ProgramNode>();
  }

  return prog;
}

void Parser::accept(ProgramNode *prog)
{
  *progNode = prog;
}

// ------------------------------ InterpreterImp -------------------------------

InterpreterImp* InterpreterImp::s_hook = 0L;

typedef HashMap<JSObject *, InterpreterImp *> InterpreterMap;

static inline InterpreterMap &interpreterMap()
{
    static InterpreterMap *map = new InterpreterMap;
    return *map;
}

InterpreterImp::InterpreterImp(Interpreter *interp, JSObject *glob)
    : globExec(interp, 0)
    , _context(0)
{
  // add this interpreter to the global chain
  // as a root set for garbage collection
  JSLock lock;

  m_interpreter = interp;
  if (s_hook) {
    prev = s_hook;
    next = s_hook->next;
    s_hook->next->prev = this;
    s_hook->next = this;
  } else {
    // This is the first interpreter
    s_hook = next = prev = this;
  }

  interpreterMap().set(glob, this);

  global = glob;
  dbg = 0;
  m_compatMode = Interpreter::NativeMode;

  // initialize properties of the global object
  initGlobalObject();

  recursion = 0;
}

void InterpreterImp::initGlobalObject()
{
  Identifier::init();
  
  // Contructor prototype objects (Object.prototype, Array.prototype etc)

  FunctionPrototype *funcProto = new FunctionPrototype(&globExec);
  b_FunctionPrototype = funcProto;
  ObjectPrototype *objProto = new ObjectPrototype(&globExec, funcProto);
  b_ObjectPrototype = objProto;
  funcProto->setPrototype(b_ObjectPrototype);

  ArrayPrototype *arrayProto = new ArrayPrototype(&globExec, objProto);
  b_ArrayPrototype = arrayProto;
  StringPrototype *stringProto = new StringPrototype(&globExec, objProto);
  b_StringPrototype = stringProto;
  BooleanPrototype *booleanProto = new BooleanPrototype(&globExec, objProto, funcProto);
  b_BooleanPrototype = booleanProto;
  NumberPrototype *numberProto = new NumberPrototype(&globExec, objProto, funcProto);
  b_NumberPrototype = numberProto;
  DatePrototype *dateProto = new DatePrototype(&globExec, objProto);
  b_DatePrototype = dateProto;
  RegExpPrototype *regexpProto = new RegExpPrototype(&globExec, objProto, funcProto);
  b_RegExpPrototype = regexpProto;
  ErrorPrototype *errorProto = new ErrorPrototype(&globExec, objProto, funcProto);
  b_ErrorPrototype = errorProto;

  static_cast<JSObject*>(global)->setPrototype(b_ObjectPrototype);

  // Constructors (Object, Array, etc.)
  b_Object = new ObjectObjectImp(&globExec, objProto, funcProto);
  b_Function = new FunctionObjectImp(&globExec, funcProto);
  b_Array = new ArrayObjectImp(&globExec, funcProto, arrayProto);
  b_String = new StringObjectImp(&globExec, funcProto, stringProto);
  b_Boolean = new BooleanObjectImp(&globExec, funcProto, booleanProto);
  b_Number = new NumberObjectImp(&globExec, funcProto, numberProto);
  b_Date = new DateObjectImp(&globExec, funcProto, dateProto);
  b_RegExp = new RegExpObjectImp(&globExec, funcProto, regexpProto);
  b_Error = new ErrorObjectImp(&globExec, funcProto, errorProto);

  // Error object prototypes
  b_evalErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, EvalError, "EvalError", "EvalError");
  b_rangeErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, RangeError, "RangeError", "RangeError");
  b_referenceErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, ReferenceError, "ReferenceError", "ReferenceError");
  b_syntaxErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, SyntaxError, "SyntaxError", "SyntaxError");
  b_typeErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, TypeError, "TypeError", "TypeError");
  b_uriErrorPrototype = new NativeErrorPrototype(&globExec, errorProto, URIError, "URIError", "URIError");

  // Error objects
  b_evalError = new NativeErrorImp(&globExec, funcProto, b_evalErrorPrototype);
  b_rangeError = new NativeErrorImp(&globExec, funcProto, b_rangeErrorPrototype);
  b_referenceError = new NativeErrorImp(&globExec, funcProto, b_referenceErrorPrototype);
  b_syntaxError = new NativeErrorImp(&globExec, funcProto, b_syntaxErrorPrototype);
  b_typeError = new NativeErrorImp(&globExec, funcProto, b_typeErrorPrototype);
  b_uriError = new NativeErrorImp(&globExec, funcProto, b_uriErrorPrototype);

  // ECMA 15.3.4.1
  funcProto->put(&globExec, constructorPropertyName, b_Function, DontEnum);

  global->put(&globExec, "Object", b_Object, DontEnum);
  global->put(&globExec, "Function", b_Function, DontEnum);
  global->put(&globExec, "Array", b_Array, DontEnum);
  global->put(&globExec, "Boolean", b_Boolean, DontEnum);
  global->put(&globExec, "String", b_String, DontEnum);
  global->put(&globExec, "Number", b_Number, DontEnum);
  global->put(&globExec, "Date", b_Date, DontEnum);
  global->put(&globExec, "RegExp", b_RegExp, DontEnum);
  global->put(&globExec, "Error", b_Error, DontEnum);
  // Using Internal for those to have something != 0
  // (see kjs_window). Maybe DontEnum would be ok too ?
  global->put(&globExec, "EvalError",b_evalError, Internal);
  global->put(&globExec, "RangeError",b_rangeError, Internal);
  global->put(&globExec, "ReferenceError",b_referenceError, Internal);
  global->put(&globExec, "SyntaxError",b_syntaxError, Internal);
  global->put(&globExec, "TypeError",b_typeError, Internal);
  global->put(&globExec, "URIError",b_uriError, Internal);

  // Set the constructorPropertyName property of all builtin constructors
  objProto->put(&globExec, constructorPropertyName, b_Object, DontEnum | DontDelete | ReadOnly);
  funcProto->put(&globExec, constructorPropertyName, b_Function, DontEnum | DontDelete | ReadOnly);
  arrayProto->put(&globExec, constructorPropertyName, b_Array, DontEnum | DontDelete | ReadOnly);
  booleanProto->put(&globExec, constructorPropertyName, b_Boolean, DontEnum | DontDelete | ReadOnly);
  stringProto->put(&globExec, constructorPropertyName, b_String, DontEnum | DontDelete | ReadOnly);
  numberProto->put(&globExec, constructorPropertyName, b_Number, DontEnum | DontDelete | ReadOnly);
  dateProto->put(&globExec, constructorPropertyName, b_Date, DontEnum | DontDelete | ReadOnly);
  regexpProto->put(&globExec, constructorPropertyName, b_RegExp, DontEnum | DontDelete | ReadOnly);
  errorProto->put(&globExec, constructorPropertyName, b_Error, DontEnum | DontDelete | ReadOnly);
  b_evalErrorPrototype->put(&globExec, constructorPropertyName, b_evalError, DontEnum | DontDelete | ReadOnly);
  b_rangeErrorPrototype->put(&globExec, constructorPropertyName, b_rangeError, DontEnum | DontDelete | ReadOnly);
  b_referenceErrorPrototype->put(&globExec, constructorPropertyName, b_referenceError, DontEnum | DontDelete | ReadOnly);
  b_syntaxErrorPrototype->put(&globExec, constructorPropertyName, b_syntaxError, DontEnum | DontDelete | ReadOnly);
  b_typeErrorPrototype->put(&globExec, constructorPropertyName, b_typeError, DontEnum | DontDelete | ReadOnly);
  b_uriErrorPrototype->put(&globExec, constructorPropertyName, b_uriError, DontEnum | DontDelete | ReadOnly);

  // built-in values
  global->put(&globExec, "NaN",        jsNaN(), DontEnum|DontDelete);
  global->put(&globExec, "Infinity",   jsNumber(Inf), DontEnum|DontDelete);
  global->put(&globExec, "undefined",  jsUndefined(), DontEnum|DontDelete);

  // built-in functions
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::Eval, 1, "eval"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::ParseInt, 2, "parseInt"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::ParseFloat, 1, "parseFloat"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::IsNaN, 1, "isNaN"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::IsFinite, 1, "isFinite"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::Escape, 1, "escape"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::UnEscape, 1, "unescape"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::DecodeURI, 1, "decodeURI"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::DecodeURIComponent, 1, "decodeURIComponent"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::EncodeURI, 1, "encodeURI"), DontEnum);
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::EncodeURIComponent, 1, "encodeURIComponent"), DontEnum);
#ifndef NDEBUG
  global->putDirectFunction(new GlobalFuncImp(&globExec, funcProto, GlobalFuncImp::KJSPrint, 1, "kjsprint"), DontEnum);
#endif

  // built-in objects
  global->put(&globExec, "Math", new MathObjectImp(&globExec, objProto), DontEnum);
}

InterpreterImp::~InterpreterImp()
{
  if (dbg)
    dbg->detach(m_interpreter);
  clear();
}

void InterpreterImp::clear()
{
  //fprintf(stderr,"InterpreterImp::clear\n");
  // remove from global chain (see init())
  JSLock lock;

  next->prev = prev;
  prev->next = next;
  s_hook = next;
  if (s_hook == this)
  {
    // This was the last interpreter
    s_hook = 0L;
  }
  interpreterMap().remove(global);
}

void InterpreterImp::mark()
{
  if (m_interpreter)
    m_interpreter->mark();
  if (_context)
    _context->mark();
  if (global)
      global->mark();
  if (globExec._exception)
      globExec._exception->mark();
}

bool InterpreterImp::checkSyntax(const UString &code)
{
  JSLock lock;

  // Parser::parse() returns 0 in a syntax error occurs, so we just check for that
  RefPtr<ProgramNode> progNode = Parser::parse(UString(), 0, code.data(), code.size(), 0, 0, 0);
  return progNode;
}

Completion InterpreterImp::evaluate(const UChar* code, int codeLength, JSValue* thisV, const UString& sourceURL, int startingLineNumber)
{
  JSLock lock;

  // prevent against infinite recursion
  if (recursion >= 20)
    return Completion(Throw, Error::create(&globExec, GeneralError, "Recursion too deep"));

  // parse the source code
  int sid;
  int errLine;
  UString errMsg;
  RefPtr<ProgramNode> progNode = Parser::parse(sourceURL, startingLineNumber, code, codeLength, &sid, &errLine, &errMsg);

  // notify debugger that source has been parsed
  if (dbg) {
    bool cont = dbg->sourceParsed(&globExec, sid, sourceURL, UString(code, codeLength), errLine);
    if (!cont)
      return Completion(Break);
  }
  
  // no program node means a syntax error occurred
  if (!progNode)
    return Completion(Throw, Error::create(&globExec, SyntaxError, errMsg, errLine, sid, &sourceURL));

  globExec.clearException();

  recursion++;

  JSObject* globalObj = globalObject();
  JSObject* thisObj = globalObj;

  // "this" must be an object... use same rules as Function.prototype.apply()
  if (thisV && !thisV->isUndefinedOrNull())
      thisObj = thisV->toObject(&globExec);

  Completion res;
  if (globExec.hadException())
    // the thisV->toObject() conversion above might have thrown an exception - if so, propagate it
    res = Completion(Throw, globExec.exception());
  else {
    // execute the code
    ContextImp ctx(globalObj, this, thisObj);
    ExecState newExec(m_interpreter, &ctx);
    progNode->processVarDecls(&newExec);
    res = progNode->execute(&newExec);
  }

  recursion--;

  return res;
}

void InterpreterImp::saveBuiltins (SavedBuiltins &builtins) const
{
  if (!builtins._internal) {
    builtins._internal = new SavedBuiltinsInternal;
  }

  builtins._internal->b_Object = b_Object;
  builtins._internal->b_Function = b_Function;
  builtins._internal->b_Array = b_Array;
  builtins._internal->b_Boolean = b_Boolean;
  builtins._internal->b_String = b_String;
  builtins._internal->b_Number = b_Number;
  builtins._internal->b_Date = b_Date;
  builtins._internal->b_RegExp = b_RegExp;
  builtins._internal->b_Error = b_Error;
  
  builtins._internal->b_ObjectPrototype = b_ObjectPrototype;
  builtins._internal->b_FunctionPrototype = b_FunctionPrototype;
  builtins._internal->b_ArrayPrototype = b_ArrayPrototype;
  builtins._internal->b_BooleanPrototype = b_BooleanPrototype;
  builtins._internal->b_StringPrototype = b_StringPrototype;
  builtins._internal->b_NumberPrototype = b_NumberPrototype;
  builtins._internal->b_DatePrototype = b_DatePrototype;
  builtins._internal->b_RegExpPrototype = b_RegExpPrototype;
  builtins._internal->b_ErrorPrototype = b_ErrorPrototype;
  
  builtins._internal->b_evalError = b_evalError;
  builtins._internal->b_rangeError = b_rangeError;
  builtins._internal->b_referenceError = b_referenceError;
  builtins._internal->b_syntaxError = b_syntaxError;
  builtins._internal->b_typeError = b_typeError;
  builtins._internal->b_uriError = b_uriError;
  
  builtins._internal->b_evalErrorPrototype = b_evalErrorPrototype;
  builtins._internal->b_rangeErrorPrototype = b_rangeErrorPrototype;
  builtins._internal->b_referenceErrorPrototype = b_referenceErrorPrototype;
  builtins._internal->b_syntaxErrorPrototype = b_syntaxErrorPrototype;
  builtins._internal->b_typeErrorPrototype = b_typeErrorPrototype;
  builtins._internal->b_uriErrorPrototype = b_uriErrorPrototype;
}

void InterpreterImp::restoreBuiltins (const SavedBuiltins &builtins)
{
  if (!builtins._internal) {
    return;
  }

  b_Object = builtins._internal->b_Object;
  b_Function = builtins._internal->b_Function;
  b_Array = builtins._internal->b_Array;
  b_Boolean = builtins._internal->b_Boolean;
  b_String = builtins._internal->b_String;
  b_Number = builtins._internal->b_Number;
  b_Date = builtins._internal->b_Date;
  b_RegExp = builtins._internal->b_RegExp;
  b_Error = builtins._internal->b_Error;
  
  b_ObjectPrototype = builtins._internal->b_ObjectPrototype;
  b_FunctionPrototype = builtins._internal->b_FunctionPrototype;
  b_ArrayPrototype = builtins._internal->b_ArrayPrototype;
  b_BooleanPrototype = builtins._internal->b_BooleanPrototype;
  b_StringPrototype = builtins._internal->b_StringPrototype;
  b_NumberPrototype = builtins._internal->b_NumberPrototype;
  b_DatePrototype = builtins._internal->b_DatePrototype;
  b_RegExpPrototype = builtins._internal->b_RegExpPrototype;
  b_ErrorPrototype = builtins._internal->b_ErrorPrototype;
  
  b_evalError = builtins._internal->b_evalError;
  b_rangeError = builtins._internal->b_rangeError;
  b_referenceError = builtins._internal->b_referenceError;
  b_syntaxError = builtins._internal->b_syntaxError;
  b_typeError = builtins._internal->b_typeError;
  b_uriError = builtins._internal->b_uriError;
  
  b_evalErrorPrototype = builtins._internal->b_evalErrorPrototype;
  b_rangeErrorPrototype = builtins._internal->b_rangeErrorPrototype;
  b_referenceErrorPrototype = builtins._internal->b_referenceErrorPrototype;
  b_syntaxErrorPrototype = builtins._internal->b_syntaxErrorPrototype;
  b_typeErrorPrototype = builtins._internal->b_typeErrorPrototype;
  b_uriErrorPrototype = builtins._internal->b_uriErrorPrototype;
}

InterpreterImp *InterpreterImp::interpreterWithGlobalObject(JSObject *global)
{
    return interpreterMap().get(global);
}


// ------------------------------ InternalFunctionImp --------------------------

const ClassInfo InternalFunctionImp::info = {"Function", 0, 0, 0};

InternalFunctionImp::InternalFunctionImp()
{
}

InternalFunctionImp::InternalFunctionImp(FunctionPrototype* funcProto)
  : JSObject(funcProto)
{
}

InternalFunctionImp::InternalFunctionImp(FunctionPrototype* funcProto, const Identifier& name)
  : JSObject(funcProto)
  , m_name(name)
{
}

bool InternalFunctionImp::implementsHasCall() const
{
  return true;
}

bool InternalFunctionImp::implementsHasInstance() const
{
  return true;
}

bool InternalFunctionImp::hasInstance(ExecState *exec, JSValue *value)
{
  if (!value->isObject())
    return false;

  JSValue *prot = get(exec,prototypePropertyName);
  if (!prot->isObject() && !prot->isNull()) {
    throwError(exec, TypeError, "Invalid prototype encountered in instanceof operation.");
    return false;
  }

  JSObject *v = static_cast<JSObject *>(value);
  while ((v = v->prototype()->getObject())) {
    if (v == prot)
      return true;
  }
  return false;
}

// ------------------------------ global functions -----------------------------

double roundValue(ExecState *exec, JSValue *v)
{
  double d = v->toNumber(exec);
  double ad = fabs(d);
  if (ad == 0 || isNaN(d) || isInf(d))
    return d;
  return copysign(floor(ad), d);
}

#ifndef NDEBUG
#include <stdio.h>
void printInfo(ExecState *exec, const char *s, JSValue *o, int lineno)
{
  if (!o)
    fprintf(stderr, "KJS: %s: (null)", s);
  else {
    JSValue *v = o;

    UString name;
    switch (v->type()) {
    case UnspecifiedType:
      name = "Unspecified";
      break;
    case UndefinedType:
      name = "Undefined";
      break;
    case NullType:
      name = "Null";
      break;
    case BooleanType:
      name = "Boolean";
      break;
    case StringType:
      name = "String";
      break;
    case NumberType:
      name = "Number";
      break;
    case ObjectType:
      name = static_cast<JSObject *>(v)->className();
      if (name.isNull())
        name = "(unknown class)";
      break;
    case GetterSetterType:
      name = "GetterSetter";
      break;
    }
    UString vString = v->toString(exec);
    if ( vString.size() > 50 )
      vString = vString.substr( 0, 50 ) + "...";
    // Can't use two UString::ascii() in the same fprintf call
    CString tempString( vString.cstring() );

    fprintf(stderr, "KJS: %s: %s : %s (%p)",
            s, tempString.c_str(), name.ascii(), (void*)v);

    if (lineno >= 0)
      fprintf(stderr, ", line %d\n",lineno);
    else
      fprintf(stderr, "\n");
  }
}
#endif

}
