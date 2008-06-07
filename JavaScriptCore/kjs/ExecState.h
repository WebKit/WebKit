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

#include "JSGlobalData.h"
#include "LabelStack.h"
#include "LocalStorageEntry.h"
#include "completion.h"
#include "list.h"
#include "scope_chain.h"

namespace KJS  {

    class EvalNode;
    class FunctionBodyNode;
    class FunctionImp;
    class GlobalFuncImp;
    class Interpreter;
    class JSGlobalObject;
    class JSVariableObject;
    class Machine;
    class ProgramNode;
    class RegisterFile;
    class ScopeNode;

    struct Instruction;
    
    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : Noncopyable {
        friend class Machine;
        friend class DebuggerCallFrame;

    public:
        ExecState(JSGlobalObject*, JSObject* globalThisValue, ScopeChainNode* globalScopeChain);

        // Global object in which execution began.
        JSGlobalObject* dynamicGlobalObject() const { return m_globalObject; }
        
        // Global object in which the current script was defined. (Can differ
        // from dynamicGlobalObject() during function calls across frames.)
        JSGlobalObject* lexicalGlobalObject() const
        {
            return m_scopeChain->globalObject();
        }
        
        JSObject* globalThisValue() const { return m_scopeChain->globalThisObject(); }
        
        Machine* machine() const { return m_machine; }
        
        // Exception propogation.
        void setException(JSValue* exception) { m_exception = exception; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }

        JSGlobalData& globalData() { return *m_globalData; }

        IdentifierTable* identifierTable() { return m_globalData->identifierTable; }
        const CommonIdentifiers& propertyNames() const { return *m_globalData->propertyNames; }
        const List& emptyList() const { return m_globalData->emptyList; }
        Lexer* lexer() { return m_globalData->lexer; }
        Parser* parser() { return m_globalData->parser; }
        static const HashTable* arrayTable(ExecState* exec) { return exec->m_globalData->arrayTable; }
        static const HashTable* dateTable(ExecState* exec) { return exec->m_globalData->dateTable; }
        static const HashTable* mathTable(ExecState* exec) { return exec->m_globalData->mathTable; }
        static const HashTable* numberTable(ExecState* exec) { return exec->m_globalData->numberTable; }
        static const HashTable* RegExpImpTable(ExecState* exec) { return exec->m_globalData->RegExpImpTable; }
        static const HashTable* RegExpObjectImpTable(ExecState* exec) { return exec->m_globalData->RegExpObjectImpTable; }
        static const HashTable* stringTable(ExecState* exec) { return exec->m_globalData->stringTable; }

    private:
        // Default constructor required for gcc 3.
        ExecState() { }

        ExecState(ExecState*, Machine*, RegisterFile*, ScopeChainNode*, int callFrameOffset);

        bool isGlobalObject(JSObject*) const;

        JSGlobalObject* m_globalObject;
        JSObject* m_globalThisValue;

        JSValue* m_exception;

        JSGlobalData* m_globalData;

        // These values are controlled by the machine.
        ExecState* m_prev;
        Machine* m_machine;
        RegisterFile* m_registerFile;
        ScopeChainNode* m_scopeChain;
        int m_callFrameOffset; // A negative offset indicates a non-function scope.
    };

    enum CodeType { GlobalCode, EvalCode, FunctionCode };

} // namespace KJS

#endif // ExecState_h
