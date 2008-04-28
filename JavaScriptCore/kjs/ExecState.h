// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef ExecState_h
#define ExecState_h

#include "LabelStack.h"
#include "LocalStorageEntry.h"
#include "completion.h"
#include "list.h"
#include "scope_chain.h"

namespace KJS  {

    class ActivationImp;
    class CommonIdentifiers;
    class EvalNode;
    class FunctionBodyNode;
    class FunctionImp;
    class GlobalFuncImp;
    struct HashTable;
    class Interpreter;
    class JSGlobalObject;
    class JSVariableObject;
    class ProgramNode;
    class ScopeNode;
    
    enum CodeType { GlobalCode, EvalCode, FunctionCode };

    struct PerThreadData {
        const HashTable* arrayTable;
        const HashTable* dateTable;
        const HashTable* mathTable;
        const HashTable* numberTable;
        const HashTable* RegExpImpTable;
        const HashTable* RegExpObjectImpTable;
        const HashTable* stringTable;

        CommonIdentifiers* propertyNames;
        const List emptyList;
    };

    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : Noncopyable {
        friend class JSGlobalObject;
    public:
        // Global object that was in scope when the current script started executing.
        JSGlobalObject* dynamicGlobalObject() const { return m_globalObject; }
        
        // Global object that was in scope when the current body of code was defined.
        JSGlobalObject* lexicalGlobalObject() const;
                
        void setException(JSValue* exception) { m_exception = exception; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
        JSValue* takeException() { JSValue* exception = m_exception; m_exception = 0; return exception; }
        
        const ScopeChain& scopeChain() const { return m_scopeChain; }
        void pushScope(JSObject* s) { m_scopeChain.push(s); }
        void popScope() { m_scopeChain.pop(); }
        void replaceScopeChainTop(JSObject* o) { m_scopeChain.replaceTop(o); }
        
        JSVariableObject* variableObject() const { return m_variableObject; }
        void setVariableObject(JSVariableObject* v) { m_variableObject = v; }
        
        JSObject* thisValue() const { return m_thisValue; }
        JSObject* globalThisValue() const { return m_globalThisValue; }
        
        ExecState* callingExecState() { return m_callingExec; }
        
        ActivationImp* activationObject() { return m_activation; }
        void setActivationObject(ActivationImp* a) { m_activation = a; }
        CodeType codeType() { return m_codeType; }
        ScopeNode* scopeNode() { return m_scopeNode; }
        FunctionImp* function() const { return m_function; }
        const List* arguments() const { return m_arguments; }
        
        LabelStack& seenLabels() { return m_labelStack; }
        
        void pushIteration() { m_iterationDepth++; }
        void popIteration() { m_iterationDepth--; }
        bool inIteration() const { return (m_iterationDepth > 0); }
        
        void pushSwitch() { m_switchDepth++; }
        void popSwitch() { m_switchDepth--; }
        bool inSwitch() const { return (m_switchDepth > 0); }

        // These pointers are used to avoid accessing global variables for these,
        // to avoid taking PIC branches in Mach-O binaries.
        const CommonIdentifiers& propertyNames() const { return *m_perThreadData->propertyNames; }
        const List& emptyList() const { return m_perThreadData->emptyList; }
        static const HashTable* arrayTable(ExecState* exec) { return exec->m_perThreadData->arrayTable; }
        static const HashTable* dateTable(ExecState* exec) { return exec->m_perThreadData->dateTable; }
        static const HashTable* mathTable(ExecState* exec) { return exec->m_perThreadData->mathTable; }
        static const HashTable* numberTable(ExecState* exec) { return exec->m_perThreadData->numberTable; }
        static const HashTable* RegExpImpTable(ExecState* exec) { return exec->m_perThreadData->RegExpImpTable; }
        static const HashTable* RegExpObjectImpTable(ExecState* exec) { return exec->m_perThreadData->RegExpObjectImpTable; }
        static const HashTable* stringTable(ExecState* exec) { return exec->m_perThreadData->stringTable; }

        LocalStorage& localStorage() { return *m_localStorage; }
        void setLocalStorage(LocalStorage* s) { m_localStorage = s; }

        // These are only valid right after calling execute().
        ComplType completionType() const { return m_completionType; }
        const Identifier& breakOrContinueTarget() const
        {
            ASSERT(m_completionType == Break || m_completionType == Continue);
            return *m_breakOrContinueTarget;
        }

        // Only for use in the implementation of execute().
        void setCompletionType(ComplType type)
        {
            ASSERT(type != Break);
            ASSERT(type != Continue);
            m_completionType = type;
        }
        JSValue* setNormalCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return 0;
        }
        JSValue* setNormalCompletion(JSValue* value)
        {
            ASSERT(!hadException());
            m_completionType = Normal;
            return value;
        }
        JSValue* setBreakCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Break;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setContinueCompletion(const Identifier* target)
        {
            ASSERT(!hadException());
            m_completionType = Continue;
            m_breakOrContinueTarget = target;
            return 0;
        }
        JSValue* setReturnValueCompletion(JSValue* returnValue)
        {
            ASSERT(!hadException());
            ASSERT(returnValue);
            m_completionType = ReturnValue;
            return returnValue;
        }
        JSValue* setThrowCompletion(JSValue* exception)
        {
            ASSERT(!hadException());
            ASSERT(exception);
            m_completionType = Throw;
            return exception;
        }
        JSValue* setInterruptedCompletion()
        {
            ASSERT(!hadException());
            m_completionType = Interrupted;
            return 0;
        }

    protected:
        ExecState(JSGlobalObject*, JSObject* thisObject);
        ExecState(JSGlobalObject*, JSObject* thisObject, ProgramNode*);
        ExecState(JSGlobalObject*, JSObject* thisObject, EvalNode*, ExecState* callingExecState, const ScopeChain&, JSVariableObject*);
        ExecState(JSGlobalObject*, JSObject* thisObject, JSObject* globalThisValue, FunctionBodyNode*, ExecState* callingExecState, FunctionImp*, const List& args);
        ~ExecState();

        // ExecStates are always stack-allocated, and the garbage collector
        // marks the stack, so we don't need to protect the objects below from GC.

        JSGlobalObject* m_globalObject;
        JSValue* m_exception;

        ExecState* m_callingExec;

        const PerThreadData* m_perThreadData;

        ScopeNode* m_scopeNode;
        
        FunctionImp* m_function;
        const List* m_arguments;
        ActivationImp* m_activation;
        LocalStorage* m_localStorage;

        ScopeChain m_scopeChain;
        ScopeChainNode m_inlineScopeChainNode;
        JSVariableObject* m_variableObject;

        JSObject* m_thisValue;
        JSObject* m_globalThisValue;
        
        LabelStack m_labelStack;
        int m_iterationDepth;
        int m_switchDepth;
        CodeType m_codeType;

        ComplType m_completionType;
        const Identifier* m_breakOrContinueTarget;
    };

    class GlobalExecState : public ExecState {
    public:
        GlobalExecState(JSGlobalObject*, JSObject* thisObject);
        ~GlobalExecState();
    };

    class InterpreterExecState : public ExecState {
    public:
        InterpreterExecState(JSGlobalObject*, JSObject* thisObject, ProgramNode*);
        ~InterpreterExecState();
    };

    class EvalExecState : public ExecState {
    public:
        EvalExecState(JSGlobalObject*, JSObject* thisObj, EvalNode*, ExecState* callingExec, const ScopeChain&, JSVariableObject*);
        ~EvalExecState();
    };

    class FunctionExecState : public ExecState {
    public:
        FunctionExecState(JSGlobalObject*, JSObject* thisObject, JSObject* globalThisValue, FunctionBodyNode*,
            ExecState* callingExecState, FunctionImp*, const List& args);
        ~FunctionExecState();
    };

} // namespace KJS

#endif // ExecState_h
