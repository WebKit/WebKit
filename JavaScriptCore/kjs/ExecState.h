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
    
    class JSGlobalObject;
    class ScopeChain;
    class Interpreter;
    class FunctionImp;
    class GlobalFuncImp;
    class FunctionBodyNode;
    class LocalStorageEntry;
    
    /**
     * Represents the current state of script execution. This is
     * passed as the first argument to most functions.
     */
    class ExecState {
        friend class Interpreter;
        friend class FunctionImp;
        friend class GlobalFuncImp;
    public:
        /**
         * Returns the interpreter currently running code
         */
        Interpreter* dynamicInterpreter() const { return m_interpreter; }
        
        /**
         * Returns the interpreter associated with the current scope's global object
         */
        Interpreter* lexicalInterpreter() const;
                
        void setException(JSValue* e) { m_exception = e; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
        
        const ScopeChain& scopeChain() const { return scope; }
        
        JSObject* variableObject() const { return m_variable; }
        void setVariableObject(JSObject* v) { m_variable = v; }
        
        JSObject* thisValue() const { return m_thisVal; }
        
        ExecState* callingExecState() { return m_callingExecState; }
        
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
        
        // This is a workaround to avoid accessing the global variables for these identifiers in
        // important property lookup functions, to avoid taking PIC branches in Mach-O binaries
        const CommonIdentifiers& propertyNames() const { return *m_propertyNames; }

        LocalStorageEntry* localStorage() { return m_localStorageBuffer; }
        void updateLocalStorage();
    
    private:
        ExecState(Interpreter* interp, JSGlobalObject* glob, JSObject* thisV,
                  FunctionBodyNode* currentBody, CodeType type = GlobalCode,
                  ExecState* callingExecState = 0, FunctionImp* function = 0, const List* args = 0);
        ~ExecState();

        // ExecStates are always stack-allocated, and the garbage collector
        // marks the stack, so we don't need to protect the objects below from GC.

        Interpreter* m_interpreter;
        JSValue* m_exception;
        CommonIdentifiers* m_propertyNames;

        ExecState* m_callingExecState;
        ExecState* m_savedExecState;
        FunctionBodyNode* m_currentBody;
        
        FunctionImp* m_function;
        const List* m_arguments;
        JSObject* m_activation;
        LocalStorageEntry* m_localStorageBuffer;

        ScopeChain scope;
        JSObject* m_variable;
        JSObject* m_thisVal;
        
        LabelStack ls;
        int m_iterationDepth;
        int m_switchDepth;
        CodeType m_codeType;
    };

} // namespace KJS

#endif // ExecState_H
