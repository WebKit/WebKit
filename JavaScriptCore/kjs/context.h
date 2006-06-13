// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#ifndef KJS_Context_h
#define KJS_Context_h

#include "function.h"

namespace KJS  {

  /** 
   * @short Execution context.
   *
   * Represents an execution context, as specified by section 10 of the ECMA
   * spec.
   *
   * An execution context contains information about the current state of the
   * script - the scope for variable lookup, the value of "this", etc. A new
   * execution context is entered whenever global code is executed (e.g. with
   * Interpreter::evaluate()), a function is called (see
   * Object::call()), or the builtin "eval" function is executed.
   *
   * Most inheritable functions in the KJS api take a ExecState pointer as
   * their first parameter. This can be used to obtain a handle to the current
   * execution context.
   */    
  class Context {
  public:
    Context(JSObject* global, Interpreter*, JSObject* thisV, 
            FunctionBodyNode* currentBody, CodeType type = GlobalCode, 
            Context* callingContext = 0, FunctionImp* function = 0, const List* args = 0);
    ~Context();

    /**
     * Returns the scope chain for this execution context. This is used for
     * variable lookup, with the list being searched from start to end until a
     * variable is found.
     *
     * @return The execution context's scope chain
     */
    const ScopeChain& scopeChain() const { return scope; }

    /**
     * Returns the variable object for the execution context. This contains a
     * property for each variable declared in the execution context.
     *
     * @return The execution context's variable object
     */
    JSObject* variableObject() const { return m_variable; }
    void setVariableObject(JSObject* v) { m_variable = v; }

    /**
     * Returns the "this" value for the execution context. This is the value
     * returned when a script references the special variable "this". It should
     * always be an Object, unless application-specific code has passed in a
     * different type.
     *
     * The object that is used as the "this" value depends on the type of
     * execution context - for global contexts, the global object is used. For
     * function objewcts, the value is given by the caller (e.g. in the case of
     * obj.func(), obj would be the "this" value). For code executed by the
     * built-in "eval" function, the this value is the same as the calling
     * context.
     *
     * @return The execution context's "this" value
     */
    JSObject* thisValue() const { return m_thisVal; }

    /**
     * Returns the context from which the current context was invoked. For
     * global code this will be a null context (i.e. one for which
     * isNull() returns true). You should check isNull() on the returned
     * value before calling any of it's methods.
     *
     * @return The calling execution context
     */
    Context* callingContext() { return m_callingContext; }
    
    JSObject* activationObject() { return m_activation; }
    CodeType codeType() { return m_codeType; }
    FunctionBodyNode* currentBody() { return m_currentBody; }
    FunctionImp* function() const { return m_function; }
    const List* arguments() const { return m_arguments; }

    void pushScope(JSObject* s) { scope.push(s); }
    void popScope() { scope.pop(); }
    LabelStack* seenLabels() { return &ls; }

    void pushIteration() { m_iterationDepth++; }
    void popIteration() { m_iterationDepth--; }
    bool inIteration() const { return (m_iterationDepth > 0); }
    
    void pushSwitch() { m_switchDepth++; }
    void popSwitch() { m_switchDepth--; }
    bool inSwitch() const { return (m_switchDepth > 0); }
        
    void mark();

  private:
    // Contexts are always stack-allocated, and the garbage collector
    // marks the stack, so we don't need to protect the objects below from GC.

    Interpreter* m_interpreter;
    Context* m_callingContext;
    FunctionBodyNode* m_currentBody;

    FunctionImp* m_function;
    const List* m_arguments;
    JSObject* m_activation;
    
    ScopeChain scope;
    JSObject* m_variable;
    JSObject* m_thisVal;

    LabelStack ls;
    int m_iterationDepth;
    int m_switchDepth;
    CodeType m_codeType;
  };

} // namespace KJS

#endif

