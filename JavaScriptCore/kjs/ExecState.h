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

    class ExecState;
    class JSValue;
    class Register;

    typedef ExecState CallFrame;

    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : private Register, Noncopyable {
    public:
        static CallFrame* create(Register* callFrameBase) { return static_cast<CallFrame*>(callFrameBase); }
        Register* registers() { return this; }

        // Global object in which execution began.
        JSGlobalObject* dynamicGlobalObject()
        {
            return Machine::scopeChain(Machine::firstCallFrame(this))->globalObject();
        }

        // Global object in which the currently executing code was defined.
        // Differs from dynamicGlobalObject() during function calls across web browser frames.
        JSGlobalObject* lexicalGlobalObject()
        {
            return Machine::scopeChain(this)->globalObject();
        }

        // Differs from lexicalGlobalObject because this will have DOM window shell rather than
        // the actual DOM window.
        JSObject* globalThisValue()
        {
            return Machine::scopeChain(this)->globalThisObject();
        }

        JSGlobalData& globalData()
        {
            return *Machine::scopeChain(this)->globalData;
        }

        // Convenience functions for access to global data.

        void setException(JSValue* exception) { globalData().exception = exception; }
        void clearException() { globalData().exception = 0; }
        JSValue* exception() { return globalData().exception; }
        JSValue** exceptionSlot() { return &globalData().exception; }
        bool hadException() { return !!globalData().exception; }

        IdentifierTable* identifierTable() { return globalData().identifierTable; }
        const CommonIdentifiers& propertyNames() { return *globalData().propertyNames; }
        const ArgList& emptyList() { return *globalData().emptyList; }
        Lexer* lexer() { return globalData().lexer; }
        Parser* parser() { return globalData().parser; }
        Machine* machine() { return globalData().machine; }
        Heap* heap() { return &globalData().heap; }

        static const HashTable* arrayTable(CallFrame* callFrame) { return callFrame->globalData().arrayTable; }
        static const HashTable* dateTable(CallFrame* callFrame) { return callFrame->globalData().dateTable; }
        static const HashTable* mathTable(CallFrame* callFrame) { return callFrame->globalData().mathTable; }
        static const HashTable* numberTable(CallFrame* callFrame) { return callFrame->globalData().numberTable; }
        static const HashTable* regExpTable(CallFrame* callFrame) { return callFrame->globalData().regExpTable; }
        static const HashTable* regExpConstructorTable(CallFrame* callFrame) { return callFrame->globalData().regExpConstructorTable; }
        static const HashTable* stringTable(CallFrame* callFrame) { return callFrame->globalData().stringTable; }

    private:
        ExecState();
        ~ExecState();
    };

} // namespace JSC

#endif // ExecState_h
