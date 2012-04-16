/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef Interpreter_h
#define Interpreter_h

#include "ArgList.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSValue.h"
#include "JSObject.h"
#include "LLIntData.h"
#include "Opcode.h"
#include "RegisterFile.h"

#include <wtf/HashMap.h>

namespace JSC {

    class CodeBlock;
    class EvalExecutable;
    class ExecutableBase;
    class FunctionExecutable;
    class JSGlobalObject;
    class LLIntOffsetsExtractor;
    class ProgramExecutable;
    class Register;
    class ScopeChainNode;
    class SamplingTool;
    struct CallFrameClosure;
    struct HandlerInfo;
    struct Instruction;
    
    enum DebugHookID {
        WillExecuteProgram,
        DidExecuteProgram,
        DidEnterCallFrame,
        DidReachBreakpoint,
        WillLeaveCallFrame,
        WillExecuteStatement
    };

    enum StackFrameCodeType {
        StackFrameGlobalCode,
        StackFrameEvalCode,
        StackFrameFunctionCode,
        StackFrameNativeCode
    };

    struct StackFrame {
        Strong<JSObject> callee;
        StackFrameCodeType codeType;
        Strong<ExecutableBase> executable;
        int line;
        UString sourceURL;
        UString toString(CallFrame* callFrame) const
        {
            bool hasSourceURLInfo = !sourceURL.isNull() && !sourceURL.isEmpty();
            bool hasLineInfo = line > -1;
            String traceLine;
            JSObject* stackFrameCallee = callee.get();

            switch (codeType) {
            case StackFrameEvalCode:
                if (hasSourceURLInfo) {
                    traceLine = hasLineInfo ? String::format("eval code@%s:%d", sourceURL.ascii().data(), line) 
                                            : String::format("eval code@%s", sourceURL.ascii().data());
                } else
                    traceLine = String::format("eval code");
                break;
            case StackFrameNativeCode: {
                if (callee) {
                    UString functionName = getCalculatedDisplayName(callFrame, stackFrameCallee);
                    traceLine = String::format("%s@[native code]", functionName.ascii().data());
                } else
                    traceLine = "[native code]";
                break;
            }
            case StackFrameFunctionCode: {
                UString functionName = getCalculatedDisplayName(callFrame, stackFrameCallee);
                if (hasSourceURLInfo) {
                    traceLine = hasLineInfo ? String::format("%s@%s:%d", functionName.ascii().data(), sourceURL.ascii().data(), line)
                                            : String::format("%s@%s", functionName.ascii().data(), sourceURL.ascii().data());
                } else
                    traceLine = String::format("%s\n", functionName.ascii().data());
                break;
            }
            case StackFrameGlobalCode:
                if (hasSourceURLInfo) {
                    traceLine = hasLineInfo ? String::format("global code@%s:%d", sourceURL.ascii().data(), line)
                                            : String::format("global code@%s", sourceURL.ascii().data());
                } else
                    traceLine = String::format("global code");
                    
            }
            return traceLine.impl();
        }
    };

    class TopCallFrameSetter {
    public:
        TopCallFrameSetter(JSGlobalData& global, CallFrame* callFrame)
            : globalData(global)
            , oldCallFrame(global.topCallFrame) 
        {
            global.topCallFrame = callFrame;
        }
        
        ~TopCallFrameSetter() 
        {
            globalData.topCallFrame = oldCallFrame;
        }
    private:
        JSGlobalData& globalData;
        CallFrame* oldCallFrame;
    };
    
    class NativeCallFrameTracer {
    public:
        ALWAYS_INLINE NativeCallFrameTracer(JSGlobalData* global, CallFrame* callFrame)
        {
            ASSERT(global);
            ASSERT(callFrame);
            global->topCallFrame = callFrame;
        }
    };

    // We use a smaller reentrancy limit on iPhone because of the high amount of
    // stack space required on the web thread.
#if PLATFORM(IOS)
    enum { MaxLargeThreadReentryDepth = 64, MaxSmallThreadReentryDepth = 16 };
#else
    enum { MaxLargeThreadReentryDepth = 256, MaxSmallThreadReentryDepth = 16 };
#endif // PLATFORM(IOS)

    class Interpreter {
        WTF_MAKE_FAST_ALLOCATED;
        friend class CachedCall;
        friend class LLIntOffsetsExtractor;
        friend class JIT;
    public:
        Interpreter();
        ~Interpreter();
        
        void initialize(LLInt::Data*, bool canUseJIT);

        RegisterFile& registerFile() { return m_registerFile; }
        
        Opcode getOpcode(OpcodeID id)
        {
            ASSERT(m_initialized);
#if ENABLE(COMPUTED_GOTO_CLASSIC_INTERPRETER) || ENABLE(LLINT)
            return m_opcodeTable[id];
#else
            return id;
#endif
        }

        OpcodeID getOpcodeID(Opcode opcode)
        {
            ASSERT(m_initialized);
#if ENABLE(LLINT)
            ASSERT(isOpcode(opcode));
            return m_opcodeIDTable.get(opcode);
#elif ENABLE(COMPUTED_GOTO_CLASSIC_INTERPRETER)
            ASSERT(isOpcode(opcode));
            if (!m_classicEnabled)
                return static_cast<OpcodeID>(bitwise_cast<uintptr_t>(opcode));

            return m_opcodeIDTable.get(opcode);
#else
            return opcode;
#endif
        }
        
        bool classicEnabled()
        {
            return m_classicEnabled;
        }

        bool isOpcode(Opcode);

        JSValue execute(ProgramExecutable*, CallFrame*, ScopeChainNode*, JSObject* thisObj);
        JSValue executeCall(CallFrame*, JSObject* function, CallType, const CallData&, JSValue thisValue, const ArgList&);
        JSObject* executeConstruct(CallFrame*, JSObject* function, ConstructType, const ConstructData&, const ArgList&);
        JSValue execute(EvalExecutable*, CallFrame*, JSValue thisValue, ScopeChainNode*);
        JSValue execute(EvalExecutable*, CallFrame*, JSValue thisValue, ScopeChainNode*, int globalRegisterOffset);

        JSValue retrieveArgumentsFromVMCode(CallFrame*, JSFunction*) const;
        JSValue retrieveCallerFromVMCode(CallFrame*, JSFunction*) const;
        JS_EXPORT_PRIVATE void retrieveLastCaller(CallFrame*, int& lineNumber, intptr_t& sourceID, UString& sourceURL, JSValue& function) const;
        
        void getArgumentsData(CallFrame*, JSFunction*&, ptrdiff_t& firstParameterIndex, Register*& argv, int& argc);
        
        SamplingTool* sampler() { return m_sampler.get(); }

        NEVER_INLINE HandlerInfo* throwException(CallFrame*&, JSValue&, unsigned bytecodeOffset);
        NEVER_INLINE void debug(CallFrame*, DebugHookID, int firstLine, int lastLine);
        static const UString getTraceLine(CallFrame*, StackFrameCodeType, const UString&, int);
        JS_EXPORT_PRIVATE static void getStackTrace(JSGlobalData*, Vector<StackFrame>& results);
        static void addStackTraceIfNecessary(CallFrame*, JSObject* error);

        void dumpSampleData(ExecState* exec);
        void startSampling();
        void stopSampling();
    private:
        enum ExecutionFlag { Normal, InitializeAndReturn };

        CallFrameClosure prepareForRepeatCall(FunctionExecutable*, CallFrame*, JSFunction*, int argumentCountIncludingThis, ScopeChainNode*);
        void endRepeatCall(CallFrameClosure&);
        JSValue execute(CallFrameClosure&);

#if ENABLE(CLASSIC_INTERPRETER)
        NEVER_INLINE bool resolve(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE bool resolveSkip(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE bool resolveGlobal(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE bool resolveGlobalDynamic(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE void resolveBase(CallFrame*, Instruction* vPC);
        NEVER_INLINE bool resolveBaseAndProperty(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE bool resolveThisAndProperty(CallFrame*, Instruction*, JSValue& exceptionValue);
        NEVER_INLINE ScopeChainNode* createExceptionScope(CallFrame*, const Instruction* vPC);

        void tryCacheGetByID(CallFrame*, CodeBlock*, Instruction*, JSValue baseValue, const Identifier& propertyName, const PropertySlot&);
        void uncacheGetByID(CodeBlock*, Instruction* vPC);
        void tryCachePutByID(CallFrame*, CodeBlock*, Instruction*, JSValue baseValue, const PutPropertySlot&);
        void uncachePutByID(CodeBlock*, Instruction* vPC);        
#endif // ENABLE(CLASSIC_INTERPRETER)

        NEVER_INLINE bool unwindCallFrame(CallFrame*&, JSValue, unsigned& bytecodeOffset, CodeBlock*&);

        static ALWAYS_INLINE CallFrame* slideRegisterWindowForCall(CodeBlock*, RegisterFile*, CallFrame*, size_t registerOffset, int argc);

        static CallFrame* findFunctionCallFrameFromVMCode(CallFrame*, JSFunction*);

        JSValue privateExecute(ExecutionFlag, RegisterFile*, CallFrame*);

        void dumpCallFrame(CallFrame*);
        void dumpRegisters(CallFrame*);
        
        bool isCallBytecode(Opcode opcode) { return opcode == getOpcode(op_call) || opcode == getOpcode(op_construct) || opcode == getOpcode(op_call_eval); }

        void enableSampler();
        int m_sampleEntryDepth;
        OwnPtr<SamplingTool> m_sampler;

        int m_reentryDepth;

        RegisterFile m_registerFile;
        
#if ENABLE(LLINT)
        Opcode* m_opcodeTable; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#elif ENABLE(COMPUTED_GOTO_CLASSIC_INTERPRETER)
        Opcode m_opcodeTable[numOpcodeIDs]; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#endif

#if !ASSERT_DISABLED
        bool m_initialized;
#endif
        bool m_classicEnabled;
    };

    // This value must not be an object that would require this conversion (WebCore's global object).
    inline bool isValidThisObject(JSValue thisValue, ExecState* exec)
    {
        return !thisValue.isObject() || thisValue.toThisObject(exec) == thisValue;
    }

    inline JSValue Interpreter::execute(EvalExecutable* eval, CallFrame* callFrame, JSValue thisValue, ScopeChainNode* scopeChain)
    {
        return execute(eval, callFrame, thisValue, scopeChain, m_registerFile.size() + 1 + RegisterFile::CallFrameHeaderSize);
    }

    JSValue eval(CallFrame*);
    CallFrame* loadVarargs(CallFrame*, RegisterFile*, JSValue thisValue, JSValue arguments, int firstFreeRegister);

} // namespace JSC

#endif // Interpreter_h
