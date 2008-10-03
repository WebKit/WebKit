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
#include "Machine.h"
#include "ScopeChain.h"

namespace JSC  {

    class JSValue;
    class Register;

    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : Noncopyable {
#if ENABLE(CTI)
        friend class CTI;
#endif
        friend class Machine;
        friend class DebuggerCallFrame;
    public:
        explicit ExecState(Register* callFrame)
            : m_exception(0)
            , m_callFrame(callFrame)
        {
        }

        // Global object in which execution began.
        JSGlobalObject* dynamicGlobalObject() const
        {
            return Machine::scopeChain(Machine::firstCallFrame(m_callFrame))->globalObject();
        }

        // Global object in which the current script was defined. (Can differ
        // from dynamicGlobalObject() during function calls across frames.)
        JSGlobalObject* lexicalGlobalObject() const
        {
            return Machine::scopeChain(m_callFrame)->globalObject();
        }

        JSObject* globalThisValue() const
        {
            return Machine::scopeChain(m_callFrame)->globalThisObject();
        }

        // Exception propogation.
        void setException(JSValue* exception) { m_exception = exception; }
        void clearException() { m_exception = 0; }
        JSValue* exception() const { return m_exception; }
        JSValue** exceptionSlot() { return &m_exception; }
        bool hadException() const { return !!m_exception; }
#if ENABLE(CTI)
        void setCTIReturnAddress(void* ctiRA) { m_ctiReturnAddress = ctiRA; }
        void* ctiReturnAddress() const { return m_ctiReturnAddress; }
#endif

        JSGlobalData& globalData() const
        {
            return *Machine::scopeChain(m_callFrame)->globalData;
        }

        IdentifierTable* identifierTable() { return globalData().identifierTable; }
        const CommonIdentifiers& propertyNames() const { return *globalData().propertyNames; }
        const ArgList& emptyList() const { return *globalData().emptyList; }
        Lexer* lexer() { return globalData().lexer; }
        Parser* parser() { return globalData().parser; }
        Machine* machine() const { return globalData().machine; }
        static const HashTable* arrayTable(ExecState* exec) { return exec->globalData().arrayTable; }
        static const HashTable* dateTable(ExecState* exec) { return exec->globalData().dateTable; }
        static const HashTable* mathTable(ExecState* exec) { return exec->globalData().mathTable; }
        static const HashTable* numberTable(ExecState* exec) { return exec->globalData().numberTable; }
        static const HashTable* regExpTable(ExecState* exec) { return exec->globalData().regExpTable; }
        static const HashTable* regExpConstructorTable(ExecState* exec) { return exec->globalData().regExpConstructorTable; }
        static const HashTable* stringTable(ExecState* exec) { return exec->globalData().stringTable; }

        Heap* heap() const { return &globalData().heap; }

    private:
        // Default constructor required for gcc 3.
        ExecState() { }

        bool isGlobalObject(JSObject*) const;

        JSValue* m_exception;
#if ENABLE(CTI)
        void* m_ctiReturnAddress;
#endif
        Register* m_callFrame; // The most recent call frame.
    };

} // namespace JSC

#endif // ExecState_h
