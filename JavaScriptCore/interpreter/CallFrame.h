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

#ifndef CallFrame_h
#define CallFrame_h

#include "JSGlobalData.h"
#include "RegisterFile.h"
#include "ScopeChain.h"

namespace JSC  {

    class Arguments;
    class JSActivation;
    class Interpreter;

    // Represents the current state of script execution.
    // Passed as the first argument to most functions.
    class ExecState : private Register {
    public:
        JSFunction* callee() const { return this[RegisterFile::Callee].function(); }
        CodeBlock* codeBlock() const { return this[RegisterFile::CodeBlock].Register::codeBlock(); }
        ScopeChainNode* scopeChain() const
        {
            ASSERT(this[RegisterFile::ScopeChain].Register::scopeChain());
            return this[RegisterFile::ScopeChain].Register::scopeChain();
        }
        int argumentCount() const { return this[RegisterFile::ArgumentCount].i(); }

        JSValue thisValue();

        // Global object in which execution began.
        JSGlobalObject* dynamicGlobalObject();

        // Global object in which the currently executing code was defined.
        // Differs from dynamicGlobalObject() during function calls across web browser frames.
        JSGlobalObject* lexicalGlobalObject() const
        {
            return scopeChain()->globalObject;
        }

        // Differs from lexicalGlobalObject because this will have DOM window shell rather than
        // the actual DOM window, which can't be "this" for security reasons.
        JSObject* globalThisValue() const
        {
            return scopeChain()->globalThis;
        }

        // FIXME: Elsewhere, we use JSGlobalData* rather than JSGlobalData&.
        // We should make this more uniform and either use a reference everywhere
        // or a pointer everywhere.
        JSGlobalData& globalData() const
        {
            ASSERT(scopeChain()->globalData);
            return *scopeChain()->globalData;
        }

        // Convenience functions for access to global data.
        // It takes a few memory references to get from a call frame to the global data
        // pointer, so these are inefficient, and should be used sparingly in new code.
        // But they're used in many places in legacy code, so they're not going away any time soon.

        void setException(JSValue exception) { globalData().exception = exception; }
        void clearException() { globalData().exception = JSValue(); }
        JSValue exception() const { return globalData().exception; }
        JSValue* exceptionSlot() { return &globalData().exception; }
        bool hadException() const { return globalData().exception; }

        const CommonIdentifiers& propertyNames() const { return *globalData().propertyNames; }
        const MarkedArgumentBuffer& emptyList() const { return *globalData().emptyList; }
        Interpreter* interpreter() { return globalData().interpreter; }
        Heap* heap() { return &globalData().heap; }
#ifndef NDEBUG
        void dumpCaller();
#endif
        static const HashTable* arrayTable(CallFrame* callFrame) { return callFrame->globalData().arrayTable; }
        static const HashTable* dateTable(CallFrame* callFrame) { return callFrame->globalData().dateTable; }
        static const HashTable* jsonTable(CallFrame* callFrame) { return callFrame->globalData().jsonTable; }
        static const HashTable* mathTable(CallFrame* callFrame) { return callFrame->globalData().mathTable; }
        static const HashTable* numberTable(CallFrame* callFrame) { return callFrame->globalData().numberTable; }
        static const HashTable* regExpTable(CallFrame* callFrame) { return callFrame->globalData().regExpTable; }
        static const HashTable* regExpConstructorTable(CallFrame* callFrame) { return callFrame->globalData().regExpConstructorTable; }
        static const HashTable* stringTable(CallFrame* callFrame) { return callFrame->globalData().stringTable; }

        static CallFrame* create(Register* callFrameBase) { return static_cast<CallFrame*>(callFrameBase); }
        Register* registers() { return this; }

        CallFrame& operator=(const Register& r) { *static_cast<Register*>(this) = r; return *this; }

        CallFrame* callerFrame() const { return this[RegisterFile::CallerFrame].callFrame(); }
        Arguments* optionalCalleeArguments() const { return this[RegisterFile::OptionalCalleeArguments].arguments(); }
        Instruction* returnPC() const { return this[RegisterFile::ReturnPC].vPC(); }

        void setCalleeArguments(JSValue arguments) { static_cast<Register*>(this)[RegisterFile::OptionalCalleeArguments] = arguments; }
        void setCallerFrame(CallFrame* callerFrame) { static_cast<Register*>(this)[RegisterFile::CallerFrame] = callerFrame; }
        void setScopeChain(ScopeChainNode* scopeChain) { static_cast<Register*>(this)[RegisterFile::ScopeChain] = scopeChain; }

        ALWAYS_INLINE void init(CodeBlock* codeBlock, Instruction* vPC, ScopeChainNode* scopeChain,
            CallFrame* callerFrame, int returnValueRegister, int argc, JSFunction* function)
        {
            ASSERT(callerFrame); // Use noCaller() rather than 0 for the outer host call frame caller.

            setCodeBlock(codeBlock);
            setScopeChain(scopeChain);
            setCallerFrame(callerFrame);
            static_cast<Register*>(this)[RegisterFile::ReturnPC] = vPC; // This is either an Instruction* or a pointer into JIT generated code stored as an Instruction*.
            static_cast<Register*>(this)[RegisterFile::ReturnValueRegister] = Register::withInt(returnValueRegister);
            setArgumentCount(argc); // original argument count (for the sake of the "arguments" object)
            setCallee(function);
            setCalleeArguments(JSValue());
        }

        // Read a register from the codeframe (or constant from the CodeBlock).
        inline Register& r(int);

        static CallFrame* noCaller() { return reinterpret_cast<CallFrame*>(HostCallFrameFlag); }
        int returnValueRegister() const { return this[RegisterFile::ReturnValueRegister].i(); }

        bool hasHostCallFrameFlag() const { return reinterpret_cast<intptr_t>(this) & HostCallFrameFlag; }
        CallFrame* addHostCallFrameFlag() const { return reinterpret_cast<CallFrame*>(reinterpret_cast<intptr_t>(this) | HostCallFrameFlag); }
        CallFrame* removeHostCallFrameFlag() { return reinterpret_cast<CallFrame*>(reinterpret_cast<intptr_t>(this) & ~HostCallFrameFlag); }

    private:
        void setArgumentCount(int count) { static_cast<Register*>(this)[RegisterFile::ArgumentCount] = Register::withInt(count); }
        void setCallee(JSFunction* callee) { static_cast<Register*>(this)[RegisterFile::Callee] = callee; }
        void setCodeBlock(CodeBlock* codeBlock) { static_cast<Register*>(this)[RegisterFile::CodeBlock] = codeBlock; }

        static const intptr_t HostCallFrameFlag = 1;

        ExecState();
        ~ExecState();
    };

} // namespace JSC

#endif // CallFrame_h
