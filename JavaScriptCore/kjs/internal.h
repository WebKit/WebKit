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

#ifndef INTERNAL_H
#define INTERNAL_H

#include "ustring.h"
#include "object.h"
#include "protect.h"
#include "types.h"
#include "interpreter.h"
#include "scope_chain.h"
#include <kxmlcore/SharedPtr.h>

#define I18N_NOOP(s) s

namespace KJS {

  class Node;
  class ProgramNode;
  class FunctionBodyNode;
  class FunctionPrototypeImp;
  class FunctionImp;
  class Debugger;

  // ---------------------------------------------------------------------------
  //                            Primitive impls
  // ---------------------------------------------------------------------------

  class UndefinedImp : public AllocatedValueImp {
  public:
    Type type() const { return UndefinedType; }

    ValueImp *toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;
  };

  class NullImp : public AllocatedValueImp {
  public:
    Type type() const { return NullType; }

    ValueImp *toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;
  };

  class BooleanImp : public AllocatedValueImp {
  public:
    BooleanImp(bool v = false) : val(v) { }
    bool value() const { return val; }

    Type type() const { return BooleanType; }

    ValueImp *toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;

  private:
    bool val;
  };
  
  class StringImp : public AllocatedValueImp {
  public:
    StringImp(const UString& v) : val(v) { }
    UString value() const { return val; }

    Type type() const { return StringType; }

    ValueImp *toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;

  private:
    UString val;
  };

  class NumberImp : public AllocatedValueImp {
    friend class ConstantValues;
    friend class InterpreterImp;
    friend ValueImp *jsNumber(double);
  public:
    double value() const { return val; }

    Type type() const { return NumberType; }

    ValueImp *toPrimitive(ExecState *exec, Type preferred = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;

  private:
    NumberImp(double v) : val(v) { }

    virtual bool getUInt32(uint32_t&) const;

    double val;
  };
  

  /**
   * @short The "label set" in Ecma-262 spec
   */
  class LabelStack {
  public:
    LabelStack(): tos(0L), iterationDepth(0), switchDepth(0) {}
    ~LabelStack();

    /**
     * If id is not empty and is not in the stack already, puts it on top of
     * the stack and returns true, otherwise returns false
     */
    bool push(const Identifier &id);
    /**
     * Is the id in the stack?
     */
    bool contains(const Identifier &id) const;
    /**
     * Removes from the stack the last pushed id (what else?)
     */
    void pop();
    
    void pushIteration() { iterationDepth++; }
    void popIteration() { iterationDepth--; }
    bool inIteration() const { return (iterationDepth > 0); }
    
    void pushSwitch() { switchDepth++; }
    void popSwitch() { switchDepth--; }
    bool inSwitch() const { return (switchDepth > 0); }
    
  private:
    LabelStack(const LabelStack &other);
    LabelStack &operator=(const LabelStack &other);

    struct StackElem {
      Identifier id;
      StackElem *prev;
    };

    StackElem *tos;
    int iterationDepth;
    int switchDepth;
  };


  // ---------------------------------------------------------------------------
  //                            Parsing & evaluation
  // ---------------------------------------------------------------------------

  enum CodeType { GlobalCode,
		  EvalCode,
		  FunctionCode,
		  AnonymousCode };

  /**
   * @internal
   *
   * Parses ECMAScript source code and converts into ProgramNode objects, which
   * represent the root of a parse tree. This class provides a conveniant workaround
   * for the problem of the bison parser working in a static context.
   */
  class Parser {
  public:
    static SharedPtr<ProgramNode> parse(const UString &sourceURL, int startingLineNumber,
                                        const UChar *code, unsigned int length, int *sourceId = 0,
                                        int *errLine = 0, UString *errMsg = 0);
    static void accept(ProgramNode *prog);

    static void saveNewNode(Node *node);

    static int sid;
  };

  class SavedBuiltinsInternal {
    friend class InterpreterImp;
  private:
    ProtectedPtr<ObjectImp> b_Object;
    ProtectedPtr<ObjectImp> b_Function;
    ProtectedPtr<ObjectImp> b_Array;
    ProtectedPtr<ObjectImp> b_Boolean;
    ProtectedPtr<ObjectImp> b_String;
    ProtectedPtr<ObjectImp> b_Number;
    ProtectedPtr<ObjectImp> b_Date;
    ProtectedPtr<ObjectImp> b_RegExp;
    ProtectedPtr<ObjectImp> b_Error;

    ProtectedPtr<ObjectImp> b_ObjectPrototype;
    ProtectedPtr<ObjectImp> b_FunctionPrototype;
    ProtectedPtr<ObjectImp> b_ArrayPrototype;
    ProtectedPtr<ObjectImp> b_BooleanPrototype;
    ProtectedPtr<ObjectImp> b_StringPrototype;
    ProtectedPtr<ObjectImp> b_NumberPrototype;
    ProtectedPtr<ObjectImp> b_DatePrototype;
    ProtectedPtr<ObjectImp> b_RegExpPrototype;
    ProtectedPtr<ObjectImp> b_ErrorPrototype;

    ProtectedPtr<ObjectImp> b_evalError;
    ProtectedPtr<ObjectImp> b_rangeError;
    ProtectedPtr<ObjectImp> b_referenceError;
    ProtectedPtr<ObjectImp> b_syntaxError;
    ProtectedPtr<ObjectImp> b_typeError;
    ProtectedPtr<ObjectImp> b_uriError;

    ProtectedPtr<ObjectImp> b_evalErrorPrototype;
    ProtectedPtr<ObjectImp> b_rangeErrorPrototype;
    ProtectedPtr<ObjectImp> b_referenceErrorPrototype;
    ProtectedPtr<ObjectImp> b_syntaxErrorPrototype;
    ProtectedPtr<ObjectImp> b_typeErrorPrototype;
    ProtectedPtr<ObjectImp> b_uriErrorPrototype;
  };

  class InterpreterImp {
    friend class Collector;
  public:
    InterpreterImp(Interpreter *interp, ObjectImp *glob);
    ~InterpreterImp();

    ObjectImp *globalObject() { return global; }
    Interpreter *interpreter() const { return m_interpreter; }

    void initGlobalObject();

    void mark();

    ExecState *globalExec() { return &globExec; }
    bool checkSyntax(const UString &code);
    Completion evaluate(const UString &code, ValueImp *thisV, const UString &sourceURL, int startingLineNumber);
    Debugger *debugger() const { return dbg; }
    void setDebugger(Debugger *d) { dbg = d; }

    ObjectImp *builtinObject() const { return b_Object; }
    ObjectImp *builtinFunction() const { return b_Function; }
    ObjectImp *builtinArray() const { return b_Array; }
    ObjectImp *builtinBoolean() const { return b_Boolean; }
    ObjectImp *builtinString() const { return b_String; }
    ObjectImp *builtinNumber() const { return b_Number; }
    ObjectImp *builtinDate() const { return b_Date; }
    ObjectImp *builtinRegExp() const { return b_RegExp; }
    ObjectImp *builtinError() const { return b_Error; }

    ObjectImp *builtinObjectPrototype() const { return b_ObjectPrototype; }
    ObjectImp *builtinFunctionPrototype() const { return b_FunctionPrototype; }
    ObjectImp *builtinArrayPrototype() const { return b_ArrayPrototype; }
    ObjectImp *builtinBooleanPrototype() const { return b_BooleanPrototype; }
    ObjectImp *builtinStringPrototype() const { return b_StringPrototype; }
    ObjectImp *builtinNumberPrototype() const { return b_NumberPrototype; }
    ObjectImp *builtinDatePrototype() const { return b_DatePrototype; }
    ObjectImp *builtinRegExpPrototype() const { return b_RegExpPrototype; }
    ObjectImp *builtinErrorPrototype() const { return b_ErrorPrototype; }

    ObjectImp *builtinEvalError() const { return b_evalError; }
    ObjectImp *builtinRangeError() const { return b_rangeError; }
    ObjectImp *builtinReferenceError() const { return b_referenceError; }
    ObjectImp *builtinSyntaxError() const { return b_syntaxError; }
    ObjectImp *builtinTypeError() const { return b_typeError; }
    ObjectImp *builtinURIError() const { return b_uriError; }

    ObjectImp *builtinEvalErrorPrototype() const { return b_evalErrorPrototype; }
    ObjectImp *builtinRangeErrorPrototype() const { return b_rangeErrorPrototype; }
    ObjectImp *builtinReferenceErrorPrototype() const { return b_referenceErrorPrototype; }
    ObjectImp *builtinSyntaxErrorPrototype() const { return b_syntaxErrorPrototype; }
    ObjectImp *builtinTypeErrorPrototype() const { return b_typeErrorPrototype; }
    ObjectImp *builtinURIErrorPrototype() const { return b_uriErrorPrototype; }

    void setCompatMode(Interpreter::CompatMode mode) { m_compatMode = mode; }
    Interpreter::CompatMode compatMode() const { return m_compatMode; }

    // Chained list of interpreters (ring)
    static InterpreterImp* firstInterpreter() { return s_hook; }
    InterpreterImp *nextInterpreter() const { return next; }
    InterpreterImp *prevInterpreter() const { return prev; }

    static InterpreterImp *interpreterWithGlobalObject(ObjectImp *);
    
    void setContext(ContextImp *c) { _context = c; }
    ContextImp *context() const { return _context; }

    void saveBuiltins (SavedBuiltins &builtins) const;
    void restoreBuiltins (const SavedBuiltins &builtins);

  private:
    void clear();
    Interpreter *m_interpreter;
    ObjectImp *global;
    Debugger *dbg;

    // Built-in properties of the object prototype. These are accessible
    // from here even if they are replaced by js code (e.g. assigning to
    // Array.prototype)

    ProtectedPtr<ObjectImp> b_Object;
    ProtectedPtr<ObjectImp> b_Function;
    ProtectedPtr<ObjectImp> b_Array;
    ProtectedPtr<ObjectImp> b_Boolean;
    ProtectedPtr<ObjectImp> b_String;
    ProtectedPtr<ObjectImp> b_Number;
    ProtectedPtr<ObjectImp> b_Date;
    ProtectedPtr<ObjectImp> b_RegExp;
    ProtectedPtr<ObjectImp> b_Error;

    ProtectedPtr<ObjectImp> b_ObjectPrototype;
    ProtectedPtr<ObjectImp> b_FunctionPrototype;
    ProtectedPtr<ObjectImp> b_ArrayPrototype;
    ProtectedPtr<ObjectImp> b_BooleanPrototype;
    ProtectedPtr<ObjectImp> b_StringPrototype;
    ProtectedPtr<ObjectImp> b_NumberPrototype;
    ProtectedPtr<ObjectImp> b_DatePrototype;
    ProtectedPtr<ObjectImp> b_RegExpPrototype;
    ProtectedPtr<ObjectImp> b_ErrorPrototype;

    ProtectedPtr<ObjectImp> b_evalError;
    ProtectedPtr<ObjectImp> b_rangeError;
    ProtectedPtr<ObjectImp> b_referenceError;
    ProtectedPtr<ObjectImp> b_syntaxError;
    ProtectedPtr<ObjectImp> b_typeError;
    ProtectedPtr<ObjectImp> b_uriError;

    ProtectedPtr<ObjectImp> b_evalErrorPrototype;
    ProtectedPtr<ObjectImp> b_rangeErrorPrototype;
    ProtectedPtr<ObjectImp> b_referenceErrorPrototype;
    ProtectedPtr<ObjectImp> b_syntaxErrorPrototype;
    ProtectedPtr<ObjectImp> b_typeErrorPrototype;
    ProtectedPtr<ObjectImp> b_uriErrorPrototype;

    ExecState globExec;
    Interpreter::CompatMode m_compatMode;

    // Chained list of interpreters (ring) - for collector
    static InterpreterImp* s_hook;
    InterpreterImp *next, *prev;
    
    ContextImp *_context;

    int recursion;
  };

  class AttachedInterpreter;
  class DebuggerImp {
  public:

    DebuggerImp() {
      interps = 0;
      isAborted = false;
    }

    void abort() { isAborted = true; }
    bool aborted() const { return isAborted; }

    AttachedInterpreter *interps;
    bool isAborted;
  };



  class InternalFunctionImp : public ObjectImp {
  public:
    InternalFunctionImp();
    InternalFunctionImp(FunctionPrototypeImp *funcProto);
    bool implementsHasInstance() const;
    bool hasInstance(ExecState *exec, ValueImp *value);

    virtual const ClassInfo *classInfo() const { return &info; }
    static const ClassInfo info;
  };

  // helper function for toInteger, toInt32, toUInt32 and toUInt16
  double roundValue(ExecState *, ValueImp *);

#ifndef NDEBUG
  void printInfo(ExecState *exec, const char *s, ValueImp *, int lineno = -1);
#endif

inline LabelStack::~LabelStack()
{
    StackElem *prev;
    for (StackElem *e = tos; e; e = prev) {
        prev = e->prev;
        delete e;
    }
}

inline void LabelStack::pop()
{
    if (StackElem *e = tos) {
        tos = e->prev;
        delete e;
    }
}

} // namespace

#endif //  INTERNAL_H
