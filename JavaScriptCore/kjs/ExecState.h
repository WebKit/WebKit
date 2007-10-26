// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
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

#ifndef ExecState_H
#define ExecState_H

#include "value.h"
#include "types.h"
#include "CommonIdentifiers.h"
#include "LabelStack.h"
#include "scope_chain.h"

namespace KJS  {

    enum CodeType {
        GlobalCode,
        EvalCode,
        FunctionCode,
    };
    
    class ExecState;
    class JSGlobalObject;
    class ScopeChain;
    class Interpreter;
    class FunctionImp;
    class GlobalFuncImp;
    class FunctionBodyNode;
    
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
        Context(JSGlobalObject*, Interpreter*, JSObject* thisV,
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
        
        void setExecState(ExecState* exec) { m_execState = exec; }
        ExecState* execState() { return m_execState; }
        
    private:
        // Contexts are always stack-allocated, and the garbage collector
        // marks the stack, so we don't need to protect the objects below from GC.
        
        Interpreter* m_interpreter;
        Context* m_callingContext;
        Context* m_savedContext;
        FunctionBodyNode* m_currentBody;
        ExecState* m_execState;
        
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
    
    
    /**
     * Represents the current state of script execution. This object allows you
     * obtain a handle the interpreter that is currently executing the script,
     * and also the current execution context.
     */
    class ExecState {
        friend class Interpreter;
        friend class FunctionImp;
        friend class GlobalFuncImp;
    public:
        /**
         * Returns the interpreter associated with this execution state
         *
         * @return The interpreter executing the script
         */
        Interpreter* dynamicInterpreter() const { return m_interpreter; }
        
        /**
         * Returns the interpreter associated with the current scope's
         * global object
         *
         * @return The interpreter currently in scope
         */
        Interpreter* lexicalInterpreter() const;
        
        /**
         * Returns the execution context associated with this execution state
         *
         * @return The current execution state context
         */
        Context* context() const { return m_context; }
        
        void setException(JSValue* e) { m_exception = e; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
        
        // This is a workaround to avoid accessing the global variables for these identifiers in
        // important property lookup functions, to avoid taking PIC branches in Mach-O binaries
        const CommonIdentifiers& propertyNames() const { return *m_propertyNames; }
        
    private:
        ExecState(Interpreter* interp, Context* con)
            : m_interpreter(interp)
            , m_context(con)
            , m_exception(0)
            , m_propertyNames(CommonIdentifiers::shared())
        { 
        }
        Interpreter* m_interpreter;
        Context* m_context;
        JSValue* m_exception;
        CommonIdentifiers* m_propertyNames;
    };

} // namespace KJS

#endif // ExecState_H
