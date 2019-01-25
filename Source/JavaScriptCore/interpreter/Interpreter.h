/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ArgList.h"
#include "JSCJSValue.h"
#include "JSObject.h"
#include "Opcode.h"
#include "StackAlignment.h"
#include <wtf/HashMap.h>

#if ENABLE(C_LOOP)
#include "CLoopStack.h"
#endif


namespace JSC {

    class CodeBlock;
    class EvalExecutable;
    class FunctionExecutable;
    class VM;
    class JSFunction;
    class JSGlobalObject;
    class JSModuleEnvironment;
    class JSModuleRecord;
    class LLIntOffsetsExtractor;
    class ProgramExecutable;
    class ModuleProgramExecutable;
    class Register;
    class JSScope;
    class SourceCode;
    class StackFrame;
    struct CallFrameClosure;
    struct HandlerInfo;
    struct Instruction;
    struct ProtoCallFrame;

    enum DebugHookType {
        WillExecuteProgram,
        DidExecuteProgram,
        DidEnterCallFrame,
        DidReachBreakpoint,
        WillLeaveCallFrame,
        WillExecuteStatement,
        WillExecuteExpression,
    };

    enum StackFrameCodeType {
        StackFrameGlobalCode,
        StackFrameEvalCode,
        StackFrameModuleCode,
        StackFrameFunctionCode,
        StackFrameNativeCode
    };

    class Interpreter {
        WTF_MAKE_FAST_ALLOCATED;
        friend class CachedCall;
        friend class LLIntOffsetsExtractor;
        friend class JIT;
        friend class VM;

    public:
        Interpreter(VM &);
        ~Interpreter();
        
#if ENABLE(C_LOOP)
        CLoopStack& cloopStack() { return m_cloopStack; }
#endif
        
        static inline Opcode getOpcode(OpcodeID);

        static inline OpcodeID getOpcodeID(Opcode);

#if !ASSERT_DISABLED
        static bool isOpcode(Opcode);
#endif

        JSValue executeProgram(const SourceCode&, CallFrame*, JSObject* thisObj);
        JSValue executeModuleProgram(ModuleProgramExecutable*, CallFrame*, JSModuleEnvironment*);
        JSValue executeCall(CallFrame*, JSObject* function, CallType, const CallData&, JSValue thisValue, const ArgList&);
        JSObject* executeConstruct(CallFrame*, JSObject* function, ConstructType, const ConstructData&, const ArgList&, JSValue newTarget);
        JSValue execute(EvalExecutable*, CallFrame*, JSValue thisValue, JSScope*);

        void getArgumentsData(CallFrame*, JSFunction*&, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc);

        NEVER_INLINE HandlerInfo* unwind(VM&, CallFrame*&, Exception*);
        void notifyDebuggerOfExceptionToBeThrown(VM&, CallFrame*, Exception*);
        NEVER_INLINE void debug(CallFrame*, DebugHookType);
        static String stackTraceAsString(VM&, const Vector<StackFrame>&);

        static EncodedJSValue JSC_HOST_CALL constructWithNativeErrorConstructor(ExecState*);
        static EncodedJSValue JSC_HOST_CALL callNativeErrorConstructor(ExecState*);

        void getStackTrace(JSCell* owner, Vector<StackFrame>& results, size_t framesToSkip = 0, size_t maxStackSize = std::numeric_limits<size_t>::max());

    private:
        enum ExecutionFlag { Normal, InitializeAndReturn };
        
        static JSValue checkedReturn(JSValue returnValue)
        {
            ASSERT(returnValue);
            return returnValue;
        }
        
        static JSObject* checkedReturn(JSObject* returnValue)
        {
            ASSERT(returnValue);
            return returnValue;
        }

        CallFrameClosure prepareForRepeatCall(FunctionExecutable*, CallFrame*, ProtoCallFrame*, JSFunction*, int argumentCountIncludingThis, JSScope*, const ArgList&);

        JSValue execute(CallFrameClosure&);

        VM& m_vm;
#if ENABLE(C_LOOP)
        CLoopStack m_cloopStack;
#endif
        
#if ENABLE(COMPUTED_GOTO_OPCODES)
#if !USE(LLINT_EMBEDDED_OPCODE_ID) || !ASSERT_DISABLED
        static HashMap<Opcode, OpcodeID>& opcodeIDTable(); // Maps Opcode => OpcodeID.
#endif // !USE(LLINT_EMBEDDED_OPCODE_ID) || !ASSERT_DISABLED
#endif // ENABLE(COMPUTED_GOTO_OPCODES)
    };

    JSValue eval(CallFrame*);

    inline CallFrame* calleeFrameForVarargs(CallFrame* callFrame, unsigned numUsedStackSlots, unsigned argumentCountIncludingThis)
    {
        // We want the new frame to be allocated on a stack aligned offset with a stack
        // aligned size. Align the size here.
        argumentCountIncludingThis = WTF::roundUpToMultipleOf(
            stackAlignmentRegisters(),
            argumentCountIncludingThis + CallFrame::headerSizeInRegisters) - CallFrame::headerSizeInRegisters;

        // Align the frame offset here.
        unsigned paddedCalleeFrameOffset = WTF::roundUpToMultipleOf(
            stackAlignmentRegisters(),
            numUsedStackSlots + argumentCountIncludingThis + CallFrame::headerSizeInRegisters);
        return CallFrame::create(callFrame->registers() - paddedCalleeFrameOffset);
    }

    unsigned sizeOfVarargs(CallFrame* exec, JSValue arguments, uint32_t firstVarArgOffset);
    static const unsigned maxArguments = 0x10000;
    unsigned sizeFrameForVarargs(CallFrame* exec, VM&, JSValue arguments, unsigned numUsedStackSlots, uint32_t firstVarArgOffset);
    unsigned sizeFrameForForwardArguments(CallFrame* exec, VM&, unsigned numUsedStackSlots);
    void loadVarargs(CallFrame* execCaller, VirtualRegister firstElementDest, JSValue source, uint32_t offset, uint32_t length);
    void setupVarargsFrame(CallFrame* execCaller, CallFrame* execCallee, JSValue arguments, uint32_t firstVarArgOffset, uint32_t length);
    void setupVarargsFrameAndSetThis(CallFrame* execCaller, CallFrame* execCallee, JSValue thisValue, JSValue arguments, uint32_t firstVarArgOffset, uint32_t length);
    void setupForwardArgumentsFrame(CallFrame* execCaller, CallFrame* execCallee, uint32_t length);
    void setupForwardArgumentsFrameAndSetThis(CallFrame* execCaller, CallFrame* execCallee, JSValue thisValue, uint32_t length);
    
} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::DebugHookType);

} // namespace WTF
